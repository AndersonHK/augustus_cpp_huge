#include "health.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/count.h"
#include "building/destruction.h"
#include "building/granary.h"
#include "building/house.h"
#include "building/HousingProfileDef.h"
#include "building/housing_profile_registry.h"
#include "building/warehouse.h"
#include "city/culture.h"
#include "city/data_private.h"
#include "city/message.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/random.h"
#include "game/tutorial.h"
#include "scenario/property.h"

#define SICKNESS_SPREAD_DIVISION_FACTOR 4

static int active_count(const char *text_id)
{
    int active = 0;
    const building_type type = building_type_registry_impl::type_from_attr(text_id);
    for (const Building &building : Building::of_type(type)) {
        const ::building *b = building.record();
        if (building_is_active(b) &&
            (!building.Composition || !building.Composition->is_child()) &&
            !building.is_dynamic_bridge_segment()) {
            active++;
        }
    }
    return active;
}

int city_health(void)
{
    return city_data.health.value;
}

void city_health_change(int amount)
{
    city_data.health.value = calc_bound(city_data.health.value + amount, 0, 100);
}

void city_health_set(int new_value)
{
    city_data.health.value = calc_bound(new_value, 0, 100);
}

static int is_plague_building(const Building &building)
{
    const building_type_registry_impl::BuildingType *definition = building.type;
    return definition &&
        (building.matches("dock") ||
            definition->is_warehouse() ||
            definition->is_granary());
}

static void cause_disease_in_building(Building &building_object)
{
    building *b = const_cast<building *>(building_object.record());
    if (!b) {
        return;
    }
    if (!b->has_plague) {

        // Remove half the granary's food
        if (building_object.type && building_object.type->is_granary()) {
            for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
                if (!resource_is_food(r)) {
                    continue;
                }
                building_granary_try_remove_resource(building_object, r, building_granary_get_amount(building_object, r) / 2);
            }
        } else if (building_object.type && building_object.type->is_warehouse()) {
            // Remove all food from warehouse
            for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
                if (!resource_is_food(r)) {
                    continue;
                }
                building_warehouse_try_remove_resource(building_object, r, FULL_WAREHOUSE);
            }
            // Remove half of oil and wine from warehouse
            const resource_type goods[] = { resource_wine(), resource_oil() };
            for (size_t i = 0; i < sizeof(goods) / sizeof(goods[0]); ++i) {
                resource_type r = goods[i];
                building_warehouse_try_remove_resource(building_object, r, building_warehouse_get_amount(building_object, r) / 2);
            }
        }

        // Set building to plague status and use fire process to manage plague on it
        b->has_plague = 1;
        b->sickness_duration = 0;

        if (is_plague_building(building_object)) {
            city_message_post(1, MESSAGE_SICKNESS, b->type, b->grid_offset);
        }
    }
}

void city_health_update_sickness_level_in_building(Building *runtime_building)
{
    building *b = runtime_building ? const_cast<::building *>(runtime_building->record()) : nullptr;

    if (b && !b->has_plague && b->state == BUILDING_STATE_IN_USE) {
        b->sickness_level += 1;

        if (b->sickness_level > 2 * MAX_SICKNESS_LEVEL) {
            b->sickness_level = 2 * MAX_SICKNESS_LEVEL;
        }
    }
}

void city_health_dispatch_sickness(Figure *f)
{
    Building *source = f ? f->building : nullptr;
    Building *destination = f ? f->destination_building : nullptr;
    building *b = source ? const_cast<building *>(source->record()) : nullptr;
    building *dest_b = destination ? const_cast<building *>(destination->record()) : nullptr;
    if (!b || !dest_b) {
        return;
    }

    // Dispatch sickness level sub value between granaries, warehouses and docks
    if (is_plague_building(*destination) && b->sickness_level && b->sickness_level > dest_b->sickness_level) {
        unsigned char value = b->sickness_level <= SICKNESS_SPREAD_DIVISION_FACTOR ? 1 :
            b->sickness_level / SICKNESS_SPREAD_DIVISION_FACTOR;
        dest_b->sickness_level = static_cast<unsigned char>(dest_b->sickness_level + value);
        if (dest_b->sickness_level > b->sickness_level) {
            dest_b->sickness_level = b->sickness_level;
        }
    } else if (is_plague_building(*source) && dest_b->sickness_level && dest_b->sickness_level > b->sickness_level) {
        unsigned char value = b->sickness_level <= SICKNESS_SPREAD_DIVISION_FACTOR ? 1 :
            b->sickness_level / SICKNESS_SPREAD_DIVISION_FACTOR;
        b->sickness_level = static_cast<unsigned char>(b->sickness_level + value);
        if (b->sickness_level > dest_b->sickness_level) {
            b->sickness_level = dest_b->sickness_level;
        }
    }
}

static int cause_disease(void)
{
    int sick_people = 0;
    building_type sick_building_type = BUILDING_NONE;
    int grid_offset = 0;
    // Kill people who have sickness level to max in houses
    Building::for_each(BuildingRuntimeList::Housing, [&](Building *house) {
        HousingModule &housing = *house->Housing;
        HousingState &state = housing.state();
        building *record = const_cast<building *>(house->record());
        const int population = state.population;
        if (housing.is_occupied()) {
            if (record->sickness_level >= MAX_SICKNESS_LEVEL) {
                sick_people = 1;
                sick_building_type = record->type;
                grid_offset = house->grid_offset();
                if (city_health() < 40) {
                    house->destroy_by_plague();
                } else {
                    int killed_people = population -
                        calc_adjust_with_percentage(population, city_health());
                    if (killed_people == 0) {
                        killed_people = 1;
                    }
                    if (killed_people < population) {
                        state.population = static_cast<int16_t>(population - killed_people);
                    } else {
                        building_house_change_to_vacant_lot(*house);
                    }
                    city_population_remove_home_removed(killed_people);

                    // Cause plague in the house
                    housing.clear_immigrant_reference();
                    record->has_plague = 1;
                    record->sickness_duration = 0;
                }
            }
        }
    });

    if (sick_people) {
        city_message_post_with_popup_delay(MESSAGE_CAT_ILLNESS, MESSAGE_SICKNESS, sick_building_type, static_cast<short>(grid_offset));
    }

    Building::for_each([&](Building *runtime_building) {
        building *record = const_cast<building *>(runtime_building->record());
        if (is_plague_building(*runtime_building) && record->sickness_level >= MAX_SICKNESS_LEVEL) {
            cause_disease_in_building(*runtime_building);
        }
    });

    return sick_people;
}

static int count_hospital_workers(void)
{
    int total_workers = 0;
    const building_type hospital = building_type_registry_impl::type_from_attr("hospital");
    for (const Building &building : Building::of_type(hospital)) {
        const ::building *b = building.record();
        if (b->state == BUILDING_STATE_IN_USE) {
            total_workers += b->num_workers;
        }
    }
    return total_workers;
}

static void cause_plague(int total_people)
{
    if (cause_disease()) {
        return;
    }

    if (city_data.health.value >= 40) {
        return;
    }
    int chance_value = random_byte() & 0x3f;
    if (city_data.religion.venus_curse_active) {
        // force plague
        chance_value = 0;
        city_data.religion.venus_curse_active = 0;
    }
    if (chance_value > 40 - city_data.health.value) {
        return;
    }

    int sick_people = calc_adjust_with_percentage(total_people, 7 + (random_byte() & 3));
    if (sick_people <= 0) {
        return;
    }
    city_health_change(10);
    int num_hospital_workers = count_hospital_workers();
    int people_to_kill = sick_people - num_hospital_workers;
    if (people_to_kill <= 0) {
        city_message_post(1, MESSAGE_HEALTH_ILLNESS, 0, 0);
        return;
    }
    if (num_hospital_workers > 0) {
        city_message_post(1, MESSAGE_HEALTH_DISEASE, 0, 0);
    } else {
        city_message_post(1, MESSAGE_HEALTH_PESTILENCE, 0, 0);
    }
    tutorial_on_disease();
    // kill people who don't have access to a doctor
    int housing_level_count = building_type_registry_impl::housing_profile_compatibility_level_count();
    for (int level_index = 0; level_index < housing_level_count; level_index++) {
        int level = building_type_registry_impl::housing_profile_compatibility_level_at(level_index);
        if (level < 0) {
            continue;
        }
        Building::for_each(BuildingRuntimeList::Housing, [&](Building *house) {
            if (people_to_kill <= 0) {
                return;
            }
            const HousingModule &housing = *house->Housing;
            if (housing.is_occupied_at_compatibility_level(level)) {
                const HousingState &state = housing.state();
                if (!state.services.clinic) {
                    people_to_kill -= state.population;
                    house->destroy_by_plague();
                }
            }
        });
        if (people_to_kill <= 0) {
            return;
        }
    }
    // kill anyone, starting with tents and working up the housing levels
    for (int level_index = 0; level_index < housing_level_count; level_index++) {
        int level = building_type_registry_impl::housing_profile_compatibility_level_at(level_index);
        if (level < 0) {
            continue;
        }
        Building::for_each(BuildingRuntimeList::Housing, [&](Building *house) {
            if (people_to_kill <= 0) {
                return;
            }
            const HousingModule &housing = *house->Housing;
            if (housing.is_occupied_at_compatibility_level(level)) {
                people_to_kill -= housing.state().population;
                house->destroy_by_plague();
            }
        });
        if (people_to_kill <= 0) {
            return;
        }
    }
}

static void adjust_sickness_level_in_plague_buildings(int hospital_coverage_bonus)
{
    Building::for_each([hospital_coverage_bonus](Building *runtime_building) {
        building *record = const_cast<building *>(runtime_building->record());
        if (!is_plague_building(*runtime_building) || record->has_plague || !record->sickness_level) {
            return;
        }
        int decrease_percentage = city_health();
        decrease_percentage += calc_adjust_with_percentage(decrease_percentage, hospital_coverage_bonus);
        if (decrease_percentage > 100) {
            decrease_percentage = 100;
        }
        record->sickness_level = static_cast<unsigned char>(
            record->sickness_level - calc_adjust_with_percentage(record->sickness_level, decrease_percentage));
    });
}

// House Health Calculation
int city_health_get_house_health_level(Building house, int update_city_data)
{
    const building *b = house.record();
    if (!b || !house.Housing || !*house.Housing) {
        return 0;
    }
    // Define house health to be 0 as a starting point
    int house_health = 0;

    const HousingModule &housing = *house.Housing;
    const HousingState &state = housing.state();
    const HousingServiceState &services = state.services;
    const building_type_registry_impl::HousingProfileDef &profile = *housing.definition().profile;
    // House Level: What is the level of the house?
    house_health = calc_bound(profile.compatibility_level, 0, 10);

    // Healthcare: Do they have access to a Clinic and/or Hospital?
    if (services.clinic && services.hospital) {
        house_health += 50; // Hospital + Clinic is best
    } else if (services.hospital) {
        house_health += 40; // Hospital alone is still good
    } else if (services.clinic) {
        house_health += 30; // Clinics are better than nothing
    }

    // Bathhouse: Do they have access to a bathhouse?
    if (services.bathhouse) {
        house_health += 15;
    }

    // Barber: Do they have access to a barber?
    if (services.barber) {
        house_health += 10;
    }

    // Hygiene: Do they have access to clean water or latrines?
    if (b->has_water_access || b->has_latrines_access) {
        house_health += 10;
    }

    // Mausoleum: Are they in range of a Mausoleum so they may bury their dead?
    int mausoleum_health = active_count("small_mausoleum") * 2;
    mausoleum_health += active_count("large_mausoleum") * 5;
    // Sums the combined health from all Mausoleums and clamps it between 0 and 10
    house_health += calc_bound(mausoleum_health, 0, 10);

    // Diet: How many foods do they have access to?
    house_health += services.num_foods * 10;
    // Cap health to 40 if their house level requires food but they don't have any
    int health_cap = (profile.requirements.food_types && !services.num_foods) ? 40 : 100;
    house_health = calc_bound(house_health, 0, health_cap);

    // Update city_data
    if (update_city_data) {
        if (services.clinic) {
            city_data.health.population_access.clinic += state.population;
        }
        if (services.barber) {
            city_data.health.population_access.barber += state.population;
        }
        if (services.bathhouse) {
            city_data.health.population_access.baths += state.population;
        }
        if (b->has_well_access) {
            city_data.health.population_access.wells += state.population;
        }
        if (b->has_latrines_access) {
            city_data.health.population_access.latrines += state.population;
        }
        if (b->has_water_access) {
            city_data.health.population_access.fountains += state.population;
        }
    }
    return house_health;
}

void city_health_update(void)
{
    int only_gather_stats = 0;
    if (city_data.population.population < 200 || scenario_is_tutorial_1() || scenario_is_tutorial_2()) {
        city_data.health.value = 50;
        city_data.health.target_value = 50;
        only_gather_stats = 1;
    }
    int total_population = 0;
    int healthy_population = 0;
    int hospital_coverage_bonus = city_culture_coverage_hospital() / 20 * 5;
    city_data.health.population_access.clinic = 0;
    city_data.health.population_access.barber = 0;
    city_data.health.population_access.baths = 0;
    city_data.health.population_access.wells = 0;
    city_data.health.population_access.latrines = 0;
    city_data.health.population_access.fountains = 0;

    Building::for_each(BuildingRuntimeList::Housing, [&](Building *house) {
        HousingModule &housing = *house->Housing;
        HousingState &state = housing.state();
        building *record = const_cast<building *>(house->record());
        if (!house->is_in_use()) {
            return;
        }
        const int population = state.population;
        if (!population) {
            record->sickness_level = 0;
            return;
        }
        int house_health = city_health_get_house_health_level(*house, 1);

        total_population += population;
        if (!only_gather_stats) {
            healthy_population += calc_adjust_with_percentage(population, house_health);
        }
    });
    if (only_gather_stats) {
        return;
    }
    city_data.health.target_value = calc_percentage(healthy_population, total_population);
    if (city_data.health.value < city_data.health.target_value) {
        city_data.health.value += 2;
        if (city_data.health.value > city_data.health.target_value) {
            city_data.health.value = city_data.health.target_value;
        }
    } else if (city_data.health.value > city_data.health.target_value) {
        city_data.health.value -= 2;
        if (city_data.health.value < city_data.health.target_value) {
            city_data.health.value = city_data.health.target_value;
        }
    }
    city_data.health.value = calc_bound(city_data.health.value, 0, 100);

    adjust_sickness_level_in_plague_buildings(hospital_coverage_bonus);

    cause_plague(total_population);
}

int city_health_get_global_sickness_level(void)
{
    int building_number = 0;
    int max_sickness_level = 0;

    Building::for_each(BuildingRuntimeList::Housing, [&](Building *house) {
        const HousingModule &housing = *house->Housing;
        building *record = const_cast<building *>(house->record());
        if (housing.is_occupied()) {
            building_number++;
            if (record->sickness_level > max_sickness_level) {
                max_sickness_level = record->sickness_level;
            }
        }
    });

    Building::for_each([&](Building *runtime_building) {
        building *record = const_cast<building *>(runtime_building->record());
        if (!is_plague_building(*runtime_building)) {
            return;
        }
        building_number++;
        if (record->sickness_level > max_sickness_level) {
            max_sickness_level = record->sickness_level;
        }
    });

    if (max_sickness_level < MAX_SICKNESS_LEVEL) {
        const building_type burning_ruin = building_type_registry_impl::type_from_attr("burning_ruin");
        for (const Building &building : Building::of_type(burning_ruin)) {
            const ::building *b = building.record();
            if (b->state == BUILDING_STATE_IN_USE && b->has_plague) {
                max_sickness_level = MAX_SICKNESS_LEVEL;
                break;
            }
        }
    }

    if (building_number == 0) {
        return SICKNESS_LEVEL_LOW;
    }

    int global_sickness_level = SICKNESS_LEVEL_LOW;

    if (max_sickness_level == MAX_SICKNESS_LEVEL) { // one or many houses is plagued
        global_sickness_level = SICKNESS_LEVEL_PLAGUE;
    } else if (max_sickness_level >= HIGH_SICKNESS_LEVEL) { // one or many houses have sickness_level >= 90
        global_sickness_level = SICKNESS_LEVEL_HIGH;
    } else if (max_sickness_level >= MEDIUM_SICKNESS_LEVEL) { // one or many houses have sickness_level >= 60
        global_sickness_level = SICKNESS_LEVEL_MEDIUM;
    }

    return global_sickness_level;
}

int city_health_get_population_with_clinic_access(void)
{
    return city_data.health.population_access.clinic;
}

int city_health_get_population_with_barber_access(void)
{
    return city_data.health.population_access.barber;
}

int city_health_get_population_with_baths_access(void)
{
    return city_data.health.population_access.baths;
}

int city_health_get_population_with_well_access(void)
{
    return city_data.health.population_access.wells;
}

int city_health_get_population_with_latrines_access(void)
{
    return city_data.health.population_access.latrines;
}

int city_health_get_population_with_water_access(void)
{
    return city_data.health.population_access.fountains;
}
