#include "game/undo.h"
#include "map/building.h"
#include "map/tiles.h"
#include "figure/figure.h"

#include "building/building_record.h"
#include "house_evolution.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/house.h"
#include "building/housing_type.h"
#include "city/houses.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/time.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/grid.h"
#include "figure/route.h"
#include "map/terrain.h"

#define DEVOLVE_DELAY 2
#define DEVOLVE_DELAY_WITH_VENUS 20

typedef enum {
    EVOLVE = 1,
    NONE = 0,
    DEVOLVE = -1
} evolve_status;

static int active_devolve_delay;

static void consume_resources(building *b);

static const model_house *model_for_house(Building house)
{
    return building_house_get_model(house);
}

static const model_house *model_for_house_requirements(building *house, int for_upgrade, int with_bonus, int *out_level)
{
    Building house_object(house);
    int level = building_house_legacy_level(house_object);
    if (for_upgrade) {
        ++level;
    }
    if (with_bonus) {
        --level;
    }
    level = calc_bound(level, HOUSE_MIN, HOUSE_MAX);
    if (out_level) {
        *out_level = level;
    }

    if (house_object.type && house_object.type->has_housing()) {
        building_type target = BUILDING_NONE;
        if (for_upgrade) {
            target = house_object.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::EvolveTo);
        }
        const auto *target_type = target == BUILDING_NONE ? nullptr :
            building_type_registry_impl::definition_for_type(target);
        const auto *target_housing = target_type ? target_type->housing_type() : nullptr;
        return target_housing ? &target_housing->model() : model_for_house(house_object);
    }

    return model_get_house(static_cast<house_level>(level));
}

static evolve_status check_evolve_desirability(building *house, int bonus)
{
    int level = building_house_legacy_level(Building(house));
    level -= bonus;
    level = calc_bound(level, HOUSE_MIN, HOUSE_MAX);
    const model_house *model = model_for_house(Building(house));
    int evolve_des = model->evolve_desirability;
    if (level >= HOUSE_LUXURY_PALACE) {
        evolve_des = 1000;
    }
    int current_des = house->desirability;
    evolve_status status;
    if (current_des <= model->devolve_desirability) {
        status = DEVOLVE;
    } else if (current_des >= evolve_des) {
        status = EVOLVE;
    } else {
        status = NONE;
    }
    house->data.house.evolve_text_id = status; // BUG? -1 in an unsigned char?
    return status;
}

static int has_required_goods_and_services(building *house, int for_upgrade, int with_bonus, house_demands *demands)
{
    int level = 0;
    const model_house *model = model_for_house_requirements(house, for_upgrade, with_bonus, &level);
    // water
    int water = model->water;
    if (!house->has_water_access) {
        if (water >= 2) {
            if (level > HOUSE_SMALL_CASA) {
                ++demands->missing.fountain;
                return  0;
            } else if (!house->has_well_access) {
                ++demands->missing.well;
                return 0;
            } else if (level > HOUSE_LARGE_SHACK && !house->has_latrines_access) {
                return 0;
            }
        }
        if (water == 1 && !house->has_well_access) {
            ++demands->missing.well;
            return 0;
        }
    }

    // entertainment
    int entertainment = model->entertainment;
    if (house->data.house.entertainment < entertainment) {
        if (house->data.house.entertainment) {
            ++demands->missing.more_entertainment;
        } else {
            ++demands->missing.entertainment;
        }
        return 0;
    }
    // education
    int education = model->education;
    if (house->data.house.education < education) {
        if (house->data.house.education) {
            ++demands->missing.more_education;
        } else {
            ++demands->missing.education;
        }
        return 0;
    }
    if (education == 2) {
        ++demands->requiring.school;
        ++demands->requiring.library;
    } else if (education == 1) {
        ++demands->requiring.school;
    }
    // religion
    int religion = model->religion;
    if (religion > 3) {
        religion = 3;
    }
    if (house->data.house.num_gods < religion) {
        if (religion == 1) {
            ++demands->missing.religion;
            return 0;
        } else if (religion == 2) {
            ++demands->missing.second_religion;
            return 0;
        } else if (religion >= 3) {
            ++demands->missing.third_religion;
            return 0;
        }
    } else if (religion > 0) {
        ++demands->requiring.religion;
    }
    // barber
    int barber = model->barber;
    if (house->data.house.barber < barber) {
        ++demands->missing.barber;
        return 0;
    }
    if (barber == 1) {
        ++demands->requiring.barber;
    }
    // bathhouse
    int bathhouse = model->bathhouse;
    if (house->data.house.bathhouse < bathhouse) {
        ++demands->missing.bathhouse;
        return 0;
    }
    if (bathhouse == 1) {
        ++demands->requiring.bathhouse;
    }
    // health
    int health = model->health;
    if (house->data.house.health < health) {
        if (health < 2) {
            ++demands->missing.clinic;
        } else {
            ++demands->missing.hospital;
        }
        return 0;
    }
    if (health >= 1) {
        ++demands->requiring.clinic;
    }
    // food types
    int foodtypes_required = model->food_types;
    int foodtypes_available = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (house->resources[r] && resource_is_food(r)) {
            foodtypes_available++;
        }
    }
    if (foodtypes_available < foodtypes_required) {
        ++demands->missing.food;
        return 0;
    }
    // goods
    if (house->resources[resource_pottery()] < model->pottery) {
        return 0;
    }
    if (house->resources[resource_oil()] < model->oil) {
        return 0;
    }
    if (house->resources[resource_furniture()] < model->furniture) {
        return 0;
    }
    int wine = model->wine;
    if (wine && house->resources[resource_wine()] <= 0) {
        return 0;
    }
    if (wine > 1 && !city_resource_multiple_wine_available()) {
        ++demands->missing.second_wine;
        return 0;
    }
    return 1;
}

static evolve_status check_requirements(building *house, house_demands *demands)
{
    int bonus = 0;
    if (building_monument_pantheon_module_is_active(PANTHEON_MODULE_2_HOUSING_EVOLUTION) && house->house_pantheon_access) {
        bonus++;
    }
    evolve_status status = check_evolve_desirability(house, bonus);
    if (!has_required_goods_and_services(house, 0, bonus, demands)) {
        status = DEVOLVE;
    } else if (status == EVOLVE) {
        status = has_required_goods_and_services(house, 1, bonus, demands) ? EVOLVE : NONE;
    }
    return status;
}

static int has_devolve_delay(building *house, evolve_status status)
{
    if (status == DEVOLVE && house->data.house.devolve_delay < active_devolve_delay) {
        house->data.house.devolve_delay++;
        return 1;
    } else {
        house->data.house.devolve_delay = 0;
        return 0;
    }
}

static int evolve_xml_housing(building *house, house_demands *demands, time_millis last_update)
{
    Building house_object(house);
    building_type merge_to = house_object.type ?
        house_object.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::MergeTo) :
        BUILDING_NONE;
    int is_empty_vacant_lot =
        house->house_population <= 0 && house->type == building_type_registry_impl::vacant_lot_fill_type();
    if (merge_to != BUILDING_NONE && house->house_size == 1 &&
        (house->house_population > 0 ||
            (is_empty_vacant_lot && config_get(CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS)))) {
        unsigned int merged_id = building_house_merge(house_object);
        if (merged_id) {
            building *merged_house = building_get(merged_id);
            if (merged_house) {
                if (game_time_day() == 0 || (game_time_day() == 7 && merged_house->house_size > 1)) {
                    consume_resources(merged_house);
                }
                merged_house->last_update = last_update;
            }
            return 0;
        }
    }

    if (house->house_population <= 0) {
        return 0;
    }

    evolve_status status = check_requirements(house, demands);
    if (has_devolve_delay(house, status)) {
        return 0;
    }

    building_type target = BUILDING_NONE;
    if (status == EVOLVE) {
        target = house_object.type ?
            house_object.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::EvolveTo) :
            BUILDING_NONE;
    } else if (status == DEVOLVE) {
        target = house_object.type ?
            house_object.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::DevolveTo) :
            BUILDING_NONE;
    }

    if (target != BUILDING_NONE) {
        int current_size = house->house_size;
        const building_type_registry_impl::BuildingType *target_definition =
            building_type_registry_impl::definition_for_type(target);
        int target_size = target_definition ? target_definition->declared_model_size() : 0;
        if (status == EVOLVE && target_size > current_size) {
            if (building_house_can_expand(house_object, target_size * target_size)) {
                game_undo_disable();
                building_house_expand_to_type(house_object, target);
                map_tiles_update_all_gardens();
                return 1;
            }
        } else if (status == DEVOLVE) {
            if (current_size > 1) {
                game_undo_disable();
            }
            building_house_devolve_to_type(house_object, target);
        } else {
            building_house_change_to(house_object, target);
        }
    }
    return 0;
}

static void consume_resource(building *b, int inventory, int amount)
{
    if (amount > 0) {
        if (amount > b->resources[inventory]) {
            b->resources[inventory] = 0;
        } else {
            b->resources[inventory] -= amount;
        }
    }
}

static void consume_resources(building *b)
{
    int consumption_reduction[RESOURCE_SLOT_COUNT] = { 0 };

    // mercury module 1 - pottery and furniture reduced by 20%
    if (building_monument_gt_module_is_active(MERCURY_MODULE_1_POTTERY_FURN)) {
        consumption_reduction[resource_pottery()] += 20;
        consumption_reduction[resource_furniture()] += 20;
    }
    // mercury module 2 - oil and wine reduced by 20%
    if (b->data.house.temple_mercury && building_monument_gt_module_is_active(MERCURY_MODULE_2_OIL_WINE)) {
        consumption_reduction[resource_wine()] += 20;
        consumption_reduction[resource_oil()] += 20;
    }
    // mars module 2 - all goods reduced by 10%
    if (b->data.house.temple_mars && building_monument_gt_module_is_active(MARS_MODULE_2_ALL_GOODS)) {
        consumption_reduction[resource_wine()] += 10;
        consumption_reduction[resource_oil()] += 10;
        consumption_reduction[resource_pottery()] += 10;
        consumption_reduction[resource_furniture()] += 10;
    }

    const model_house *model = model_for_house(Building(b));
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (!resource_is_inventory_good(r)) {
            continue;
        }
        if (!consumption_reduction[r] ||
            (game_time_total_months() % (100 / consumption_reduction[r]))) {
            int amount = 0;
            if (r == resource_wine()) {
                amount = model->wine;
            } else if (r == resource_oil()) {
                amount = model->oil;
            } else if (r == resource_furniture()) {
                amount = model->furniture;
            } else if (r == resource_pottery()) {
                amount = model->pottery;
            }
            consume_resource(b, r, amount);
        }
    }
}

void building_house_process_evolve_and_consume_goods(void)
{
    city_houses_reset_demands();
    house_demands *demands = city_houses_demands();
    int has_expanded = 0;

    if (building_monument_working_grand_temple_for_god(GOD_VENUS)) {
        active_devolve_delay = DEVOLVE_DELAY_WITH_VENUS;
    } else {
        active_devolve_delay = DEVOLVE_DELAY;
    }

    time_millis last_update = time_get_millis();

    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (!b || b->state != BUILDING_STATE_IN_USE || !b->house_size || b->last_update == last_update) {
            continue;
        }
        Building house_object(b);
        building_house_check_for_corruption(house_object);
        if (!b->has_plague) {
            if (house_object.type && house_object.type->has_housing()) {
                has_expanded |= evolve_xml_housing(b, demands, last_update);
            }
        }
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
            continue;
        }
        // 1x1 houses only consume half of the goods
        if (game_time_day() == 0 || (game_time_day() == 7 && b->house_size > 1)) {
            consume_resources(b);
        }
        b->last_update = last_update;
    }
    if (has_expanded) {
        Route::updateLandTerrain();
    }
}

void building_house_determine_evolve_text(Building house_object, int worst_desirability_building)
{
    building *house = house_object.id() ? building_get(house_object.id()) : nullptr;
    if (!house) {
        return;
    }
    int level = building_house_legacy_level(house_object);
    if (building_monument_pantheon_module_is_active(PANTHEON_MODULE_2_HOUSING_EVOLUTION) && house->house_pantheon_access) {
        level--;
    }
    level = calc_bound(level, HOUSE_MIN, HOUSE_MAX);

    // this house will devolve soon because...

    const model_house *model = model_for_house(house_object);
    // desirability
    if (house->desirability <= model->devolve_desirability) {
        house->data.house.evolve_text_id = 0;
        return;
    }
    // water
    int water = model->water;
    if (water == 1 && !house->has_water_access) {
        if (!house->has_well_access) {
            house->data.house.evolve_text_id = 1;
            return;
        } else if (!house->has_latrines_access) {
            house->data.house.evolve_text_id = 68;
            return;
        }
    }

    if (water == 2 && !house->has_water_access) {
        if (!house->has_latrines_access) {
            house->data.house.evolve_text_id = 67;
            return;
        } else if (level >= HOUSE_LARGE_CASA) {
            house->data.house.evolve_text_id = 2;
            return;
        }
    }

    // entertainment
    int entertainment = model->entertainment;
    if (house->data.house.entertainment < entertainment) {
        if (!house->data.house.entertainment) {
            house->data.house.evolve_text_id = 3;
        } else if (entertainment < 10) {
            house->data.house.evolve_text_id = 4;
        } else if (entertainment < 25) {
            house->data.house.evolve_text_id = 5;
        } else if (entertainment < 50) {
            house->data.house.evolve_text_id = 6;
        } else if (entertainment < 80) {
            house->data.house.evolve_text_id = 7;
        } else {
            house->data.house.evolve_text_id = 8;
        }
        return;
    }
    // food types
    int foodtypes_required = model->food_types;
    int foodtypes_available = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (house->resources[r] && resource_is_food(r)) {
            foodtypes_available++;
        }
    }
    if (foodtypes_available < foodtypes_required) {
        if (foodtypes_required == 1) {
            house->data.house.evolve_text_id = 9;
            return;
        } else if (foodtypes_required == 2) {
            house->data.house.evolve_text_id = 10;
            return;
        } else if (foodtypes_required == 3) {
            house->data.house.evolve_text_id = 11;
            return;
        }
    }
    // education
    int education = model->education;
    if (house->data.house.education < education) {
        if (education == 1) {
            house->data.house.evolve_text_id = 14;
            return;
        } else if (education == 2) {
            if (house->data.house.school) {
                house->data.house.evolve_text_id = 15;
                return;
            } else if (house->data.house.library) {
                house->data.house.evolve_text_id = 16;
                return;
            }
        } else if (education == 3) {
            house->data.house.evolve_text_id = 17;
            return;
        }
    }
    // bathhouse
    if (house->data.house.bathhouse < model->bathhouse) {
        house->data.house.evolve_text_id = 18;
        return;
    }
    // pottery
    if (house->resources[resource_pottery()] < model->pottery) {
        house->data.house.evolve_text_id = 19;
        return;
    }
    // religion
    int religion = model->religion;
    if (religion > 3) {
        religion = 3;
    }
    if (house->data.house.num_gods < religion) {
        if (religion == 1) {
            house->data.house.evolve_text_id = 20;
            return;
        } else if (religion == 2) {
            house->data.house.evolve_text_id = 21;
            return;
        } else if (religion == 3) {
            house->data.house.evolve_text_id = 22;
            return;
        }
    }
    // barber
    if (house->data.house.barber < model->barber) {
        house->data.house.evolve_text_id = 23;
        return;
    }
    // health
    int health = model->health;
    if (house->data.house.health < health) {
        if (health == 1) {
            house->data.house.evolve_text_id = 24;
        } else if (house->data.house.clinic) {
            house->data.house.evolve_text_id = 25;
        } else {
            house->data.house.evolve_text_id = 26;
        }
        return;
    }
    // oil
    if (house->resources[resource_oil()] < model->oil) {
        house->data.house.evolve_text_id = 27;
        return;
    }
    // furniture
    if (house->resources[resource_furniture()] < model->furniture) {
        house->data.house.evolve_text_id = 28;
        return;
    }
    // wine
    int wine = model->wine;
    if (house->resources[resource_wine()] < wine) {
        house->data.house.evolve_text_id = 29;
        return;
    }
    if (wine > 1 && !city_resource_multiple_wine_available()) {
        house->data.house.evolve_text_id = 65;
        return;
    }
    if (level >= HOUSE_LUXURY_PALACE) { // max level!
        house->data.house.evolve_text_id = 60;
        return;
    }

    // this house will evolve if ...

    // desirability
    if (house->desirability < model->evolve_desirability) {
        if (worst_desirability_building) {
            house->data.house.evolve_text_id = 62;
        } else {
            house->data.house.evolve_text_id = 30;
        }
        return;
    }
    if (house_object.type && house_object.type->has_housing()) {
        building_type target =
            house_object.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::EvolveTo);
        const auto *target_type = target == BUILDING_NONE ? nullptr :
            building_type_registry_impl::definition_for_type(target);
        const auto *target_housing = target_type ? target_type->housing_type() : nullptr;
        model = target_housing ? &target_housing->model() : nullptr;
        if (!model) {
            model = model_for_house(house_object);
        }
    } else {
        model = model_get_house(static_cast<house_level>(++level));
    }
    // water
    water = model->water;
    if (water == 1 && !house->has_water_access) {
        if (!house->has_well_access) {
            house->data.house.evolve_text_id = 31;
            return;
        } else if (!house->has_latrines_access) {
            house->data.house.evolve_text_id = 68;
            return;
        }
    }

    if (water == 2 && !house->has_water_access) {
        if (level >= HOUSE_LARGE_CASA && house->has_well_access && house->has_latrines_access) {
            house->data.house.evolve_text_id = 32;
            return;
        }
    }


    // entertainment
    entertainment = model->entertainment;
    if (house->data.house.entertainment < entertainment) {
        if (!house->data.house.entertainment) {
            house->data.house.evolve_text_id = 33;
        } else if (entertainment < 10) {
            house->data.house.evolve_text_id = 34;
        } else if (entertainment < 25) {
            house->data.house.evolve_text_id = 35;
        } else if (entertainment < 50) {
            house->data.house.evolve_text_id = 36;
        } else if (entertainment < 80) {
            house->data.house.evolve_text_id = 37;
        } else {
            house->data.house.evolve_text_id = 38;
        }
        return;
    }
    // food types
    foodtypes_required = model->food_types;
    if (foodtypes_available < foodtypes_required) {
        if (foodtypes_required == 1) {
            house->data.house.evolve_text_id = 39;
            return;
        } else if (foodtypes_required == 2) {
            house->data.house.evolve_text_id = 40;
            return;
        } else if (foodtypes_required == 3) {
            house->data.house.evolve_text_id = 41;
            return;
        }
    }
    // education
    education = model->education;
    if (house->data.house.education < education) {
        if (education == 1) {
            house->data.house.evolve_text_id = 44;
            return;
        } else if (education == 2) {
            if (house->data.house.school) {
                house->data.house.evolve_text_id = 45;
                return;
            } else if (house->data.house.library) {
                house->data.house.evolve_text_id = 46;
                return;
            }
        } else if (education == 3) {
            house->data.house.evolve_text_id = 47;
            return;
        }
    }
    // bathhouse
    if (house->data.house.bathhouse < model->bathhouse) {
        house->data.house.evolve_text_id = 48;
        return;
    }
    // pottery
    if (house->resources[resource_pottery()] < model->pottery) {
        house->data.house.evolve_text_id = 49;
        return;
    }
    // religion
    religion = model->religion;
    if (religion > 3) {
        religion = 3;
    }
    if (house->data.house.num_gods < religion) {
        if (religion == 1) {
            house->data.house.evolve_text_id = 50;
            return;
        } else if (religion == 2) {
            house->data.house.evolve_text_id = 51;
            return;
        } else if (religion == 3) {
            house->data.house.evolve_text_id = 52;
            return;
        }
    }
    // barber
    if (house->data.house.barber < model->barber) {
        house->data.house.evolve_text_id = 53;
        return;
    }
    // health
    health = model->health;
    if (house->data.house.health < health) {
        if (health == 1) {
            house->data.house.evolve_text_id = 54;
        } else if (house->data.house.clinic) {
            house->data.house.evolve_text_id = 55;
        } else {
            house->data.house.evolve_text_id = 56;
        }
        return;
    }
    // oil
    if (house->resources[resource_oil()] < model->oil) {
        house->data.house.evolve_text_id = 57;
        return;
    }
    // furniture
    if (house->resources[resource_furniture()] < model->furniture) {
        house->data.house.evolve_text_id = 58;
        return;
    }
    // wine
    wine = model->wine;
    if (house->resources[resource_wine()] < wine) {
        house->data.house.evolve_text_id = 59;
        return;
    }
    if (wine > 1 && !city_resource_multiple_wine_available()) {
        house->data.house.evolve_text_id = 66;
        return;
    }
    // house is evolving
    house->data.house.evolve_text_id = 61;
    if (house->data.house.no_space_to_expand == 1) {
        // house would like to evolve but can't
        house->data.house.evolve_text_id = 64;
    }
}

static building_type get_building_type_at_tile(Building house_object, int x, int y)
{
    const building *house = house_object.id() ? building_get(house_object.id()) : nullptr;
    if (!house) {
        return BUILDING_NONE;
    }
    int grid_offset = map_grid_offset(x, y);
    unsigned int building_id = (unsigned int) map_building_at(grid_offset);
    if (building_id <= 0) {
        if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
            return building_type_registry_impl::type_from_attr("highway");
        } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
            return building_type_registry_impl::type_from_attr("draggable_reservoir");
        } else {
            return BUILDING_NONE;
        }
    }
    building *b = building_get(building_id);
    if (b->state != BUILDING_STATE_IN_USE || building_id == house->id) {
        return BUILDING_NONE;
    }
    if (b->house_size) {
        int house_level = building_house_legacy_level(house_object);
        int other_house_level = building_house_legacy_level(Building(b));
        if (house_level >= 0 && other_house_level >= house_level) {
            return BUILDING_NONE;
        }
    }
    Building other(b);
    return other.type ? other.type->type() : BUILDING_NONE;
}

building_type building_house_determine_worst_desirability_building_type(Building house_object)
{
    const building *house = house_object.id() ? building_get(house_object.id()) : nullptr;
    if (!house) {
        return BUILDING_NONE;
    }
    int lowest_desirability = 0;
    building_type lowest_building_type = BUILDING_NONE;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(house->x, house->y, 1, 8, &x_min, &y_min, &x_max, &y_max);

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            building_type type = get_building_type_at_tile(house_object, x, y);
            const model_building *model = model_get_building(type);
            // simplified desirability calculation
            int des = model->desirability_value;
            int step = model->desirability_step;
            int step_size = model->desirability_step_size;
            int range = model->desirability_range;
            int tiles_within_step = 0;
            int dist = calc_maximum_distance(x, y, house->x, house->y);
            if (dist <= range) {
                while (dist >= 1) {
                    tiles_within_step++;
                    if (tiles_within_step >= step) {
                        des += step_size;
                        tiles_within_step = 0;
                    }
                    dist--;
                }
                if (des < lowest_desirability) {
                    lowest_desirability = des;
                    lowest_building_type = type;
                }
            }
        }
    }
    return lowest_building_type;
}
