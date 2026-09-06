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
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>

inline void validate_scenario_model_overrides()
{
    using namespace building_type_registry_impl;
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    struct Snapshot {
        buffer models{}, production{};
        Snapshot() { model_save_model_data(&models); production_rates_save(&production); }
        ~Snapshot() {
            model_reset(); building_type_startup_bridge_apply_model_overrides();
            model_load_model_data(&models, true); production_rates_load(&production, true);
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
    require(model_load_model_data(&saved, true) == 1, "Keyed scenario model did not reload");
    require(model_get_building(type)->cost == 731 && model_get_building(type)->laborers == default_labor + 3, "Scenario overlay froze an unrelated mod field");
    std::free(saved.data);
    buffer empty{};
    model_reset(); building_type_startup_bridge_apply_model_overrides(); model_save_model_data(&empty);
    require(empty.size == 16, "Default models were serialized as scenario overrides");
    std::free(empty.data);
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
    require(model_load_model_data(&overlays, true) == 1, "New scenario overlays did not reload");
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
