#include "house_population.h"

extern "C" {
#include "building/list.h"
#include "building/building_type_api.h"
#include "building/local_workforce.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/labor.h"
#include "city/message.h"
#include "city/migration.h"
#include "city/population.h"
#include "core/calc.h"
#include "figuretype/migrant.h"
}

static int house_is_plebeian(const building *b)
{
    int resident_class = building_type_registry_get_housing_resident_class(b->type);
    return resident_class == BUILDING_TYPE_HOUSING_RESIDENT_PLEBEIAN ||
        (!resident_class && b->type >= BUILDING_HOUSE_SMALL_TENT && b->type <= BUILDING_HOUSE_GRAND_INSULA);
}

int house_population_add_to_city(int num_people)
{
    int added = 0;
    int building_id = city_population_last_used_house_add();
    for (int i = 1; i < building_count() && added < num_people; i++) {
        if (++building_id >= building_count()) {
            building_id = 1;
        }
        building *b = building_get(building_id);
        if (b->state == BUILDING_STATE_IN_USE && b->house_size
            && b->distance_from_entry > 0 && b->house_population > 0) {
            city_population_set_last_used_house_add(building_id);
            int max_people = house_population_get_capacity(b);
            if (b->house_population < max_people) {
                ++added;
                ++b->house_population;
                b->house_population_room = max_people - b->house_population;
            }
        }
    }
    return added;
}

int house_population_remove_from_city(int num_people)
{
    int removed = 0;
    int building_id = city_population_last_used_house_remove();
    int buildings_without_removal = 0;
    const int max_buildings = building_count();
    if (max_buildings <= 1) {
        return 0;
    }
    while (removed < num_people && buildings_without_removal < max_buildings) {
        if (++building_id >= max_buildings) {
            building_id = 1;
        }
        building *b = building_get(building_id);
        if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population > 0) {
            city_population_set_last_used_house_remove(building_id);
            ++removed;
            --b->house_population;
            building_local_workforce_reconcile_house(b);
            buildings_without_removal = 0;
        } else {
            buildings_without_removal++;
        }
    }
    return removed;
}

int house_population_get_capacity(building *house)
{
    const model_house *housing_model = building_type_registry_get_housing_model(house->type);
    int max_pop = housing_model ? housing_model->max_people :
        model_get_house(static_cast<house_level>(house->subtype.house_level))->max_people;

    if (house->house_is_merged && !building_type_registry_has_housing(house->type)) {
        max_pop *= 4;
    } else if (building_type_registry_has_housing(house->type) && house->house_size > 1) {
        max_pop *= house->house_size * house->house_size;
    }

    // Neptune module 2 bonus
    if (building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER) &&
        house->data.house.temple_neptune) {
        max_pop += (max_pop + 1) / 20;
    }
    return max_pop;
}

void house_population_update_room(void)
{
    city_population_clear_capacity();

    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
            continue;
        }
        b->house_population_room = 0;
        if (b->distance_from_entry > 0) {
            int max_pop = house_population_get_capacity(b);
            city_population_add_capacity(b->house_population, max_pop);
            b->house_population_room = max_pop - b->house_population;
            if (b->house_population > b->house_highest_population) {
                b->house_highest_population = b->house_population;
            }
        } else if (b->house_population) {
            // not connected to Rome, mark people for eviction
            b->house_population_room = -b->house_population;
        }
    }
}

int house_population_create_immigrants(int num_people)
{
    int to_immigrate = num_people;
    // clean up any dead immigrants
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->house_size && b->immigrant_figure_id && figure_get(b->immigrant_figure_id)->state != FIGURE_STATE_ALIVE) {
            b->immigrant_figure_id = 0;
        }
    }
    // houses with plenty of room
    for (int i = 1; i < building_count() && to_immigrate > 0; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->has_plague) {
            continue;
        }
        if (b->distance_from_entry > 0 && b->house_population_room >= 8 && !b->immigrant_figure_id) {
            if (to_immigrate <= 4) {
                figure_create_immigrant(b, to_immigrate);
                to_immigrate = 0;
            } else {
                figure_create_immigrant(b, 4);
                to_immigrate -= 4;
            }
        }
    }
    // houses with less room
    for (int i = 1; i < building_count() && to_immigrate > 0; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->has_plague) {
            continue;
        }
        if (b->distance_from_entry > 0 && b->house_population_room > 0 && !b->immigrant_figure_id) {
            if (to_immigrate <= b->house_population_room) {
                figure_create_immigrant(b, to_immigrate);
                to_immigrate = 0;
            } else {
                figure_create_immigrant(b, b->house_population_room);
                to_immigrate -= b->house_population_room;
            }
        }
    }
    return num_people - to_immigrate;
}

int house_population_create_emigrants(int num_people)
{
    int to_emigrate = num_people;
    for (int i = 1; i < building_count() && to_emigrate > 0; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->house_population <= 0 || !house_is_plebeian(b)) {
            continue;
        }
        int current_people = b->house_population >= 4 ? 4 : b->house_population;
        if (to_emigrate <= current_people) {
            figure_create_emigrant(b, to_emigrate);
            to_emigrate = 0;
        } else {
            figure_create_emigrant(b, current_people);
            to_emigrate -= current_people;
        }
    }
    return num_people - to_emigrate;
}

static void calculate_working_population(void)
{
    int num_plebs = 0;
    int num_patricians = 0;
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
            continue;
        }
        int resident_class = building_type_registry_get_housing_resident_class(b->type);
        if (resident_class == BUILDING_TYPE_HOUSING_RESIDENT_PATRICIAN ||
            (!resident_class && b->type >= BUILDING_HOUSE_SMALL_VILLA && b->type <= BUILDING_HOUSE_LUXURY_PALACE)) {
            num_patricians += b->house_population;
        } else {
            num_plebs += b->house_population;
        }
    }
    city_labor_calculate_workers(num_plebs, num_patricians);
}

void house_population_update_migration(void)
{
    city_migration_update();

    city_population_yearly_update();
    calculate_working_population();
    // population messages
    int population = city_population();
    if (population >= 500 && city_message_mark_population_shown(500)) {
        city_message_post(1, MESSAGE_POPULATION_500, 0, 0);
    }
    if (population >= 1000 && city_message_mark_population_shown(1000)) {
        city_message_post(1, MESSAGE_POPULATION_1000, 0, 0);
    }
    if (population >= 2000 && city_message_mark_population_shown(2000)) {
        city_message_post(1, MESSAGE_POPULATION_2000, 0, 0);
    }
    if (population >= 3000 && city_message_mark_population_shown(3000)) {
        city_message_post(1, MESSAGE_POPULATION_3000, 0, 0);
    }
    if (population >= 5000 && city_message_mark_population_shown(5000)) {
        city_message_post(1, MESSAGE_POPULATION_5000, 0, 0);
    }
    if (population >= 10000 && city_message_mark_population_shown(10000)) {
        city_message_post(1, MESSAGE_POPULATION_10000, 0, 0);
    }
    if (population >= 15000 && city_message_mark_population_shown(15000)) {
        city_message_post(1, MESSAGE_POPULATION_15000, 0, 0);
    }
    if (population >= 20000 && city_message_mark_population_shown(20000)) {
        city_message_post(1, MESSAGE_POPULATION_20000, 0, 0);
    }
    if (population >= 25000 && city_message_mark_population_shown(25000)) {
        city_message_post(1, MESSAGE_POPULATION_25000, 0, 0);
    }
}

void house_population_evict_overcrowded(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->house_population_room >= 0) {
            continue;
        }
        int num_people_to_evict = -b->house_population_room;
        figure_create_homeless(b, num_people_to_evict);
        if (num_people_to_evict < b->house_population) {
            b->house_population -= num_people_to_evict;
            building_local_workforce_reconcile_house(b);
        } else {
            // house has been removed
            b->house_population = 0;
            building_local_workforce_reconcile_house(b);
            b->state = BUILDING_STATE_UNDO;
        }
    }
}
