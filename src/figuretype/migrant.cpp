#include "game/undo.h"
#include "map/road_access.h"
#include "migrant.h"
#include "building/building.h"
#include "building/house.h"
#include "building/house_population.h"
#include "building/building_type_api.h"
#include "city/map.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/time.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"

static struct {
    int available;
    time_millis last_check;
} houses_with_room;

static int house_is_valid(const Building &house, unsigned int figure_id)
{
    return house.id() && house.is_in_use() && house.has_house_size() && !house.has_plague() &&
        house.immigrant_figure_id() == figure_id;
}

static void set_migrant_house(Figure &migrant, Building &house)
{
    house.set_immigrant_figure_id(migrant.id());
    migrant.immigrant_building = house;
    migrant.destination_building = house;
}

static void send_homeless_to_house(Figure &homeless, Building &house, int road_x, int road_y)
{
    set_migrant_house(homeless, house);
    homeless.action_state = FIGURE_ACTION_8_HOMELESS_GOING_TO_HOUSE;
    homeless.destination_x = road_x;
    homeless.destination_y = road_y;
    homeless.roam_length = 0;
}

static void add_migrant_people_to_house(Figure &migrant, Building &house, int homeless)
{
    const int capacity = house_population_get_capacity(house);
    const int old_population = house.house_population();
    int room = capacity - old_population;
    if (room < 0) {
        room = 0;
    }
    if (room < migrant.migrant_num_people) {
        migrant.migrant_num_people = room;
    }
    house.set_house_population(old_population + migrant.migrant_num_people);
    house.set_house_population_room(capacity - house.house_population());
    house.set_immigrant_figure_id(0);
    if (homeless) {
        city_population_add_homeless(migrant.migrant_num_people);
        game_undo_disable();
    } else {
        city_population_add(migrant.migrant_num_people);
    }
    if (!old_population) {
        building_house_change_to(house, building_type_registry_get_vacant_lot_occupancy_type());
    }
}

void migrant_create_immigrant(Building &house, int num_people)
{
    const map_tile *entry = city_map_entry_point();
    Figure *f = Figure::create(FIGURE_IMMIGRANT, entry->x, entry->y, DIR_0_TOP);
    f->action_state = FIGURE_ACTION_1_IMMIGRANT_CREATED;
    set_migrant_house(*f, house);
    f->wait_ticks = game_time_scale_legacy_day_ticks(10 + (house.house_figure_generation_delay() & 0x7f));
    f->migrant_num_people = num_people;
}

void migrant_create_emigrant(Building &house, int num_people)
{
    city_population_remove(num_people);
    if (num_people < house.house_population()) {
        house.set_house_population(house.house_population() - num_people);
    } else {
        building_house_change_to_vacant_lot(house);
    }
    Figure *f = Figure::create(FIGURE_EMIGRANT, house.x(), house.y(), DIR_0_TOP);
    f->action_state = FIGURE_ACTION_4_EMIGRANT_CREATED;
    f->wait_ticks = 0;
    f->migrant_num_people = num_people;
}

Figure *migrant_create_homeless(Building &house, int num_people)
{
    Figure *f = Figure::create(FIGURE_HOMELESS, house.x(), house.y(), DIR_0_TOP);
    f->building = house;
    f->action_state = FIGURE_ACTION_7_HOMELESS_CREATED;
    f->wait_ticks = 0;
    f->migrant_num_people = num_people;
    city_population_remove_homeless(num_people);
    return f;
}

static void update_direction_and_image(Figure *f)
{
    figure_image_update(f, image_group(GROUP_FIGURE_MIGRANT));
    if (f->action_state == FIGURE_ACTION_2_IMMIGRANT_ARRIVING ||
        f->action_state == FIGURE_ACTION_6_EMIGRANT_LEAVING) {
        int dir = figure_image_direction(f);
        f->cart_image_id = image_group(GROUP_FIGURE_MIGRANT_CART) + dir;
        figure_image_set_cart_offset(f, (dir + 4) % 8);
    }
}

static Building closest_house_with_room(int x, int y)
{
    if (houses_with_room.last_check == time_get_millis() && !houses_with_room.available) {
        return Building(nullptr);
    }

    int available_houses = 0;
    int min_dist = 1000;
    Building closest(nullptr);
    for (building_type type = static_cast<building_type>(BUILDING_NONE + 1); type < BUILDING_TYPE_MAX;
        type = static_cast<building_type>(type + 1)) {
        for (Building house = Building::first_of_type(type); house.id(); house = house.next_of_type()) {
            if (!house.type || !house.type->has_housing() || !house.is_in_use() || !house.has_house_size() || house.has_plague() ||
                house.distance_from_entry() <= 0 || house.house_population_room() <= 0 || house.immigrant_figure_id()) {
                continue;
            }
            const int dist = calc_maximum_distance(x, y, house.x(), house.y());
            available_houses++;
            if (dist < min_dist) {
                min_dist = dist;
                closest = house;
            }
        }
    }

    houses_with_room.last_check = time_get_millis();
    houses_with_room.available = available_houses - 1;
    return closest;
}

void figure_immigrant_action(Figure *f)
{
    Building house = f->immigrant_building;

    f->terrain_usage = TERRAIN_USAGE_ANY;
    f->cart_image_id = 0;
    if (!house_is_valid(house, f->id())) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }

    figure_image_increase_offset(f, 12);

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK: figure_combat_handle_attack(f); break;
        case FIGURE_ACTION_149_CORPSE: figure_combat_handle_corpse(f); break;
        case FIGURE_ACTION_1_IMMIGRANT_CREATED:
            f->is_ghost = 1;
            f->image_offset = 0;
            f->wait_ticks--;
            if (f->wait_ticks <= 0) {
                if (house.has_cached_road_access()) {
                    f->action_state = FIGURE_ACTION_2_IMMIGRANT_ARRIVING;
                    f->destination_x = house.road_access_x();
                    f->destination_y = house.road_access_y();
                    f->roam_length = 0;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
        case FIGURE_ACTION_2_IMMIGRANT_ARRIVING:
            f->is_ghost = 0;
            figure_movement_move_ticks(f, 1);
            switch (f->direction) {
                case DIR_FIGURE_AT_DESTINATION:
                    f->action_state = FIGURE_ACTION_3_IMMIGRANT_ENTERING_HOUSE;
                    figure_movement_set_cross_country_destination(f, house.x(), house.y());
                    f->roam_length = 0;
                    break;
                case DIR_FIGURE_REROUTE: figure_route_remove(f); break;
                case DIR_FIGURE_LOST:
                    house.set_immigrant_figure_id(0);
                    house.set_distance_from_entry(0);
                    f->state = FIGURE_STATE_DEAD;
                    break;
            }
            break;
        case FIGURE_ACTION_3_IMMIGRANT_ENTERING_HOUSE:
            f->use_cross_country = 1;
            f->is_ghost = 1;
            if (figure_movement_move_ticks_cross_country(f, 1) == 1) {
                f->state = FIGURE_STATE_DEAD;
                add_migrant_people_to_house(*f, house, 0);
            }
            f->is_ghost = f->in_building_wait_ticks ? 1 : 0;
            break;
    }

    update_direction_and_image(f);
}

void figure_emigrant_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ANY;
    f->cart_image_id = 0;

    figure_image_increase_offset(f, 12);

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK: figure_combat_handle_attack(f); break;
        case FIGURE_ACTION_149_CORPSE: figure_combat_handle_corpse(f); break;
        case FIGURE_ACTION_4_EMIGRANT_CREATED:
        {
            f->is_ghost = 1;
            f->image_offset = 0;
            f->wait_ticks++;
            if (f->wait_ticks >= game_time_scale_legacy_day_ticks(5)) {
                int x_road, y_road;
                if (!map_closest_road_within_radius(f->x, f->y, 1, 5, &x_road, &y_road)) {
                    f->state = FIGURE_STATE_DEAD;
                }
                f->action_state = FIGURE_ACTION_5_EMIGRANT_EXITING_HOUSE;
                figure_movement_set_cross_country_destination(f, x_road, y_road);
                f->roam_length = 0;
            }
            break;
        }
        case FIGURE_ACTION_5_EMIGRANT_EXITING_HOUSE:
            f->use_cross_country = 1;
            f->is_ghost = 1;
            if (figure_movement_move_ticks_cross_country(f, 1) == 1) {
                const map_tile *entry = city_map_entry_point();
                f->action_state = FIGURE_ACTION_6_EMIGRANT_LEAVING;
                f->destination_x = entry->x;
                f->destination_y = entry->y;
                f->roam_length = 0;
                f->progress_on_tile = 15;
            }
            f->is_ghost = f->in_building_wait_ticks ? 1 : 0;
            break;
        case FIGURE_ACTION_6_EMIGRANT_LEAVING:
            f->use_cross_country = 0;
            f->is_ghost = 0;
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                f->direction == DIR_FIGURE_REROUTE ||
                f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }
    update_direction_and_image(f);
}

void figure_homeless_action(Figure *f)
{
    figure_image_increase_offset(f, 12);
    f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK: figure_combat_handle_attack(f); break;
        case FIGURE_ACTION_149_CORPSE: figure_combat_handle_corpse(f); break;
        case FIGURE_ACTION_7_HOMELESS_CREATED:
        {
            f->image_offset = 0;
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(51)) {
                Building house = closest_house_with_room(f->x, f->y);
                if (house.id()) {
                    int x_road, y_road;
                    if (map_closest_road_within_radius(house.x(), house.y(), house.size(), 2, &x_road, &y_road)) {
                        send_homeless_to_house(*f, house, x_road, y_road);
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                } else {
                    const map_tile *exit = city_map_exit_point();
                    f->action_state = FIGURE_ACTION_10_HOMELESS_LEAVING;
                    f->destination_x = exit->x;
                    f->destination_y = exit->y;
                    f->roam_length = 0;
                    f->wait_ticks = 0;
                }
            }
            break;
        }
        case FIGURE_ACTION_8_HOMELESS_GOING_TO_HOUSE:
        {
            f->is_ghost = 0;
            figure_movement_move_ticks(f, 1);
            Building house = f->immigrant_building;
            if (!house_is_valid(house, f->id())) {
                figure_route_remove(f);
                f->action_state = FIGURE_ACTION_7_HOMELESS_CREATED;
                f->wait_ticks = game_time_scale_legacy_day_ticks(30);
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                house.set_immigrant_figure_id(0);
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_9_HOMELESS_ENTERING_HOUSE;
                figure_movement_set_cross_country_destination(f, house.x(), house.y());
                f->roam_length = 0;
            }
            break;
        }
        case FIGURE_ACTION_9_HOMELESS_ENTERING_HOUSE:
        {
            f->use_cross_country = 1;
            f->is_ghost = 1;
            Building house = f->immigrant_building;
            if (!house_is_valid(house, f->id())) {
                f->state = FIGURE_STATE_DEAD;
            } else if (figure_movement_move_ticks_cross_country(f, 1) == 1) {
                f->state = FIGURE_STATE_DEAD;
                if (house.type && house.type->has_housing() && !house.has_plague()) {
                    add_migrant_people_to_house(*f, house, 1);
                }
            }
            break;
        }
        case FIGURE_ACTION_10_HOMELESS_LEAVING:
        {
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            }
            f->wait_ticks++;
            if (f->wait_ticks > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->wait_ticks = 0;
                Building house = closest_house_with_room(f->x, f->y);
                int x_road, y_road;
                if (house.id() &&
                    map_closest_road_within_radius(house.x(), house.y(), house.size(), 2, &x_road, &y_road)) {
                    send_homeless_to_house(*f, house, x_road, y_road);
                    figure_route_remove(f);
                }
            }
            break;
        }
    }
    figure_image_update(f, image_group(GROUP_FIGURE_HOMELESS));
}
