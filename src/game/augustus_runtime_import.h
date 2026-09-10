#pragma once

#include "game/augustus_common_records.h"
#include "game/augustus_model_bridge.h"
#include "game/augustus_accounting_bridge.h"
#include "game/augustus_city_bridge.h"
#include "game/augustus_scenario_bridge.h"
#include "game/augustus_monument_bridge.h"
#include "game/augustus_empire_bridge.h"
#include "scenario/definition_overrides.h"
#include "city/houses.h"
#include "building/production_method_registry.h"
#include "city/trade_ledger.h"
#include "figure/trader.h"
#include "figure/route.h"
#include "building/BuildingCityService.h"

// Private runtime-import context. The producer identity stays intact; the
// reviewed common wire fields are separate from source-only semantic payloads.
struct AugustusImport {
    AugustusArchive archive;
    augustus_save::CommonRecords records;
    std::vector<augustus_save::ModelException> models;
    std::vector<augustus_save::ConstructionException> construction;
    augustus_save::AccountingArchive accounting;
    augustus_save::TradeRouteArchive routes;
    augustus_save::CityRecord city;
    augustus_save::ScenarioRecords scenario;
    augustus_save::EmpireRecords empire;
    std::map<std::string, std::vector<uint8_t>> common_pieces;
    std::map<std::string, buffer> buffers;
    savegame_state state{};

    bool prepare(const buffer &source, std::string &diagnostic)
    {
        using namespace augustus_save;
        if (!game_file_io_decode_augustus_archive(source.data, source.size, archive, diagnostic) ||
            !decode_common_records(archive, [](const char *name) { return figure_type_from_xml_name(name); }, records, diagnostic) ||
            !decode_model_exceptions(archive, models, diagnostic) || !decode_accounting(archive, accounting, diagnostic) ||
            !decode_trade_routes(archive, routes, diagnostic) || !decode_city(archive, city, diagnostic) ||
            !decode_scenario_records(archive, ActionOrder::Unspecified, scenario, diagnostic) || !decode_construction_exceptions(archive, construction, diagnostic) || !decode_empire(archive, empire, diagnostic)) return false;
        common_pieces = archive.pieces;
        auto &pieces = common_pieces;
        pieces["buildings"] = records.buildings;
        pieces["figures"] = records.figures;
        pieces["city_data"] = city.common;
        pieces["scenario_actions"] = scenario.actions;
        pieces["scenario_formulas"] = scenario.formulas;
        pieces["custom_empire"] = empire.common;
        auto &route_bytes = pieces["trade_routes"];
        route_bytes.assign(4 + routes.current.size() * 22 * 16, 0);
        write_u32(route_bytes, 0, static_cast<uint32_t>(routes.current.size()));
        size_t at = 4;
        for (const auto &route : routes.current) for (int direction = 0; direction < 2; ++direction) for (int r = 0; r < 22; ++r) {
            write_u32(route_bytes, at, route.values[direction * 2][r]);
            write_u32(route_bytes, at + 4, route.values[direction * 2 + 1][r]); at += 8;
        }
        // Every common field is selected by name, never by source piece index.
        // This constant identifies the reviewed common-reader wire format; it
        // is never written into, or substituted for, the archive's origin.
        bool valid = true;
        visit_native_savegame_layout(SAVE_GAME_LAST_NO_MOD_METADATA, 22, 6, [&](size_t offset, const char *name, int, int) {
            const std::string key = std::string(name) == "building_model_data" ? "model_data" : name;
            const auto found = pieces.find(key);
            if (found == pieces.end()) { diagnostic = "Missing common Augustus piece: " + key; valid = false; return; }
            auto &view = buffers[key];
            buffer_init(&view, found->second.data(), found->second.size());
            *reinterpret_cast<buffer **>(reinterpret_cast<uint8_t *>(&state) + offset) = &view;
        });
        return valid;
    }

    bool apply_models()
    {
        constexpr int model_building::*fields[] = {&model_building::cost, &model_building::desirability_value,
            &model_building::desirability_step, &model_building::desirability_step_size, &model_building::desirability_range, &model_building::laborers};
        for (const auto &entry : models) {
            const auto type = building_type_id_bridge_runtime_from_text(entry.target.c_str());
            if (type == BUILDING_NONE) return false;
            if (entry.housing) {
                if (!scenario_house_model_change(type, entry.field, entry.value, true)) return false;
                if (entry.field == 15) {
                    const auto merged = building_type_id_bridge_runtime_from_text((entry.target + "_2x2").c_str());
                    if (building_type_registry_impl::definition_for_type(merged) && !scenario_house_model_change(merged, 15, static_cast<int>(std::clamp<int64_t>(int64_t(entry.value) * 4, 0, INT_MAX)), true)) return false;
                }
            } else {
                model_get_building(type)->*fields[entry.field] = entry.value;
                model_mark_scenario_override(type, entry.field);
            }
        }
        for (const auto &entry : construction) {
            const auto type = building_type_id_bridge_runtime_from_text(entry.building.c_str());
            if (type == BUILDING_NONE || !scenario_construction_requirement_change(type, entry.phase, resource_remap(entry.resource), entry.amount)) return false;
        }
        if (city.immigration_percent != 100 && !scenario_definition_override_set({ScenarioOverrideKind::Migration, {}, 1, {}, city.immigration_percent})) return false;
        if (city.emigration_percent != 100 && !scenario_definition_override_set({ScenarioOverrideKind::Migration, {}, 0, {}, city.emigration_percent})) return false;
        for (const auto &[id, text] : scenario.texts) if (!scenario_definition_override_set({ScenarioOverrideKind::Text, std::to_string(id), 0, {}, 0, text})) return false;
        return true;
    }

    bool apply_production()
    {
        // Source resource.c writes slots 1..23 consecutively into a 48-byte
        // allocation. The final two bytes are unwritten; no resource ledger is
        // stored. These reviewed producer identities/defaults are bridge-only.
        constexpr const char *names[] = {"wheat", "vegetables", "fruit", "meat", "fish", "clay", "timber", "olives", "vines", "iron", "marble", "gold", "sand", "stone", "pottery", "furniture", "oil", "wine", "weapons", "concrete", "bricks", "denarii", "troops"};
        constexpr int defaults[] = {160, 80, 80, 80, 100, 80, 80, 80, 80, 80, 40, 20, 120, 80, 40, 40, 40, 40, 40, 120, 60, 200, 100};
        const auto &bytes = archive.pieces.at("production_rates");
        if (bytes.size() != 48) return false;
        for (size_t slot = 0; slot < std::size(names); ++slot) {
            const int value = augustus_save::read_u16(bytes, slot * 2);
            const int baseline = slot == 22 && archive.origin.save_version < 184 ? 0 : defaults[slot];
            if (value == baseline) continue;
            if (slot == 22) {
                if (!production_method_registry_import_recruitment_delay(value)) return false;
                continue;
            }
            const auto resource = resource_type_from_text_id(names[slot]);
            if (resource == RESOURCE_NONE || !production_method_registry_set_production_per_month_for_resource(resource, value)) return false;
        }
        return true;
    }

    bool apply_empire()
    {
        for (const auto &entry : empire.exceptions) {
            const auto route = std::to_string(entry.route);
            if (entry.hidden && !scenario_definition_override_set({ScenarioOverrideKind::HiddenRoute, route, 0, {}, 1})) return false;
            for (int source = 1; source < 22; ++source) if (entry.costs[source]) {
                const auto resource = resource_remap(source);
                if (resource == RESOURCE_NONE || !scenario_definition_override_set({ScenarioOverrideKind::RouteResource, route, 0, resource_text_id(resource), static_cast<int>(entry.costs[source])})) return false;
            }
        }
        return true;
    }

    void bind_event_value_domains()
    {
        // Source house levels share their capacity with four merged lots. The
        // native definitions name those separately. Domain mappings preserve
        // source ordinal set/add semantics without reevaluating random formulas.
        for (int id = 0; id < scenario_events_get_count(); ++id) {
            auto *event = scenario_event_get(id);
            if (!event) continue;
            for (auto &action : event->actions) {
                if (action.type == ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA && action.parameter2 == 3) action.value_domain = {0, 1, 3, 2};
                if (action.type != ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA || action.parameter2 != 15) continue;
                const char *name = building_type_id_bridge_text_from_runtime(static_cast<building_type>(action.parameter1));
                if (!name) continue;
                const auto merged = building_type_id_bridge_runtime_from_text((std::string(name) + "_2x2").c_str());
                if (!building_type_registry_impl::definition_for_type(merged)) continue;
                action.model_targets.push_back({merged, 4});
            }
        }
    }

    void repair_trader_references()
    {
        std::array<bool, 100> occupied{};
        for (const auto &figure : records.source_figures) {
            if (figure.state && figure.trader < occupied.size() && (figure.type == 19 || figure.type == 20 || figure.type == 21 || figure.type == 38 || figure.type == 58)) occupied[figure.trader] = true;
        }
        std::map<uint16_t, int> replacements;
        for (size_t id : records.invalid_trader_references) {
            auto *figure = Figure::get(static_cast<unsigned int>(id));
            if (!figure || !figure->state) continue;
            const auto source = records.source_figures[id].trader;
            auto found = replacements.find(source);
            if (found == replacements.end()) {
                const auto free = std::find(occupied.begin(), occupied.end(), false);
                const int slot = free == occupied.end() ? -1 : static_cast<int>(free - occupied.begin());
                if (slot >= 0) { occupied[slot] = true; trader_reset_import_slot(slot); }
                found = replacements.emplace(source, slot).first;
            }
            if (found->second < 0) {
                Logger::warning("Removing unrecoverable imported trader: every accounting slot is occupied", nullptr, static_cast<int>(id));
                figure->remove();
            } else {
                figure->trader_id = static_cast<unsigned char>(found->second);
                Logger::warningf("Repairing imported trader reference: figure=%zu source=%u new_slot=%d", id, static_cast<unsigned int>(source), found->second);
            }
        }
    }

    void bind_service_deliveries()
    {
        for (size_t id = 1; id < records.source_figures.size(); ++id) {
            auto *figure = Figure::get(static_cast<unsigned int>(id));
            if (!figure || figure->state != FIGURE_STATE_ALIVE) continue;
            const auto &source = records.source_figures[id];
            if (source.type == 99) {
                // Global-stockpile consumers no longer send a supplier to fetch
                // stock. Finish cargo already collected, retire unstarted trips.
                const int trip_state = figure->action_state == FIGURE_ACTION_150_ATTACK ? figure->action_state_before_attack : figure->action_state;
                if (trip_state != FIGURE_ACTION_146_SUPPLIER_RETURNING || !figure->collecting_item_id || !figure->building) { figure->remove(); continue; }
                figure->set_destination_building(figure->building);
                if (!figure->loads_sold_or_carrying) figure->loads_sold_or_carrying = 1;
            } else if (figure->type == FIGURE_WORK_CAMP_WORKER) {
                if (figure->action_state == 251) {
                    figure->action_state = FIGURE_ACTION_203_WORK_CAMP_WORKER_CREATED;
                    figure->set_destination_building(nullptr); Route::remove(figure);
                } else if (figure->action_state == 252) {
                    figure->action_state = FIGURE_ACTION_205_WORK_CAMP_WORKER_GOING_TO_MONUMENT;
                    if (!figure->loads_sold_or_carrying) figure->loads_sold_or_carrying = 1;
                }
            }
            if ((figure->type == FIGURE_WORK_CAMP_WORKER || figure->type == FIGURE_WORK_CAMP_SLAVE) && figure->destination_building && BuildingCityService(*figure->destination_building).definition() && figure->loads_sold_or_carrying) {
                building_monument_add_delivery(figure->destination_building->id, figure->id(), figure->collecting_item_id, figure->loads_sold_or_carrying);
            }
        }
    }

    void apply_accounting()
    {
        if (archive.origin.save_version < 182) { city_trade_ledger_reset(true); return; }
        std::vector<AccountingPeriod> history(std::max<size_t>({size_t(accounting.history_years) + 1, size_t(accounting.finance_years) + 1, size_t(routes.years) + 1, accounting.transactions[1].empty() ? 1u : 2u}));
        const int units = resource_units_per_load();
        size_t uncertain_transactions = 0;
        for (size_t age = 0; age < history.size(); ++age) {
            auto &period = history[age];
            period.year = game_time_year() - static_cast<int>(age);
            period.partial = true; // The source has no gross resource cash flows or native visit identities.
            if (age <= accounting.history_years) {
                const auto &source = accounting.years[age];
                if (source.year) period.year = source.year;
                for (int r = 1; r < 22; ++r) {
                    const auto resource = resource_remap(r);
                    if (resource == RESOURCE_NONE) continue;
                    auto &amount = period.resources[resource_text_id(resource)];
                    amount.stock = int64_t(source.resources[0][r]) * units;
                    amount.imported = int64_t(source.resources[1][r]) * units;
                    amount.exported = int64_t(source.resources[2][r]) * units;
                    amount.produced = int64_t(source.resources[3][r]) * units;
                    amount.consumed = source.resources[4][r];
                    amount.balance_adjustment = source.resources[5][r];
                    amount.complete_cash_flows = false;
                }
            }
            if (age && age <= accounting.finance_years) {
                const auto &f = accounting.finances[age - 1];
                auto &target = period.finance;
                target.income = {f[0], f[1], f[2], f[3]};
                target.expenses = {f[4], f[5], f[6], f[7], f[8], f[9], f[10], f[11], f[12]};
                target.net_in_out = f[13]; target.balance = f[14];
            } else if (age == 1) period.finance = *city_finance_overview_last_year();
            if (age < accounting.transactions.size()) for (const auto &source : accounting.transactions[age]) {
                const auto resource = resource_remap(source.resource);
                if (resource == RESOURCE_NONE) continue;
                TradeTransaction entry;
                entry.resource = resource_text_id(resource);
                entry.source_trader = source.trader; entry.source_storage = source.storage;
                entry.city = source.city; entry.month = source.month; entry.price = source.price;
                // The source writer initializes and aggregates opposite signs.
                // Retain its signed quantity, but never invent an import/export
                // direction or use this entry to reconstruct cash-flow totals.
                entry.units = int64_t(source.quantity) * units; entry.direction_known = false;
                period.transactions.push_back(std::move(entry)); ++uncertain_transactions;
            }
            const auto *source_routes = age == 0 ? &routes.current : age <= routes.years ? &routes.history[age - 1] : nullptr;
            if (!source_routes) continue;
            for (int city_id = 1; city_id < empire_city_get_array_size(); ++city_id) {
                const auto *trade_city = empire_city_get(city_id);
                if (!trade_city->in_use || trade_city->type != EMPIRE_CITY_TRADE || trade_city->route_id < 0 || size_t(trade_city->route_id) >= source_routes->size()) continue;
                const auto &source = (*source_routes)[trade_city->route_id];
                auto &route = period.routes[city_id];
                route.open = source.open; route.sea = trade_city->is_sea_trade != 0; route.cost = age ? -1 : trade_city->cost_to_open;
                for (int r = 1; r < 22; ++r) {
                    const auto resource = resource_remap(r);
                    if (resource == RESOURCE_NONE) continue;
                    route.resources[resource_text_id(resource)] = {source.values[0][r], source.values[2][r], source.values[1][r], source.values[3][r]};
                }
            }
        }
        if (uncertain_transactions) Logger::warning("Preserving ambiguous Augustus transaction signs; cash-flow directions remain unknown", nullptr, static_cast<int>(uncertain_transactions));
        city_trade_ledger_import(std::move(history));
    }
};
