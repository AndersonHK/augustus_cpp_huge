#include "terrain.h"
#pragma once
#include "scenario_overrides.h"
#include "archive_origin.h"
#include "fort_orientation.h"
#include "april.h"
#include "building/BuildingCityService.h"
#include "building/BuildingFoundation.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/figure_runtime_native.h"
#include <memory>
#include <array>
#include "building/BuildingComposition.h"
#include "building/production.h"
#include "building/production_method_registry.h"
#include "building/warehouse.h"
#include "building/house_evolution.h"
#include "building/HousingModule.h"
#include "city/data_private.h"
#include "city/god.h"
#include "religion.h"
#include "city/resource.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/monument.h"
#include "city/monument_gifts.h"
#include "city/trade_ledger.h"
#include "core/buffer.h"
#include "game/time.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "map/TerrainMap.h"
#include "window/advisors.h"
#include "window/city.h"
#include "window/building/common.h"
#include "window/building_info_screen.h"
#include "city/message.h"
#include "window/empire.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "input/mouse.h"
#include "SDL.h"
#include <filesystem>
#include "window/epithets.h"
#include "window/trade_ledger.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include "assets/image_group_payload.h"
#include "placement_supports.h"
#include "cart_criminal_graphics.h"
#include "presentation.h"

inline bool run_catch_up_runtime_test()
{
    using namespace building_type_registry_impl;
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    std::vector<uint8_t> accounting_backup(64 * 1024 * 1024), gift_backup(65536), time_backup(32);
    buffer accounting, gifts, time;
    buffer_init(&accounting, accounting_backup.data(), accounting_backup.size()); city_trade_ledger_save(&accounting);
    buffer_init(&gifts, gift_backup.data(), gift_backup.size()); city_monument_gifts_save(&gifts);
    buffer_init(&time, time_backup.data(), time_backup.size()); game_time_save_state(&time);
    bool success = true;
    std::vector<std::pair<int, TerrainSet>> terrain;
    try {
        validate_fort_orientation();
        validate_april_placement_and_counters();
        {
            struct RestoreDemands { house_demands saved = *city_houses_demands(); ~RestoreDemands() { *city_houses_demands() = saved; } } restore_demands;
            city_houses_reset_demands();
            for (int requirement = 1; requirement <= 5; ++requirement) {
                require(!city_houses_check_religion_requirement(requirement, requirement - 1), "Housing accepted fewer religions than its definition requires");
                require(city_houses_check_religion_requirement(requirement, requirement), "Housing rejected its exact religion requirement");
            }
            require(city_houses_demands()->missing.fourth_religion == 1 && city_houses_demands()->missing.fifth_religion == 1 && city_houses_demands()->requiring.religion == 5, "Religion demand counters omitted fourth/fifth requirements");
            city_houses_demands()->missing.third_religion = 7;
            city_houses_demands()->missing.fourth_religion = 17;
            city_houses_demands()->missing.fifth_religion = 9;
            city_houses_calculate_culture_demands();
            require(city_houses_demands()->religion == 4, "Smaller fifth-religion shortage displaced the largest unmet demand");
            city_houses_demands()->missing.fifth_religion = 20;
            city_houses_calculate_culture_demands();
            require(city_houses_demands()->religion == 5, "Fifth-religion demand was ignored");
            city_houses_reset_demands();
            require(!city_houses_demands()->missing.fourth_religion && !city_houses_demands()->missing.fifth_religion, "Religion demands accumulated across recalculations");
        }
        {
            const auto *house = definition_for_type(type_from_attr("house_small_tent"));
            require(house && house->housing_def().profile, "Religion evolution fixture needs a house definition");
            const auto *target = house->housing_def().transition(HousingTransitionKind::EvolveTo).type;
            require(target && target->housing_def().profile, "Religion evolution fixture needs an evolution target");
            auto *profile = const_cast<HousingProfileDef *>(house->housing_def().profile);
            auto *target_profile = const_cast<HousingProfileDef *>(target->housing_def().profile);
            struct RestoreProfiles {
                HousingProfileDef *first, *second;
                HousingProfileDef first_saved, second_saved;
                ~RestoreProfiles() { *first = first_saved; *second = second_saved; }
            } restore_profiles{profile, target_profile, *profile, *target_profile};
            profile->requirements = {}; target_profile->requirements = {};
            profile->evolution.devolve_desirability = -100; profile->evolution.evolve_desirability = 0;
            building record{}; record.id = 65006; record.type = house->type(); record.state = BUILDING_STATE_IN_USE;
            record.x = record.y = 10; record.grid_offset = static_cast<short>(map_grid_offset(10, 10)); record.desirability = 100;
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{record.id, record.id, &record, house, BuildingGraphicsState{}}});
            auto *runtime = scope.runtime_for_record(&record);
            require(runtime && runtime->building.Housing, "Religion evolution fixture has no housing module");
            for (int requirement : {4, 5}) {
                runtime->building.Housing->state().services.num_gods = static_cast<uint8_t>(requirement - 1);
                profile->requirements.religion = target_profile->requirements.religion = requirement;
                building_house_determine_evolve_text(runtime->building, 0);
                require(runtime->building.Housing->state().evolve_text_id == (requirement == 4 ? HOUSE_EVOLUTION_FOURTH_RELIGION_DEVOLVE : HOUSE_EVOLUTION_FIFTH_RELIGION_DEVOLVE), "House warning still capped devolution at three religions");
                profile->requirements.religion = 0;
                building_house_determine_evolve_text(runtime->building, 0);
                const int warning = runtime->building.Housing->state().evolve_text_id;
                require(warning == (requirement == 4 ? HOUSE_EVOLUTION_FOURTH_RELIGION_EVOLVE : HOUSE_EVOLUTION_FIFTH_RELIGION_EVOLVE) && building_house_extended_evolution_translation(warning), "House warning still capped evolution at three religions");
            }
        }
        Figure unrouted(65005);
        unrouted.x = 17; unrouted.y = 19; unrouted.direction = 2;
        Route::advanceTile(unrouted);
        require(!unrouted.routing_path_id && unrouted.x == 17 && unrouted.y == 19 && unrouted.direction == 2, "Advancing a nonexistent roaming route changed figure state");
        for (int reduction = 0; reduction <= 100; ++reduction) {
            int consumed = 0;
            for (int month = 0; month < 100; ++month) {
                consumed += building_house_consumes_goods_this_month(month, reduction);
                require(building_house_consumes_goods_this_month(month, reduction) == building_house_consumes_goods_this_month(month + 2000000000, reduction), "House savings schedule must remain stable in late years");
            }
            require(consumed == 100 - reduction, "House consumption must apply the exact combined reduction percentage");
        }
        if (const auto *wharf = definition_for_type(type_from_attr("wharf"))) {
            const auto *method = find_production_method_definition("fish_wharf_basic");
            require(method && method->efficiency_limit() == 0, "Augustus wharf must declare uncapped displayed efficiency");
            building record{}; record.id = 65004; record.type = wharf->type(); record.state = BUILDING_STATE_IN_USE;
            record.x = record.y = 10; record.grid_offset = static_cast<short>(map_grid_offset(10, 10));
            record.data.industry.age_months = 1;
            record.data.industry.average_production_per_month = 200;
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{record.id, record.id, &record, wharf, BuildingGraphicsState{}}});
            auto *runtime = scope.runtime_for_record(&record);
            auto measured = *method;
            measured.override_base_monthly_production(100);
            require(runtime && Production(runtime->building, &measured, 0).efficiency() == 200, "Delivery-based efficiency was incorrectly capped");
            auto capped = measured;
            capped.set_efficiency_limit(100);
            require(Production(runtime->building, &capped, 0).efficiency() == 100, "Default efficiency cap was lost");
        }
        const mouse saved_mouse = *mouse_get_pixel();
        mouse_set_position(-500, screen_pixel_height() + 500);
        const bool low_high_clamped = mouse_get_pixel()->x == 0 && mouse_get_pixel()->y == screen_pixel_height() - 1;
        mouse_set_position(screen_pixel_width() + 500, -500);
        const bool high_low_clamped = mouse_get_pixel()->x == screen_pixel_width() - 1 && mouse_get_pixel()->y == 0;
        const bool scaled_once = mouse_get()->x == screen_pixel_to_ui(screen_pixel_width() - 1);
        mouse_set_position(saved_mouse.x, saved_mouse.y);
        require(low_high_clamped && high_low_clamped && scaled_once, "Mouse bounds must clamp in framebuffer pixels before UI conversion");
        validate_scenario_model_overrides();
        validate_archive_origins();
        validate_placement_supports();
        validate_terrain_runtime();
        validate_cart_criminal_graphics();
        validate_presentation_runtime();
        require(!accounting.overflow && !gifts.overflow, "Could not preserve fixture accounting");
        const auto dog_type = figure_type_from_xml_name("dog");
        if (dog_type != FIGURE_NONE) {
            int road = -1;
            for (int y = 2; y < map_grid_height() - 2 && road < 0; ++y) for (int x = 2; x < map_grid_width() - 2; ++x) {
                const int offset = map_grid_offset(x, y);
                if (map_grid_is_inside(x, y, 2) && terrain_map().contains(offset, terrain_types().road) && terrain_map().contains(map_grid_offset(x + 1, y), terrain_types().road)) { road = offset; break; }
            }
            require(road >= 0, "Dog roaming test requires connected roads");
            auto cleanup = [](Figure *figure) { if (figure && figure->id()) figure->remove(); };
            std::unique_ptr<Figure, decltype(cleanup)> dog(Figure::create(dog_type, map_grid_offset_to_x(road), map_grid_offset_to_y(road), DIR_0_TOP), cleanup);
            require(dog && dog->id(), "Could not create dog fixture");
            Building *owner = nullptr;
            Building::for_each(BuildingRuntimeList::Housing, [&](Building *building) { if (!owner && building->is_in_use()) owner = building; });
            require(owner != nullptr, "Dog fixture requires a residential owner");
            dog->set_home_building(owner);
            for (int direction = 0; direction < 8; ++direction) for (int frame = 0; frame < 8; ++frame) {
                dog->direction = static_cast<signed char>(direction); dog->image_offset = static_cast<unsigned char>(frame);
                FigureGraphicDrawRequest request;
                require(figure_graphics_resolve_draw_request(*dog, request) && request.has_base_slice(), "Dog walk frame does not resolve");
                require(request.sprite_offset_x == 19 && request.sprite_offset_y == 29, "Dog frame does not use its ground-contact anchor");
            }
            dog->direction = DIR_0_TOP; dog->image_offset = 0;
            int moved = 0, previous = dog->grid_offset;
            for (int tick = 0; tick < 420 && dog->state == FIGURE_STATE_ALIVE; ++tick) {
                require(figure_runtime_execute(dog.get()) != 0, "Dog did not use its native data profile");
                require(!dog->use_cross_country && terrain_map().contains(dog->grid_offset, terrain_types().road | terrain_types().highway), "Dog left its road network");
                if (dog->grid_offset != previous) { ++moved; previous = dog->grid_offset; }
            }
            require(moved > 0, "Dog road-roaming fixture did not move");
            std::fprintf(stdout, "Dog contracts passed: 64 anchored frames, %d road tile transitions.\n", moved);
            const auto citizen_type = figure_type_from_xml_name("wandering_citizen");
            if (citizen_type != FIGURE_NONE) {
                std::unique_ptr<Figure, decltype(cleanup)> citizen(Figure::create(citizen_type, map_grid_offset_to_x(road), map_grid_offset_to_y(road), DIR_0_TOP), cleanup);
                require(citizen && citizen->id(), "Could not create wandering citizen fixture");
                citizen->set_home_building(owner);
                for (int direction = 0; direction < 8; ++direction) for (int frame = 0; frame < 12; ++frame) {
                    citizen->direction = static_cast<signed char>(direction); citizen->image_offset = static_cast<unsigned char>(frame);
                    FigureGraphicDrawRequest request;
                    require(figure_graphics_resolve_draw_request(*citizen, request) && request.has_base_slice(), "Wandering citizen frame does not resolve");
                }
                citizen->direction = DIR_0_TOP; citizen->image_offset = 0;
                int citizen_moves = 0, previous_offset = citizen->grid_offset;
                for (int tick = 0; tick < 420 && citizen->state == FIGURE_STATE_ALIVE; ++tick) {
                    require(figure_runtime_execute(citizen.get()) != 0, "Citizen did not use its native data profile");
                    require(!citizen->use_cross_country && terrain_map().contains(citizen->grid_offset, terrain_types().road | terrain_types().highway), "Citizen left its road network");
                    if (citizen->grid_offset != previous_offset) { ++citizen_moves; previous_offset = citizen->grid_offset; }
                }
                require(citizen_moves > 0, "Wandering citizen did not move");
                for (const char *path : {"Walkers\\Vespasian_Dog", "Walkers\\Wandering_Citizen"}) {
                    const auto *payload = image_group_payload_get(path);
                    if (!payload) continue; // Augustus retains its original logical scale.
                    require(payload->entry_count() > 0, "Scaled ambient walker has no entries");
                    for (int index = 0; index < payload->entry_count(); ++index) {
                        const auto *entry = payload->entry_at_index(index);
                        const auto *slice = entry->footprint();
                        if (slice && slice->is_valid()) require(slice->fixed_logical_size.width == slice->width * 96 && slice->fixed_logical_size.height == slice->height * 96, "Ambient walker did not use Vespasian logical scaling");
                    }
                }
                std::fprintf(stdout, "Citizen contracts passed: 96 walk frames, %d road tile transitions, Vespasian ambient walker scaling.\n", citizen_moves);
            }
        }
        city_trade_ledger_reset(); game_time_init(-20);
        // Older native ledgers have no route snapshots. Keep their genuine totals and
        // leave historical route state unknown instead of projecting today's quotas.
        std::array<uint8_t, 256> old_ledger{};
        buffer old;
        buffer_init(&old, old_ledger.data(), old_ledger.size());
        buffer_write_u32(&old, 1); buffer_write_u32(&old, 0); // next visit
        buffer_write_u32(&old, 0); buffer_write_u32(&old, 2); // visits, periods
        for (int year : {-20, -21}) {
            buffer_write_i32(&old, year); buffer_write_i32(&old, 0); buffer_write_i32(&old, 0); buffer_write_u8(&old, 0);
            for (int field = 0; field < 16; ++field) buffer_write_i32(&old, 0); // finances and miscellaneous income
            buffer_write_u32(&old, 0); buffer_write_u32(&old, 0); // resources, transactions
        }
        buffer_reset(&old); city_trade_ledger_load(&old, false, false);
        require(!old.overflow && city_trade_ledger_periods().size() == 2 && city_trade_ledger_periods()[1].routes.empty(), "Old accounting history invented route snapshots");
        city_trade_ledger_reset();
        const auto resource = resource_wheat();
        const std::string identity = resource_text_id(resource);
        city_trade_ledger_produced(resource, 173);
        city_trade_ledger_consumed(resource, 41);
        city_trade_ledger_exchange(resource, 2, 11, true, 7);
        city_trade_ledger_exchange(resource, 2, 11, true, 7);
        city_trade_ledger_exchange(resource, 1, 12, true, 7);
        city_trade_ledger_exchange(resource, 3, 17, false, 7);
        const auto &period = city_trade_ledger_periods().front();
        const auto &totals = period.resources.at(identity);
        require(totals.produced == 173 && totals.consumed == 41 && totals.imported == 500 && totals.exported == 300 && totals.income == 51 && totals.expense == 56, "Accounting amounts or direction are incorrect");
        require(period.transactions.size() == 3 && period.transactions.front().units == 400, "Transactions do not aggregate by visit, direction, storage and price");
        {
            int city_id = 0;
            for (int id = 1; id < empire_city_get_array_size(); ++id) {
                const auto *city = empire_city_get(id);
                if (city->in_use && city->type == EMPIRE_CITY_TRADE) { city_id = id; break; }
            }
            require(city_id != 0, "History fixture needs an empire trade city");
            struct RestoreRoute {
                empire_city *city;
                empire_city original;
                buffer routes{};
                explicit RestoreRoute(empire_city *value) : city(value), original(*value) { trade_routes_save_state(&routes); }
                ~RestoreRoute() { *city = original; buffer_reset(&routes); trade_routes_load_state(&routes); free(routes.data); }
            } restore(empire_city_get(city_id));
            auto *city = restore.city;
            city->is_open = 1; city->sells_resource[resource] = 1;
            trade_route_set(city->route_id, resource, 25, false);
            for (int i = 0; i < 7; ++i) trade_route_increase_traded(city->route_id, resource, false);
            game_time_advance_year(); city_trade_ledger_year_change();
            city->is_open = 0; trade_route_reset_traded(city->route_id); trade_route_set_limit(city->route_id, resource, 40, false);
            const auto &history = city_trade_ledger_periods();
            const auto &previous = history[1].routes.at(city_id);
            const auto &latest = history[0].routes.at(city_id);
            require(previous.open && previous.resources.at(identity).import_limit == 25 && previous.resources.at(identity).imported == 7, "Rollover lost historical open state, quota or traded amounts");
            require(!latest.open && latest.resources.at(identity).import_limit == 40 && latest.resources.at(identity).imported == 0, "Historical route snapshot leaked into the current year");
            std::vector<uint8_t> bytes(1024 * 1024);
            buffer saved;
            buffer_init(&saved, bytes.data(), bytes.size()); city_trade_ledger_save(&saved);
            buffer_reset(&saved); city_trade_ledger_load(&saved);
            require(city_trade_ledger_periods()[1].routes.at(city_id).resources.at(identity).imported == 7, "Historical route amounts did not survive save/load");
        }
        require(city_trade_ledger_periods()[1].resources.at(identity).produced == 173 && city_trade_ledger_periods().front().resources.at(identity).produced == 0, "Year rollover lost or duplicated production");
        for (int i = 0; i < 10; ++i) { game_time_advance_year(); city_trade_ledger_year_change(); }
        require(city_trade_ledger_periods().size() == 8 && city_trade_ledger_periods()[7].year == game_time_year() - 7, "History retention or empty-year rollover is incorrect");
        city_trade_ledger_exchange(resource, 7, 13, true, 42);
        city_trade_ledger_consumed(resource, 37);
        std::vector<uint8_t> encoded(1024 * 1024), encoded_again(encoded.size());
        buffer first, second;
        buffer_init(&first, encoded.data(), encoded.size()); city_trade_ledger_save(&first);
        const auto length = first.index; buffer_reset(&first); city_trade_ledger_load(&first);
        buffer_init(&second, encoded_again.data(), encoded_again.size()); city_trade_ledger_save(&second);
        require(second.index == length && std::equal(encoded.begin(), encoded.begin() + length, encoded_again.begin()), "Accounting save roundtrip changed history");
        {
            AccountingPeriod imported;
            imported.year = game_time_year(); imported.partial = true;
            auto &amount = imported.resources[identity];
            amount.imported = 1200; amount.exported = 500; amount.balance_adjustment = -317; amount.complete_cash_flows = false;
            TradeTransaction ambiguous{identity, 0, 1, 0, 2, 30, false, -100};
            ambiguous.direction_known = false;
            ambiguous.source_trader = 300; ambiguous.source_storage = 255;
            imported.transactions.push_back(ambiguous);
            city_trade_ledger_import({imported});
            city_trade_ledger_exchange(resource, 2, 20, false, 1);
            buffer_reset(&second); city_trade_ledger_save(&second);
            buffer_reset(&second); city_trade_ledger_load(&second);
            const auto &restored = city_trade_ledger_periods().front();
            require(restored.resources.at(identity).balance() == -277 && !restored.resources.at(identity).complete_cash_flows, "Imported net balance or unknown gross totals were fabricated/lost");
            require(restored.transactions.front().units == -100 && !restored.transactions.front().direction_known && restored.transactions.front().visit == 0 && restored.transactions.front().source_trader == 300 && restored.transactions.front().source_storage == 255, "Ambiguous source transaction was discarded or assigned a false native identity");
            buffer_reset(&first); city_trade_ledger_load(&first);
        }
        const auto *station = definition_for_type(type_from_attr("highway_station"));
        if (station && station->city_service().enabled()) {
            for (const auto *entry : {"Highway_Station_OFF", "Highway_Station_ON", "Highway_Station_Sand", "Highway_Station_Stone"}) {
                const std::string group = std::string("Admin_Logistics\\") + (std::string(entry) == "Highway_Station_ON" ? "Highway_Station_OFF" : entry);
                const auto image = ImageGroupEntryRef::from_group(group, entry);
                require(image.width() > 0 && image.height() > 0, "Highway Station graphics are missing from the installed pack");
                image.draw(0, 0);
            }
            std::vector<int> cells;
            for (int y = 0; y < map_grid_height(); ++y) for (int x = 0; x < map_grid_width(); ++x) {
                if (!map_grid_is_inside(x, y, 1)) continue;
                const int offset = map_grid_offset(x, y);
                terrain.emplace_back(offset, terrain_map().at(offset));
                terrain_map().remove(offset, terrain_types().highway);
                cells.push_back(offset);
            }
            require(cells.size() > 204, "Fixture map too small for infrastructure test");
            require(terrain_map().count(terrain_types().highway) == 0, "Terrain count cache failed removals");
            for (int i = 0; i < 204; ++i) terrain_map().add(cells[i], terrain_types().highway);
            require(terrain_map().count(terrain_types().highway) == 204, "Terrain count cache failed additions");
            building record{}; record.id = 65000; record.type = station->type(); record.state = BUILDING_STATE_IN_USE; record.num_workers = 1;
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{65000, 65000, &record, station, {}}});
            require(scope.valid(), "Could not create service runtime fixture");
            auto &service_definition = const_cast<BuildingType *>(station)->city_service();
            const auto original_service = service_definition;
            auto restore_service = std::shared_ptr<void>(nullptr, [&](void *) { service_definition = original_service; });
            require(original_service.input_source == ResourceConsumptionSource::GlobalStockpile, "Station must use the global stockpile");
            service_definition.input_source = ResourceConsumptionSource::Building;
            service_definition.stock_periods = 6;
            BuildingCityService service(scope.runtime_for_record(&record)->building);
            require(service.infrastructure_units() == 51 && service.demand(resource_stone()) == 200 && service.stock_target(resource_sand()) == 1200, "Station demand does not round up per 50 highway blocks");
            record.resources[resource_stone()] = 200; record.resources[resource_sand()] = 100;
            require(!service.operational(), "Station operates without a full monthly input");
            service.consume_monthly(); require(record.resources[resource_stone()] == 200, "Station consumed a partial month");
            record.resources[resource_sand()] = 200;
            require(service.operational(), "Supplied staffed station is not operational");
            service.consume_monthly(); require(record.resources[resource_stone()] == 0 && record.resources[resource_sand()] == 0, "Station did not consume exactly one month");
            require(service.receive_load(resource_sand()) && record.resources[resource_sand()] == 100 && !service.receive_load(resource_wheat()), "Station delivery accepted the wrong resource or amount");
            struct StockSnapshot { Building *building; resource_type assigned; std::array<short, RESOURCE_SLOT_COUNT> amounts; };
            std::vector<StockSnapshot> stocks;
            std::vector<Building *> warehouses;
            std::vector<Building *> usable_spaces;
            const auto resource_cache = city_data.resource;
            auto restore_stocks = std::shared_ptr<void>(nullptr, [&](void *) {
                for (const auto &stock : stocks) {
                    stock.building->set_warehouse_resource_id(stock.assigned);
                    for (int r = 0; r < RESOURCE_SLOT_COUNT; ++r) stock.building->set_resource_amount(static_cast<resource_type>(r), stock.amounts[r]);
                }
                for (auto *warehouse : warehouses) building_warehouse_recount_resources(*warehouse);
                city_data.resource = resource_cache;
            });
            Building::for_each(BuildingRuntimeList::Warehouses, [&](Building *warehouse) {
                if (!warehouse->is_in_use() || !warehouse->Composition) return;
                warehouses.push_back(warehouse);
                for (auto *part : warehouse->Composition->children()) {
                    Building *space = part->building();
                    if (!space || !space->type->attr_is("warehouse_space")) continue;
                    StockSnapshot snapshot{space, space->warehouse_resource_id()};
                    for (int r = 0; r < RESOURCE_SLOT_COUNT; ++r) snapshot.amounts[r] = static_cast<short>(space->resource_amount(static_cast<resource_type>(r)));
                    stocks.push_back(snapshot);
                    if (!warehouse->has_plague()) usable_spaces.push_back(space);
                }
            });
            require(usable_spaces.size() >= 2, "Stockpile fixture requires two warehouse bays");
            for (const auto &stock : stocks) {
                for (int r = 0; r < RESOURCE_SLOT_COUNT; ++r) stock.building->set_resource_amount(static_cast<resource_type>(r), 0);
                stock.building->set_warehouse_resource_id(RESOURCE_NONE);
            }
            auto stock = [&](int index, resource_type resource, int amount) {
                usable_spaces[index]->set_warehouse_resource_id(resource);
                usable_spaces[index]->set_resource_amount(resource, amount);
                building_warehouse_recount_resources(*usable_spaces[index]->Composition->owner());
            };
            service_definition = original_service;
            for (auto &amount : record.resources) amount = 0;
            stock(0, resource_stone(), 200); stock(1, resource_sand(), 100);
            require(!service.operational() && service.stock_target(resource_sand()) == 0 && service.delivery_loads_needed(resource_stone()) == 0, "Global service queued a delivery or operated without all inputs");
            service.consume_monthly();
            require(resource_stockpile_amount(resource_stone()) == 200 && resource_stockpile_amount(resource_sand()) == 100, "Global stockpile consumed a partial month");
            stock(1, resource_sand(), 200);
            require(service.operational(), "Stockpile-backed service is not operational");
            service.consume_monthly();
            require(resource_stockpile_amount(resource_stone()) == 0 && resource_stockpile_amount(resource_sand()) == 0, "Global service did not consume exactly one month");
            stock(0, resource_stone(), 200);
            require(!resource_stockpile_consume({{resource_stone(), 150}, {resource_stone(), 150}}) && resource_stockpile_amount(resource_stone()) == 200, "Duplicate stockpile inputs bypassed atomic preflight");
            record.resources[resource_stone()] = 100; record.resources[resource_sand()] = 200;
            require(service.operational(), "Legacy station stock was stranded by global sourcing");
            service.consume_monthly();
            require(record.resources[resource_stone()] == 0 && record.resources[resource_sand()] == 0 && resource_stockpile_amount(resource_stone()) == 100, "Legacy buffers and global stockpile were not accounted together");
            const auto delivery_type = figure_type_from_xml_name("resource_delivery");
            const auto *delivery_definition = figure_type_registry_impl::definition_for(delivery_type);
            require(delivery_definition && delivery_definition->default_profile(), "Resource delivery must have a complete data-owned profile");
            const int tile = cells.front();
            Figure *carrier = Figure::create(delivery_type, map_grid_offset_to_x(tile), map_grid_offset_to_y(tile), DIR_0_TOP);
            carrier->set_home_building(&scope.runtime_for_record(&record)->building);
            carrier->set_destination_building(&scope.runtime_for_record(&record)->building);
            carrier->destination_x = carrier->x; carrier->destination_y = carrier->y;
            carrier->collecting_item_id = static_cast<unsigned char>(resource_stone()); carrier->loads_sold_or_carrying = 2;
            carrier->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
            auto carrier_controller = figure_runtime_native_impl::make_controller(carrier, delivery_definition, delivery_definition->default_profile());
            require(carrier_controller != nullptr, "Resource delivery controller is missing");
            for (int tick = 0; tick < 40 && carrier->state == FIGURE_STATE_ALIVE; ++tick) carrier_controller->execute();
            require(record.resources[resource_stone()] == 200 && carrier->loads_sold_or_carrying == 0 && carrier->state == FIGURE_STATE_DEAD, "In-flight service cargo was not delivered exactly once");
            carrier->remove();

        }
        for (auto [offset, original] : terrain) terrain_map().set(offset, original);
        terrain.clear();
        const auto *arch = definition_for_type(type_from_attr("triumphal_arch"));
        if (arch && arch->has_phased_construction()) {
            std::array<std::vector<uint8_t>, 5> message_bytes;
            std::array<buffer, 5> messages;
            for (size_t i = 0; i < messages.size(); ++i) { message_bytes[i].resize(1024 * 1024); buffer_init(&messages[i], message_bytes[i].data(), message_bytes[i].size()); }
            city_message_save_state(&messages[0], &messages[1], &messages[2], &messages[3], &messages[4]);
            auto restore_messages = std::shared_ptr<void>(nullptr, [&](void *) {
                for (auto &message : messages) buffer_reset(&message);
                city_message_load_state(&messages[0], &messages[1], &messages[2], &messages[3], &messages[4]);
            });
            require(arch->construction().completion_message == MESSAGE_TRIUMPHAL_ARCH_COMPLETE && !arch->construction().window.empty(), "Arch construction must own its completion message and XML panel");
            building record{}; record.id = 65001; record.type = arch->type(); record.state = BUILDING_STATE_IN_USE; record.x = 10; record.y = 10; record.grid_offset = static_cast<short>(map_grid_offset(10, 10));
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{65001, 65001, &record, arch, {}}});
            require(scope.valid(), "Could not create single-building monument fixture");
            building_monument_set_phase(&record, 1);
            require(record.resources[resource_stone()] == 12 && record.resources[resource_timber()] == 8, "Arch first phase requirements differ from Augustus");
            require(building_monument_deliver_resource(&record, resource_stone()) && record.resources[resource_stone()] == 11, "Single-building monument cannot receive materials");
            auto &building = scope.runtime_for_record(&record)->building;
            require(!BuildingInfoScreenSelection::resolve(building, 1).isFoundationRoadblockFamily(), "Unfinished monument must not expose roadblock orders");
            building_info_context panel{}; panel.building = &building; panel.width_blocks = 29; panel.height_blocks = 28;
            require(window_building_draw_construction_panel(&panel), "Arch phase panel did not render through XML");
            {
                const int width = screen_ui_to_pixel(464), height = screen_ui_to_pixel(448);
                std::vector<color_t> pixels(static_cast<size_t>(width) * height);
                require(graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width), "Could not capture construction phase panel");
                SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                require(surface != nullptr, "Could not create construction phase screenshot");
                std::filesystem::create_directories("out/catch-up-ui");
                const int saved = SDL_SaveBMP(surface, "out/catch-up-ui/arch-construction.bmp"); SDL_FreeSurface(surface);
                require(saved == 0, "Could not write construction phase screenshot");
            }
            building.Foundation->state().begin_publication(10, 10, 0);
            city_monument_gift_supply(building, true, true, 4);
            require(record.figure_id != 0, "External monument material supplier did not spawn");
            const Figure *supplier = Figure::get(record.figure_id);
            require(supplier->building == &building && supplier->destination_building == &building && supplier->collecting_item_id != RESOURCE_NONE, "Gift supplier did not retain monument ownership");
            require(building_monument_resource_in_delivery(&record, supplier->collecting_item_id) == 4, "Gift supplier material reservation differs from its convoy");
            auto clear_convoy = [&]() {
                auto figures = Figure::figures_referencing_building(building);
                for (auto *figure : figures) figure->remove();
                record.figure_id = 0;
            };
            clear_convoy();
            require(!building_monument_has_delivery_for_building(record.id), "Gift convoy left stale reservations after removal");
            for (auto &amount : record.resources) amount = 0;
            record.resources[RESOURCE_NONE] = 1;
            city_monument_gift_supply(building, false, true, 4);
            require(record.figure_id && Figure::get(record.figure_id)->type == FIGURE_WORK_CAMP_ARCHITECT && Figure::figures_referencing_building(building).size() == 1, "External architect spawned an invalid material convoy");
            clear_convoy();
            building.Foundation->state().clear();
            for (auto &amount : record.resources) amount = 0;
            require(building_monument_progress(&record) && record.monument.phase == 2 && record.resources[resource_marble()] == 32, "Single-building monument cannot advance phases");
            for (auto &amount : record.resources) amount = 0;
            const int before_message = city_message_count();
            require(building_monument_progress(&record) && record.monument.phase == MONUMENT_FINISHED, "Single-building monument cannot finish");
            require(city_message_count() == before_message + 1, "Monument completion message was not posted");
            require(!building_monument_progress(&record) && city_message_count() == before_message + 1, "Finished monument repeated its completion message");
        }
        city_monument_gifts_reset();
        const auto gift_type = type_from_attr("triumphal_arch");
        require(!city_monument_gift_available(gift_type), "Unearned monument gift is available");
        city_monument_gifts_award("distant_battle_victory", 100);
        require(city_monument_gift_available(gift_type), "Parameterized victory gift was not awarded");
        buffer_reset(&time); game_time_load_state(&time);
        buffer_reset(&gifts); city_monument_gifts_load(&gifts);
        buffer_reset(&accounting); city_trade_ledger_load(&accounting);
        auto render = [&](const char *name, window_id expected) {
            require(window_is(expected), "XML feature window did not open");
            window_draw(1); window_draw(1);
            const int width = screen_pixel_width(), height = screen_pixel_height();
            std::vector<color_t> pixels(static_cast<size_t>(width) * height);
            require(graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width) != 0, "Could not read feature window render");
            require(std::any_of(pixels.begin(), pixels.end(), [&](color_t pixel) { return pixel != pixels.front(); }), "Feature window rendered a blank surface");
            std::filesystem::create_directories("out/catch-up-ui");
            SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            require(surface != nullptr, "Could not create screenshot surface");
            const int result = SDL_SaveBMP(surface, (std::string("out/catch-up-ui/") + name + ".bmp").c_str());
            SDL_FreeSurface(surface);
            require(result == 0, "Could not save feature window review image");
        };
        window_city_show(); window_advisors_show_advisor(ADVISOR_FINANCIAL); render("financial", WINDOW_ADVISORS);
        window_city_show(); window_advisors_show_advisor(ADVISOR_RELIGION); render("religion", WINDOW_ADVISORS);
        if (window_epithets_available()) { window_epithets_show(); render("epithets", WINDOW_EPITHETS); }
        window_city_show(); window_trade_ledger_show(); render("ledger", WINDOW_TRADE_LEDGER);
        window_city_show(); window_trade_ledger_show(0); render("transactions", WINDOW_TRADE_LEDGER);
        window_city_show(); window_empire_show(); render("empire", WINDOW_EMPIRE);
        window_city_show();
        validate_religion_callbacks_in_city();
        validate_full_city_capture();
        std::fprintf(stdout, "Catch-up contracts passed: accounting, history, roundtrip, service demand/consumption, single-building phases, gifts.\n");
    } catch (const std::exception &error) { std::fprintf(stderr, "Catch-up contract failed: %s\n", error.what()); success = false; }
    for (auto [offset, original] : terrain) terrain_map().set(offset, original);
    buffer_reset(&time); game_time_load_state(&time);
    buffer_reset(&gifts); city_monument_gifts_load(&gifts);
    buffer_reset(&accounting); city_trade_ledger_load(&accounting);
    return success;
}
