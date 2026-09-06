#include "map/figure.h"
#include "map/desirability.h"
#include "city/festival.h"
#include "city/god.h"
#include "building/HousingProfileDef.h"
#include "map/building.h"
#include <unordered_set>
#include "city/figures.h"
#include "condition_types.h"

#include "building/count.h"
#include "building/building_type.h"
#include "city/data_private.h"
#include "city/emperor.h"
#include "city/finance.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/military.h"
#include "city/ratings.h"
#include "core/random.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "game/settings.h"
#include "game/time.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "scenario/custom_variable.h"
#include "scenario/event/condition_comparison_helper.h"
#include "scenario/event/controller.h"
#include "scenario/event/formula.h"
#include "scenario/event/parameter_data.h"
#include "scenario/request.h"
#include "scenario/scenario.h"

static int count_no_condition(int grid_offset)
{
    (void) grid_offset;
    return 1;
}

static int count_not_overgrown(int grid_offset)
{
    return !map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset);
}

static int special_building_count(int type, int active_only, int minx, int miny, int maxx, int maxy, int in_area, int *handled)
{
    *handled = 1;
    switch (type) {
        case SCENARIO_BUILDING_MENU_FARMS:
            return in_area ? building_set_area_count_farms(minx, miny, maxx, maxy) : building_set_count_farms(active_only);
        case SCENARIO_BUILDING_MENU_RAW_MATERIALS:
            return in_area ? building_set_area_count_raw_materials(minx, miny, maxx, maxy) : building_set_count_raw_materials(active_only);
        case SCENARIO_BUILDING_MENU_WORKSHOPS:
            return in_area ? building_set_area_count_workshops(minx, miny, maxx, maxy) : building_set_count_workshops(active_only);
        case SCENARIO_BUILDING_MENU_SMALL_TEMPLES:
            return in_area ? building_set_area_count_small_temples(minx, miny, maxx, maxy) : building_set_count_small_temples(active_only);
        case SCENARIO_BUILDING_MENU_LARGE_TEMPLES:
            return in_area ? building_set_area_count_large_temples(minx, miny, maxx, maxy) : building_set_count_large_temples(active_only);
        case SCENARIO_BUILDING_MENU_GRAND_TEMPLES:
            return in_area ? building_set_area_count_grand_temples(minx, miny, maxx, maxy) :
                active_only ? building_count_grand_temples_active() : building_count_grand_temples();
        case SCENARIO_BUILDING_MENU_TREES:
            return in_area ? building_set_area_count_deco_trees(minx, miny, maxx, maxy) : building_set_count_deco_trees();
        case SCENARIO_BUILDING_MENU_PATHS:
            return in_area ? building_set_area_count_deco_paths(minx, miny, maxx, maxy) : building_set_count_deco_paths();
        case SCENARIO_BUILDING_MENU_PARKS:
            return in_area ? building_set_area_count_deco_statues(minx, miny, maxx, maxy) : building_set_count_deco_statues();
        case SCENARIO_BUILDING_ANY:
            if (!in_area) {
                return building_count_any_total(active_only);
            }
            break;
        case SCENARIO_BUILDING_ROAD:
            return in_area ? building_count_terrain_in_area(minx, miny, maxx + 1, maxy + 1,
                TERRAIN_ROAD, count_no_condition) : building_count_terrain(TERRAIN_ROAD, count_no_condition);
        case SCENARIO_BUILDING_HIGHWAY:
            return in_area ? building_count_terrain_in_area(minx, miny, maxx + 1, maxy + 1,
                TERRAIN_HIGHWAY, count_no_condition) : building_count_terrain(TERRAIN_HIGHWAY, count_no_condition);
        case SCENARIO_BUILDING_PLAZA:
            return in_area ? building_count_terrain_in_area(minx, miny, maxx + 1, maxy + 1,
                TERRAIN_ROAD, map_property_is_plaza_earthquake_or_overgrown_garden) :
                building_count_terrain(TERRAIN_ROAD, map_property_is_plaza_earthquake_or_overgrown_garden);
        case SCENARIO_BUILDING_GARDENS:
            return in_area ? building_count_terrain_in_area(minx, miny, maxx + 1, maxy + 1,
                TERRAIN_GARDEN, count_not_overgrown) : building_count_terrain(TERRAIN_GARDEN, count_not_overgrown);
        case SCENARIO_BUILDING_OVERGROWN_GARDENS:
            return in_area ? building_count_terrain_in_area(minx, miny, maxx + 1, maxy + 1,
                TERRAIN_GARDEN, map_property_is_plaza_earthquake_or_overgrown_garden) :
                building_count_terrain(TERRAIN_GARDEN, map_property_is_plaza_earthquake_or_overgrown_garden);
        case SCENARIO_BUILDING_RUBBLE:
            return in_area ? building_count_terrain_in_area(minx, miny, maxx + 1, maxy + 1,
                TERRAIN_RUBBLE, count_no_condition) : building_count_terrain(TERRAIN_RUBBLE, count_no_condition);
        case SCENARIO_BUILDING_LOW_BRIDGE:
            return in_area ? building_count_bridges_in_area(minx, miny, maxx + 1, maxy + 1, 0) : building_count_bridges(0);
        case SCENARIO_BUILDING_SHIP_BRIDGE:
            return in_area ? building_count_bridges_in_area(minx, miny, maxx + 1, maxy + 1, 1) : building_count_bridges(1);
        default:
            break;
    }
    *handled = 0;
    return 0;
}

int scenario_condition_type_building_count_active_met(const scenario_condition_t *condition)
{
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);
    building_type type = static_cast<building_type>(condition->parameter3);

    int handled = 0;
    int total_count = special_building_count(type, 1, 0, 0, 0, 0, 0, &handled);
    if (!handled) {
        total_count = building_count_active(type);
    }

    return comparison_helper_compare_values(comparison, total_count, value);
}

int scenario_condition_type_building_count_any_met(const scenario_condition_t *condition)
{
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);
    building_type type = static_cast<building_type>(condition->parameter3);

    int handled = 0;
    int total_count = special_building_count(type, 0, 0, 0, 0, 0, 0, &handled);
    if (!handled) {
        total_count = building_count_total(type);
    }

    return comparison_helper_compare_values(comparison, total_count, value);
}

int scenario_condition_type_check_formulas(const scenario_condition_t *condition)
{
    int formula_id1 = condition->parameter1;
    int comparison = condition->parameter2;
    int formula_id2 = condition->parameter3;
    int formula_evaluation1 = scenario_formula_evaluate_formula(formula_id1);
    int formula_evaluation2 = scenario_formula_evaluate_formula(formula_id2);

    return comparison_helper_compare_values(comparison, formula_evaluation1, formula_evaluation2);
}

int scenario_condition_type_terrain_count_area_met(const scenario_condition_t *condition)
{
    int grid_offset1 = condition->parameter1;
    int grid_offset2 = condition->parameter2;
    int terrain_type = condition->parameter3;
    int comparison = condition->parameter4;
    int value = scenario_formula_evaluate_formula(condition->parameter5);

    int current_count = 0;
    grid_slice *slice = map_grid_get_grid_slice_from_corner_offsets(grid_offset1, grid_offset2);
    for (int i = 0; i < slice->size; i++) {
        int grid_offset = slice->grid_offsets[i];
        if (map_terrain_is(grid_offset, terrain_type)) {
            current_count++;
        }
    }
    return comparison_helper_compare_values(comparison, current_count, value);
}

int scenario_condition_type_building_count_area_met(const scenario_condition_t *condition)
{
    int grid_offset1 = condition->parameter1;
    int grid_offset2 = condition->parameter2;
    building_type type = static_cast<building_type>(condition->parameter3);
    int comparison = condition->parameter4;
    int value = scenario_formula_evaluate_formula(condition->parameter5);

    int minx = map_grid_offset_to_x(grid_offset1);
    int miny = map_grid_offset_to_y(grid_offset1);
    int maxx = map_grid_offset_to_x(grid_offset2);
    int maxy = map_grid_offset_to_y(grid_offset2);
    int handled = 0;
    int buildings_in_area = special_building_count(type, 0, minx, miny, maxx, maxy, 1, &handled);
    if (!handled) {
        buildings_in_area = building_count_in_area(type, minx, miny, maxx, maxy);
    }

    return comparison_helper_compare_values(comparison, buildings_in_area, value);
}

int scenario_condition_type_city_population_met(const scenario_condition_t *condition)
{
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);
    int population_class = condition->parameter3;

    int population_value_to_use = city_data.population.population;
    if (population_class == POP_CLASS_PATRICIAN) {
        population_value_to_use = city_data.population.people_in_villas_palaces;
    } else if (population_class == POP_CLASS_PLEBEIAN) {
        population_value_to_use = city_data.population.population - city_data.population.people_in_villas_palaces;
    } else if (population_class == POP_CLASS_SLUMS) {
        population_value_to_use = city_data.population.people_in_tents_shacks;
    }

    return comparison_helper_compare_values(comparison, population_value_to_use, value);
}

int scenario_condition_type_count_own_troops_met(const scenario_condition_t *condition)
{
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);
    int in_city_only = condition->parameter3;

    int soldier_count = in_city_only ? city_military_total_soldiers_in_city() : city_military_total_soldiers();

    return comparison_helper_compare_values(comparison, soldier_count, value);
}

int scenario_condition_type_custom_variable_check_met(const scenario_condition_t *condition)
{
    int target_variable = scenario_custom_variable_get_value(condition->parameter1);
    int comparison = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);

    return comparison_helper_compare_values(comparison, target_variable, value);
}

int scenario_condition_type_difficulty_met(const scenario_condition_t *condition)
{
    int difficulty = setting_difficulty();
    int comparison = condition->parameter1;
    int value = condition->parameter2;

    return comparison_helper_compare_values(comparison, difficulty, value);
}

int scenario_condition_type_money_met(const scenario_condition_t *condition)
{
    int funds = city_finance_treasury();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, funds, value);
}

int scenario_condition_type_population_unemployed_met(const scenario_condition_t *condition)
{
    int use_percentage = condition->parameter1;
    int comparison = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);

    int unemployed_total = use_percentage ? city_labor_unemployment_percentage() : city_labor_workers_unemployed();

    return comparison_helper_compare_values(comparison, unemployed_total, value);
}

int scenario_condition_type_request_is_ongoing_met(const scenario_condition_t *condition)
{
    int request_id = condition->parameter1;
    int check_for_ongoing = condition->parameter2;
    int is_ongoing = scenario_request_is_ongoing(request_id);

    return check_for_ongoing ? is_ongoing : !is_ongoing;
}

int scenario_condition_type_resource_storage_available_met(const scenario_condition_t *condition)
{
    int resource = condition->parameter1;
    int comparison = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);
    storage_types storage_type = static_cast<storage_types>(condition->parameter4);

    if (resource < (RESOURCE_NONE + 1) || resource > RESOURCE_SLOT_COUNT) {
        return 0;
    }

    int storage_available = 0;
    switch (storage_type) {
        case STORAGE_TYPE_ALL:
            storage_available += city_resource_get_available_empty_space_warehouses(resource);
            storage_available += city_resource_get_available_empty_space_granaries(resource);
            break;
        case STORAGE_TYPE_GRANARIES:
            storage_available += city_resource_get_available_empty_space_granaries(resource);
            break;
        case STORAGE_TYPE_WAREHOUSES:
            storage_available += city_resource_get_available_empty_space_warehouses(resource);
            break;
        default:
            break;
    }

    return comparison_helper_compare_values(comparison, storage_available, value);
}

int scenario_condition_type_resource_stored_count_met(const scenario_condition_t *condition)
{
    int resource = condition->parameter1;
    int comparison = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);
    storage_types storage_type = static_cast<storage_types>(condition->parameter4);

    if (resource < (RESOURCE_NONE + 1) || resource > RESOURCE_SLOT_COUNT) {
        return 0;
    }

    int amount_stored = 0;
    switch (storage_type) {
        case STORAGE_TYPE_ALL:
            amount_stored += city_resource_count_warehouses_amount(resource);
            if (resource_is_food(resource)) {
                amount_stored += city_resource_count_food_on_granaries(resource) / resource_units_per_load();
            }
            break;
        case STORAGE_TYPE_GRANARIES:
            if (resource_is_food(resource)) {
                amount_stored += city_resource_count_food_on_granaries(resource) / resource_units_per_load();
            }
            break;
        case STORAGE_TYPE_WAREHOUSES:
            amount_stored += city_resource_count_warehouses_amount(resource);
            break;
        default:
            break;
    }

    return comparison_helper_compare_values(comparison, amount_stored, value);
}

int scenario_condition_type_rome_wages_met(const scenario_condition_t *condition)
{
    int wages = city_labor_wages_rome();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, wages, value);
}

int scenario_condition_type_savings_met(const scenario_condition_t *condition)
{
    int funds = city_emperor_personal_savings();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, funds, value);
}

int scenario_condition_type_stats_city_health_met(const scenario_condition_t *condition)
{
    int stat_value = city_health();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, stat_value, value);
}

int scenario_condition_type_stats_culture_met(const scenario_condition_t *condition)
{
    int stat_value = city_rating_culture();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, stat_value, value);
}

int scenario_condition_type_stats_favor_met(const scenario_condition_t *condition)
{
    int stat_value = city_rating_favor();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, stat_value, value);
}

int scenario_condition_type_stats_peace_met(const scenario_condition_t *condition)
{
    int stat_value = city_rating_peace();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, stat_value, value);
}

int scenario_condition_type_stats_prosperity_met(const scenario_condition_t *condition)
{
    int stat_value = city_rating_prosperity();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, stat_value, value);
}

void scenario_condition_type_time_init(scenario_condition_t *condition)
{
    if (condition->parameter5 == 1) condition->parameter4 = scenario_formula_evaluate_formula(condition->parameter2);
}

int scenario_condition_type_time_met(const scenario_condition_t *condition)
{
    int total_months = game_time_total_months();
    int comparison = condition->parameter1;
    int target_months = condition->parameter5 == 1 ? condition->parameter4 : scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, total_months, target_months);
}

int scenario_condition_type_trade_route_open_met(const scenario_condition_t *condition)
{
    int route_id = condition->parameter1;
    int check_for_open = condition->parameter2;

    if (!trade_route_is_valid(route_id)) {
        return 0;
    }

    int route_is_open = empire_city_is_trade_route_open(route_id);
    return route_is_open == check_for_open;
}

int scenario_condition_type_trade_route_price_met(const scenario_condition_t *condition)
{
    int route_id = condition->parameter1;
    int comparison = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);

    if (!trade_route_is_valid(route_id)) {
        return 0;
    }

    int route_is_open = empire_city_is_trade_route_open(route_id);
    int route_price = route_is_open ? 0 : empire_city_get_trade_route_cost(route_id);
    return comparison_helper_compare_values(comparison, route_price, value);
}

int scenario_condition_type_trade_sell_price_met(const scenario_condition_t *condition)
{
    int resource = condition->parameter1;
    int comparison = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);

    if (resource < (RESOURCE_NONE + 1) || resource > RESOURCE_SLOT_COUNT) {
        return 0;
    }

    int trade_sell_price = trade_price_base_sell(resource);
    return comparison_helper_compare_values(comparison, trade_sell_price, value);
}

int scenario_condition_type_tax_rate_met(const scenario_condition_t *condition)
{
    int tax_rate = city_finance_tax_percentage();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, tax_rate, value);
}

int scenario_condition_type_count_enemies_in_city_met(const scenario_condition_t *condition)
{
    int enemies_in_city = city_figures_total_invading_enemies();
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, enemies_in_city, value);
}

int scenario_condition_type_land_trade_problems_met(const scenario_condition_t *condition)
{
    int trade_problem_duration = city_data.trade.land_trade_problem_duration;
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, trade_problem_duration, value);
}

int scenario_condition_type_sea_trade_problems_met(const scenario_condition_t *condition)
{
    int trade_problem_duration = city_data.trade.sea_trade_problem_duration;
    int comparison = condition->parameter1;
    int value = scenario_formula_evaluate_formula(condition->parameter2);

    return comparison_helper_compare_values(comparison, trade_problem_duration, value);
}

int scenario_condition_type_months_since_last_festival_met(const scenario_condition_t *condition)
{
    int comparison = condition->parameter1;
    int god = condition->parameter2;
    int value = scenario_formula_evaluate_formula(condition->parameter3);

    int months_since_last_festival = city_festival_months_since_last();
    if (god != GOD_ALL) {
        months_since_last_festival = city_god_months_since_festival(god);
    }

    return comparison_helper_compare_values(comparison, months_since_last_festival, value);
}

int scenario_condition_type_desirability_in_area_met(const scenario_condition_t *condition)
{
    int grid_offset1 = condition->parameter1;
    int grid_offset2 = condition->parameter2;
    int comparison = condition->parameter3;
    int value = scenario_formula_evaluate_formula(condition->parameter4);

    int64_t desirability_sum = 0;
    int tiles = 0;
    grid_slice *slice = map_grid_get_grid_slice_from_corner_offsets(grid_offset1, grid_offset2);

    for (int i = 0; i < slice->size; i++) {
        int grid_offset = slice->grid_offsets[i];
        if (!map_grid_is_valid_offset(grid_offset)) continue;
        ++tiles;
        desirability_sum += map_desirability_get(grid_offset);
    }
    if (!tiles) return 0;
    int desirability_mean = static_cast<int>(desirability_sum / tiles);

    return comparison_helper_compare_values(comparison, desirability_mean, value);
}

int scenario_condition_type_figures_in_area_met(const scenario_condition_t *condition)
{
    int grid_offset1 = condition->parameter1;
    int grid_offset2 = condition->parameter2;
    auto category = static_cast<figure_category_mask>(condition->parameter3);
    int comparison = condition->parameter4;
    int value = scenario_formula_evaluate_formula(condition->parameter5);

    grid_slice *slice = map_grid_get_grid_slice_from_corner_offsets(grid_offset1, grid_offset2);

    int total_figures = map_count_figures_category_in_area(slice, category);

    return comparison_helper_compare_values(comparison, total_figures, value);
}

int scenario_condition_type_population_in_area_met(const scenario_condition_t *condition)
{
    const int selection = condition->parameter3;
    int min_level = selection, max_level = selection;
    switch (selection) {
        case HOUSE_GROUP_TENT: min_level = HOUSE_SMALL_TENT; max_level = HOUSE_LARGE_TENT; break;
        case HOUSE_GROUP_SHACK: min_level = HOUSE_SMALL_SHACK; max_level = HOUSE_LARGE_SHACK; break;
        case HOUSE_GROUP_HOVEL: min_level = HOUSE_SMALL_HOVEL; max_level = HOUSE_LARGE_HOVEL; break;
        case HOUSE_GROUP_CASA: min_level = HOUSE_SMALL_CASA; max_level = HOUSE_LARGE_CASA; break;
        case HOUSE_GROUP_INSULA: min_level = HOUSE_SMALL_INSULA; max_level = HOUSE_GRAND_INSULA; break;
        case HOUSE_GROUP_VILLA: min_level = HOUSE_SMALL_VILLA; max_level = HOUSE_GRAND_VILLA; break;
        case HOUSE_GROUP_PALACE: min_level = HOUSE_SMALL_PALACE; max_level = HOUSE_LUXURY_PALACE; break;
    }
    int population = 0;
    std::unordered_set<const Building *> counted;
    const grid_slice *slice = map_grid_get_grid_slice_from_corner_offsets(condition->parameter1, condition->parameter2);
    for (int i = 0; i < slice->size; ++i) {
        int tile = slice->grid_offsets[i];
        if (!map_grid_is_valid_offset(tile) || !map_building_exists_at(tile)) continue;
        const Building &building = map_building_at(tile);
        if (!building.Housing || !building.Housing->is_occupied() || !counted.insert(&building).second) continue;
        const auto *profile = building.Housing->definition().profile;
        if (profile && profile->compatibility_level >= min_level && profile->compatibility_level <= max_level) population += building.Housing->state().population;
    }
    return comparison_helper_compare_values(condition->parameter4, population, scenario_formula_evaluate_formula(condition->parameter5));
}
