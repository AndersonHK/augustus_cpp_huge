#include "building/storage.h"
#include "figure/action.h"

#include "workcamp.h"

#include "building/building.h"
#include "city/god.h"

#include "building/building_record.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/random.h"
#include "figure/combat.h"
#include "figure/figure_runtime_api.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/ResourceGraphics.h"
#include "game/time.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/ui_constants.h"
#include "map/figure.h"
#include "map/grid.h"
#include "translation/translation.h"
#include "window/building/common.h"

#define VALID_MONUMENT_RECHECK_TICKS 60

void figuretype::WorkCampWorker::draw(building_info_context *c)
{
    draw_big_people_image(c->x_offset + 28, c->y_offset + 112);

    lang_text_draw(current_string_key(65, name), c->x_offset + 90, c->y_offset + 108,
        FONT_LARGE_BROWN, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BROWN)->line_height));
    int width = text_draw(translation_for(new_type_translation_key(static_cast<figure_type>(type))),
        c->x_offset + 92, c->y_offset + 139,
        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    resource_type resource = static_cast<resource_type>(collecting_item_id);

    if (action_state == FIGURE_ACTION_204_WORK_CAMP_WORKER_GETTING_RESOURCES) {
        width += lang_text_draw("main_strings.129.17", c->x_offset + 90 + width, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        resource_graphics(resource).panel_icon().draw(c->x_offset + 90 + width, c->y_offset + 135);
    } else if (action_state == FIGURE_ACTION_205_WORK_CAMP_WORKER_GOING_TO_MONUMENT ||
        action_state == FIGURE_ACTION_209_WORK_CAMP_SLAVE_FOLLOWING ||
        action_state == FIGURE_ACTION_210_WORK_CAMP_SLAVE_GOING_TO_MONUMENT ||
        action_state == FIGURE_ACTION_211_WORK_CAMP_SLAVE_DELIVERING_RESOURCES) {
        width += lang_text_draw("main_strings.129.18", c->x_offset + 90 + width, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        resource_graphics(resource).panel_icon().draw(c->x_offset + 90 + width, c->y_offset + 135);
    }
    if (c->figure.phrase_id >= 0) {
        lang_text_draw_multiline(current_string_key(130, 21 * c->figure.sound_id + c->figure.phrase_id + 1),
            c->x_offset + 90, c->y_offset + 160, BLOCK_SIZE * (c->width_blocks - 8),
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
}

static int create_slave_workers(int leader_id, int first_figure_id)
{
    Figure *f = Figure::get(first_figure_id);
    Figure *slave = Figure::create(FIGURE_WORK_CAMP_SLAVE, f->x, f->y, DIR_0_TOP);
    f = Figure::get(first_figure_id);
    slave->leading_figure_id = static_cast<short>(leader_id);
    slave->collecting_item_id = f->collecting_item_id;
    slave->set_home_building(f->building);
    slave->set_destination_building(f->destination_building);
    slave->destination_x = f->destination_x;
    slave->destination_y = f->destination_y;
    slave->action_state = FIGURE_ACTION_209_WORK_CAMP_SLAVE_FOLLOWING;
    slave->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
    if (slave->destination_building) {
        building_monument_add_delivery(slave->destination_building->id, slave->id(), slave->collecting_item_id, 1);
    }
    return slave->id();
}

static int take_resource_from_warehouse(Figure *f, Building &warehouse)
{
    const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
    if (!f->destination_building) {
        return 0;
    }
    Building &monument = *f->destination_building;
    building *monument_record = const_cast<building *>(monument.record());
    int resources_needed = monument.resource_amount(resource) -
        building_monument_resource_in_delivery(monument_record, resource);
    int num_loads;
    int stored = building_warehouse_get_amount(warehouse, resource);
    if (stored <= CARTLOADS_PER_MONUMENT_DELIVERY) {
        num_loads = stored;
    } else {
        num_loads = CARTLOADS_PER_MONUMENT_DELIVERY;
    }
    if (num_loads > resources_needed) {
        num_loads = resources_needed;
    }

    if (num_loads <= 0) {
        return 0;
    }

    building_warehouse_try_remove_resource(warehouse, resource, num_loads);

    // create slave workers
    int slave = f->id();
    for (int i = 0; i < num_loads; i++) {
        slave = create_slave_workers(slave, f->id());
    }
    return 1;
}

static int has_valid_monument_destination(Figure *f)
{
    if (f->wait_ticks++ >= game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS)) {
        if (!building_monument_has_delivery_for_worker(f->id())) {
            return 0;
        }
        f->wait_ticks = 0;
    }
    return 1;
}

void figure_workcamp_worker_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    if (!f->building) {
        f->state = FIGURE_STATE_DEAD;
        figure_runtime_update_graphics(f);
        return;
    }
    Building &source = *f->building;
    building *b = const_cast<building *>(source.record());
    map_point dst;
    if (b->state != BUILDING_STATE_IN_USE || b->figure_id != f->id()) {
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
        case FIGURE_ACTION_203_WORK_CAMP_WORKER_CREATED:
            if (!building_monument_has_unfinished_monuments()) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            for (int resource_id = (RESOURCE_NONE + 1); resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
                const resource_type resource = static_cast<resource_type>(resource_id);
                if (city_resource_is_stockpiled(resource) || !resource_is_storable(resource)) {
                    continue;
                }
                Building *monument = building_monument_get_monument(
                    source.x(), source.y(), resource, source.road_network_id(), 0);
                if (!monument) {
                    continue;
                }
                Building *warehouse = building_warehouse_with_resource(
                    f->x, f->y, resource, source.road_network_id(), 0, &dst, BUILDING_STORAGE_PERMISSION_WORKCAMP);
                if (!warehouse) {
                    continue;
                }

                f->collecting_item_id = static_cast<unsigned char>(resource);
    f->set_destination_building(warehouse);
                f->destination_x = static_cast<unsigned char>(dst.x);
                f->destination_y = static_cast<unsigned char>(dst.y);
                f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                f->action_state = FIGURE_ACTION_204_WORK_CAMP_WORKER_GETTING_RESOURCES;
                building *monument_record = const_cast<building *>(monument->record());
                int resources_needed = monument_record->resources[resource] -
                    building_monument_resource_in_delivery(monument_record, resource);
                resources_needed = calc_bound(resources_needed, 0, CARTLOADS_PER_MONUMENT_DELIVERY);
                building_monument_add_delivery(monument->id, f->id(), resource, resources_needed);
                break;
            }
            if (!f->destination_building) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;

        case FIGURE_ACTION_204_WORK_CAMP_WORKER_GETTING_RESOURCES:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                building_monument_remove_delivery(f->id());
                Building *warehouse = f->destination_building;
                Building *monument = building_monument_get_monument(
                    source.x(), source.y(), f->collecting_item_id, source.road_network_id(), &dst);
                f->action_state = FIGURE_ACTION_205_WORK_CAMP_WORKER_GOING_TO_MONUMENT;
    f->set_destination_building(monument);
                f->destination_x = static_cast<unsigned char>(dst.x);
                f->destination_y = static_cast<unsigned char>(dst.y);
                f->previous_tile_x = f->x;
                f->previous_tile_y = f->y;
                f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                if (!monument || !warehouse) {
                    f->state = FIGURE_STATE_DEAD;
                } else if (!take_resource_from_warehouse(f, *warehouse)) {
                    f->state = FIGURE_STATE_DEAD;
                } else {
                    // Placeholder delivery
                    building_monument_add_delivery(monument->id, f->id(), f->collecting_item_id, 0);
                }
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;

        case FIGURE_ACTION_205_WORK_CAMP_WORKER_GOING_TO_MONUMENT:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                f->action_state = FIGURE_ACTION_216_WORK_CAMP_WORKER_ENTERING_MONUMENT;
                building *monument = f->destination_building ?
                    const_cast<building *>(f->destination_building->record()) :
                    nullptr;
                if (!monument || !building_monument_access_point(monument, &dst)) {
                    f->state = FIGURE_STATE_DEAD;
                    break;
                }
                figure_movement_set_cross_country_destination(f, dst.x, dst.y);
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            }
            break;

        case FIGURE_ACTION_216_WORK_CAMP_WORKER_ENTERING_MONUMENT:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            f->terrain_usage = TERRAIN_USAGE_ANY;
            f->use_cross_country = 1;
            f->dont_draw_elevated = 1;
            if (figure_movement_move_ticks_cross_country(f, 1)) {
                f->state = FIGURE_STATE_DEAD;
            } else {
                if (f->direction == DIR_FIGURE_REROUTE) {
                    Route::remove(f);
                }
            }
            break;
    }

    figure_runtime_update_graphics(f);
}

void figure_workcamp_slave_action(Figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    Figure *leader = Figure::get(f->leading_figure_id);
    map_point dst;
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_209_WORK_CAMP_SLAVE_FOLLOWING:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            if (f->leading_figure_id <= 0 || leader->action_state == FIGURE_ACTION_149_CORPSE) {
                f->state = FIGURE_STATE_DEAD;
            } else {
                if (leader->state == FIGURE_STATE_ALIVE) {
                    if (leader->type == FIGURE_WORK_CAMP_WORKER || leader->type == FIGURE_WORK_CAMP_SLAVE) {
                        figure_movement_follow_ticks(f, 1);
                        if (leader->action_state == FIGURE_ACTION_210_WORK_CAMP_SLAVE_GOING_TO_MONUMENT ||
                            leader->action_state == FIGURE_ACTION_216_WORK_CAMP_WORKER_ENTERING_MONUMENT) {
                            f->action_state = FIGURE_ACTION_210_WORK_CAMP_SLAVE_GOING_TO_MONUMENT;
                            f->wait_ticks = static_cast<short>(
                                game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                        }
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                } else { // leader arrived at the monument, continue on your own
                    f->action_state = FIGURE_ACTION_210_WORK_CAMP_SLAVE_GOING_TO_MONUMENT;
                    f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                }
            }
            if (leader->is_ghost && !leader->height_adjusted_ticks) {
                f->is_ghost = 1;
            }
            break;

        case FIGURE_ACTION_210_WORK_CAMP_SLAVE_GOING_TO_MONUMENT:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_211_WORK_CAMP_SLAVE_DELIVERING_RESOURCES;
                building *monument = f->destination_building ?
                    const_cast<building *>(f->destination_building->record()) :
                    nullptr;
                if (!monument || !building_monument_access_point(monument, &dst)) {
                    f->state = FIGURE_STATE_DEAD;
                    break;
                }
                f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                figure_movement_set_cross_country_destination(f, dst.x, dst.y);
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            }
            break;

        case FIGURE_ACTION_211_WORK_CAMP_SLAVE_DELIVERING_RESOURCES:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            f->terrain_usage = TERRAIN_USAGE_ANY;
            f->use_cross_country = 1;
            f->dont_draw_elevated = 1;
            if (figure_movement_move_ticks_cross_country(f, 1)) {
                building *monument = f->destination_building ?
                    const_cast<building *>(f->destination_building->record()) :
                    nullptr;
                if (monument) {
                    building_monument_deliver_resource(monument, f->collecting_item_id);
                }
                f->state = FIGURE_STATE_DEAD;
            } else {
                if (f->direction == DIR_FIGURE_REROUTE) {
                    Route::remove(f);
                }
            }
            break;
    }

    figure_runtime_update_graphics(f);
}

void figure_workcamp_architect_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    if (!f->building) {
        f->state = FIGURE_STATE_DEAD;
        figure_runtime_update_graphics(f);
        return;
    }
    Building &source = *f->building;
    building *b = const_cast<building *>(source.record());
    map_point dst;
    if (b->state != BUILDING_STATE_IN_USE || b->figure_id != f->id()) {
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
        case FIGURE_ACTION_206_WORK_CAMP_ARCHITECT_CREATED:
            if (!building_monument_has_unfinished_monuments()) {
                f->state = FIGURE_STATE_DEAD;
            } else {
                Building *monument = building_monument_get_monument(
                    source.x(), source.y(), RESOURCE_NONE, source.road_network_id(), &dst);
                building *monument_record = monument ? const_cast<building *>(monument->record()) : nullptr;
                if (monument && !building_monument_is_construction_halted(monument_record)) {
    f->set_destination_building(monument);
                    f->destination_x = static_cast<unsigned char>(dst.x);
                    f->destination_y = static_cast<unsigned char>(dst.y);
                    // Only send 1 architect
                    building_monument_add_delivery(monument->id, f->id(), RESOURCE_NONE, 10);
                    f->action_state = FIGURE_ACTION_207_WORK_CAMP_ARCHITECT_GOING_TO_MONUMENT;
                    f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS));
                    break;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;

        case FIGURE_ACTION_207_WORK_CAMP_ARCHITECT_GOING_TO_MONUMENT:
            if (!has_valid_monument_destination(f)) {
                f->state = FIGURE_STATE_DEAD;
                break;
            }
            figure_movement_move_ticks(f, 1);
            {
                building *monument = f->destination_building ?
                    const_cast<building *>(f->destination_building->record()) :
                    nullptr;
                if (!monument || monument->state == BUILDING_STATE_UNUSED ||
                    !building_monument_access_point(monument, &dst) ||
                    monument->monument.phase == MONUMENT_FINISHED) {
                f->state = FIGURE_STATE_DEAD;
                } else {
                    if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                        f->action_state = FIGURE_ACTION_208_WORK_CAMP_ARCHITECT_WORKING_ON_MONUMENT;
                        figure_movement_set_cross_country_destination(f, dst.x, dst.y);
                        f->wait_ticks = 1;
                    } else if (f->direction == DIR_FIGURE_REROUTE) {
                        Route::remove(f);
                    }
                }
            }
            break;
        case FIGURE_ACTION_208_WORK_CAMP_ARCHITECT_WORKING_ON_MONUMENT:
            f->terrain_usage = TERRAIN_USAGE_ANY;
            f->use_cross_country = 1;
            f->dont_draw_elevated = 1;
            if (figure_movement_move_ticks_cross_country(f, 1)) {
                if (f->wait_ticks >= 384) {
                    f->state = FIGURE_STATE_DEAD;
                    building *monument = f->destination_building ?
                        const_cast<building *>(f->destination_building->record()) :
                        nullptr;
                    if (monument) {
                        monument->resources[RESOURCE_NONE]--;
                        if (monument->resources[RESOURCE_NONE] <= 0) {
                            building_monument_progress(monument);
                        }
                    }
                } else {
                    f->wait_ticks++;
                }
            } else {
                if (f->direction == DIR_FIGURE_REROUTE) {
                    Route::remove(f);
                }
            }
            break;
    }

    figure_runtime_update_graphics(f);
}
