#include "building/list.h"
#include "building/local_workforce.h"
#include "city/labor.h"
#include "city/migration.h"
#include "figure/figure.h"
#include "figuretype/migrant.h"

#include "building/building_record.h"
#include "house_population.h"

#include "building/building.h"
#include "building/house.h"

#include "building/monument.h"
#include "city/message.h"
#include "city/population.h"

#include <vector>

static int house_is_plebeian(Building &house)
{
    return building_house_has_plebeian_residents(house);
}

static int house_is_patrician(Building &house)
{
    return building_house_has_patrician_residents(house);
}

int house_population_add_to_city(int num_people)
{
    if (num_people <= 0) {
        return 0;
    }

    int added = 0;
    int building_id = city_population_last_used_house_add();
    std::vector<Building *> houses;
    Building::for_each({ .hasHousing = true }, [&houses](Building *building) {
        houses.push_back(building);
    });
    if (houses.empty()) {
        return 0;
    }

    size_t house_index = 0;
    for (size_t i = 0; i < houses.size(); i++) {
        if (houses[i]->id > static_cast<unsigned int>(building_id)) {
            house_index = i;
            break;
        }
    }

    for (size_t checked = 0; checked < houses.size() && added < num_people; checked++) {
        Building *house = houses[house_index];
        house_index = (house_index + 1) % houses.size();
        ::building *b = const_cast<::building *>(house->record());
        if (b->state == BUILDING_STATE_IN_USE && b->house_size
            && b->distance_from_entry > 0 && b->house_population > 0) {
            city_population_set_last_used_house_add(b->id);
            int capacity = house_population_get_capacity(*house);
            if (b->house_population < capacity) {
                ++added;
                ++b->house_population;
                b->house_population_room = static_cast<short>(capacity - b->house_population);
            }
        }
    }
    return added;
}

int house_population_remove_from_city(int num_people)
{
    if (num_people <= 0) {
        return 0;
    }

    int removed = 0;
    int building_id = city_population_last_used_house_remove();
    std::vector<Building *> houses;
    Building::for_each({ .hasHousing = true }, [&houses](Building *building) {
        houses.push_back(building);
    });
    if (houses.empty()) {
        return 0;
    }

    size_t house_index = 0;
    for (size_t i = 0; i < houses.size(); i++) {
        if (houses[i]->id > static_cast<unsigned int>(building_id)) {
            house_index = i;
            break;
        }
    }

    size_t houses_without_removal = 0;
    while (removed < num_people && houses_without_removal < houses.size()) {
        Building *house = houses[house_index];
        house_index = (house_index + 1) % houses.size();
        ::building *b = const_cast<::building *>(house->record());
        if (b->state == BUILDING_STATE_IN_USE && b->house_size && b->house_population > 0) {
            city_population_set_last_used_house_remove(b->id);
            ++removed;
            --b->house_population;
            building_local_workforce::reconcile_house(*house);
            houses_without_removal = 0;
        } else {
            houses_without_removal++;
        }
    }
    return removed;
}

int house_population_get_capacity(Building house_object)
{
    const ::building *house = house_object.record();
    if (!house || !house_object.type) {
        return 0;
    }
    // This is the single runtime capacity path for houses; XML load has already
    // rejected residential BuildingTypes without an authored whole-building capacity.
    // Empty vacant lots borrow the capacity of the validated first-occupancy
    // house so immigration can target them before they transform into housing.
    int capacity = house_object.type->housing_capacity();

    // Neptune module 2 bonus
    if (building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER) &&
        house->data.house.temple_neptune) {
        capacity += (capacity + 1) / 20;
    }
    return capacity;
}

void house_population_update_room(void)
{
    city_population_clear_capacity();

    Building::for_each({ .hasHousing = true }, [](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
            return;
        }
        b->house_population_room = 0;
        if (b->distance_from_entry > 0) {
            int capacity = house_population_get_capacity(*building);
            city_population_add_capacity(b->house_population, capacity);
            b->house_population_room = static_cast<short>(capacity - b->house_population);
            if (b->house_population > b->house_highest_population) {
                b->house_highest_population = b->house_population;
            }
        } else if (b->house_population) {
            // not connected to Rome, mark people for eviction
            b->house_population_room = -b->house_population;
        }
    });
}

int house_population_create_immigrants(int num_people)
{
    int to_immigrate = num_people;
    // clean up any dead immigrants
    Building::for_each({ .hasHousing = true }, [](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (b->house_size && b->immigrant_figure_id && Figure::get(b->immigrant_figure_id)->state != FIGURE_STATE_ALIVE) {
            b->immigrant_figure_id = 0;
        }
    });
    // houses with plenty of room
    Building::for_each({ .hasHousing = true }, [&to_immigrate](Building *building) {
        if (to_immigrate <= 0) {
            return;
        }
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->has_plague) {
            return;
        }
        if (b->distance_from_entry > 0 && b->house_population_room >= 8 && !b->immigrant_figure_id) {
            if (to_immigrate <= 4) {
                migrant_create_immigrant(*building, to_immigrate);
                to_immigrate = 0;
            } else {
                migrant_create_immigrant(*building, 4);
                to_immigrate -= 4;
            }
        }
    });
    // houses with less room
    Building::for_each({ .hasHousing = true }, [&to_immigrate](Building *building) {
        if (to_immigrate <= 0) {
            return;
        }
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->has_plague) {
            return;
        }
        if (b->distance_from_entry > 0 && b->house_population_room > 0 && !b->immigrant_figure_id) {
            if (to_immigrate <= b->house_population_room) {
                migrant_create_immigrant(*building, to_immigrate);
                to_immigrate = 0;
            } else {
                migrant_create_immigrant(*building, b->house_population_room);
                to_immigrate -= b->house_population_room;
            }
        }
    });
    return num_people - to_immigrate;
}

int house_population_create_emigrants(int num_people)
{
    int to_emigrate = num_people;
    Building::for_each({ .hasHousing = true }, [&to_emigrate](Building *building) {
        if (to_emigrate <= 0) {
            return;
        }
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->house_population <= 0 ||
            !house_is_plebeian(*building)) {
            return;
        }
        int current_people = b->house_population >= 4 ? 4 : b->house_population;
        if (to_emigrate <= current_people) {
            migrant_create_emigrant(*building, to_emigrate);
            to_emigrate = 0;
        } else {
            migrant_create_emigrant(*building, current_people);
            to_emigrate -= current_people;
        }
    });
    return num_people - to_emigrate;
}

static void calculate_working_population(void)
{
    int num_plebs = 0;
    int num_patricians = 0;
    Building::for_each({ .hasHousing = true }, [&num_plebs, &num_patricians](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
            return;
        }
        if (house_is_patrician(*building)) {
            num_patricians += b->house_population;
        } else if (house_is_plebeian(*building)) {
            num_plebs += b->house_population;
        }
    });
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
    Building::for_each({ .hasHousing = true }, [](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || !b->house_size || b->house_population_room >= 0) {
            return;
        }
        int num_people_to_evict = -b->house_population_room;
        migrant_create_homeless(*building, num_people_to_evict);
        if (num_people_to_evict < b->house_population) {
            b->house_population = static_cast<short>(b->house_population - num_people_to_evict);
            building_local_workforce::reconcile_house(*building);
        } else {
            // house has been removed
            b->house_population = 0;
            building_local_workforce::reconcile_house(*building);
            b->state = BUILDING_STATE_UNDO;
        }
    });
}
