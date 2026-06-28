#include "building/house.h"
#include "city/health.h"
#include "figuretype/supplier.h"
#include "map/building.h"
#include "map/road_access.h"

#include "service.h"

#include "building/building.h"
#include "building/market.h"

#include "assets/assets.h"
#include "building/building_record.h"
#include "city/buildings.h"
#include "core/calc.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/figure_runtime_api.h"
#include "game/time.h"

static const int DOCTOR_HEALING_OFFSETS[] = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1};

static Building *first_plague_building_matching(const char *attr)
{
    Building *match = nullptr;
    Building::for_each([&](Building *building) {
        if (!match && building->matches(attr) && building->has_plague()) {
            match = building;
        }
    });
    return match;
}

static void roamer_action(Figure *f, int num_ticks)
{
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_125_ROAMING:
            f->is_ghost = 0;
            f->roam_length++;
            if (f->roam_length >= f->max_roam_length) {
                int x, y;
                Building &owner = *f->building;
                if (map_closest_road_within_radius(owner.x(), owner.y(), owner.size(), 2, &x, &y)) {
                    f->action_state = FIGURE_ACTION_126_ROAMER_RETURNING;
                    f->destination_x = static_cast<unsigned char>(x);
                    f->destination_y = static_cast<unsigned char>(y);
                    Route::remove(f);
                    f->roam_length = 0;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            figure_movement_roam_ticks(f, num_ticks);
            break;
        case FIGURE_ACTION_126_ROAMER_RETURNING:
            figure_movement_move_ticks(f, num_ticks);
            if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }
}

static void culture_action(Figure *f)
{
    figure_runtime_apply_profile_movement(f);
    Building &owner = *f->building;
    ::building *record = const_cast<::building *>(owner.record());
    if (!owner.is_in_use() || record->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    roamer_action(f, 1);
    figure_runtime_update_graphics(f);
}

void figure_destination_priest_action(Figure *f)
{
    Building &owner = *f->building;
    Building &destination = *f->destination_building;
    ::building *owner_record = const_cast<::building *>(owner.record());
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    if (!owner.is_in_use() ||
        (owner_record->figure_id4 != f->id() && owner_record->figure_id2 != f->id()) ||
        !destination.is_in_use()) {
        f->state = FIGURE_STATE_DEAD;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_212_DESTINATION_PRIEST_CREATED:
            f->destination_x = static_cast<unsigned char>(destination.road_access_x());
            f->destination_y = static_cast<unsigned char>(destination.road_access_y());
            f->action_state = FIGURE_ACTION_213_PRIEST_GOING_TO_PANTHEON;

            break;
        case FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED:
        {
            f->destination_x = static_cast<unsigned char>(destination.road_access_x());
            f->destination_y = static_cast<unsigned char>(destination.road_access_y());
            int market_units = owner.resource_amount(static_cast<resource_type>(f->collecting_item_id));
            int num_loads;
            int max_units = MAX_FOOD_STOCKED_MESS_HALL - market_units;

            if (market_units >= 800) {
                num_loads = 8;
            } else if (market_units >= 700) {
                num_loads = 7;
            } else if (market_units >= 600) {
                num_loads = 6;
            } else if (market_units >= 500) {
                num_loads = 5;
            } else if (market_units >= 400) {
                num_loads = 4;
            } else if (market_units >= 300) {
                num_loads = 3;
            } else if (market_units >= 200) {
                num_loads = 2;
            } else if (market_units >= 100) {
                num_loads = 1;
            } else {
                num_loads = 0;
            }
            if (num_loads > max_units / 100) {
                num_loads = max_units / 100;
            }
            if (num_loads <= 0) {
                return;
            }

            owner.set_resource_amount(
                static_cast<resource_type>(f->collecting_item_id), market_units - (100 * num_loads));

            // create delivery boys
            int priest_id = f->id();
            int previous_boy = f->id();
            for (int i = 0; i < num_loads; i++) {
                previous_boy = figure_supplier_create_delivery_boy(previous_boy, priest_id, FIGURE_DELIVERY_BOY);
            }
            f = Figure::get(priest_id);

            f->action_state = FIGURE_ACTION_215_PRIEST_GOING_TO_MESS_HALL;
            break;
        }

        case FIGURE_ACTION_213_PRIEST_GOING_TO_PANTHEON:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_215_PRIEST_GOING_TO_MESS_HALL:
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }

    figure_image_increase_offset(f, 12);
    figure_image_update(f, image_group(GROUP_FIGURE_PRIEST));
}


void figure_priest_action(Figure *f)
{
    if (f->destination_building) {
        figure_destination_priest_action(f);
    } else {
        culture_action(f);
    }
}

void figure_school_child_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = 192;

    Building &owner = *f->building;
    if (!owner.is_in_use() || !owner.matches("school")) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_125_ROAMING:
            f->is_ghost = 0;
            f->roam_length++;
            if (f->roam_length >= f->max_roam_length) {
                f->state = FIGURE_STATE_DEAD;
            }
            figure_movement_roam_ticks(f, 2);
            break;
    }
    figure_image_update(f, image_group(GROUP_FIGURE_SCHOOL_CHILD));
}

void figure_teacher_action(Figure *f)
{
    culture_action(f);
}

void figure_librarian_action(Figure *f)
{
    culture_action(f);
}

void figure_barber_action(Figure *f)
{
    culture_action(f);
}

void figure_bathhouse_worker_action(Figure *f)
{
    culture_action(f);
}

void figure_tavern_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = 384;
    Building &owner = *f->building;
    ::building *record = const_cast<::building *>(owner.record());
    if (!owner.is_in_use() || record->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    roamer_action(f, 1);
    figure_runtime_update_graphics(f);
}

static int fight_plague(Figure *f, int force)
{
    Building *building_with_plague = nullptr;

    // Find in houses
    Building::for_each({ .hasHousing = true }, [&](Building *house) {
        if (!building_with_plague && building_house_is_active(*house) && house->has_plague()) {
            building_with_plague = house;
        }
    });

    // If no houses, find in docks
    if (!building_with_plague) {
        Building::for_each([&](Building *building) {
            if (!building_with_plague && building->matches("dock") && building->has_plague()) {
                building_with_plague = building;
            }
        });
    }

    // If no docks, find in warehouses
    if (!building_with_plague) {
        building_with_plague = first_plague_building_matching("warehouse");

        // If no warehouse, find in granaries
        if (!building_with_plague) {
            building_with_plague = first_plague_building_matching("granary");
        }
    }

    // No plague in buildings
    if (!building_with_plague) {
        return 0;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_231_DOCTOR_GOING_TO_PLAGUE:
        case FIGURE_ACTION_232_DOCTOR_AT_PLAGUE:
            if (!force) {
                return 0;
            }
            if (f->destination_building && f->destination_building->has_plague()) {
                return 1;
            }
    }

    f->wait_ticks_missile++;
    if (f->wait_ticks_missile < 20 && !force) {
        return 0;
    }
    int distance;
    int building_with_plague_id = city_buildings_get_closest_plague(f->x, f->y, &distance);
    building_with_plague = nullptr;
    if (building_with_plague_id > 0) {
        Building::for_each([&](Building *building) {
            if (!building_with_plague && building->id == static_cast<unsigned int>(building_with_plague_id)) {
                building_with_plague = building;
            }
        });
    }
    if (building_with_plague && distance <= 25) {
        ::building *plague_record = const_cast<::building *>(building_with_plague->record());
        f->wait_ticks_missile = 0;
        f->action_state = FIGURE_ACTION_231_DOCTOR_GOING_TO_PLAGUE;
        f->wait_ticks = 0;
        f->destination_x = static_cast<unsigned char>(building_with_plague->road_access_x());
        f->destination_y = static_cast<unsigned char>(building_with_plague->road_access_y());
        f->destination_building = building_with_plague;
        Route::remove(f);
        plague_record->figure_id4 = f->id();
        return 1;
    }
    return 0;
}

static void heal_plague(Figure *f)
{
    Building &building_with_plague = *f->destination_building;
    ::building *plague_record = const_cast<::building *>(building_with_plague.record());
    int distance = calc_maximum_distance(f->x, f->y, building_with_plague.x(), building_with_plague.y());

    if (building_with_plague.has_plague() && distance < 5) {
        if (plague_record->sickness_duration < 95) {
            plague_record->sickness_duration = 95;
        }
        plague_record->sickness_doctor_cure = 99; // Use sickness_doctor_cure = 99 to know if doctor is currently healing building (Need to stay 99 for retro-compatibility)
    } else {
        f->wait_ticks = 1;
    }

    f->wait_ticks--;
    if (f->wait_ticks <= 0) {
        if (!fight_plague(f, 1)) {
            Building &owner = *f->building;
            int x_road, y_road;
            if (map_closest_road_within_radius(owner.x(), owner.y(), owner.size(), 2, &x_road, &y_road)) {
                f->action_state = FIGURE_ACTION_126_ROAMER_RETURNING;
                f->destination_x = static_cast<unsigned char>(x_road);
                f->destination_y = static_cast<unsigned char>(y_road);
                Route::remove(f);
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
        }
    }
}

void figure_doctor_action(Figure *f)
{
    Building &owner = *f->building;

    // special actions
    if (!fight_plague(f, 0)) {
        f->terrain_usage = TERRAIN_USAGE_ROADS;
        culture_action(f);
    }
    switch (f->action_state) {
        case FIGURE_ACTION_231_DOCTOR_GOING_TO_PLAGUE:
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_232_DOCTOR_AT_PLAGUE;
                Route::remove(f);
                f->roam_length = 0;
                f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(50));
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS && !fight_plague(f, 1)) {
                int x_road, y_road;
                if (map_closest_road_within_radius(owner.x(), owner.y(), owner.size(), 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_126_ROAMER_RETURNING;
                    f->destination_x = static_cast<unsigned char>(x_road);
                    f->destination_y = static_cast<unsigned char>(y_road);
                    f->wait_ticks = 0;
                    Route::remove(f);
                }
            }
            break;
        case FIGURE_ACTION_232_DOCTOR_AT_PLAGUE:
            heal_plague(f);
            // Nonstandard number of walker animation frames
            if (f->image_offset >= sizeof DOCTOR_HEALING_OFFSETS / sizeof DOCTOR_HEALING_OFFSETS[0]) {
                f->image_offset = 0;
            }
            f->select_legacy_frame_image(
                assets_get_image_id("Health_Culture\\Doctor_heal", "Doctor heal"),
                DOCTOR_HEALING_OFFSETS[f->image_offset]);
            break;
    }
}

void figure_missionary_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = 192;
    Building &owner = *f->building;
    ::building *record = const_cast<::building *>(owner.record());
    if (!owner.is_in_use() || record->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    roamer_action(f, 1);
    figure_image_update(f, image_group(GROUP_FIGURE_MISSIONARY));
}

void figure_labor_seeker_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = 384;
    Building &owner = *f->building;
    ::building *record = const_cast<::building *>(owner.record());
    if (!owner.is_in_use() || record->figure_id2 != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    roamer_action(f, 1);
    figure_runtime_update_graphics(f);
}

void figure_market_trader_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = 384;
    Building &market = *f->building;
    ::building *record = const_cast<::building *>(market.record());
    if (!market.is_in_use() || record->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);

    roamer_action(f, 1);
    figure_runtime_update_graphics(f);
}

void figure_tax_collector_action(Figure *f)
{
    Building &owner = *f->building;
    ::building *record = const_cast<::building *>(owner.record());

    figure_runtime_apply_profile_movement(f);
    if (!owner.is_in_use() || record->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_40_TAX_COLLECTOR_CREATED:
            f->is_ghost = 1;
            f->image_offset = 0;
            f->wait_ticks--;
            if (f->wait_ticks <= 0) {
                int x_road, y_road;
                if (map_closest_road_within_radius(owner.x(), owner.y(), owner.size(), 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_41_TAX_COLLECTOR_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, x_road, y_road);
                    f->roam_length = 0;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
        case FIGURE_ACTION_41_TAX_COLLECTOR_ENTERING_EXITING:
            f->use_cross_country = 1;
            f->is_ghost = 1;
            if (figure_movement_move_ticks_cross_country(f, 1) == 1) {
                if (map_building_exists_at(f->grid_offset) && map_building_at(f->grid_offset).id == owner.id) {
                    // returned to own building
                    f->state = FIGURE_STATE_DEAD;
                } else {
                    f->action_state = FIGURE_ACTION_42_TAX_COLLECTOR_ROAMING;
                    figure_movement_init_roaming(f);
                    f->roam_length = 0;
                }
            }
            break;
        case FIGURE_ACTION_42_TAX_COLLECTOR_ROAMING:
            f->is_ghost = 0;
            f->roam_length++;
            if (f->roam_length >= f->max_roam_length) {
                int x_road, y_road;
                if (map_closest_road_within_radius(owner.x(), owner.y(), owner.size(), 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_43_TAX_COLLECTOR_RETURNING;
                    f->destination_x = static_cast<unsigned char>(x_road);
                    f->destination_y = static_cast<unsigned char>(y_road);
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            figure_movement_roam_ticks(f, 1);
            break;
        case FIGURE_ACTION_43_TAX_COLLECTOR_RETURNING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_41_TAX_COLLECTOR_ENTERING_EXITING;
                figure_movement_set_cross_country_destination(f, owner.x(), owner.y());
                f->roam_length = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }
    figure_runtime_update_graphics(f);
}
