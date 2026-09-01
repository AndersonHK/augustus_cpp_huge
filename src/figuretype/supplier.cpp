#include "building/distribution.h"
#include "building/storage.h"
#include "figuretype/wall.h"
#include "map/road_access.h"

#include "supplier.h"

#include "building/building.h"
#include "building/market.h"

#include "building/building_record.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "core/config.h"
#include "core/image.h"
#include "city/data_private.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/figure_runtime_api.h"
#include "game/ResourceGraphics.h"
#include "game/resource.h"
#include "game/time.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "map/data.h"
#include "map/road_network.h"
#include "translation/translation.h"
#include "window/building/common.h"


int figure_supplier_max_stocked_mess_hall_adjusted(void)
{
    int max_stock;
    if (city_data.military.total_legions < 10) {
        max_stock = MAX_FOOD_STOCKED_MESS_HALL;
    } else if (city_data.military.total_legions == 10) {
        max_stock = (MAX_FOOD_STOCKED_MESS_HALL * 3) / 2; //increase by 50% if max legions
    } else {   //cheat code activated
        max_stock = MAX_FOOD_STOCKED_MESS_HALL * 2; // double the possible stock
    }
    return max_stock;
}

int figure_supplier_create_delivery_boy(int leader_id, int first_figure_id, int type)
{
    Figure *f = Figure::get(first_figure_id);
    Figure *boy = Figure::create(static_cast<figure_type>(type), f->x, f->y, DIR_0_TOP);
    f = Figure::get(first_figure_id);
    boy->leading_figure_id = static_cast<short>(leader_id);
    boy->collecting_item_id = f->collecting_item_id;
    boy->loads_sold_or_carrying = 1; // for consistency
    // deliver to destination instead of origin
    if (f->action_state == FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED) {
        boy->set_home_building(f->destination_building);
    } else {
        boy->set_home_building(f->building);
    }
    return boy->id();
}

void figuretype::Supplier::draw(building_info_context *c)
{
    draw_big_people_image(c->x_offset + 28, c->y_offset + 112);

    lang_text_draw(current_string_key(65, name), c->x_offset + 90, c->y_offset + 108,
        FONT_LARGE_BROWN, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BROWN)->line_height));
    int width = 0;
    if (type == FIGURE_MESS_HALL_SUPPLIER || type == FIGURE_PRIEST_SUPPLIER ||
        type == FIGURE_BARKEEP_SUPPLIER || type == FIGURE_CARAVANSERAI_SUPPLIER ||
        type == FIGURE_LIGHTHOUSE_SUPPLIER) {
        width = text_draw(translation_for(new_type_translation_key(static_cast<figure_type>(type))),
            c->x_offset + 92, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    } else {
        width = lang_text_draw(current_string_key(64, type), c->x_offset + 92, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }

    resource_type resource = static_cast<resource_type>(collecting_item_id);

    if (action_state == FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE) {
        width += lang_text_draw("main_strings.129.17", c->x_offset + 90 + width, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        resource_graphics(resource).panel_icon().draw(c->x_offset + 90 + width, c->y_offset + 135);
    } else if (action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING && resource != RESOURCE_NONE) {
        width += lang_text_draw("main_strings.129.18", c->x_offset + 90 + width, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        resource_graphics(resource).panel_icon().draw(c->x_offset + 90 + width, c->y_offset + 135);
    }
    if (c->figure.phrase_id >= 0) {
        lang_text_draw_multiline(current_string_key(130, 21 * c->figure.sound_id + c->figure.phrase_id + 1),
            c->x_offset + 90, c->y_offset + 160, 16 * (c->width_blocks - 8),
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
}

static int take_food_from_storage(Figure *f, Building &market, Building &storage)
{
    resource_type resource = static_cast<resource_type>(f->collecting_item_id);

    if (!resource_is_food(resource)) {
        return 0;
    }
    const auto *market_type = market.type;

    int market_units = market.resource_amount(resource);
    int max_units = 0;

    if (market_type && market_type->is_mess_hall()) {
        max_units = figure_supplier_max_stocked_mess_hall_adjusted() - market_units;
    } else if (market_type && market_type->is_caravanserai()) {
        max_units = MAX_FOOD_STOCKED_CARAVANSERAI - market_units;
    } else {
        max_units = MAX_FOOD_STOCKED_MARKET - market_units;
    }
    int max_loads = max_units / resource_units_per_load();
    if (max_loads <= 0) {
        return 0;
    }

    int amount_taken = 0;
    const auto *storage_type = storage.type;
    if (storage_type && storage_type->is_warehouse()) {
        int warehouse_loads_stored = building_warehouse_get_available_amount(storage, resource);
        int warehouse_loads_take = warehouse_loads_stored > max_loads ? max_loads : warehouse_loads_stored;
        amount_taken = building_warehouse_try_remove_resource(storage, resource, warehouse_loads_take);
    } else if (storage_type && storage_type->is_granary()) {
        int granary_loads_stored = building_granary_count_available_resource(storage, resource, 1);
        int granary_loads_take = granary_loads_stored > max_loads ? max_loads : granary_loads_stored;
        amount_taken = building_granary_try_remove_resource(storage, resource, granary_loads_take);
    } else {
        return 0;
    }
    if (!amount_taken) {
        return 0;
    }

    // create delivery boys
    int type = FIGURE_DELIVERY_BOY;
    if (f->type == FIGURE_MESS_HALL_SUPPLIER) {
        type = FIGURE_MESS_HALL_COLLECTOR;
    } else if (f->type == FIGURE_CARAVANSERAI_SUPPLIER) {
        type = FIGURE_CARAVANSERAI_COLLECTOR;
    }
    int leader_id = f->id();
    int previous_boy = f->id();
    for (int i = 0; i < amount_taken; i++) {
        previous_boy = figure_supplier_create_delivery_boy(previous_boy, leader_id, type);
    }
    return 1;
}

// Venus Grand Temple wine
static int take_resource_from_generic_building(Figure *f, Building &building)
{
    int num_loads;
    int stored = building.resource_amount(resource_wine());
    if (stored < 2) {
        num_loads = stored;
    } else {
        num_loads = 2;
    }
    if (num_loads <= 0) {
        return 0;
    }
    building.add_resource(resource_wine(), -num_loads);

    // create delivery boys
    int priest_id = f->id();
    int boy1 = figure_supplier_create_delivery_boy(priest_id, priest_id, FIGURE_DELIVERY_BOY);
    if (num_loads > 1) {
        figure_supplier_create_delivery_boy(boy1, priest_id, FIGURE_DELIVERY_BOY);
    }
    return 1;
}

static int take_resource_from_warehouse(Figure *f, Building &warehouse, int max_amount)
{
    const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
    if (!warehouse.type || !warehouse.type->is_warehouse()) {
        return take_resource_from_generic_building(f, warehouse);
    }
    int num_loads;
    int stored = building_warehouse_get_available_amount(warehouse, resource);
    if (stored < max_amount) {
        num_loads = stored;
    } else {
        num_loads = max_amount;
    }
    if (num_loads <= 0) {
        return 0;
    }
    building_warehouse_try_remove_resource(warehouse, resource, num_loads);

    // create delivery boys
    if (f->type != FIGURE_LIGHTHOUSE_SUPPLIER) {
        int supplier_id = f->id();
        int boy1 = figure_supplier_create_delivery_boy(supplier_id, supplier_id, FIGURE_DELIVERY_BOY);
        if (num_loads > 1) {
            figure_supplier_create_delivery_boy(boy1, supplier_id, FIGURE_DELIVERY_BOY);
        }
    }
    return 1;
}

static int change_market_supplier_destination(Figure *f, Building *destination)
{
    Route::remove(f);
    if (!destination) {
        return 0;
    }
    f->set_destination_building(destination);
    const auto *destination_type = destination->type;
    map_point road = { 0 };
    int has_road_access = 0;
    if (destination_type && destination_type->is_storage()) {
        has_road_access = map_has_road_access_building(destination->x(), destination->y(), &road);
    }
    if (!has_road_access) {
        return 0;
    }

    f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
    f->destination_x = static_cast<unsigned char>(road.x);
    f->destination_y = static_cast<unsigned char>(road.y);
    return 1;
}

static int is_better_destination(Figure *f, resource_type r, resource_storage_info *info)
{
    Building *old_destination = f->destination_building;
    if (!old_destination) {
        return 1;
    }
    building *old_dest = const_cast<building *>(old_destination->record());
    const auto *old_dest_type = old_destination->type;
    // if any of these are true, the new building is automatically better
    if (!building_is_active(old_dest)) {
        return 1;
    } else if (old_dest_type && old_dest_type->is_granary() && old_dest->resources[r] <= 0) {
        return 1;
    } else if (old_dest_type && old_dest_type->is_warehouse() &&
        building_warehouse_get_amount(*old_destination, r) <= 0) {
        return 1;
    }
    // make sure the new building is less than or equal to half the distance from the old
    // building to help prevent market ladies from "ping ponging" back and forth
    int old_dest_dist = building_dist(f->x, f->y, 1, 1, old_dest);
    if (info->min_distance <= old_dest_dist / 2) {
        return 1;
    }
    return 0;
}

static int recalculate_market_supplier_destination(Figure *f)
{
    resource_type item = static_cast<resource_type>(f->collecting_item_id);
    Building *market = f->building;
    if (!market) {
        return 0;
    }
    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };

    Market market_object(*market);
    if (!market_object.needed_inventory(info) ||
        !market_object.resource_storages_for_supplier(info, f)) {
        return 0;
    }

    Building *item_storage = info[item].source;
    if (market == item_storage || f->destination_building == item_storage) {
        return 1;
    }

    if (item_storage) {
        if (is_better_destination(f, item, &info[item])) {
            return change_market_supplier_destination(f, item_storage);
        } else {
            return 1;
        }
    }
    resource_type fetch_inventory = market_object.fetch_inventory(info);
    if (fetch_inventory == RESOURCE_NONE) {
        return 0;
    }
    market_object.set_fetch_inventory_id(fetch_inventory);
    f->collecting_item_id = static_cast<unsigned char>(fetch_inventory);
    return change_market_supplier_destination(f, info[fetch_inventory].source);
}

void figure_supplier_action(Figure *f)
{
    if (f->type == FIGURE_MESS_HALL_SUPPLIER) {
        figure_runtime_apply_profile_movement(f);
    } else {
        f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
        f->use_cross_country = 0;
        f->max_roam_length = 800;
    }

    Building *source = f->building;
    building *source_record = source ? const_cast<building *>(source->record()) : nullptr;
    if (!source_record || !source->is_in_use() ||
        (source_record->figure_id2 != f->id() &&
            source_record->figure_id != f->id() &&
            source_record->figure_id4 != f->id())) {
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
        case FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE:
            if (f->type == FIGURE_MARKET_SUPPLIER &&
                (!f->building || !f->building->accepts_good(static_cast<resource_type>(f->collecting_item_id)))) {
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->collecting_item_id = RESOURCE_NONE;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                Route::remove(f);
                break;
            }
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->wait_ticks = 0;
                f->previous_tile_x = f->x;
                f->previous_tile_y = f->y;
                int id = f->id();
                if (!resource_is_food(static_cast<resource_type>(f->collecting_item_id))) {
                    int max_amount = f->type == FIGURE_LIGHTHOUSE_SUPPLIER ? 1 : 2;
                    if (!f->destination_building ||
                        !take_resource_from_warehouse(f, *f->destination_building, max_amount)) {
                        f->state = FIGURE_STATE_DEAD;
                    }
                } else {
                    if (!f->building || !f->destination_building ||
                        !take_food_from_storage(f, *f->building, *f->destination_building)) {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                f = Figure::get(id);
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                Route::remove(f);
            } else if (f->type == FIGURE_MARKET_SUPPLIER && f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->wait_ticks = 0;
                if (!recalculate_market_supplier_destination(f)) {
                    f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                    f->collecting_item_id = RESOURCE_NONE;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                    Route::remove(f);
                }
            }
            break;
        case FIGURE_ACTION_146_SUPPLIER_RETURNING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                if (f->direction == DIR_FIGURE_AT_DESTINATION && f->type == FIGURE_LIGHTHOUSE_SUPPLIER) {
                    if (f->building) {
                        f->building->add_resource(resource_timber(), 100);
                    }
                }
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            }
            break;
    }
    if (f->type == FIGURE_PRIEST_SUPPLIER) {
        figure_image_update(f, image_group(GROUP_FIGURE_PRIEST));
    } else if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->select_legacy_corpse_image(image_group(GROUP_FIGURE_CARTPUSHER) + 96);
        } else {
            f->select_legacy_directional_frame_image(image_group(GROUP_FIGURE_CARTPUSHER), dir, f->image_offset);
        }
    }
}

void figure_delivery_boy_action(Figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();

    Figure *leader = Figure::get(f->leading_figure_id);
    if (f->leading_figure_id <= 0 || leader->action_state == FIGURE_ACTION_149_CORPSE) {
        f->state = FIGURE_STATE_DEAD;
    } else {
        if (leader->state == FIGURE_STATE_ALIVE) {
            if (leader->type == FIGURE_MARKET_SUPPLIER || leader->type == FIGURE_DELIVERY_BOY ||
                leader->type == FIGURE_MESS_HALL_SUPPLIER || leader->type == FIGURE_MESS_HALL_COLLECTOR ||
                leader->type == FIGURE_PRIEST_SUPPLIER || leader->type == FIGURE_PRIEST ||
                leader->type == FIGURE_BARKEEP_SUPPLIER || leader->type == FIGURE_CARAVANSERAI_SUPPLIER ||
                leader->type == FIGURE_CARAVANSERAI_COLLECTOR) {
                figure_movement_follow_ticks(f, 1);
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
        } else { // leader arrived at market, drop resource at market
            if (f->building) {
                f->building->add_resource(static_cast<resource_type>(f->collecting_item_id), 100);
            }
            f->state = FIGURE_STATE_DEAD;
        }
    }
    if (leader->is_ghost && !leader->height_adjusted_ticks) {
        f->is_ghost = 1;
    }
}

void figure_fort_supplier_action(Figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);

    Building *mess_hall = f->building;
    if (!mess_hall || !mess_hall->is_in_use() ||
        !mess_hall->type || !mess_hall->type->is_mess_hall()) {
        f->state = FIGURE_STATE_DEAD;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_236_SUPPLY_POST_GOING_TO_FORT:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_237_SUPPLY_POST_RETURNING_FROM_FORT;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(20));
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_237_SUPPLY_POST_RETURNING_FROM_FORT:
            if (f->wait_ticks) {
                f->wait_ticks--;
            } else {
                figure_movement_move_ticks(f, 1);
                if (f->direction == DIR_FIGURE_REROUTE) {
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
    }

}
