#pragma once
#include "building/properties.h"
#include "building/building_type_registry_internal.h"
#include "building/building_type_startup_bridge.h"
#include "building/production_method_registry.h"
#include "building/housing_profile_registry.h"
#include "scenario/event/action_types.h"
#include "scenario/event/controller.h"
#include "scenario/event/parameter_data.h"
#include "scenario/definition_overrides.h"
#include "SDL.h"
#include "core/log.h"
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
        SDL_LogOutputFunction previous{}; void *context{};
        int repairs = 0; bool unexpected = false;
        RepairLog() {
            log_repeated_messages();
            SDL_LogGetOutputFunction(&previous, &context);
            SDL_LogSetOutputFunction([](void *data, int category, SDL_LogPriority priority, const char *message) {
                auto &self = *static_cast<RepairLog *>(data);
                if (priority == SDL_LOG_PRIORITY_WARN && (std::strstr(message, "Repairing corrupted legacy native building model") || std::strstr(message, "Repairing ambiguous legacy model values"))) ++self.repairs;
                else if (priority >= SDL_LOG_PRIORITY_WARN) { self.unexpected = true; self.previous(self.context, category, priority, message); }
            }, this);
        }
        ~RepairLog() { log_repeated_messages(); SDL_LogSetOutputFunction(previous, context); }
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
    const auto house_type = type_from_attr("house_small_tent");
    const auto *house = definition_for_type(house_type);
    require(house && house->housing_def().profile, "Scenario housing fixture is unavailable");
    const int capacity = house->housing_def().capacity;
    require(scenario_house_model_change(house_type, 15, capacity + 7, true), "Housing capacity action failed");
    require(scenario_house_model_change(house_type, 0, -73, true), "Housing desirability action failed");
    require(scenario_construction_requirement_change(type, 0, resource_wheat(), 11), "Instant construction requirement action failed");
    require(scenario_definition_override_set({ScenarioOverrideKind::Migration, {}, 1, {}, 25}), "Migration action failed");
    buffer overlays{}; model_save_model_data(&overlays);
    model_reset(); building_type_startup_bridge_apply_model_overrides();
    require(house->housing_def().capacity == capacity, "New scenario retained housing capacity override");
    require(model_load_model_data(&overlays) == 1, "New scenario overlays did not reload");
    require(house->housing_def().capacity == capacity + 7 && house->housing_def().profile->evolution.devolve_desirability == -73, "Housing overlays did not survive save/reset/load");
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
    std::fprintf(stdout, "D08 contracts passed: housing/construction/migration overlays, save/reset/restore, independent copied formulas and texts.\n");
    std::fprintf(stdout, "Scenario override contracts passed: action precedence, keyed restore, unrelated mod defaults, reset.\n");
}
