#include "building/industry.h"
#include "building/storage.h"
#include "city/health.h"
#include "game/resource_graphics.h"

#include "cartpusher.h"

#include "building/barracks.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/religion.h"

#include "assets/assets.h"
#include "building/building_type_api.h"
#include "building/granary.h"
#include "building/monument.h"
#include "building/warehouse.h"
#include "city/map.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/road_network.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"

#include <cstring>

#define NON_STORABLE_RESOURCE_CARTPUSHER_MAX_WAIT_TICKS 300
#define VALID_MONUMENT_RECHECK_TICKS 60
#define GRANARY_EMPTY_ALL_CARTLOADS 8
#define WAREHOUSE_EMPTY_ALL_CARTLOADS 4
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int building_matches(const Building &site, const char *attr)
{
    const building_type_registry_impl::BuildingType *definition = site.type;
    return definition && definition->attr() && std::strcmp(definition->attr(), attr) == 0;
}

static building *runtime_record(const Building &site)
{
    return site.id() ? building_get(site.id()) : nullptr;
}

static Building building_from_runtime_id(unsigned int id)
{
    return Building(id ? building_get(id) : nullptr);
}

static resource_type output_resource(const Building &site)
{
    const building *record = runtime_record(site);
    return record ? static_cast<resource_type>(record->output_resource_id) : RESOURCE_NONE;
}

static int is_warehouse_storage(const Building &site)
{
    return building_matches(site, "warehouse") || building_matches(site, "warehouse_space");
}

static int is_runtime_storage(const Building &site)
{
    return is_warehouse_storage(site) || building_matches(site, "granary") ||
        building_matches(site, "armoury");
}

static int is_close_delivery_food_source(const Building &site)
{
    return (site.type && site.type->is_farm()) ||
        building_matches(site, "wharf");
}

static int is_ceres_speed_food_source(const Building &site)
{
    return site.type && site.type->is_farm();
}

static int cartpusher_carries_food(Figure *f)
{
    return resource_is_food(static_cast<resource_type>(f->resource_id));
}

static void set_cart_graphic(Figure *f, int always_carries_resource)
{
    const resource_type resource = static_cast<resource_type>(f->resource_id);
    const int carried = resource == RESOURCE_NONE ? 0 :
        (f->loads_sold_or_carrying == 0 ? always_carries_resource : f->loads_sold_or_carrying);
    f->cart_image_id = carried > 0 ?
        resource_graphics_cart_marker_for_direction(0) :
        image_group(GROUP_FIGURE_CARTPUSHER_CART);
}

static void cartpusher_return_to_source(Figure *f, const Building &origin)
{
    // some fallbacks for cartpushers
    if (is_runtime_storage(origin)) {
        if (f->loads_sold_or_carrying > 0) {
            // If the cartpusher is carrying resources, it should return with correct action state
            if (building_matches(origin, "granary")) {
                f->action_state = FIGURE_ACTION_56_WAREHOUSEMAN_RETURNING_WITH_FOOD;
            } else {
                f->action_state = FIGURE_ACTION_59_WAREHOUSEMAN_RETURNING_WITH_RESOURCE;
            }
        } else {
            f->action_state = FIGURE_ACTION_53_WAREHOUSEMAN_RETURNING_EMPTY;
        }

    } else { // non-warehouseman
        f->action_state = FIGURE_ACTION_27_CARTPUSHER_RETURNING;
    }

    f->wait_ticks = 0;
    f->last_destination_id = f->destination_building.id(); //record last destination
    f->destination_building = f->building;
    f->destination_x = f->source_x;
    f->destination_y = f->source_y;
    set_cart_graphic(f, 0);
}

static int should_change_destination(
    const Figure *f, const Building &origin, const Building &new_destination, int x_dst, int y_dst)
{
    if (!f->destination_building.id()) {
        return 1;
    }
    Building current_destination_obj = f->destination_building;
    // Same building
    if (current_destination_obj.id() == new_destination.id() && f->destination_x == x_dst &&
        f->destination_y == y_dst && current_destination_obj.type == new_destination.type) {
        return 0;
    }
    switch (f->action_state) {
        case FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE:
        case FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY:
        {
            if (!building_storage_accepts_storage(current_destination_obj, static_cast<resource_type>(f->resource_id), 0)) {
                return 1;
            }
            break;
        }
        case FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE:
            if (!is_warehouse_storage(current_destination_obj) && !building_matches(current_destination_obj, "granary")) {
                return current_destination_obj.type == new_destination.type &&
                    new_destination.resource_amount(static_cast<resource_type>(f->resource_id)) <
                        current_destination_obj.resource_amount(static_cast<resource_type>(f->resource_id));
            }
            if (building_matches(current_destination_obj, "granary") ||
                building_matches(current_destination_obj, "warehouse")) {
                if (!building_storage_accepts_storage(current_destination_obj, static_cast<resource_type>(f->resource_id), 0)) {
                    return 1;
                }
            }
            break;
        case FIGURE_ACTION_54_WAREHOUSEMAN_GETTING_FOOD:
            if (!building_granary_amount_can_get_from(
                current_destination_obj, origin, static_cast<resource_type>(f->resource_id))) {
                return 1;
            }
            break;
        case FIGURE_ACTION_57_WAREHOUSEMAN_GETTING_RESOURCE:
            if (building_warehouse_amount_can_get_from(
                current_destination_obj, static_cast<resource_type>(f->collecting_item_id)) == 0) {
                return 1;
            }
            break;
        default:
            return 0;
    }
    int distance_current = calc_maximum_distance(current_destination_obj.x(), current_destination_obj.y(), f->x, f->y);
    int distance_new = calc_maximum_distance(x_dst, y_dst, f->x, f->y);
    return distance_current / 2 > distance_new;
}

static void validate_action_for_old_destination(Figure *f, const Building &destination)
{
    if (f->type == FIGURE_CART_PUSHER) {
        switch (f->action_state) {
            case FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE:
            case FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY:
            case FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP:
                if (building_is_workshop(destination.type ? destination.type->type() : BUILDING_NONE)) {
                    f->action_state = FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP;
                } else if (building_matches(destination, "granary")) {
                    f->action_state = FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY;
                } else {
                    f->action_state = FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE;
                }
                break;
            default:
                break;
        }
    }
}

static void set_destination(Figure *f, int action, const Building &origin, const Building &destination, int x_dst, int y_dst)
{
    f->action_state = action;
    f->wait_ticks = 0;
    if (should_change_destination(f, origin, destination, x_dst, y_dst)) {
        figure_route_remove(f);
        f->destination_building = destination;
        f->destination_x = x_dst;
        f->destination_y = y_dst;
    } else {
        validate_action_for_old_destination(f, destination);
    }
}

static void set_destination_from_runtime_id(
    Figure *f, int action, const Building &origin, unsigned int destination_id, int x_dst, int y_dst)
{
    set_destination(f, action, origin, building_from_runtime_id(destination_id), x_dst, y_dst);
}

static void determine_cartpusher_destination(Figure *f, Building &source, int road_network_id)
{
    map_point dst = { 0, 0 };
    int understaffed_storages = 0;

    int dst_building_id = 0;
    const resource_type source_output = output_resource(source);
    int is_storable = resource_is_storable(source_output);
    // priority 1: warehouse if resource is on stockpile
    if (is_storable && (city_resource_is_stockpiled(source_output) || source.industry_is_stockpiling())) {
        dst_building_id = building_warehouse_for_storing(0, f->x, f->y,
                            source_output, road_network_id, &understaffed_storages, &dst);
    }
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // priority 2: accepting granary for food
    dst_building_id = building_granary_for_storing(f->x, f->y,
        source_output, road_network_id, 0, &understaffed_storages, &dst);
    if (dst_building_id && config_get(CONFIG_GP_CH_FARMS_DELIVER_CLOSE)) {
        int dist = 0;
        Building dst_building = building_from_runtime_id(dst_building_id);
        if (is_close_delivery_food_source(source)) {
            dist = source.max_distance_to(dst_building);
        }
        if (dist >= 64) {
            dst_building_id = 0;
        }
    }
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // priority 3: workshop for raw material
    dst_building_id = building_get_workshop_for_raw_material_with_room(f->x, f->y,
        source_output, road_network_id, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    if (!is_storable) {
        // Special priority for non-storable resource: monument under construction
        dst_building_id = building_monument_get_monument(f->x, f->y, source_output, road_network_id, &dst);
        if (dst_building_id) {
            f->wait_ticks = game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS);
            set_destination_from_runtime_id(f, FIGURE_ACTION_246_CARTPUSHER_DELIVERING_TO_MONUMENT,
                source, dst_building_id, dst.x, dst.y);
            building_monument_add_delivery(dst_building_id, f->id(), source_output, 1);
        } else {
            f->action_state = FIGURE_ACTION_245_CARTPUSHER_WAITING_FOR_DESTINATION;
        }
        return;
    }
    // priority 4: warehouse
    dst_building_id = building_warehouse_for_storing(0, f->x, f->y,
        source_output, road_network_id, &understaffed_storages, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // priority 5: granary forced when on stockpile
    dst_building_id = building_granary_for_storing(f->x, f->y,
        source_output, road_network_id, 1, &understaffed_storages, &dst);
    if (dst_building_id && config_get(CONFIG_GP_CH_FARMS_DELIVER_CLOSE)) {
        int dist = 0;
        Building dst_building = building_from_runtime_id(dst_building_id);
        if (is_close_delivery_food_source(source)) {
            dist = source.max_distance_to(dst_building);
        }
        if (dist >= 64) {
            dst_building_id = 0;
        }
    }
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // no one will accept
    f->wait_ticks = 0;
    // set cartpusher text
    f->min_max_seen = understaffed_storages ? 2 : 1;
}

static void determine_cartpusher_destination_food(Figure *f, Building &source, int road_network_id)
{
    map_point dst;
    const resource_type source_output = output_resource(source);
    // priority 1: accepting granary for food
    int dst_building_id = building_granary_for_storing(f->x, f->y,
        source_output, road_network_id, 0, 0, &dst);
    if (dst_building_id && config_get(CONFIG_GP_CH_FARMS_DELIVER_CLOSE)) {
        int dist = 0;
        Building dst_building = building_from_runtime_id(dst_building_id);
        if (is_close_delivery_food_source(source)) {
            dist = source.max_distance_to(dst_building);
        }
        if (dist >= 64) {
            dst_building_id = 0;
        }
    }
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // priority 2: warehouse
    dst_building_id = building_warehouse_for_storing(0, f->x, f->y,
        source_output, road_network_id, 0, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // priority 3: granary
    dst_building_id = building_granary_for_storing(f->x, f->y, source_output, road_network_id, 1, 0, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY,
            source, dst_building_id, dst.x, dst.y);
        return;
    }
    // no one will accept, stand idle
    f->wait_ticks = 0;
}

static void update_image(Figure *f, const Building &source)
{
    int dir = figure_image_normalize_direction(
        f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (building_matches(source, "armoury")) {
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers\\barracks_worker_death_01", "barracks_worker_death_01") + figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers\\barracks_worker_ne_01", "barracks_worker_ne_01") + dir * 12 + f->image_offset;
        }
    } else {
        int base_group = f->type == FIGURE_CART_PUSHER ? GROUP_FIGURE_CARTPUSHER : GROUP_FIGURE_MIGRANT;

        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = image_group(base_group) + figure_image_corpse_offset(f) + 96;
            f->cart_image_id = 0;
        } else {
            f->image_id = image_group(base_group) + dir + 8 * f->image_offset;
        }
    }
    if (f->cart_image_id) {
        if (resource_graphics_cart_marker_is(f->cart_image_id)) {
            f->cart_image_id = resource_graphics_cart_marker_for_direction(dir);
        } else {
            f->cart_image_id += dir;
        }
        figure_image_set_cart_offset(f, dir);
        if (f->loads_sold_or_carrying >= 8 && cartpusher_carries_food(f)) {
            f->y_offset_cart -= 40;
        }
    }
}

static int cartpusher_percentage_speed(const Building &source)
{
    // Ceres grand temple base bonus
    if (is_ceres_speed_food_source(source)) {
        if (building_monument_working_grand_temple_for_god(GOD_CERES)) {
            return 50;
        }
    }
    return 0;
}

static void reroute_cartpusher(Figure *f)
{
    figure_route_remove(f);
    if (!map_routing_citizen_is_passable_terrain(f->grid_offset)) {
        f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
    }
    f->wait_ticks = 0;
}

void figure_cartpusher_action(Figure *f)
{
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    Building source = f->building;
    const building *source_record = runtime_record(source);
    int percentage_speed = cartpusher_percentage_speed(source);

    // Assume we're always on the source road network
    // Fixes walkers stopping when deciding to recalculate best destination when on different network
    int road_network_id = source.road_network_id();

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_20_CARTPUSHER_INITIAL:
            set_cart_graphic(f, 1);
            if (!map_routing_citizen_is_passable(f->grid_offset)) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!source.is_in_use() || !source_record || source_record->figure_id != f->id()) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!road_network_id) {
                f->state = FIGURE_STATE_DEAD;
            }
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(30) && road_network_id) {
                determine_cartpusher_destination(f, source, road_network_id);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_245_CARTPUSHER_WAITING_FOR_DESTINATION:
            set_cart_graphic(f, 1);
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(NON_STORABLE_RESOURCE_CARTPUSHER_MAX_WAIT_TICKS)) {
                f->state = FIGURE_STATE_DEAD;
            } else if ((f->wait_ticks % game_time_scale_legacy_day_ticks(NON_STORABLE_RESOURCE_CARTPUSHER_MAX_WAIT_TICKS / 10) == 0)) {
                determine_cartpusher_destination(f, source, road_network_id);
            }
            break;
        case FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE:
            set_cart_graphic(f, 1);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_24_CARTPUSHER_AT_WAREHOUSE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                reroute_cartpusher(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                figure_cartpusher_action(f);
                return;
            }
            if (!f->destination_building.is_in_use()) {
                f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                f->wait_ticks = 0;
            }
            break;
        case FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY:
            set_cart_graphic(f, 1);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_25_CARTPUSHER_AT_GRANARY;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                reroute_cartpusher(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                f->wait_ticks = 0;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                figure_cartpusher_action(f);
                return;
            }
            if (!f->destination_building.is_in_use()) {
                f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                f->wait_ticks = 0;
            }
            break;
        case FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP:
            set_cart_graphic(f, 1);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_26_CARTPUSHER_AT_WORKSHOP;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                reroute_cartpusher(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_246_CARTPUSHER_DELIVERING_TO_MONUMENT:
            if (f->wait_ticks++ >= game_time_scale_legacy_day_ticks(VALID_MONUMENT_RECHECK_TICKS)) {
                if (!building_monument_has_delivery_for_worker(f->id())) {
                    f->state = FIGURE_STATE_DEAD;
                    break;
                }
                f->wait_ticks = 0;
            }
            set_cart_graphic(f, 1);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_247_CARTPUSHER_AT_MONUMENT;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                reroute_cartpusher(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_24_CARTPUSHER_AT_WAREHOUSE:
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                Building destination = f->destination_building;
                int delivered = building_warehouse_try_add_resource(
                    destination, static_cast<resource_type>(f->resource_id), f->loads_sold_or_carrying, 1);
                if (delivered) {
                    f->loads_sold_or_carrying -= delivered; //sure hope it equals 0
                    city_health_dispatch_sickness(f);
                    cartpusher_return_to_source(f, source);
                } else {
                    if (should_change_destination(f, source, destination, f->destination_x, f->destination_y)) {
                        determine_cartpusher_destination(f, source, road_network_id);
                        break;
                    }
                    figure_route_remove(f);
                    f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                    f->wait_ticks = 0;
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_25_CARTPUSHER_AT_GRANARY:
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(5)) {
                Building destination = f->destination_building;
                int delivered = building_granary_try_add_resource(destination,
                    static_cast<resource_type>(f->resource_id), f->loads_sold_or_carrying, 1, 1);
                if (delivered) {
                    f->loads_sold_or_carrying -= delivered; //sure hope it equals 0
                    city_health_dispatch_sickness(f);
                    cartpusher_return_to_source(f, source);
                } else {
                    if (!f->loads_sold_or_carrying) {
                        cartpusher_return_to_source(f, source);
                        break;
                    }
                    if (should_change_destination(f, source, destination, f->destination_x, f->destination_y)) {
                        determine_cartpusher_destination(f, source, road_network_id);
                        break;
                    }
                    f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                    determine_cartpusher_destination_food(f, source, road_network_id);
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_26_CARTPUSHER_AT_WORKSHOP:
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(5)) {
                Building destination = f->destination_building;
                building_workshop_add_raw_material(runtime_record(destination), f->resource_id);
                cartpusher_return_to_source(f, source);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_247_CARTPUSHER_AT_MONUMENT:
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(5)) {
                if (!building_monument_has_delivery_for_worker(f->id())) {
                    f->state = FIGURE_STATE_DEAD;
                    break;
                }
                Building destination = f->destination_building;
                building_monument_deliver_resource(runtime_record(destination), f->resource_id);
                building_monument_remove_delivery(f->id());
                cartpusher_return_to_source(f, source);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_27_CARTPUSHER_RETURNING:
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_234_CARTPUSHER_GOING_TO_ROME_CREATED:
        {
            set_cart_graphic(f, 0);
            const map_tile *entry = city_map_entry_point();
            f->action_state = FIGURE_ACTION_235_CARTPUSHER_GOING_TO_ROME;
            f->destination_x = entry->x;
            f->destination_y = entry->y;
            break;
        }
        case FIGURE_ACTION_235_CARTPUSHER_GOING_TO_ROME:
            set_cart_graphic(f, 0);
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            }
    }
    update_image(f, source);
}

static void determine_granaryman_destination(Figure *f, Building &granary, int road_network_id, int remove_resources)
{
    f->is_ghost = 0;
    map_point dst;
    int dst_building_id;
    int loads_to_remove = building_storage_get_empty_all(granary.id()) ? GRANARY_EMPTY_ALL_CARTLOADS : 1;
    if (!f->resource_id) {
        // getting granaryman
        dst_building_id = building_granary_for_getting(granary, &dst, 4);
        if (!dst_building_id) {
            dst_building_id = building_granary_for_getting(granary, &dst, 1);
        }
        if (dst_building_id) {
            f->loads_sold_or_carrying = 0;
            set_destination_from_runtime_id(f, FIGURE_ACTION_54_WAREHOUSEMAN_GETTING_FOOD,
                granary, dst_building_id, dst.x, dst.y);
            if (config_get(CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD)) {
                f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            }
        } else {
            f->state = FIGURE_STATE_DEAD;
            f->is_ghost = 1;
        }
        return;
    }
    // delivering resource
    // priority 1: another granary
    dst_building_id = building_granary_for_storing(
        f->x, f->y, static_cast<resource_type>(f->resource_id), road_network_id, 0, 0, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            granary, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            f->loads_sold_or_carrying =
                building_granary_try_remove_resource(granary, static_cast<resource_type>(f->resource_id), loads_to_remove);
            if (f->loads_sold_or_carrying == 0) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1; // no resources left, stand idle
            }
        }
        return;
    }
    // priority 2: warehouse
    dst_building_id = building_warehouse_for_storing(
        0, f->x, f->y, static_cast<resource_type>(f->resource_id), road_network_id, 0, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            granary, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            f->loads_sold_or_carrying =
                building_granary_try_remove_resource(granary, static_cast<resource_type>(f->resource_id), loads_to_remove);
            if (f->loads_sold_or_carrying == 0) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1; // no resources left, stand idle
            }
        }
        return;
    }
    // priority 3: granary even though resource is on stockpile
    dst_building_id = building_granary_for_storing(
        f->x, f->y, static_cast<resource_type>(f->resource_id), road_network_id, 1, 0, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            granary, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            f->loads_sold_or_carrying =
                building_granary_try_remove_resource(granary, static_cast<resource_type>(f->resource_id), loads_to_remove);
        }
        if (f->loads_sold_or_carrying == 0) {
            f->state = FIGURE_STATE_DEAD;
            f->is_ghost = 1; // no resources left, stand idle
        }
        return;
    }
    // no one will accept, stand idle
            f->wait_ticks = game_time_scale_legacy_day_ticks(2);
}

static void determine_armoury_supplier_destination(Figure *f, Building &armoury)
{
    f->is_ghost = 0;

    map_point dst;
    int dst_building_id;

    // Has weapons, deliver to barracks
    if (f->resource_id) {
        dst_building_id =
            Barracks::for_weapon(armoury.x(), armoury.y(), resource_weapons(),
                armoury.road_network_id(), &dst).id();
        if (dst_building_id) {
            set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
                armoury, dst_building_id, dst.x, dst.y);
            return;
        }
    } else {
        // Go grab weapons
        dst_building_id = building_warehouse_with_resource(armoury.x(), armoury.y(), resource_weapons(),
            armoury.road_network_id(), 0, &dst, BUILDING_STORAGE_PERMISSION_ARMOURY);
        if (dst_building_id) {
            set_destination_from_runtime_id(f, FIGURE_ACTION_248_ARMOURY_SUPPLIER_GETTING_WEAPONS,
                armoury, dst_building_id, dst.x, dst.y);
            return;
        }
    }

    // no one will accept, stand idle
    f->wait_ticks = game_time_scale_legacy_day_ticks(5);
}

static int remove_resource_from_warehouse(Figure *f, Building &warehouse, int remove_quantity)
{
    int loads_taken = 0;
    if (f->state != FIGURE_STATE_DEAD) {
        resource_type resource = static_cast<resource_type>(f->resource_id);
        remove_quantity = MIN(remove_quantity, building_warehouse_get_amount(
            warehouse, resource));
        loads_taken = building_warehouse_try_remove_resource(warehouse, resource, remove_quantity);
        f->loads_sold_or_carrying = loads_taken;
    }
    return loads_taken;
}

static void determine_warehouseman_destination(Figure *f, Building &warehouse, int road_network_id, int remove_resources)
{
    f->is_ghost = 0;
    map_point dst;
    unsigned int dst_building_id;
    if (!f->resource_id) {
        // getting warehouseman
        dst_building_id = building_warehouse_for_getting(
            warehouse, static_cast<resource_type>(f->collecting_item_id), &dst);
        if (dst_building_id) {
            f->loads_sold_or_carrying = 0;
            set_destination_from_runtime_id(f, FIGURE_ACTION_57_WAREHOUSEMAN_GETTING_RESOURCE,
                warehouse, dst_building_id, dst.x, dst.y);
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
        } else {
            f->state = FIGURE_STATE_DEAD;
            f->is_ghost = 1;
        }
        return;
    }
    // delivering resource
    // priority 1: weapons to barracks
    dst_building_id = Barracks::for_weapon(f->x, f->y, static_cast<resource_type>(f->resource_id), road_network_id, &dst)
        .id();
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            warehouse, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            if (!remove_resource_from_warehouse(f, warehouse, 1)) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
        }
        return;
    }
    // priority 2: raw materials to workshop
    dst_building_id = building_get_workshop_for_raw_material_with_room(f->x, f->y, f->resource_id,
        road_network_id, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            warehouse, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            if (!remove_resource_from_warehouse(f, warehouse, 1)) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
        }
        return;
    }
    // priority 3: food to granary
    dst_building_id = building_granary_for_storing(f->x, f->y, static_cast<resource_type>(f->resource_id),
        road_network_id, 0, 0, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            warehouse, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            if (!remove_resource_from_warehouse(f, warehouse, 1)) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
        }
        return;
    }
    // priority 4: food to getting granary
    dst_building_id = building_getting_granary_for_storing(
        f->x, f->y, static_cast<resource_type>(f->resource_id), road_network_id, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            warehouse, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            if (!remove_resource_from_warehouse(f, warehouse, 1)) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
        }
        return;
    }

    // priority 5: another warehouse to empty this one
    if (building_storage_get(warehouse.storage_id())->empty_all) {
        dst_building_id = building_warehouse_for_storing(
            warehouse.id(), f->x, f->y, static_cast<resource_type>(f->resource_id), -1, 0, &dst);

        // deliver to another warehouse because this one is being emptied
        if (dst_building_id) {
            if (dst_building_id == warehouse.id()) {
                f->state = FIGURE_STATE_DEAD;
            } else {
                set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
                    warehouse, dst_building_id, dst.x, dst.y);
                f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
                if (remove_resources) {
                    if (!remove_resource_from_warehouse(f, warehouse, WAREHOUSE_EMPTY_ALL_CARTLOADS)) {
                        f->state = FIGURE_STATE_DEAD;
                        f->is_ghost = 1;
                    }
                }
            }
            return;
        }
    }
    // priority 6: raw material to well-stocked workshop
    dst_building_id = building_get_workshop_for_raw_material(f->x, f->y, f->resource_id, road_network_id, &dst);
    if (dst_building_id) {
        set_destination_from_runtime_id(f, FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE,
            warehouse, dst_building_id, dst.x, dst.y);
        if (remove_resources) {
            if (!remove_resource_from_warehouse(f, warehouse, 1)) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
        }
        return;
    }
    // no one will accept, stand idle
    f->wait_ticks = game_time_scale_legacy_day_ticks(2);
}

static void warehouseman_initial_action(Figure *f, Building &source, int road_network_id, int remove_resources)
{
    if (!road_network_id &&
        (f->terrain_usage == TERRAIN_USAGE_ROADS_HIGHWAY || f->terrain_usage == TERRAIN_USAGE_ROADS)) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }

    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;

    f->is_ghost = 1;
    f->wait_ticks++;
    if (f->wait_ticks > game_time_scale_legacy_day_ticks(2)) {
        f->wait_ticks = 0;
        if (building_matches(source, "granary")) {
            determine_granaryman_destination(f, source, road_network_id, remove_resources);
        } else if (building_matches(source, "armoury")) {
            determine_armoury_supplier_destination(f, source);
        } else {
            determine_warehouseman_destination(f, source, road_network_id, remove_resources);
        }
        set_cart_graphic(f, 1);
    }
    f->image_offset = 0;
}

void figure_warehouseman_action(Figure *f)
{
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    Building source = f->building;
    const building *source_record = runtime_record(source);
    int percentage_speed = cartpusher_percentage_speed(source);

    if (!source.is_in_use() || !source_record ||
        (source_record->figure_id != f->id() && source_record->figure_id4 != f->id())) {
        f->state = FIGURE_STATE_DEAD;
    }

    // Assume we're always on the source road network
    // Fixes walkers stopping when deciding to recalculate best destination when on different network
    int road_network_id = source.road_network_id();

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_50_WAREHOUSEMAN_CREATED:
        {
            f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
            warehouseman_initial_action(f, source, road_network_id, 1);
            break;
        }
        case FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE:
            set_cart_graphic(f, 1);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_52_WAREHOUSEMAN_AT_DELIVERY_BUILDING;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->action_state = FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET;
                figure_warehouseman_action(f);
                return;
            }
            break;
        case FIGURE_ACTION_52_WAREHOUSEMAN_AT_DELIVERY_BUILDING:
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(4)) {
                Building destination = f->destination_building;
                building *destination_record = runtime_record(destination);
                int delivered = 1;
                if (building_matches(destination, "granary") || is_warehouse_storage(destination)) {
                    delivered = building_storage_try_add_resource(destination, f->resource_id, f->loads_sold_or_carrying, 0);
                    if (delivered) {
                        city_health_dispatch_sickness(f);
                        f->loads_sold_or_carrying -= delivered;
                    }
                } else if (building_matches(destination, "barracks") ||
                    (destination.type &&
                        destination.type->is_temple(GOD_MARS, building_type_registry_impl::ReligionTier::Grand))) {
                    destination.add_resource(resource_weapons(), 1);
                    f->loads_sold_or_carrying = 0; // should change to be dependant on the above call in the future
                } else { // workshop
                    building_workshop_add_raw_material(destination_record, f->resource_id);
                    f->loads_sold_or_carrying = 0; // should change to be dependant on the above call in the future
                }
                if (delivered) {
                    cartpusher_return_to_source(f, source);
                } else {
                    figure_route_remove(f);
                    f->action_state = FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET;
                    f->wait_ticks = game_time_scale_legacy_day_ticks(2);
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_53_WAREHOUSEMAN_RETURNING_EMPTY:
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            }
            break;
        case FIGURE_ACTION_54_WAREHOUSEMAN_GETTING_FOOD:
            if (config_get(CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD)) {
                f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            }
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_55_WAREHOUSEMAN_AT_GRANARY;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->action_state = FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET;
                figure_warehouseman_action(f);
                return;
            }
            break;
        case FIGURE_ACTION_55_WAREHOUSEMAN_AT_GRANARY:
            if (config_get(CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD)) {
                f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            }
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(4)) {
                int resource;
                Building destination = f->destination_building;
                f->loads_sold_or_carrying = building_granary_remove_for_getting_deliveryman(
                    destination, source, &resource);
                city_health_dispatch_sickness(f);
                f->resource_id = resource;
                f->action_state = FIGURE_ACTION_56_WAREHOUSEMAN_RETURNING_WITH_FOOD;
                f->wait_ticks = 0;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                figure_route_remove(f);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_56_WAREHOUSEMAN_RETURNING_WITH_FOOD:
            if (config_get(CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD)) {
                f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            }
            // update graphic
            set_cart_graphic(f, 0);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                int delivered_loads = building_granary_try_add_resource(
                    source, static_cast<resource_type>(f->resource_id), f->loads_sold_or_carrying, 0, 1);
                f->loads_sold_or_carrying -= delivered_loads;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_57_WAREHOUSEMAN_GETTING_RESOURCE:
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_58_WAREHOUSEMAN_AT_WAREHOUSE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->action_state = FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET;
                figure_warehouseman_action(f);
                return;
            }
            break;
        case FIGURE_ACTION_58_WAREHOUSEMAN_AT_WAREHOUSE:
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(4)) {
                f->loads_sold_or_carrying = 0;
                city_health_dispatch_sickness(f);
                Building warehouse = f->destination_building;
                resource_type resource = static_cast<resource_type>(f->collecting_item_id);
                while (f->loads_sold_or_carrying < 4 && building_warehouse_try_remove_resource(
                    warehouse, resource, 1)) {
                    f->loads_sold_or_carrying++;
                }
                f->resource_id = f->collecting_item_id;
                f->action_state = FIGURE_ACTION_59_WAREHOUSEMAN_RETURNING_WITH_RESOURCE;
                f->wait_ticks = 0;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                figure_route_remove(f);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_59_WAREHOUSEMAN_RETURNING_WITH_RESOURCE:
            f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            set_cart_graphic(f, 0);
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                building_warehouse_try_add_resource(source,
                    static_cast<resource_type>(f->resource_id), f->loads_sold_or_carrying, 1);
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_248_ARMOURY_SUPPLIER_GETTING_WEAPONS:
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty
            figure_movement_move_ticks_with_percentage(f, 1, percentage_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_249_ARMOURY_SUPPLIER_AT_WAREHOUSE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
                f->action_state = FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET;
                figure_warehouseman_action(f);
                return;
            }
            break;
        case FIGURE_ACTION_249_ARMOURY_SUPPLIER_AT_WAREHOUSE:
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(4)) {
                f->loads_sold_or_carrying = 0;
                city_health_dispatch_sickness(f);
                Building warehouse = f->destination_building;
                if (building_warehouse_try_remove_resource(warehouse,
                    static_cast<resource_type>(f->collecting_item_id), 1) == 1) {
                    f->loads_sold_or_carrying++;
                    f->resource_id = f->collecting_item_id;
                    f->last_destination_id = f->destination_building.id();
                    f->destination_building = Building(nullptr);
                    figure_route_remove(f);
                }
                warehouseman_initial_action(f, source, road_network_id, 0);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET:
            warehouseman_initial_action(f, source, road_network_id, 0);
            break;
    }
    update_image(f, source);
}
