#pragma once
#include "building/properties.h"
#include "building/building_type_registry_internal.h"
#include "building/building_type_startup_bridge.h"
#include "building/production_method_registry.h"
#include "building/housing_profile_registry.h"
#include "scenario/event/action_types.h"
#include "scenario/event/action_handler.h"
#include "scenario/event/parameter_city.h"
#include "scenario/event/controller.h"
#include "scenario/event/parameter_data.h"
#include "scenario/definition_overrides.h"
#include "SDL.h"
#include "core/Logger.h"
#include "scenario/model_xml.h"
#include "game/legacy_model_defaults.generated.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <vector>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>

inline void validate_scenario_model_overrides()
{
    using namespace building_type_registry_impl;
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    const auto loaded_theater = type_from_attr("theater");
    if (!(model_scenario_override_fields(loaded_theater) & (1u << MODEL_LABORERS))) {
        require(definition_for_type(loaded_theater)->required_workers() == model_get_mod_defaults(loaded_theater)->laborers, "Loaded theater retained an incidental labor requirement");
    }
    std::fprintf(stdout, "Loaded theater requirement: %d workers; scenario field mask=%u.\n", definition_for_type(loaded_theater)->required_workers(), model_scenario_override_fields(loaded_theater));
    struct Snapshot {
        buffer models{}, production{};
        Snapshot() { model_save_model_data(&models); production_rates_save(&production); }
        ~Snapshot() {
            model_reset(); building_type_startup_bridge_apply_model_overrides();
            model_load_model_data(&models); production_rates_load(&production, true);
            std::free(models.data); std::free(production.data);
        }
    } snapshot;
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    production_method_registry_reset_production_overrides();
    const building_type type = type_from_attr("barracks");
    require(type != BUILDING_NONE, "Scenario override fixture needs barracks");
    const int default_cost = model_get_building(type)->cost;
    const int default_labor = model_get_building(type)->laborers;
    scenario_action_t action{};
    action.parameter1 = type; action.parameter2 = MODEL_COST;
    action.parameter3 = scenario_formula_add(reinterpret_cast<const uint8_t *>("731"), INT_MIN, INT_MAX);
    action.parameter4 = 1;
    require(scenario_action_type_change_model_data_execute(&action) == 1, "Scenario cost action failed");
    require(model_get_construction_cost(type) == 731, "Scenario action did not override construction cost");
    buffer saved{}; model_save_model_data(&saved);
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    require(model_get_building(type)->cost == default_cost, "New scenario retained old model override");
    // Model a fresh mod composition changing an unrelated field. Loading the
    // scenario must restore its cost and inherit this new labor default.
    model_get_building(type)->laborers = default_labor + 3;
    model_capture_mod_defaults();
    require(model_load_model_data(&saved) == 1, "Keyed scenario model did not reload");
    require(model_get_building(type)->cost == 731 && model_get_building(type)->laborers == default_labor + 3, "Scenario overlay froze an unrelated mod field");
    std::free(saved.data);
    buffer empty{};
    model_reset(); building_type_startup_bridge_apply_model_overrides(); model_save_model_data(&empty);
    require(empty.size == 16, "Default models were serialized as scenario overrides");
    std::free(empty.data);
    const building_type theater = type_from_attr("theater");
    const auto *theater_definition = definition_for_type(theater);
    require(theater_definition && theater_definition->labor().employee_count() == 8 && theater_definition->required_workers() == 8, "Theater labor does not come from its composed XML definition");
    model_get_building(theater)->laborers = 1;
    buffer unmarked{}; model_save_model_data(&unmarked);
    require(unmarked.size == 16, "An unmarked runtime model difference became a scenario override");
    std::free(unmarked.data);
    for (int employees : {0, 1, 8}) {
        model_reset(); building_type_startup_bridge_apply_model_overrides();
        model_get_building(theater)->laborers = employees;
        model_mark_scenario_override(theater, MODEL_LABORERS);
        buffer explicit_edit{}; model_save_model_data(&explicit_edit);
        require(explicit_edit.size == 35, "A single theater field serialized unchanged model values");
        model_reset(); building_type_startup_bridge_apply_model_overrides();
        require(model_load_model_data(&explicit_edit) && theater_definition->required_workers() == employees, "An explicit theater labor override did not survive reload");
        std::free(explicit_edit.data);
    }
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    require(theater_definition->required_workers() == 8, "New scenario retained a theater labor override");
    // Shared foreign snapshots have stable source IDs and a known baseline.
    // Change one source field, while the active mod changes a different field.
    auto imported = std::vector<model_building>(std::begin(legacy_model_defaults::buildings), std::end(legacy_model_defaults::buildings));
    imported[31].laborers = 3;
    model_get_building(theater)->cost = 947; model_capture_mod_defaults();
    buffer foreign{}; buffer_init(&foreign, reinterpret_cast<uint8_t *>(imported.data()), imported.size() * sizeof(model_building));
    require(model_import_legacy_source_data(&foreign, 0xae) && theater_definition->required_workers() == 3 && model_get_building(theater)->cost == 947, "Legacy scenario import froze unrelated source defaults");
    require(model_scenario_override_fields(theater) == (1u << MODEL_LABORERS), "Legacy scenario import copied the complete model");
    buffer imported_delta{}; model_save_model_data(&imported_delta);
    require(imported_delta.size == 35, "Legacy source defaults leaked into a new save");
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    require(model_load_model_data(&imported_delta) && theater_definition->required_workers() == 3, "Recovered legacy scenario exception did not survive reload");
    std::free(imported_delta.data);
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    imported.push_back(legacy_model_defaults::clear_trees_extension);
    buffer_init(&foreign, reinterpret_cast<uint8_t *>(imported.data()), imported.size() * sizeof(model_building));
    require(model_load_model_data(&foreign, ModelDataFormat::LegacyNativeSnapshot, 175) && theater_definition->required_workers() == 3, "Early fixed-enum native scenario exception was discarded");
    require(model_scenario_override_fields(theater) == (1u << MODEL_LABORERS), "Early native snapshot became a complete model override");
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    // Capture only this deliberately corrupt fixture's expected repair warning.
    // The ordinary save/reload/soak path retains its strict zero-warning policy.
    struct RepairLog {
        Logger::Destination previous{};
        int repairs = 0; bool unexpected = false;
        RepairLog() {
            Logger::flush();
            previous = Logger::set_output([](void *data, Logger::Severity severity, const char *message) {
                auto &self = *static_cast<RepairLog *>(data);
                if (severity == Logger::Severity::Warning && (std::strstr(message, "Repairing corrupted legacy native building model") || std::strstr(message, "Repairing ambiguous legacy model values"))) ++self.repairs;
                else if (severity >= Logger::Severity::Warning) { self.unexpected = true; self.previous.output(self.previous.userdata, severity, message); }
            }, this);
        }
        ~RepairLog() { Logger::flush(); Logger::set_output(previous.output, previous.userdata); }
    };
    {
        RepairLog logs;
        std::vector<model_building> obsolete(BUILDING_TYPE_MAX);
        obsolete[31].laborers = 8;
        obsolete[theater].laborers = 0;
        buffer raw{}; buffer_init(&raw, reinterpret_cast<uint8_t *>(obsolete.data()), obsolete.size() * sizeof(model_building));
        require(model_load_model_data(&raw, ModelDataFormat::LegacyNativeSnapshot) && theater_definition->required_workers() == 8, "Obsolete runtime indices changed theater labor");
        // The exact VMO2 shape that re-saved corrupted theater values, alongside
        // an independently identifiable authored construction action.
        require(scenario_construction_requirement_change(type, 0, resource_wheat(), 11), "Legacy construction fixture failed");
        buffer old{}; buffer_init_dynamic(&old, 8 + 8 + 7 + 24 + scenario_definition_overrides_serialized_size());
        buffer_write_u32(&old, 0x324f4d56); buffer_write_u32(&old, 1);
        buffer_write_u32(&old, 7); buffer_write_raw(&old, "theater", 7); buffer_write_u32(&old, 63);
        for (int field = MODEL_COST; field <= MODEL_LABORERS; ++field) buffer_write_i32(&old, field == MODEL_LABORERS ? 1 : 0);
        scenario_definition_overrides_write(&old);
        model_reset(); building_type_startup_bridge_apply_model_overrides();
        require(model_load_model_data(&old, ModelDataFormat::LegacyNativeOverlay) && theater_definition->required_workers() == 8, "VMO2 inferred override was not repaired");
        require(definition_for_type(type)->construction().instant_requirement_amount(resource_wheat()) == 11, "Repair discarded an identifiable authored construction action");
        require(model_scenario_override_fields(theater) == 0, "Repair retained a contaminated scenario mask");
        buffer clean{}; model_save_model_data(&clean);
        model_reset(); building_type_startup_bridge_apply_model_overrides();
        require(model_load_model_data(&clean) && theater_definition->required_workers() == 8, "Repaired models did not survive canonical reload");
        model_reset(); building_type_startup_bridge_apply_model_overrides();
        imported.pop_back(); imported[9].cost = 0;
        buffer_init(&foreign, reinterpret_cast<uint8_t *>(imported.data()), imported.size() * sizeof(model_building));
        require(model_import_legacy_source_data(&foreign, 170) && theater_definition->required_workers() == 3, "Version-170 source import lost a recoverable exception");
        require(model_scenario_override_fields(type_from_attr("clear_land")) == 0, "An obsolete upstream default was mistaken for a scenario edit");
        require(logs.repairs == 3 && !logs.unexpected, "Model repairs did not emit exactly their expected warnings");
        std::free(old.data); std::free(clean.data);
    }
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    // Authored XML exports are sparse as well; reimport inherits unrelated fields.
    model_get_building(theater)->laborers = 1; model_mark_scenario_override(theater, MODEL_LABORERS);
    const auto xml_path = std::filesystem::temp_directory_path() / "vespasian-model-delta-test.xml";
    require(scenario_model_export_to_xml(xml_path.string().c_str()) != 0, "Scenario model XML export failed");
    std::ifstream exported(xml_path); std::string xml((std::istreambuf_iterator<char>(exported)), {}); exported.close();
    require(xml.find("laborers=\"1\"") != std::string::npos && xml.find("desirability_value=") == std::string::npos && xml.find("cost=") == std::string::npos, "Scenario XML froze unrelated fields");
    require(scenario_model_xml_parse_file(xml_path.string().c_str()) && theater_definition->required_workers() == 1 && model_scenario_override_fields(theater) == (1u << MODEL_LABORERS), "Sparse model XML did not reload");
    std::filesystem::remove(xml_path);
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    buffer production{}; production_rates_save(&production);
    require(production.size == 12, "Default production was serialized as scenario overrides");
    std::free(production.data);
    action.parameter1 = resource_wheat(); action.parameter3 = 1;
    action.parameter2 = scenario_formula_add(reinterpret_cast<const uint8_t *>("73"), INT_MIN, INT_MAX);
    require(scenario_action_type_change_production_rate_execute(&action) == 1 && production_method_registry_production_per_month_for_resource(resource_wheat()) == 73, "Production set action did not replace the rate");
    action.parameter3 = 0;
    require(scenario_action_type_change_production_rate_execute(&action) == 1 && production_method_registry_production_per_month_for_resource(resource_wheat()) == 146, "Production add action did not add to the rate");
    action.parameter2 = scenario_formula_add(reinterpret_cast<const uint8_t *>("-200"), INT_MIN, INT_MAX);
    require(scenario_action_type_change_production_rate_execute(&action) == 1 && production_method_registry_production_per_month_for_resource(resource_wheat()) == 0, "Production rate did not clamp at zero");
    production_method_registry_reset_production_overrides();
    {
        auto *delay = find_production_method_definition("recruitment_delay");
        require(delay && delay->is_delay_factor() && !delay->has_resource_output() && delay->scale_delay(8) == 8, "Recruitment delay definition is not an independent factor");
        const auto *barracks = definition_for_type(type_from_attr("barracks"));
        require(std::find(barracks->production_methods().begin(), barracks->production_methods().end(), delay) != barracks->production_methods().end(), "Barracks does not bind its delay factor");
        action.parameter1 = resource_troops(); action.parameter3 = 1;
        action.parameter2 = scenario_formula_add(reinterpret_cast<const uint8_t *>("50"), INT_MIN, INT_MAX);
        require(scenario_action_type_change_production_rate_execute(&action) && delay->scale_delay(8) == 4, "50 percent did not halve recruitment delay");
        action.parameter3 = 0;
        action.parameter2 = scenario_formula_add(reinterpret_cast<const uint8_t *>("150"), INT_MIN, INT_MAX);
        require(scenario_action_type_change_production_rate_execute(&action) && delay->scale_delay(8) == 16, "Add action did not make recruitment delay 200 percent");
        buffer rates{}; production_rates_save(&rates);
        production_method_registry_reset_production_overrides();
        require(delay->scale_delay(8) == 8, "Scenario reset retained recruitment delay override");
        production_rates_load(&rates, true); std::free(rates.data);
        require(delay->scale_delay(8) == 16, "Delay multiplier did not survive sparse save/reload");
        production_method_registry_set_production_per_month_for_resource(resource_troops(), 0);
        require(delay->scale_delay(8) == 0 && delay->scale_delay(-1) == -1, "Zero delay lost instant or unstaffed semantics");
        production_method_registry_set_production_per_month_for_resource(resource_troops(), INT_MAX);
        require(delay->scale_delay(INT_MAX) == INT_MAX, "Delay scaling overflowed");
        production_method_registry_reset_production_overrides();
    }
    if (auto *mint = find_production_method_definition("city_mint_basic")) {
        auto *reverse = find_production_method_definition("city_mint_gold_basic");
        require(reverse && reverse->rate_source() == mint, "Reverse mint does not bind to the denarii production method");
        const int default_mint = mint->base_monthly_production();
        const int default_gold = production_method_registry_production_per_month_for_resource(resource_gold());
        action.parameter1 = resource_denarii(); action.parameter3 = 1;
        action.parameter2 = scenario_formula_add(reinterpret_cast<const uint8_t *>("73"), INT_MIN, INT_MAX);
        require(scenario_action_type_change_production_rate_execute(&action) && mint->base_monthly_production() == 73 && reverse->base_monthly_production() == 73, "Denarii set event did not control both mint directions");
        action.parameter3 = 0;
        require(scenario_action_type_change_production_rate_execute(&action) && mint->base_monthly_production() == 146 && reverse->base_monthly_production() == 146, "Denarii add event was lost or applied twice");
        production_method_registry_set_production_per_month_for_resource(resource_gold(), 51);
        require(reverse->base_monthly_production() == 146, "Gold mining rate changed reverse mint production");
        buffer mint_rates{}; production_rates_save(&mint_rates);
        production_method_registry_reset_production_overrides();
        require(mint->base_monthly_production() == default_mint && reverse->base_monthly_production() == default_mint && production_method_registry_production_per_month_for_resource(resource_gold()) == default_gold, "Scenario reset retained a linked production override");
        production_rates_load(&mint_rates, true);
        std::free(mint_rates.data);
        require(mint->base_monthly_production() == 146 && reverse->base_monthly_production() == 146 && !reverse->has_production_override(), "Production save froze a derived mint rate as an explicit scenario override");
        production_method_registry_reset_production_overrides();
        std::fprintf(stdout, "D13 production contracts passed: shared mint rate, independent gold, set/add, reset and delta roundtrip.\n");
    }
    const auto house_type = type_from_attr("house_small_tent");
    const auto *house = definition_for_type(house_type);
    require(house && house->housing_def().profile, "Scenario housing fixture is unavailable");
    const int capacity = house->housing_def().capacity;
    for (int requirement = 0; requirement < 4; ++requirement) {
        HousingRequirements water;
        water.water = static_cast<HousingWaterRequirement>(requirement);
        for (int access = 0; access < 8; ++access) {
            const bool well = access & 1, fountain = access & 2, latrine = access & 4;
            const bool expected = requirement == 0 || fountain || (requirement == 1 && well) || (requirement == 3 && latrine);
            require(water.has_required_water(well, fountain, latrine) == expected, "Data-owned housing water alternatives are incorrect");
        }
    }
    require(scenario_house_model_change(house_type, 3, static_cast<int>(HousingWaterRequirement::LatrineOrFountain), true), "Scenario cannot require a latrine or fountain independently of house level");
    scenario_action_t ordinal_action{ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA, static_cast<int>(house_type), 3,
        static_cast<int>(scenario_formula_add(reinterpret_cast<const uint8_t *>("2"), 0, 1000)), 1};
    ordinal_action.value_domain = {0, 1, 3, 2};
    uint8_t ordinal_bytes[56]{}; buffer ordinal_wire{}; buffer_init(&ordinal_wire, ordinal_bytes, sizeof(ordinal_bytes));
    scenario_action_type_save_state(&ordinal_wire, &ordinal_action, LINK_TYPE_SCENARIO_EVENT, 73);
    buffer_reset(&ordinal_wire); scenario_action_t ordinal_restored{}; int ordinal_link = 0; int32_t ordinal_id = 0;
    scenario_action_type_load_state(&ordinal_wire, &ordinal_restored, &ordinal_link, &ordinal_id, true);
    require(!ordinal_wire.overflow && ordinal_restored.value_domain == ordinal_action.value_domain && scenario_action_type_definition_execute(&ordinal_restored) && scenario_house_model_value(house_type, 3) == 3, "Authored ordinal set lost its value domain on roundtrip");
    ordinal_restored.parameter4 = 0;
    ordinal_restored.parameter3 = scenario_formula_add(reinterpret_cast<const uint8_t *>("1"), 0, 1000);
    require(scenario_action_type_definition_execute(&ordinal_restored) && scenario_house_model_value(house_type, 3) == 2, "Authored ordinal add used the native ordinal order");
    require(scenario_action_type_definition_execute(&ordinal_action), "Ordinal fixture restore failed");
    const auto merged_type = type_from_attr("house_small_tent_2x2");
    if (definition_for_type(merged_type)) {
        scenario_action_t shared{ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA, static_cast<int>(house_type), 15,
            static_cast<int>(scenario_formula_add(reinterpret_cast<const uint8_t *>("(1,1000)"), 0, 1000)), 1};
        shared.model_targets.push_back({merged_type, 4});
        uint8_t shared_bytes[48]{}; buffer shared_wire{}; buffer_init(&shared_wire, shared_bytes, sizeof(shared_bytes));
        scenario_action_type_save_state(&shared_wire, &shared, LINK_TYPE_SCENARIO_EVENT, 73);
        buffer_reset(&shared_wire); scenario_action_t restored{};
        scenario_action_type_load_state(&shared_wire, &restored, &ordinal_link, &ordinal_id, true);
        require(!shared_wire.overflow && restored.model_targets.size() == 1 && restored.model_targets[0].building == merged_type, "Related model target lost its keyed identity");
        for (int repeat = 0; repeat < 20; ++repeat) {
            require(scenario_action_type_definition_execute(&restored) && scenario_house_model_value(merged_type, 15) == 4 * scenario_house_model_value(house_type, 15), "Related house definitions evaluated a random formula independently");
        }
    }
    require(scenario_house_model_change(house_type, 15, capacity + 7, true), "Housing capacity action failed");
    require(scenario_house_model_change(house_type, 0, -73, true), "Housing desirability action failed");
    require(scenario_construction_requirement_change(type, 0, resource_wheat(), 11), "Instant construction requirement action failed");
    scenario_action_t construction_action{ACTION_TYPE_CHANGE_MONUMENT_RESOURCES, static_cast<int>(type), 1, resource_wheat(), static_cast<int>(scenario_formula_add(reinterpret_cast<const uint8_t *>("11"), 0, 1000))};
    require(scenario_action_type_definition_execute(&construction_action) && definition_for_type(type)->construction().instant_requirement_amount(resource_wheat()) == 11, "First-stage action did not address instant construction");
    const auto *oracle = definition_for_type(type_from_attr("oracle"));
    if (oracle && oracle->has_phased_construction()) {
        construction_action.parameter1 = oracle->type(); construction_action.parameter2 = 2;
        const int first_phase = oracle->construction().requirement_amount(resource_wheat(), 1);
        require(scenario_action_type_definition_execute(&construction_action) && oracle->construction().requirement_amount(resource_wheat(), 2) == 11 && oracle->construction().requirement_amount(resource_wheat(), 1) == first_phase, "Scenario action changed the preceding construction phase");
    }
    require(scenario_definition_override_set({ScenarioOverrideKind::Migration, {}, 1, {}, 25}), "Migration action failed");
    buffer overlays{}; model_save_model_data(&overlays);
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    require(house->housing_def().capacity == capacity, "New scenario retained housing capacity override");
    require(model_load_model_data(&overlays) == 1, "New scenario overlays did not reload");
    require(house->housing_def().capacity == capacity + 7 && house->housing_def().profile->evolution.devolve_desirability == -73, "Housing overlays did not survive save/reset/load");
    require(house->housing_def().profile->requirements.water == HousingWaterRequirement::LatrineOrFountain, "Housing water alternative did not survive save/reset/load");
    require(definition_for_type(type)->construction().instant_requirement_amount(resource_wheat()) == 11, "Construction override did not survive save/reset/load");
    require(scenario_definition_override_value(ScenarioOverrideKind::Migration, {}, 1, {}, 100) == 25, "Migration override did not survive save/reset/load");
    std::free(overlays.data);
    scenario_event_t original{}, pasted{};
    pasted.id = 123;
    const int formula_id = scenario_formula_add(reinterpret_cast<const uint8_t *>("37"), 0, 1000);
    original.actions.push_back({ACTION_TYPE_CHANGE_GOAL, 0, formula_id, 1});
    original.actions.push_back({ACTION_TYPE_SEND_CITY_WARNING, scenario_text_add("Copied scenario text"), 0});
    original.condition_groups.push_back({FULFILLMENT_TYPE_ALL, {{CONDITION_TYPE_TIME_PASSED, COMPARISON_TYPE_EQUAL_OR_MORE, formula_id}}});
    const auto copy = scenario_event_copy(original);
    scenario_event_paste(copy, pasted);
    require(pasted.actions.size() == 2 && pasted.condition_groups.size() == 1 && pasted.actions[0].parent_event_id == 123, "Event copy lost groups, actions or parent identity");
    require(pasted.actions[0].parameter2 != formula_id && pasted.condition_groups[0].conditions[0].parameter2 == pasted.actions[0].parameter2, "Copy did not independently clone shared formula identity");
    scenario_formula_change(pasted.actions[0].parameter2, reinterpret_cast<const uint8_t *>("72"), 0, 1000);
    require(scenario_formula_evaluate_formula(formula_id) == 37 && scenario_formula_evaluate_formula(pasted.actions[0].parameter2) == 72, "Editing a copied formula changed the original");
    require(std::string(reinterpret_cast<const char *>(scenario_text_get(pasted.actions[1].parameter1))) == "Copied scenario text", "Event copy lost text");
    uint8_t label[512]{};
    scenario_events_parameter_data_get_display_string_for_action(&pasted.actions[0], label, sizeof(label));
    require(label[0] && std::string(reinterpret_cast<const char *>(label)).find("UNHANDLED") == std::string::npos, "New action has no usable editor description");
    // Definition selectors must roundtrip through compact ledgers, including
    // flexible city-property parameters and special resources outside inventory.
    building_type_id_bridge_prepare_new_save_table();
    god_id_bridge_prepare_new_save_table();
    buffer resource_table{}; resource_id_bridge_save_table_save_state(&resource_table);
    const scenario_action_t selector_actions[] = {
        {ACTION_TYPE_CHANGE_MODEL_DATA, static_cast<int>(theater), MODEL_LABORERS, formula_id, 1},
        {ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS, static_cast<int>(theater), 1},
        {ACTION_TYPE_CHANGE_HOUSE_MODEL_DATA, static_cast<int>(house_type), 15, formula_id, 1},
        {ACTION_TYPE_CHANGE_MONUMENT_RESOURCES, static_cast<int>(type), 1, resource_wheat(), formula_id},
        {ACTION_TYPE_CHANGE_PRODUCTION_RATE, resource_denarii(), formula_id, 1},
        {ACTION_TYPE_CHANGE_RESOURCE_STOCKPILES, resource_wheat(), formula_id, 0, 1},
        {ACTION_TYPE_CUSTOM_VARIABLE_CITY_PROPERTY, 1, CITY_PROPERTY_BUILDING_COUNT, static_cast<int>(theater)}
    };
    for (const auto &source : selector_actions) {
        uint8_t bytes[40]{}; buffer wire{}; buffer_init(&wire, bytes, sizeof(bytes));
        scenario_action_type_save_state(&wire, &source, LINK_TYPE_SCENARIO_EVENT, 73);
        buffer_reset(&wire); scenario_action_t restored{}; int link = 0; int32_t id = 0;
        scenario_action_type_load_state(&wire, &restored, &link, &id, true);
        require(!wire.overflow && restored.type == source.type && restored.parameter1 == source.parameter1 && restored.parameter2 == source.parameter2 && restored.parameter3 == source.parameter3 && restored.parameter4 == source.parameter4 && link == LINK_TYPE_SCENARIO_EVENT && id == 73, "Scenario selector lost its ledger identity");
    }
    std::free(resource_table.data);
    std::fprintf(stdout, "Scenario selector contracts passed: compact building ledgers, flexible parameters and special resource identities.\n");
    std::fprintf(stdout, "D08 contracts passed: housing/construction/migration overlays, save/reset/restore, independent copied formulas and texts.\n");
    std::fprintf(stdout, "Scenario override contracts passed: action precedence, keyed restore, unrelated mod defaults, reset.\n");
}
