#include "building/storage.h"
#include "city/health.h"
#include "city/trade.h"
#include "empire/empire.h"
#include "map/road_access.h"

#include "docker.h"

#include "building/building.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "core/config.h"
#include "core/image.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/trader.h"
#include "figuretype/trader.h"
#include "game/resource.h"
#include "game/time.h"

namespace {

constexpr int INFINITE = 10000;

} // namespace

namespace figuretype {

Figure *Docker::valid_trade_ship_for_dock(Building &dock)
{
    if (!dock.id() || !dock.dock_trade_ship_id()) {
        return nullptr;
    }
    Figure *ship = Figure::get(dock.dock_trade_ship_id());
    if (!ship || ship->state != FIGURE_STATE_ALIVE || ship->type != FIGURE_TRADE_SHIP) {
        dock.set_dock_trade_ship_id(0);
        return nullptr;
    }
    return ship;
}

int Docker::try_import_resource(Building storage, resource_type resource, int city_id, int quantity)
{
    const auto *storage_type = storage.type;
    if (!storage_type ||
        (!storage_type->is_warehouse() &&
            !(resource_is_food(resource) && storage_type->is_granary()))) {
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
    if (storage_type->is_granary()) {
        result = building_granary_add_import(storage, resource, 1, 0);
        if (result) {
            trade_route_increase_traded(route_id, resource, 0);
        }
    } else if (storage_type->is_warehouse()) {
        result = building_warehouse_add_import(storage, resource, quantity, 0);
        if (result) {
            trade_route_increase_traded(route_id, resource, 0);
        }
    }
    return result;
}

int Docker::try_export_resource(Building storage, resource_type resource, int city_id)
{
    const auto *storage_type = storage.type;
    if (!storage_type || (!storage_type->is_warehouse() && !storage_type->is_granary())) {
        return 0;
    }

    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_DOCK, storage)) {
        return 0;
    }
    int result = 0;
    if (storage_type->is_granary()) {
        result = building_granary_remove_export(storage, resource, 1, 0);
        if (result) {
            trade_route_increase_traded(empire_city_get_route_id(city_id), resource, 1);
        }
    } else if (storage_type->is_warehouse()) {
        result = building_warehouse_remove_export(storage, resource, 1, 0);
        if (result) {
            trade_route_increase_traded(empire_city_get_route_id(city_id), resource, 1);
        }
    }
    return result;
}

int Docker::store_destination_map_point(Building destination, map_point *dst)
{
    if (!destination.id()) {
        return 0;
    }
    if (destination.type && destination.type->is_granary()) {
        // go to center of granary
        map_point_store_result(destination.x() + 1, destination.y() + 1, dst);
    } else if (destination.has_cached_road_access() == 1) {
        map_point_store_result(destination.x(), destination.y(), dst);
    } else if (!map_has_road_access_warehouse(destination.x(), destination.y(), dst)) {
        return 0;
    }
    return 1;
}

int Docker::is_invalid_destination(Building destination, const Building &dock)
{
    if (!destination.id() || !dock.id()) {
        return 1;
    }
    return !destination.is_in_use() ||
        !destination.has_cached_road_access() || destination.distance_from_entry() <= 0 ||
        destination.road_network_id() != dock.road_network_id() ||
        !building_storage_get_permission(BUILDING_STORAGE_PERMISSION_DOCK, destination);
}

Building Docker::closest_import_storage_for_resource(int x, int y, const Building &dock,
    resource_type resource, map_point *dst)
{
    int min_distance = INFINITE;
    Building closest_storage(nullptr);
    for (Building storage = building_warehouse_first(); storage.id(); storage = storage.next_of_type()) {

        if (is_invalid_destination(storage, dock) ||
            !building_warehouse_maximum_receptible_amount(storage, resource)) {
            continue;
        }
        const int distance_penalty = 32 - 2 * building_warehouse_maximum_receptible_amount(storage, resource);
        if (distance_penalty == 32) {
            continue;
        }
        int distance = storage.max_distance_to(x, y);
        // prefer emptier warehouse
        distance += distance_penalty;
        if (distance < min_distance) {
            min_distance = distance;
            closest_storage = storage;
        }
    }
    if (resource_is_food(resource)) {
        for (Building storage = building_granary_first(); storage.id(); storage = storage.next_of_type()) {
            if (is_invalid_destination(storage, dock) ||
                !building_granary_maximum_receptible_amount(storage, resource)) {
                continue;
            }
            // always prefer granary
            int distance = storage.max_distance_to(x, y);
            if (distance < min_distance) {
                min_distance = distance;
                closest_storage = storage;
            }
        }
    }
    if (!store_destination_map_point(closest_storage, dst)) {
        return Building(nullptr);
    }
    return closest_storage;
}

Building Docker::closest_building_for_import(int x, int y, int city_id, const Building &dock,
    map_point *dst, resource_type *import_resource)
{
    resource_type resource = *import_resource;
    if (resource != RESOURCE_NONE) {
        if (!dock.accepts_good(resource) ||
            !empire_can_import_resource_from_city(city_id, resource)) {
            return Building(nullptr);
        }
        Building destination = closest_import_storage_for_resource(x, y, dock, resource, dst);
        if (destination.id()) {
            *import_resource = resource;
        }
        return destination;
    }

    for (int i = 0; i < resource_loaded_count(); i++) {
        resource = static_cast<resource_type>(city_trade_next_docker_import_resource());
        if (resource == RESOURCE_NONE) {
            return Building(nullptr);
        }
        if (!dock.accepts_good(resource) ||
            !empire_can_import_resource_from_city(city_id, resource)) {
            continue;
        }
        Building destination = closest_import_storage_for_resource(x, y, dock, resource, dst);
        if (destination.id()) {
            *import_resource = resource;
            return destination;
        }
    }
    return Building(nullptr);
}

Building Docker::closest_export_storage_for_resource(int x, int y, const Building &dock,
    resource_type resource, map_point *dst)
{
    int min_distance = INFINITE;
    Building closest_storage(nullptr);
    for (Building storage = building_warehouse_first(); storage.id(); storage = storage.next_of_type()) {
        if (is_invalid_destination(storage, dock)) {
            continue;
        }
        const int distance_penalty = 32 - 2 * building_warehouse_get_available_amount(storage, resource);
        if (distance_penalty == 32) {
            continue;
        }
        int distance = storage.max_distance_to(x, y);
        // prefer fuller warehouse
        distance += distance_penalty;
        if (distance < min_distance) {
            min_distance = distance;
            closest_storage = storage;
        }
    }
    if (resource_is_food(resource) && config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
        for (Building storage = building_granary_first(); storage.id(); storage = storage.next_of_type()) {
            if (is_invalid_destination(storage, dock) ||
                !building_granary_get_amount(storage, resource)) {
                continue;
            }
            int distance = storage.max_distance_to(x, y);
            // avoid granaries
            distance += 31;
            if (distance < min_distance) {
                min_distance = distance;
                closest_storage = storage;
            }
        }
    }
    if (!store_destination_map_point(closest_storage, dst)) {
        return Building(nullptr);
    }
    return closest_storage;
}

Building Docker::closest_building_for_export(int x, int y, int city_id, const Building &dock,
    map_point *dst, resource_type *export_resource)
{
    resource_type resource = *export_resource;
    if (resource != RESOURCE_NONE) {
        if (!dock.accepts_good(resource) ||
            !empire_can_export_resource_to_city(city_id, resource)) {
            return Building(nullptr);
        }
        Building destination = closest_export_storage_for_resource(x, y, dock, resource, dst);
        if (destination.id()) {
            *export_resource = resource;
        }
        return destination;
    }

    for (int i = 0; i < resource_loaded_count(); i++) {
        resource = static_cast<resource_type>(city_trade_next_docker_export_resource());
        if (resource == RESOURCE_NONE) {
            return Building(nullptr);
        }
        if (!dock.accepts_good(resource) ||
            !empire_can_export_resource_to_city(city_id, resource)) {
            continue;
        }
        Building destination = closest_export_storage_for_resource(x, y, dock, resource, dst);
        if (destination.id()) {
            *export_resource = resource;
            return destination;
        }
    }
    return Building(nullptr);
}

int Docker::deliver_import_resource(Building &dock)
{
    Figure *ship = valid_trade_ship_for_dock(dock);
    if (!ship || ship->action_state != FIGURE_ACTION_112_TRADE_SHIP_MOORED ||
        ship->loads_sold_or_carrying <= 0) {
        return 0;
    }
    map_point tile;
    resource_type resource = destination_building.id() ?
        static_cast<resource_type>(resource_id) : RESOURCE_NONE;
    Building destination = closest_building_for_import(x, y, ship->empire_city_id,
        dock, &tile, &resource);
    if (!destination.id()) {
        return 0;
    }
    if (!destination_building.id()) {
        ship->loads_sold_or_carrying--;
        action_state = FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE;
    } else {
        action_state = FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE;
    }
    if (destination_building.id() != destination.id()) {
        Route::remove(this);
    }
    destination_building = destination;
    wait_ticks = 0;
    destination_x = tile.x;
    destination_y = tile.y;
    resource_id = resource;
    return 1;
}

int Docker::fetch_export_resource(Building &dock, int add_to_bought)
{
    Figure *ship = valid_trade_ship_for_dock(dock);
    if (!ship || ship->action_state != FIGURE_ACTION_112_TRADE_SHIP_MOORED ||
        (add_to_bought && ship->trader_amount_bought >= figure_trade_sea_trade_units())) {
        return 0;
    }
    map_point tile;
    resource_type resource = add_to_bought ? RESOURCE_NONE : static_cast<resource_type>(resource_id);
    Building destination = closest_building_for_export(x, y, ship->empire_city_id,
        dock, &tile, &resource);
    if (!destination.id()) {
        return 0;
    }
    if (add_to_bought) {
        ship->trader_amount_bought++;
    }
    if (destination_building.id() != destination.id()) {
        Route::remove(this);
    }
    destination_building = destination;
    action_state = FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE;
    wait_ticks = 0;
    destination_x = tile.x;
    destination_y = tile.y;
    resource_id = resource;
    return 1;
}

void Docker::set_cart_graphic()
{
    select_legacy_cart_overlay_base_image(resource_id != RESOURCE_NONE ?
        figure_type_registry_impl::FigureGraphics::resource_cart_marker_for_direction(0) :
        image_group(GROUP_FIGURE_CARTPUSHER_CART));
}

void Docker::set_as_idle()
{
    action_state = FIGURE_ACTION_132_DOCKER_IDLING;
    resource_id = RESOURCE_NONE;
    destination_building = Building(nullptr);
    wait_ticks = 0;
    loads_sold_or_carrying = 0;
}

void Docker::docker_action()
{
    Docker *f = this;
    Building dock = f->building;

    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    if (!dock.is_in_use()) {
        f->state = FIGURE_STATE_DEAD;
        f->destination_building = Building(nullptr);
        f->clear_legacy_image();
        return;
    }
    if (!dock.matches("dock")) {
        f->state = FIGURE_STATE_DEAD;
        f->destination_building = Building(nullptr);
        f->clear_legacy_image();
        return;
    }
    dock.decrement_dock_num_ships();
    if (dock.dock_trade_ship_id()) {
        Figure *ship = valid_trade_ship_for_dock(dock);
        if (ship && ship->action_state == FIGURE_ACTION_115_TRADE_SHIP_LEAVING) {
            dock.set_dock_trade_ship_id(0);
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
            if (!f->deliver_import_resource(dock)) {
                f->fetch_export_resource(dock, 1);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE:
            f->image_offset = 0;
            if (dock.dock_queued_docker_id() <= 0) {
                dock.set_dock_queued_docker_id(f->id());
                f->wait_ticks = 0;
            }
            if ((unsigned int) dock.dock_queued_docker_id() == f->id()) {
                dock.set_dock_num_ships(120);
                f->wait_ticks++;
                if (f->wait_ticks >= 0) {
                    f->action_state = FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE;
                    f->wait_ticks = 0;
                    f->set_cart_graphic();
                    dock.set_dock_queued_docker_id(0);
                }
            } else {
                int has_queued_docker = 0;
                for (int i = 0; i < 3; i++) {
                    if (dock.distribution_cartpusher_id(i)) {
                        Figure *docker = Figure::get(dock.distribution_cartpusher_id(i));
                        if (docker->id() == (unsigned int) dock.dock_queued_docker_id() && docker->state == FIGURE_STATE_ALIVE) {
                            if (docker->action_state == FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE ||
                                docker->action_state == FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE) {
                                has_queued_docker = 1;
                            }
                        }
                    }
                }
                if (!has_queued_docker) {
                    dock.set_dock_queued_docker_id(0);
                }
            }
            break;
        case FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE:
            f->set_cart_graphic();
            if (dock.dock_queued_docker_id() <= 0) {
                dock.set_dock_queued_docker_id(f->id());
                f->wait_ticks = 0;
            }
            if ((unsigned int) dock.dock_queued_docker_id() == f->id()) {
                dock.set_dock_num_ships(120);
                f->wait_ticks++;
                if (f->wait_ticks >= game_time_scale_legacy_day_ticks(80)) {
                    f->set_as_idle();
                    f->clear_legacy_image();
                    f->clear_legacy_cart_overlay_image();
                    dock.set_dock_queued_docker_id(0);
                }
            }
            f->wait_ticks++;
            if (f->wait_ticks >= game_time_scale_legacy_day_ticks(20)) {
                f->set_as_idle();
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE:
            f->set_cart_graphic();
            figure_movement_move_ticks(f, 1);
            f->loads_sold_or_carrying = 1;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_139_DOCKER_IMPORT_AT_STORAGE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!f->destination_building.is_in_use() &&
                !f->deliver_import_resource(dock)) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE:
            f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_CARTPUSHER_CART)); // empty
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_140_DOCKER_EXPORT_AT_STORAGE;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!f->destination_building.is_in_use() &&
                !f->fetch_export_resource(dock, 0)) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_137_DOCKER_EXPORT_RETURNING:
            f->set_cart_graphic();
            figure_movement_move_ticks(f, 1);
            f->loads_sold_or_carrying = 1;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE;
                f->wait_ticks = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            if (!f->destination_building.is_in_use()) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING:
            if (f->resource_id != RESOURCE_NONE) {
                f->set_cart_graphic(); // cart with a resource if imports failed
            } else {
                f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_CARTPUSHER_CART)); // empty cart
            }
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->set_as_idle();
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_139_DOCKER_IMPORT_AT_STORAGE:
            f->set_cart_graphic();
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                Figure *ship = valid_trade_ship_for_dock(dock);
                int trade_city_id = ship ? ship->empire_city_id : 0;
                if (ship && try_import_resource(f->destination_building, static_cast<resource_type>(f->resource_id),
                    trade_city_id, f->loads_sold_or_carrying)) {
                    int trader_id = ship->trader_id;
                    trader_record_sold_resource(trader_id, static_cast<resource_type>(f->resource_id));
                    city_health_update_sickness_level_in_building(dock.id());
                    city_health_dispatch_sickness(f);
                    f->action_state = FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING;
                    f->wait_ticks = 0;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                    f->resource_id = 0;
                    f->fetch_export_resource(dock, 1);
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
            f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_CARTPUSHER_CART)); // empty
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                Figure *ship = valid_trade_ship_for_dock(dock);
                int trade_city_id = ship ? ship->empire_city_id : 0;
                f->action_state = FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                f->wait_ticks = 0;
                if (ship && try_export_resource(f->destination_building, static_cast<resource_type>(f->resource_id), trade_city_id)) {
                    int trader_id = ship->trader_id;
                    trader_record_bought_resource(trader_id, static_cast<resource_type>(f->resource_id));
                    city_health_update_sickness_level_in_building(dock.id());
                    city_health_dispatch_sickness(f);
                    f->action_state = FIGURE_ACTION_137_DOCKER_EXPORT_RETURNING;
                } else if (ship) {
                    f->fetch_export_resource(dock, 1);
                }
            }
            f->image_offset = 0;
            break;
    }

    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_group(GROUP_FIGURE_CARTPUSHER) + 96);
        f->clear_legacy_cart_overlay_image();
    } else {
        f->select_legacy_directional_frame_image(image_group(GROUP_FIGURE_CARTPUSHER), dir, f->image_offset);
    }
    f->finalize_legacy_cartpusher_overlay_image(dir);
    if (!f->cart_image_id) {
        f->clear_legacy_image();
    }
}

} // namespace figuretype
