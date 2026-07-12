#include "figuretype/fishing_boat.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/monument.h"
#include "city/message.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
#include "map/water.h"
#include "scenario/map.h"

namespace {

int lighthouse_monument_working()
{
    int working = 0;
    Building::for_each([&working](Building *building) {
        if (working || !building->type || !building->type->is_lighthouse()) {
            return;
        }
        const ::building *record = building->record();
        if (record) {
            working = building_monument_working(record->type);
        }
    });
    return working;
}

int fishing_boat_speed_bonus()
{
    return lighthouse_monument_working() ? 10 : 0;
}

int wharf_worker_percentage(const Building &wharf)
{
    const int required_workers = wharf.type ? wharf.type->required_workers() : 0;
    return required_workers > 0 ? calc_percentage(wharf.worker_count(), required_workers) : 100;
}

int image_base_for(const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    if (!definition) {
        return image_group(GROUP_FIGURE_SHIP) + 8;
    }
    const figure_type_registry_impl::FigureGraphics &graphics = definition->graphics();
    return image_group(graphics.image_group ? graphics.image_group : GROUP_FIGURE_SHIP) + graphics.image_group_offset;
}

} // namespace

FishingBoat &FishingBoat::from(Figure &figure)
{
    return reinterpret_cast<FishingBoat &>(figure);
}

void FishingBoat::go_to_wharf(const map_point &tile)
{
    action_state = FIGURE_ACTION_193_FISHING_BOAT_GOING_TO_WHARF;
    destination_x = static_cast<unsigned char>(tile.x);
    destination_y = static_cast<unsigned char>(tile.y);
    source_x = static_cast<unsigned char>(tile.x);
    source_y = static_cast<unsigned char>(tile.y);
    Route::remove(this);
}

int FishingBoat::ensure_wharf_assignment()
{
    map_point tile;
    if (action_state == FIGURE_ACTION_190_FISHING_BOAT_CREATED) {
        if (building && map_water_assign_fishing_boat_to_wharf(this, *building, &tile)) {
            go_to_wharf(tile);
        }
        return 1;
    }

    if (building && map_water_assign_fishing_boat_to_wharf(this, *building, &tile)) {
        source_x = static_cast<unsigned char>(tile.x);
        source_y = static_cast<unsigned char>(tile.y);
        if (action_state == FIGURE_ACTION_194_FISHING_BOAT_AT_WHARF) {
            destination_x = static_cast<unsigned char>(tile.x);
            destination_y = static_cast<unsigned char>(tile.y);
        }
        return 1;
    }

    Building *wharf = map_water_assign_wharf_for_new_fishing_boat(this, &tile);
    if (!wharf) {
        state = FIGURE_STATE_DEAD;
        return 0;
    }
    if (building) {
        building->clear_figure_slot_if_matches(id());
    }
    this->building = wharf;
    go_to_wharf(tile);
    return 1;
}

int FishingBoat::advance(const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    if (!building || !building->is_in_use()) {
        state = FIGURE_STATE_DEAD;
        return 1;
    }
    if (!ensure_wharf_assignment()) {
        return 1;
    }

    is_ghost = 0;
    is_boat = 1;
    clear_legacy_cart_overlay_image();
    figure_image_increase_offset(this, definition ? definition->graphics().max_image_offset : 12);

    switch (action_state) {
        case FIGURE_ACTION_190_FISHING_BOAT_CREATED:
            wait_ticks++;
            if (wait_ticks >= game_time_scale_legacy_day_ticks(50)) {
                wait_ticks = 0;
                map_point tile;
                Building *wharf = map_water_assign_wharf_for_new_fishing_boat(this, &tile);
                if (wharf) {
                    building->clear_figure_slot_if_matches(id());
                    this->building = wharf;
                    go_to_wharf(tile);
                }
            }
            break;

        case FIGURE_ACTION_191_FISHING_BOAT_GOING_TO_FISH:
            figure_movement_move_ticks_with_percentage(this, 1, fishing_boat_speed_bonus());
            height_adjusted_ticks = 0;
            if (direction == DIR_FIGURE_AT_DESTINATION) {
                map_point tile;
                if (map_water_find_alternative_fishing_boat_tile(this, &tile)) {
                    Route::remove(this);
                    destination_x = static_cast<unsigned char>(tile.x);
                    destination_y = static_cast<unsigned char>(tile.y);
                    direction = previous_tile_direction;
                } else {
                    action_state = FIGURE_ACTION_192_FISHING_BOAT_FISHING;
                    wait_ticks = 0;
                }
            } else if (direction == DIR_FIGURE_REROUTE || direction == DIR_FIGURE_LOST) {
                action_state = FIGURE_ACTION_194_FISHING_BOAT_AT_WHARF;
                destination_x = source_x;
                destination_y = source_y;
            }
            break;

        case FIGURE_ACTION_192_FISHING_BOAT_FISHING:
            wait_ticks++;
            if (wait_ticks >= 200) {
                wait_ticks = 0;
                action_state = FIGURE_ACTION_195_FISHING_BOAT_RETURNING_WITH_FISH;
                destination_x = source_x;
                destination_y = source_y;
                Route::remove(this);
            }
            break;

        case FIGURE_ACTION_193_FISHING_BOAT_GOING_TO_WHARF:
            figure_movement_move_ticks_with_percentage(this, 1, fishing_boat_speed_bonus());
            height_adjusted_ticks = 0;
            if (direction == DIR_FIGURE_AT_DESTINATION) {
                action_state = FIGURE_ACTION_194_FISHING_BOAT_AT_WHARF;
                wait_ticks = 0;
            } else if (direction == DIR_FIGURE_REROUTE) {
                Route::remove(this);
            } else if (direction == DIR_FIGURE_LOST) {
                city_message_post_with_message_delay(MESSAGE_CAT_FISHING_BLOCKED, 1, MESSAGE_FISHING_BOAT_BLOCKED, 12);
                state = FIGURE_STATE_DEAD;
            }
            break;

        case FIGURE_ACTION_194_FISHING_BOAT_AT_WHARF: {
            int pct_workers = wharf_worker_percentage(*building);
            if (building->industry_has_fish() > 0) {
                pct_workers = 0;
            }
            if (pct_workers > 0) {
                wait_ticks++;
                if (wait_ticks >= 5 * (102 - pct_workers)) {
                    wait_ticks = 0;
                    map_point tile;
                    if (scenario_map_closest_fishing_point(x, y, &tile)) {
                        action_state = FIGURE_ACTION_191_FISHING_BOAT_GOING_TO_FISH;
                        destination_x = static_cast<unsigned char>(tile.x);
                        destination_y = static_cast<unsigned char>(tile.y);
                        Route::remove(this);
                    }
                }
            }
            break;
        }

        case FIGURE_ACTION_195_FISHING_BOAT_RETURNING_WITH_FISH:
            figure_movement_move_ticks_with_percentage(this, 1, fishing_boat_speed_bonus());
            height_adjusted_ticks = 0;
            if (direction == DIR_FIGURE_AT_DESTINATION) {
                action_state = FIGURE_ACTION_194_FISHING_BOAT_AT_WHARF;
                wait_ticks = 0;
                building->set_figure_spawn_delay(1);
                building->add_industry_fish(1);
                building->add_industry_production_current_month(100);
            } else if (direction == DIR_FIGURE_REROUTE) {
                Route::remove(this);
            } else if (direction == DIR_FIGURE_LOST) {
                state = FIGURE_STATE_DEAD;
            }
            break;
    }

    update_image(definition);
    return 1;
}

void FishingBoat::sink()
{
    if (building) {
        building->clear_figure_slot_if_matches(id());
        map_water_clear_fishing_boat_from_wharf(*building, id());
    }
    this->building = nullptr;
    type = FIGURE_SHIPWRECK;
    wait_ticks = 0;
}

void FishingBoat::update_image(const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    const int dir = figure_image_normalize_direction(direction < 8 ? direction : previous_tile_direction);
    select_legacy_directional_frame_image(
        image_base_for(definition) + (action_state == FIGURE_ACTION_192_FISHING_BOAT_FISHING ? 8 : 0),
        dir,
        0);
}
