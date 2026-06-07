#include "building/distribution.h"
#include "building/storage.h"
#include "figuretype/wall.h"
#include "game/resource_graphics.h"
#include "map/road_access.h"

#include "supplier.h"

#include "building/building.h"
#include "building/market.h"

extern "C" {
#include "assets/assets.h"
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "core/config.h"
#include "core/image.h"
#include "city/data_private.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/data.h"
#include "map/road_network.h"
}


static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int building_matches(building *b, const char *text_id)
{
    building_type type = runtime_type(text_id);
    return b && type != BUILDING_NONE && b->type == type;
}

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
    figure *f = figure_get(first_figure_id);
    figure *boy = figure_create(static_cast<figure_type>(type), f->x, f->y, DIR_0_TOP);
    f = figure_get(first_figure_id);
    boy->leading_figure_id = leader_id;
    boy->collecting_item_id = f->collecting_item_id;
    boy->loads_sold_or_carrying = 1; // for consistency
    // deliver to destination instead of origin
    if (f->action_state == FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED) {
        boy->building_id = f->destination_building_id;
    } else {
        boy->building_id = f->building_id;
    }
    return boy->id;
}

static int take_food_from_storage(figure *f, int market_id, int storage_id)
{
    resource_type resource = static_cast<resource_type>(f->collecting_item_id);

    if (!resource_is_food(resource)) {
        return 0;
    }
    building *storage = building_get(storage_id);
    building *market = building_get(market_id);
    if (!storage || !market) {
        return 0;
    }
    Building storage_obj(storage);

    int market_units = market->resources[resource];
    int max_units = 0;

    if (building_matches(market, "mess_hall")) {
        max_units = figure_supplier_max_stocked_mess_hall_adjusted() - market_units;
    } else if (building_matches(market, "caravanserai")) {
        max_units = MAX_FOOD_STOCKED_CARAVANSERAI - market_units;
    } else {
        max_units = MAX_FOOD_STOCKED_MARKET - market_units;
    }
    int max_loads = max_units / resource_units_per_load();
    if (max_loads <= 0) {
        return 0;
    }

    int amount_taken = 0;
    if (building_matches(storage, "warehouse")) {
        int warehouse_loads_stored = building_warehouse_get_available_amount(storage_obj, resource);
        int warehouse_loads_take = warehouse_loads_stored > max_loads ? max_loads : warehouse_loads_stored;
        amount_taken = building_warehouse_try_remove_resource(storage_obj, resource, warehouse_loads_take);
    } else if (building_matches(storage, "granary")) {
        int granary_loads_stored = building_granary_count_available_resource(storage_obj, resource, 1);
        int granary_loads_take = granary_loads_stored > max_loads ? max_loads : granary_loads_stored;
        amount_taken = building_granary_try_remove_resource(storage_obj, resource, granary_loads_take);
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
    int leader_id = f->id;
    int previous_boy = f->id;
    for (int i = 0; i < amount_taken; i++) {
        previous_boy = figure_supplier_create_delivery_boy(previous_boy, leader_id, type);
    }
    return 1;
}

// Venus Grand Temple wine
static int take_resource_from_generic_building(figure *f, int building_id)
{
    building *b = building_get(building_id);
    int num_loads;
    int stored = b->resources[resource_wine()];
    if (stored < 2) {
        num_loads = stored;
    } else {
        num_loads = 2;
    }
    if (num_loads <= 0) {
        return 0;
    }
    b->resources[resource_wine()] -= num_loads;

    // create delivery boys
    int priest_id = f->id;
    int boy1 = figure_supplier_create_delivery_boy(priest_id, priest_id, FIGURE_DELIVERY_BOY);
    if (num_loads > 1) {
        figure_supplier_create_delivery_boy(boy1, priest_id, FIGURE_DELIVERY_BOY);
    }
    return 1;
}

static int take_resource_from_warehouse(figure *f, int warehouse_id, int max_amount)
{
    building *warehouse = building_get(warehouse_id);
    Building warehouse_obj(warehouse);
    const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
    if (!building_matches(warehouse, "warehouse")) {
        return take_resource_from_generic_building(f, warehouse_id);
    }
    int num_loads;
    int stored = building_warehouse_get_available_amount(warehouse_obj, resource);
    if (stored < max_amount) {
        num_loads = stored;
    } else {
        num_loads = max_amount;
    }
    if (num_loads <= 0) {
        return 0;
    }
    building_warehouse_try_remove_resource(warehouse_obj, resource, num_loads);

    // create delivery boys
    if (f->type != FIGURE_LIGHTHOUSE_SUPPLIER) {
        int supplier_id = f->id;
        int boy1 = figure_supplier_create_delivery_boy(supplier_id, supplier_id, FIGURE_DELIVERY_BOY);
        if (num_loads > 1) {
            figure_supplier_create_delivery_boy(boy1, supplier_id, FIGURE_DELIVERY_BOY);
        }
    }
    return 1;
}

static int change_market_supplier_destination(figure *f, int dst_building_id)
{
    figure_route_remove(f);
    f->destination_building_id = dst_building_id;
    building *b_dst = building_get(dst_building_id);
    map_point road = { 0 };
    int has_road_access = 0;
    if (building_matches(b_dst, "warehouse")) {
        has_road_access = map_has_road_access_warehouse(b_dst->x, b_dst->y, &road);
    } else if (building_matches(b_dst, "granary")) {
        has_road_access = map_has_road_access_granary(b_dst->x, b_dst->y, &road);
    }
    if (!has_road_access) {
        return 0;
    }

    f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
    f->destination_x = road.x;
    f->destination_y = road.y;
    return 1;
}

static int is_better_destination(figure *f, resource_type r, resource_storage_info *info)
{
    building *old_dest = building_get(f->destination_building_id);
    Building old_destination(old_dest);
    // if any of these are true, the new building is automatically better
    if (!building_is_active(old_dest)) {
        return 1;
    } else if (building_matches(old_dest, "granary") && old_dest->resources[r] <= 0) {
        return 1;
    } else if (building_matches(old_dest, "warehouse") && building_warehouse_get_amount(old_destination, r) <= 0) {
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

static int recalculate_market_supplier_destination(figure *f)
{
    resource_type item = static_cast<resource_type>(f->collecting_item_id);
    building *market = building_get(f->building_id);
    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };

    Market market_object(market);
    if (!market_object.needed_inventory(info) ||
        !market_object.resource_storages_for_supplier(info, f)) {
        return 0;
    }

    if (f->building_id == info[item].building_id || f->destination_building_id == info[item].building_id) {
        return 1;
    }

    if (info[item].building_id) {
        if (is_better_destination(f, item, &info[item])) {
            return change_market_supplier_destination(f, info[item].building_id);
        } else {
            return 1;
        }
    }
    resource_type fetch_inventory = market_object.fetch_inventory(info);
    if (fetch_inventory == RESOURCE_NONE) {
        return 0;
    }
    market_object.set_fetch_inventory_id(fetch_inventory);
    f->collecting_item_id = fetch_inventory;
    return change_market_supplier_destination(f, info[fetch_inventory].building_id);
}

void figure_supplier_action(figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    f->use_cross_country = 0;
    f->max_roam_length = 800;

    building *b = building_get(f->building_id);
    if (b->state != BUILDING_STATE_IN_USE ||
        (b->figure_id2 != f->id && b->figure_id != f->id && b->figure_id4 != f->id)) {
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
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->wait_ticks = 0;
                f->previous_tile_x = f->x;
                f->previous_tile_y = f->y;
                int id = f->id;
                if (!resource_is_food(static_cast<resource_type>(f->collecting_item_id))) {
                    int max_amount = f->type == FIGURE_LIGHTHOUSE_SUPPLIER ? 1 : 2;
                    if (!take_resource_from_warehouse(f, f->destination_building_id, max_amount)) {
                        f->state = FIGURE_STATE_DEAD;
                    }
                } else {
                    if (!take_food_from_storage(f, f->building_id, f->destination_building_id)) {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                f = figure_get(id);
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                figure_route_remove(f);
            } else if (f->type == FIGURE_MARKET_SUPPLIER && f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->wait_ticks = 0;
                if (!recalculate_market_supplier_destination(f)) {
                    f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                    f->collecting_item_id = RESOURCE_NONE;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                    figure_route_remove(f);
                }
            }
            break;
        case FIGURE_ACTION_146_SUPPLIER_RETURNING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                if (f->direction == DIR_FIGURE_AT_DESTINATION && f->type == FIGURE_LIGHTHOUSE_SUPPLIER) {
                    building_get(f->building_id)->resources[resource_timber()] += 100;
                }
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            }
            break;
    }
    if (f->type == FIGURE_MESS_HALL_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                if (f->attack_image_offset < 14) {
                    f->image_id = assets_get_image_id("Walkers", "quartermaster_f_ne_01") + dir * 6;
                } else {
                    f->image_id = assets_get_image_id("Walkers", "quartermaster_f_ne_01") + dir * 6 + ((f->attack_image_offset - 14) / 2);
                }
                break;
            case FIGURE_ACTION_149_CORPSE:
                f->image_id = assets_get_image_id("Walkers", "quartermaster_death_01") +
                    figure_image_corpse_offset(f);
                break;
            default:
                f->image_id = assets_get_image_id("Walkers", "quartermaster_ne_01") +
                    dir * 12 + f->image_offset;
                break;
        }
    } else if (f->type == FIGURE_PRIEST_SUPPLIER) {
        figure_image_update(f, image_group(GROUP_FIGURE_PRIEST));
    } else if (f->type == FIGURE_BARKEEP_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "Barkeep Death 01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "Barkeep NE 01") +
                dir * 12 + f->image_offset;
        }
    } else if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER) {
        const resource_type carried_resource = f->action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING ?
            static_cast<resource_type>(f->collecting_item_id) :
            RESOURCE_NONE;
        f->cart_image_id = carried_resource == RESOURCE_NONE ?
            image_group(GROUP_FIGURE_CARTPUSHER_CART) :
            resource_graphics_cart_marker_for_direction(0);
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = image_group(GROUP_FIGURE_CARTPUSHER) + figure_image_corpse_offset(f) + 96;
            f->cart_image_id = 0;
        } else {
            f->image_id = image_group(GROUP_FIGURE_CARTPUSHER) + dir + 8 * f->image_offset;
        }
        if (f->cart_image_id) {
            if (resource_graphics_cart_marker_is(f->cart_image_id)) {
                f->cart_image_id = resource_graphics_cart_marker_for_direction(dir);
            } else {
                f->cart_image_id += dir;
            }
            figure_image_set_cart_offset(f, dir);
        }
    } else if (f->type == FIGURE_CARAVANSERAI_SUPPLIER) {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_overseer_death_01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_overseer_ne_01") +
                dir * 12 + f->image_offset;
        }
    } else {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "marketbuyer_death_01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "marketbuyer_ne_01") +
                dir * 12 + f->image_offset;
        }
    }
}

void figure_delivery_boy_action(figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;

    figure *leader = figure_get(f->leading_figure_id);
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
            building_get(f->building_id)->resources[f->collecting_item_id] += 100;
            f->state = FIGURE_STATE_DEAD;
        }
    }
    if (leader->is_ghost && !leader->height_adjusted_ticks) {
        f->is_ghost = 1;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->type == FIGURE_MESS_HALL_COLLECTOR) {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "M Hall death 01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "M Hall NE 01") +
                dir * 12 + f->image_offset;
        }
    } else if (f->type == FIGURE_CARAVANSERAI_COLLECTOR) {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_walker_death_01") + figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "caravanserai_walker_ne_01")
                + dir * 12 + f->image_offset;
        }
    } else {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = image_group(GROUP_FIGURE_DELIVERY_BOY) + 96 +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = image_group(GROUP_FIGURE_DELIVERY_BOY) +
                dir + 8 * f->image_offset;
        }
    }
}

void figure_fort_supplier_action(figure *f)
{
    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 12);

    building *b = building_get(f->building_id);
    if (!b || b->state != BUILDING_STATE_IN_USE || !building_matches(b, "mess_hall")) {
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
                f->wait_ticks = game_time_scale_legacy_day_ticks(20);
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
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
                    figure_route_remove(f);
                } else if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
    }

    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->image_id = assets_get_image_id("Walkers", "M Hall death 01") +
            figure_image_corpse_offset(f);
    } else {
        f->image_id = assets_get_image_id("Walkers", "M Hall NE 01") +
            dir * 12 + f->image_offset;
    }
}
