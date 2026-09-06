#include "city/trade_ledger.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/tiles.h"
#include "figure/figure.h"

#include "building/building_record.h"
#include "house_evolution.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/house.h"
#include "building/HousingProfileDef.h"
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

static void consume_resources(Building &house_object, building *b);

static const building_type_registry_impl::HousingProfileDef *profile_for_house_requirements(
    Building &house_object, int for_upgrade, int with_bonus, int *out_level)
{
    const auto *current_profile =
        house_object.Housing ? house_object.Housing->definition().profile : nullptr;
    int level = current_profile ? current_profile->compatibility_level : -1;
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

    if (house_object.Housing) {
        const auto *target_type = for_upgrade
            ? house_object.Housing->transition_target(building_type_registry_impl::HousingTransitionKind::EvolveTo)
            : nullptr;
        const auto *target_profile = target_type ? target_type->housing_def().profile : nullptr;
        return target_profile ? target_profile : current_profile;
    }
    return nullptr;
}

static evolve_status check_evolve_desirability(Building &house_object, building *house, int bonus)
{
    const auto *profile = house_object.Housing ? house_object.Housing->definition().profile : nullptr;
    int level = profile ? profile->compatibility_level : -1;
    level -= bonus;
    level = calc_bound(level, HOUSE_MIN, HOUSE_MAX);
    if (!profile) {
        return NONE;
    }
    int evolve_des = profile->evolution.evolve_desirability;
    if (level >= HOUSE_LUXURY_PALACE) {
        evolve_des = 1000;
    }
    int current_des = house->desirability;
    evolve_status status;
    if (current_des <= profile->evolution.devolve_desirability) {
        status = DEVOLVE;
    } else if (current_des >= evolve_des) {
        status = EVOLVE;
    } else {
        status = NONE;
    }
    house_object.Housing->state().evolve_text_id = static_cast<uint8_t>(status); // BUG? -1 in an unsigned char?
    return status;
}

static int has_required_goods_and_services(Building &house_object, building *house, int for_upgrade, int with_bonus, house_demands *demands)
{
    int level = 0;
    const auto *profile = profile_for_house_requirements(house_object, for_upgrade, with_bonus, &level);
    if (!profile) {
        return 0;
    }
    const auto &requirements = profile->requirements;
    const HousingServiceState &services = house_object.Housing->state().services;
    // water
    const auto water = requirements.water;
    if (!house->has_water_access) {
        if (water == building_type_registry_impl::HousingWaterRequirement::Fountain) {
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
        if (water == building_type_registry_impl::HousingWaterRequirement::Well && !house->has_well_access) {
            ++demands->missing.well;
            return 0;
        }
    }

    // entertainment
    int entertainment = requirements.entertainment;
    if (services.entertainment < entertainment) {
        if (services.entertainment) {
            ++demands->missing.more_entertainment;
        } else {
            ++demands->missing.entertainment;
        }
        return 0;
    }
    // education
    int education = requirements.education;
    if (services.education < education) {
        if (services.education) {
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
    int religion = requirements.religion;
    if (religion > 3) {
        religion = 3;
    }
    if (services.num_gods < religion) {
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
    int barber = requirements.barber;
    if (services.barber < barber) {
        ++demands->missing.barber;
        return 0;
    }
    if (barber == 1) {
        ++demands->requiring.barber;
    }
    // bathhouse
    int bathhouse = requirements.bathhouse;
    if (services.bathhouse < bathhouse) {
        ++demands->missing.bathhouse;
        return 0;
    }
    if (bathhouse == 1) {
        ++demands->requiring.bathhouse;
    }
    // health
    int health = requirements.health;
    if (services.health < health) {
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
    int foodtypes_required = requirements.food_types;
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
    if (house->resources[resource_pottery()] < requirements.pottery) {
        return 0;
    }
    if (house->resources[resource_oil()] < requirements.oil) {
        return 0;
    }
    if (house->resources[resource_furniture()] < requirements.furniture) {
        return 0;
    }
    int wine = requirements.wine;
    if (wine && house->resources[resource_wine()] <= 0) {
        return 0;
    }
    if (wine > 1 && !city_resource_multiple_wine_available()) {
        ++demands->missing.second_wine;
        return 0;
    }
    return 1;
}

static evolve_status check_requirements(Building &house_object, building *house, house_demands *demands)
{
    int bonus = 0;
    if (building_monument_pantheon_module_is_active(PANTHEON_MODULE_2_HOUSING_EVOLUTION) &&
        house_object.Housing->state().pantheon_access) {
        bonus++;
    }
    evolve_status status = check_evolve_desirability(house_object, house, bonus);
    if (!has_required_goods_and_services(house_object, house, 0, bonus, demands)) {
        status = DEVOLVE;
    } else if (status == EVOLVE) {
        status = has_required_goods_and_services(house_object, house, 1, bonus, demands) ? EVOLVE : NONE;
    }
    return status;
}

static int has_devolve_delay(HousingState &state, evolve_status status)
{
    if (status == DEVOLVE && state.devolve_delay < active_devolve_delay) {
        ++state.devolve_delay;
        return 1;
    } else {
        state.devolve_delay = 0;
        return 0;
    }
}

static int evolve_xml_housing(Building &house_object, building *house, house_demands *demands, time_millis last_update)
{
    HousingState &state = house_object.Housing->state();
    const auto *merge_definition = house_object.Housing
        ? house_object.Housing->transition_target(building_type_registry_impl::HousingTransitionKind::MergeTo)
        : nullptr;
    int is_empty_vacant_lot =
        state.population <= 0 && house->type == building_type_registry_impl::vacant_lot_fill_type();
    if (merge_definition &&
        (state.population > 0 ||
            (is_empty_vacant_lot && config_get(CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS)))) {
        unsigned int merged_id = building_house_merge(house_object);
        if (merged_id) {
            Building *merged_house_object = Building::get(merged_id);
            if (merged_house_object) {
                building *merged_house = const_cast<::building *>(merged_house_object->record());
                if (merged_house_object->Housing &&
                    merged_house_object->Housing->consumes_goods_on_day(game_time_day())) {
                    consume_resources(*merged_house_object, merged_house);
                }
                merged_house->last_update = last_update;
            }
            return 0;
        }
    }

    if (state.population <= 0) {
        return 0;
    }

    evolve_status status = check_requirements(house_object, house, demands);
    if (has_devolve_delay(state, status)) {
        return 0;
    }

    const building_type_registry_impl::BuildingType *target_definition = nullptr;
    if (status == EVOLVE) {
        target_definition = house_object.Housing
            ? house_object.Housing->transition_target(building_type_registry_impl::HousingTransitionKind::EvolveTo)
            : nullptr;
    } else if (status == DEVOLVE) {
        target_definition = house_object.Housing
            ? house_object.Housing->transition_target(building_type_registry_impl::HousingTransitionKind::DevolveTo)
            : nullptr;
    }

    if (target_definition) {
        const building_type target = target_definition->type();
        const int current_cell_count = house_object.Foundation
            ? static_cast<int>(house_object.Foundation->cells(house_object.orientation()).size())
            : 0;
        const auto *target_foundation = target_definition ? target_definition->foundation_def() : nullptr;
        const int target_cell_count = target_foundation
            ? static_cast<int>(target_foundation->rotated_cells(house_object.orientation()).size())
            : 0;
        if (status == EVOLVE && target_cell_count > current_cell_count) {
            if (building_house_can_expand(house_object, target)) {
                game_undo_disable();
                building_house_expand_to_type(house_object, target);
                map_tiles_update_all_gardens();
                return 1;
            }
        } else if (status == DEVOLVE) {
            if (current_cell_count > 1) {
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
        city_trade_ledger_consumed(inventory, std::min<int>(amount, b->resources[inventory]));
        if (amount > b->resources[inventory]) {
            b->resources[inventory] = 0;
        } else {
            b->resources[inventory] = static_cast<short>(b->resources[inventory] - amount);
        }
    }
}

static void consume_resources(Building &house_object, building *b)
{
    int consumption_reduction[RESOURCE_SLOT_COUNT] = { 0 };
    const HousingServiceState &services = house_object.Housing->state().services;

    // mercury module 1 - pottery and furniture reduced by 20%
    if (building_monument_gt_module_is_active(MERCURY_MODULE_1_POTTERY_FURN)) {
        consumption_reduction[resource_pottery()] += 20;
        consumption_reduction[resource_furniture()] += 20;
    }
    // mercury module 2 - oil and wine reduced by 20%
    if (services.temple_mercury && building_monument_gt_module_is_active(MERCURY_MODULE_2_OIL_WINE)) {
        consumption_reduction[resource_wine()] += 20;
        consumption_reduction[resource_oil()] += 20;
    }
    // mars module 2 - all goods reduced by 10%
    if (services.temple_mars && building_monument_gt_module_is_active(MARS_MODULE_2_ALL_GOODS)) {
        consumption_reduction[resource_wine()] += 10;
        consumption_reduction[resource_oil()] += 10;
        consumption_reduction[resource_pottery()] += 10;
        consumption_reduction[resource_furniture()] += 10;
    }

    const auto *profile = house_object.Housing ? house_object.Housing->definition().profile : nullptr;
    if (!profile) {
        return;
    }
    const auto &requirements = profile->requirements;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (!resource_is_inventory_good(r)) {
            continue;
        }
        if (!consumption_reduction[r] ||
            (game_time_total_months() % (100 / consumption_reduction[r]))) {
            int amount = 0;
            if (r == resource_wine()) {
                amount = requirements.wine;
            } else if (r == resource_oil()) {
                amount = requirements.oil;
            } else if (r == resource_furniture()) {
                amount = requirements.furniture;
            } else if (r == resource_pottery()) {
                amount = requirements.pottery;
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

    if (grand_temple_for_god(GOD_VENUS, true)) {
        active_devolve_delay = DEVOLVE_DELAY_WITH_VENUS;
    } else {
        active_devolve_delay = DEVOLVE_DELAY;
    }

    time_millis last_update = time_get_millis();

    Building::for_each(BuildingRuntimeList::Housing, [&](Building *building_object) {
        building *b = const_cast<building *>(building_object->record());
        if (b->state != BUILDING_STATE_IN_USE || !building_object->Housing || b->last_update == last_update) {
            return;
        }
        Building &house_object = *building_object;
        house_object.Housing->state().no_space_to_expand = 0;
        if (!b->has_plague) {
            has_expanded |= evolve_xml_housing(house_object, b, demands, last_update);
        }
        if (b->state != BUILDING_STATE_IN_USE || !building_object->Housing) {
            return;
        }
        if (building_object->Housing->consumes_goods_on_day(game_time_day())) {
            consume_resources(house_object, b);
        }
        b->last_update = last_update;
    });
    if (has_expanded) {
        Route::updateLandTerrain();
    }
}

void building_house_determine_evolve_text(Building house_object, int worst_desirability_building)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    HousingState &state = house_object.Housing->state();
    const HousingServiceState &services = state.services;
    const auto *profile = house_object.Housing ? house_object.Housing->definition().profile : nullptr;
    int level = profile ? profile->compatibility_level : -1;
    if (building_monument_pantheon_module_is_active(PANTHEON_MODULE_2_HOUSING_EVOLUTION) && state.pantheon_access) {
        level--;
    }
    level = calc_bound(level, HOUSE_MIN, HOUSE_MAX);

    // this house will devolve soon because...

    if (!profile) {
        return;
    }
    const auto *requirements = &profile->requirements;
    // desirability
    if (house->desirability <= profile->evolution.devolve_desirability) {
        state.evolve_text_id = 0;
        return;
    }
    // water
    auto water = requirements->water;
    if (water == building_type_registry_impl::HousingWaterRequirement::Well && !house->has_water_access) {
        if (!house->has_well_access) {
            state.evolve_text_id = 1;
            return;
        } else if (!house->has_latrines_access) {
            state.evolve_text_id = 68;
            return;
        }
    }

    if (water == building_type_registry_impl::HousingWaterRequirement::Fountain && !house->has_water_access) {
        if (!house->has_latrines_access) {
            state.evolve_text_id = 67;
            return;
        } else if (level >= HOUSE_LARGE_CASA) {
            state.evolve_text_id = 2;
            return;
        }
    }

    // entertainment
    int entertainment = requirements->entertainment;
    if (services.entertainment < entertainment) {
        if (!services.entertainment) {
            state.evolve_text_id = 3;
        } else if (entertainment < 10) {
            state.evolve_text_id = 4;
        } else if (entertainment < 25) {
            state.evolve_text_id = 5;
        } else if (entertainment < 50) {
            state.evolve_text_id = 6;
        } else if (entertainment < 80) {
            state.evolve_text_id = 7;
        } else {
            state.evolve_text_id = 8;
        }
        return;
    }
    // food types
    int foodtypes_required = requirements->food_types;
    int foodtypes_available = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (house->resources[r] && resource_is_food(r)) {
            foodtypes_available++;
        }
    }
    if (foodtypes_available < foodtypes_required) {
        if (foodtypes_required == 1) {
            state.evolve_text_id = 9;
            return;
        } else if (foodtypes_required == 2) {
            state.evolve_text_id = 10;
            return;
        } else if (foodtypes_required == 3) {
            state.evolve_text_id = 11;
            return;
        }
    }
    // education
    int education = requirements->education;
    if (services.education < education) {
        if (education == 1) {
            state.evolve_text_id = 14;
            return;
        } else if (education == 2) {
            if (services.school) {
                state.evolve_text_id = 15;
                return;
            } else if (services.library) {
                state.evolve_text_id = 16;
                return;
            }
        } else if (education == 3) {
            state.evolve_text_id = 17;
            return;
        }
    }
    // bathhouse
    if (services.bathhouse < requirements->bathhouse) {
        state.evolve_text_id = 18;
        return;
    }
    // pottery
    if (house->resources[resource_pottery()] < requirements->pottery) {
        state.evolve_text_id = 19;
        return;
    }
    // religion
    int religion = requirements->religion;
    if (religion > 3) {
        religion = 3;
    }
    if (services.num_gods < religion) {
        if (religion == 1) {
            state.evolve_text_id = 20;
            return;
        } else if (religion == 2) {
            state.evolve_text_id = 21;
            return;
        } else if (religion == 3) {
            state.evolve_text_id = 22;
            return;
        }
    }
    // barber
    if (services.barber < requirements->barber) {
        state.evolve_text_id = 23;
        return;
    }
    // health
    int health = requirements->health;
    if (services.health < health) {
        if (health == 1) {
            state.evolve_text_id = 24;
        } else if (services.clinic) {
            state.evolve_text_id = 25;
        } else {
            state.evolve_text_id = 26;
        }
        return;
    }
    // oil
    if (house->resources[resource_oil()] < requirements->oil) {
        state.evolve_text_id = 27;
        return;
    }
    // furniture
    if (house->resources[resource_furniture()] < requirements->furniture) {
        state.evolve_text_id = 28;
        return;
    }
    // wine
    int wine = requirements->wine;
    if (house->resources[resource_wine()] < wine) {
        state.evolve_text_id = 29;
        return;
    }
    if (wine > 1 && !city_resource_multiple_wine_available()) {
        state.evolve_text_id = 65;
        return;
    }
    if (level >= HOUSE_LUXURY_PALACE) { // max level!
        state.evolve_text_id = 60;
        return;
    }

    // this house will evolve if ...

    // desirability
    if (house->desirability < profile->evolution.evolve_desirability) {
        if (worst_desirability_building) {
            state.evolve_text_id = 62;
        } else {
            state.evolve_text_id = 30;
        }
        return;
    }
    const auto *target_type = house_object.Housing->transition_target(
        building_type_registry_impl::HousingTransitionKind::EvolveTo);
    if (const auto *target_profile = target_type ? target_type->housing_def().profile : nullptr) {
        profile = target_profile;
        requirements = &profile->requirements;
    }
    // water
    water = requirements->water;
    if (water == building_type_registry_impl::HousingWaterRequirement::Well && !house->has_water_access) {
        if (!house->has_well_access) {
            state.evolve_text_id = 31;
            return;
        } else if (!house->has_latrines_access) {
            state.evolve_text_id = 68;
            return;
        }
    }

    if (water == building_type_registry_impl::HousingWaterRequirement::Fountain && !house->has_water_access) {
        if (level >= HOUSE_LARGE_CASA && house->has_well_access && house->has_latrines_access) {
            state.evolve_text_id = 32;
            return;
        }
    }


    // entertainment
    entertainment = requirements->entertainment;
    if (services.entertainment < entertainment) {
        if (!services.entertainment) {
            state.evolve_text_id = 33;
        } else if (entertainment < 10) {
            state.evolve_text_id = 34;
        } else if (entertainment < 25) {
            state.evolve_text_id = 35;
        } else if (entertainment < 50) {
            state.evolve_text_id = 36;
        } else if (entertainment < 80) {
            state.evolve_text_id = 37;
        } else {
            state.evolve_text_id = 38;
        }
        return;
    }
    // food types
    foodtypes_required = requirements->food_types;
    if (foodtypes_available < foodtypes_required) {
        if (foodtypes_required == 1) {
            state.evolve_text_id = 39;
            return;
        } else if (foodtypes_required == 2) {
            state.evolve_text_id = 40;
            return;
        } else if (foodtypes_required == 3) {
            state.evolve_text_id = 41;
            return;
        }
    }
    // education
    education = requirements->education;
    if (services.education < education) {
        if (education == 1) {
            state.evolve_text_id = 44;
            return;
        } else if (education == 2) {
            if (services.school) {
                state.evolve_text_id = 45;
                return;
            } else if (services.library) {
                state.evolve_text_id = 46;
                return;
            }
        } else if (education == 3) {
            state.evolve_text_id = 47;
            return;
        }
    }
    // bathhouse
    if (services.bathhouse < requirements->bathhouse) {
        state.evolve_text_id = 48;
        return;
    }
    // pottery
    if (house->resources[resource_pottery()] < requirements->pottery) {
        state.evolve_text_id = 49;
        return;
    }
    // religion
    religion = requirements->religion;
    if (religion > 3) {
        religion = 3;
    }
    if (services.num_gods < religion) {
        if (religion == 1) {
            state.evolve_text_id = 50;
            return;
        } else if (religion == 2) {
            state.evolve_text_id = 51;
            return;
        } else if (religion == 3) {
            state.evolve_text_id = 52;
            return;
        }
    }
    // barber
    if (services.barber < requirements->barber) {
        state.evolve_text_id = 53;
        return;
    }
    // health
    health = requirements->health;
    if (services.health < health) {
        if (health == 1) {
            state.evolve_text_id = 54;
        } else if (services.clinic) {
            state.evolve_text_id = 55;
        } else {
            state.evolve_text_id = 56;
        }
        return;
    }
    // oil
    if (house->resources[resource_oil()] < requirements->oil) {
        state.evolve_text_id = 57;
        return;
    }
    // furniture
    if (house->resources[resource_furniture()] < requirements->furniture) {
        state.evolve_text_id = 58;
        return;
    }
    // wine
    wine = requirements->wine;
    if (house->resources[resource_wine()] < wine) {
        state.evolve_text_id = 59;
        return;
    }
    if (wine > 1 && !city_resource_multiple_wine_available()) {
        state.evolve_text_id = 66;
        return;
    }
    // house is evolving
    state.evolve_text_id = 61;
    if (state.no_space_to_expand == 1) {
        // house would like to evolve but can't
        state.evolve_text_id = 64;
    }
}

static building_type get_building_type_at_tile(Building house_object, int x, int y)
{
    const ::building *house = house_object.record();
    if (!house) {
        return BUILDING_NONE;
    }
    int grid_offset = map_grid_offset(x, y);
    if (!map_building_exists_at(grid_offset)) {
        if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
            return building_type_registry_impl::type_from_attr("highway");
        } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
            return building_type_registry_impl::type_from_attr("reservoir");
        } else {
            return BUILDING_NONE;
        }
    }
    Building &other = map_building_at(grid_offset);
    const building *b = other.record();
    if (!b || b->state != BUILDING_STATE_IN_USE || other.id == house->id) {
        return BUILDING_NONE;
    }
    if (other.Housing) {
        const auto *house_profile =
            house_object.Housing ? house_object.Housing->definition().profile : nullptr;
        const auto *other_profile = other.Housing->definition().profile;
        int house_level = house_profile ? house_profile->compatibility_level : -1;
        int other_house_level = other_profile ? other_profile->compatibility_level : -1;
        if (house_level >= 0 && other_house_level >= house_level) {
            return BUILDING_NONE;
        }
    }
    return other.type ? other.type->type() : BUILDING_NONE;
}

building_type building_house_determine_worst_desirability_building_type(Building house_object)
{
    const ::building *house = house_object.record();
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
