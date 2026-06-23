#include "building/storage.h"
#include "city/health.h"
#include "city/trade.h"
#include "empire/empire.h"
#include "game/resource_graphics.h"
#include "map/road_access.h"

#include "docker.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/market.h"

#include "building/building_record.h"
#include "building/building_type_api.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "figuretype/trader.h"
#include "game/resource.h"
#include "game/time.h"

#include <cstring>

#define INFINITE 10000

static const building_type_registry_impl::BuildingType *definition_for_building(building *b)
{
    return b ? Building(b).type : nullptr;
}

static int building_is_warehouse(building *b)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_building(b);
    return definition && definition->is_warehouse();
}

static int building_is_granary(building *b)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_building(b);
    return definition && definition->is_granary();
}

static Figure *valid_trade_ship_for_dock(building *dock)
{
    if (!dock || !dock->data.dock.trade_ship_id) {
        return nullptr;
    }
    Figure *ship = Figure::get(dock->data.dock.trade_ship_id);
    if (!ship || ship->state != FIGURE_STATE_ALIVE || ship->type != FIGURE_TRADE_SHIP) {
        dock->data.dock.trade_ship_id = 0;
        return nullptr;
    }
    return ship;
}

static int figure_destination_building_is_in_use(Figure *f)
{
    building *destination = building_get(f->destination_building.id());
    return destination && destination->state == BUILDING_STATE_IN_USE;
}

static building *first_warehouse(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (building_is_warehouse(b)) {
            return b;
        }
    }
    return nullptr;
}

static building *first_granary(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (building_is_granary(b)) {
            return b;
        }
    }
    return nullptr;
}

static int try_import_resource(int building_id, resource_type resource, int city_id, int quantity)
{
    building *b = building_get(building_id);
    Building storage(b);
    if (!building_is_warehouse(b) &&
        !(resource_is_food(resource) && building_is_granary(b))) {
        return 0;
    }

    if (building_storage_get_state(storage, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0;
    }

    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_DOCK, storage)) {
        return 0;
    }

    int route_id = empire_city_get_route_id(city_id);
    int result = 0;
    if (building_is_granary(b)) {
        result = building_granary_add_import(storage, resource, 1, 0);
        if (result) {
            trade_route_increase_traded(route_id, resource, 0);
        }
    } else if (building_is_warehouse(b)) {
        result = building_warehouse_add_import(storage, resource, quantity, 0);
        if (result) {
            trade_route_increase_traded(route_id, resource, 0);
        }
    }
    return result;
}

static int try_export_resource(int building_id, resource_type resource, int city_id)
{
    building *b = building_get(building_id);
    Building storage(b);
    if (!building_is_warehouse(b) && !building_is_granary(b)) {
        return 0;
    }

    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_DOCK, storage)) {
        return 0;
    }
    int result = 0;
    if (building_is_granary(b)) {
        result = building_granary_remove_export(storage, resource, 1, 0);
        if (result) {
            trade_route_increase_traded(empire_city_get_route_id(city_id), resource, 1);
        }
    } else if (building_is_warehouse(b)) {
        result = building_warehouse_remove_export(storage, resource, 1, 0);
        if (result) {
            trade_route_increase_traded(empire_city_get_route_id(city_id), resource, 1);
        }
    }
    return result;
}

static int store_destination_map_point(int building_id, map_point *dst)
{
    if (!building_id) {
        return 0;
    }
    building *b = building_get(building_id);
    if (!b) {
        return 0;
    }
    if (building_is_granary(b)) {
        // go to center of granary
        map_point_store_result(b->x + 1, b->y + 1, dst);
    } else if (b->has_road_access == 1) {
        map_point_store_result(b->x, b->y, dst);
    } else if (!map_has_road_access_warehouse(b->x, b->y, dst)) {
        return 0;
    }
    return 1;
}

static int is_invalid_destination(building *b, building *dock)
{
    if (!b || !dock) {
        return 1;
    }
    Building storage(b);
    return b->state != BUILDING_STATE_IN_USE ||
        !b->has_road_access || b->distance_from_entry <= 0 ||
        b->road_network_id != dock->road_network_id ||
        !building_storage_get_permission(BUILDING_STORAGE_PERMISSION_DOCK, storage);
}

static int get_distance_penalty_imports(building *b, resource_type resource)
{
    Building storage(b);
    unsigned char max_accepted = building_warehouse_maximum_receptible_amount(storage, resource);

    int penalty = 32 - 2 * max_accepted;
    return penalty;
}

static int get_distance_penalty_exports(building *b, resource_type resource)
{
    Building storage(b);
    unsigned char currently_stored = building_warehouse_get_available_amount(storage, resource);

    int penalty = 32 - 2 * currently_stored;
    return penalty;
}

static unsigned int get_closest_import_storage_for_resource(int x, int y, building *dock,
    resource_type resource, map_point *dst)
{
    int min_distance = INFINITE;
    int min_building_id = 0;
    for (building *b = first_warehouse(); b; b = b->next_of_type) {
        Building storage(b);

        if (is_invalid_destination(b, dock) ||
            !building_warehouse_maximum_receptible_amount(storage, resource)) {
            continue;
        }
        int distance_penalty = get_distance_penalty_imports(b, resource);
        if (distance_penalty == 32) {
            continue;
        }
        int distance = calc_maximum_distance(b->x, b->y, x, y);
        // prefer emptier warehouse
        distance += distance_penalty;
        if (distance < min_distance) {
            min_distance = distance;
            min_building_id = b->id;
        }
    }
    if (resource_is_food(resource)) {
        for (building *b = first_granary(); b; b = b->next_of_type) {
            Building storage(b);
            if (is_invalid_destination(b, dock) ||
                !building_granary_maximum_receptible_amount(storage, resource)) {
                continue;
            }
            // always prefer granary
            int distance = calc_maximum_distance(b->x, b->y, x, y);
            if (distance < min_distance) {
                min_distance = distance;
                min_building_id = b->id;
            }
        }
    }
    if (!store_destination_map_point(min_building_id, dst)) {
        return 0;
    }
    return min_building_id;
}

static unsigned int get_closest_building_for_import(int x, int y, int city_id, building *dock,
    map_point *dst, resource_type *import_resource)
{
    resource_type resource = *import_resource;
    Building dock_obj(dock);
    if (resource != RESOURCE_NONE) {
        if (!dock_obj.accepts_good(resource) ||
            !empire_can_import_resource_from_city(city_id, resource)) {
            return 0;
        }
        unsigned int destination_id = get_closest_import_storage_for_resource(x, y, dock, resource, dst);
        if (destination_id) {
            *import_resource = resource;
        }
        return destination_id;
    }

    for (int i = 0; i < resource_loaded_count(); i++) {
        resource = static_cast<resource_type>(city_trade_next_docker_import_resource());
        if (resource == RESOURCE_NONE) {
            return 0;
        }
        if (!dock_obj.accepts_good(resource) ||
            !empire_can_import_resource_from_city(city_id, resource)) {
            continue;
        }
        unsigned int destination_id = get_closest_import_storage_for_resource(x, y, dock, resource, dst);
        if (destination_id) {
            *import_resource = resource;
            return destination_id;
        }
    }
    return 0;
}

static unsigned int get_closest_export_storage_for_resource(int x, int y, building *dock,
    resource_type resource, map_point *dst)
{
    int min_distance = INFINITE;
    int min_building_id = 0;
    for (building *b = first_warehouse(); b; b = b->next_of_type) {
        if (is_invalid_destination(b, dock)) {
            continue;
        }
        int distance_penalty = get_distance_penalty_exports(b, resource);
        if (distance_penalty == 32) {
            continue;
        }
        int distance = calc_maximum_distance(b->x, b->y, x, y);
        // prefer fuller warehouse
        distance += distance_penalty;
        if (distance < min_distance) {
            min_distance = distance;
            min_building_id = b->id;
        }
    }
    if (resource_is_food(resource) && config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
        for (building *b = first_granary(); b; b = b->next_of_type) {
            Building storage(b);
            if (is_invalid_destination(b, dock) ||
                !building_granary_get_amount(storage, resource)) {
                continue;
            }
            int distance = calc_maximum_distance(b->x, b->y, x, y);
            // avoid granaries
            distance += 31;
            if (distance < min_distance) {
                min_distance = distance;
                min_building_id = b->id;
            }
        }
    }
    if (!store_destination_map_point(min_building_id, dst)) {
        return 0;
    }
    return min_building_id;
}

static int get_closest_building_for_export(int x, int y, int city_id, building *dock,
    map_point *dst, resource_type *export_resource)
{
    resource_type resource = *export_resource;
    Building dock_obj(dock);
    if (resource != RESOURCE_NONE) {
        if (!dock_obj.accepts_good(resource) ||
            !empire_can_export_resource_to_city(city_id, resource)) {
            return 0;
        }
        unsigned int destination_id = get_closest_export_storage_for_resource(x, y, dock, resource, dst);
        if (destination_id) {
            *export_resource = resource;
        }
        return destination_id;
    }

    for (int i = 0; i < resource_loaded_count(); i++) {
        resource = static_cast<resource_type>(city_trade_next_docker_export_resource());
        if (resource == RESOURCE_NONE) {
            return 0;
        }
        if (!dock_obj.accepts_good(resource) ||
            !empire_can_export_resource_to_city(city_id, resource)) {
            continue;
        }
        unsigned int destination_id = get_closest_export_storage_for_resource(x, y, dock, resource, dst);
        if (destination_id) {
            *export_resource = resource;
            return destination_id;
        }
    }
    return 0;
}

static int deliver_import_resource(Figure *f, building *dock)
{
    Figure *ship = valid_trade_ship_for_dock(dock);
    if (!ship || ship->action_state != FIGURE_ACTION_112_TRADE_SHIP_MOORED ||
        ship->loads_sold_or_carrying <= 0) {
        return 0;
    }
    map_point tile;
    resource_type resource = f->destination_building.id() ?
        static_cast<resource_type>(f->resource_id) : RESOURCE_NONE;
    unsigned int destination_id = get_closest_building_for_import(f->x, f->y, ship->empire_city_id,
        dock, &tile, &resource);
    if (!destination_id) {
        return 0;
    }
    if (!f->destination_building.id()) {
        ship->loads_sold_or_carrying--;
        f->action_state = FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE;
    } else {
        f->action_state = FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE;
    }
    if (f->destination_building.id() != destination_id) {
        figure_route_remove(f);
    }
    f->destination_building = Building(building_get(destination_id));
    f->wait_ticks = 0;
    f->destination_x = tile.x;
    f->destination_y = tile.y;
    f->resource_id = resource;
    return 1;
}

static int fetch_export_resource(Figure *f, building *dock, int add_to_bought)
{
    Figure *ship = valid_trade_ship_for_dock(dock);
    if (!ship || ship->action_state != FIGURE_ACTION_112_TRADE_SHIP_MOORED ||
        (add_to_bought && ship->trader_amount_bought >= figure_trade_sea_trade_units())) {
        return 0;
    }
    map_point tile;
    resource_type resource = add_to_bought ? RESOURCE_NONE : static_cast<resource_type>(f->resource_id);
    unsigned int destination_id = get_closest_building_for_export(f->x, f->y, ship->empire_city_id,
        dock, &tile, &resource);
    if (!destination_id) {
        return 0;
    }
    if (add_to_bought) {
        ship->trader_amount_bought++;
    }
    if (f->destination_building.id() != destination_id) {
        figure_route_remove(f);
    }
    f->destination_building = Building(building_get(destination_id));
    f->action_state = FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE;
    f->wait_ticks = 0;
    f->destination_x = tile.x;
    f->destination_y = tile.y;
    f->resource_id = resource;
    return 1;
}

static void set_cart_graphic(Figure *f)
{
    f->cart_image_id = f->resource_id != RESOURCE_NONE ?
        resource_graphics_cart_marker_for_direction(0) :
        image_group(GROUP_FIGURE_CARTPUSHER_CART);
}

static void set_docker_as_idle(Figure *f)
{
    f->action_state = FIGURE_ACTION_132_DOCKER_IDLING;
    f->resource_id = RESOURCE_NONE;
    f->destination_building = Building(nullptr);
    f->wait_ticks = 0;
    f->loads_sold_or_carrying = 0;
}

void figure_docker_action(Figure *f)
{
    building *b = building_get(f->building.id());

    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    if (!b || b->state != BUILDING_STATE_IN_USE) {
        f->state = FIGURE_STATE_DEAD;
        f->destination_building = Building(nullptr);
        f->image_id = 0;
        return;
    }
    Building dock(b);
    const auto *definition = dock.type;
    if (!definition || std::strcmp(definition->attr(), "dock") != 0) {
        f->state = FIGURE_STATE_DEAD;
        f->destination_building = Building(nullptr);
        f->image_id = 0;
        return;
    }
    if (b->data.dock.num_ships) {
        b->data.dock.num_ships--;
    }
    if (b->data.dock.trade_ship_id) {
        Figure *ship = valid_trade_ship_for_dock(b);
        if (ship && ship->action_state == FIGURE_ACTION_115_TRADE_SHIP_LEAVING) {
            b->data.dock.trade_ship_id = 0;
        }
    }
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_132_DOCKER_IDLING:
            f->cart_image_id = 0;
            if (!deliver_import_resource(f, b)) {
                fetch_export_resource(f, b, 1);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE:
            f->cart_image_id = 0;
            f->image_offset = 0;
            if (b->data.dock.queued_docker_id <= 0) {
                b->data.dock.queued_docker_id = f->id();
                f->wait_ticks = 0;
            }
            if ((unsigned int) b->data.dock.queued_docker_id == f->id()) {
                b->data.dock.num_ships = 120;
                f->wait_ticks++;
                if (f->wait_ticks >= 0) {
                    f->action_state = FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE;
                    f->wait_ticks = 0;
                    set_cart_graphic(f);
                    b->data.dock.queued_docker_id = 0;
                }
            } else {
                int has_queued_docker = 0;
                for (int i = 0; i < 3; i++) {
                    if (b->data.distribution.cartpusher_ids[i]) {
                        Figure *docker = Figure::get(b->data.distribution.cartpusher_ids[i]);
                        if (docker->id() == (unsigned int) b->data.dock.queued_docker_id && docker->state == FIGURE_STATE_ALIVE) {
                            if (docker->action_state == FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE ||
                                docker->action_state == FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE) {
                                has_queued_docker = 1;
                            }
                        }
                    }
                }
                if (!has_queued_docker) {
                    b->data.dock.queued_docker_id = 0;
                }
            }
            break;
        case FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE:
            set_cart_graphic(f);
            if (b->data.dock.queued_docker_id <= 0) {
                b->data.dock.queued_docker_id = f->id();
                f->wait_ticks = 0;
            }
            if ((unsigned int) b->data.dock.queued_docker_id == f->id()) {
                b->data.dock.num_ships = 120;
                f->wait_ticks++;
                if (f->wait_ticks >= game_time_scale_legacy_day_ticks(80)) {
                    set_docker_as_idle(f);
                    f->image_id = 0;
                    f->cart_image_id = 0;
                    b->data.dock.queued_docker_id = 0;
                }
            }
            f->wait_ticks++;
            if (f->wait_ticks >= game_time_scale_legacy_day_ticks(20)) {
                set_docker_as_idle(f);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE:
            set_cart_graphic(f);
            figure_movement_move_ticks(f, 1);
            f->loads_sold_or_carrying = 1;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_139_DOCKER_IMPORT_AT_STORAGE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!figure_destination_building_is_in_use(f) &&
                !deliver_import_resource(f, b)) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE:
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_140_DOCKER_EXPORT_AT_STORAGE;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!figure_destination_building_is_in_use(f) &&
                !fetch_export_resource(f, b, 0)) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_137_DOCKER_EXPORT_RETURNING:
            set_cart_graphic(f);
            figure_movement_move_ticks(f, 1);
            f->loads_sold_or_carrying = 1;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!figure_destination_building_is_in_use(f)) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING:
            if (f->resource_id != RESOURCE_NONE) {
                set_cart_graphic(f); // cart with a resource if imports failed
            } else {
                f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty cart
            }
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                set_docker_as_idle(f);
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_139_DOCKER_IMPORT_AT_STORAGE:
            set_cart_graphic(f);
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                Figure *ship = valid_trade_ship_for_dock(b);
                int trade_city_id = ship ? ship->empire_city_id : 0;
                if (ship && try_import_resource(f->destination_building.id(), static_cast<resource_type>(f->resource_id),
                    trade_city_id, f->loads_sold_or_carrying)) {
                    int trader_id = ship->trader_id;
                    trader_record_sold_resource(trader_id, static_cast<resource_type>(f->resource_id));
                    city_health_update_sickness_level_in_building(b->id);
                    city_health_dispatch_sickness(f);
                    f->action_state = FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING;
                    f->wait_ticks = 0;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                    f->resource_id = 0;
                    fetch_export_resource(f, b, 1);
                } else {
                    f->action_state = FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                }
                f->wait_ticks = 0;
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_140_DOCKER_EXPORT_AT_STORAGE:
            f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART); // empty
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                Figure *ship = valid_trade_ship_for_dock(b);
                int trade_city_id = ship ? ship->empire_city_id : 0;
                f->action_state = FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                f->wait_ticks = 0;
                if (ship && try_export_resource(f->destination_building.id(), static_cast<resource_type>(f->resource_id), trade_city_id)) {
                    int trader_id = ship->trader_id;
                    trader_record_bought_resource(trader_id, static_cast<resource_type>(f->resource_id));
                    city_health_update_sickness_level_in_building(b->id);
                    city_health_dispatch_sickness(f);
                    f->action_state = FIGURE_ACTION_137_DOCKER_EXPORT_RETURNING;
                } else if (ship) {
                    fetch_export_resource(f, b, 1);
                }
            }
            f->image_offset = 0;
            break;
    }

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
    } else {
        f->image_id = 0;
    }
}
