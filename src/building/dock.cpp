#include "dock.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/market.h"
#include "city/buildings.h"
#include "city/resource.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "figure/figure.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "figuretype/trader.h"
#include "game/resource.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/routing.h"
#include "map/routing_data.h"
#include "map/terrain.h"
#include "scenario/map.h"

#include <cstring>
#include <cstdlib>

#define MAX_DISTANCE_FOR_REROUTING 50

struct handled_goods_by_road_network {
    unsigned char road_network_id;
    int goods[RESOURCE_SLOT_COUNT];
};

struct handled_goods {
    handled_goods_by_road_network *networks;
    int max_networks;
};

static building_type dock_type()
{
    return building_type_registry_impl::type_from_attr("dock");
}

static int is_dock(const Building &dock)
{
    return dock.type && dock.type->type() == dock_type();
}

int building_dock_count_idle_dockers(const Building &dock)
{
    int num_idle = 0;
    for (int i = 0; i < 3; i++) {
        unsigned int cartpusher_id = dock.distribution_cartpusher_id(i);
        if (cartpusher_id) {
            Figure *f = Figure::get(cartpusher_id);
            if (f->action_state == FIGURE_ACTION_132_DOCKER_IDLING ||
                f->action_state == FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE) {
                num_idle++;
            }
        }
    }
    return num_idle;
}

void building_dock_update_open_water_access(void)
{
    map_point river_entry = scenario_map_river_entry();
    map_routing_calculate_distances_water_boat(river_entry.x, river_entry.y);
    for (Building b : Building::of_type(dock_type())) {
        if (b.is_in_use() && !b.has_house_size()) {
            if (map_terrain_is_adjacent_to_open_water(b.x(), b.y(), 3)) {
                b.set_has_water_access(1);
            } else {
                b.set_has_water_access(0);
            }
        }
    }
}

int building_dock_is_connected_to_open_water(int x, int y)
{
    map_point river_entry = scenario_map_river_entry();
    map_routing_calculate_distances_water_boat(river_entry.x, river_entry.y);
    if (map_terrain_is_adjacent_to_open_water(x, y, 3)) {
        return 1;
    } else {
        return 0;
    }
}

int building_dock_accepts_ship(Figure &ship, const Building &dock)
{
    if (!is_dock(dock)) {
        return 0;
    }
    empire_city *city = empire_city_get(ship.empire_city_id);
    if (!city) {
        return 0;
    }
    if (!building_dock_can_trade_with_route(city->route_id, dock)) {
        return 0;
    }
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        if (city->sells_resource[resource] || city->buys_resource[resource]) {
            if (dock.accepts_good(resource)) {
                return 1;
            }
        }
    }
    return 0;
}

int building_dock_can_import_from_ship(const Building &dock, int ship_id)
{
    Figure *ship = Figure::get(ship_id);
    if (trader_has_sold_max(ship->trader_id)) {
        return 0;
    }

    // dock has plague, trading is disabled
    if (dock.has_plague()) {
        return 0;
    }

    empire_city *city = empire_city_get(ship->empire_city_id);
    if (!city) {
        return 0;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (dock.accepts_good(r) && city->sells_resource[r]) {
            return 1;
        }
    }
    return 0;
}

int building_dock_can_export_to_ship(const Building &dock, int ship_id)
{
    Figure *ship = Figure::get(ship_id);
    if (trader_has_bought_max(ship->trader_id)) {
        return 0;
    }

    // dock has plague, trading is disabled
    if (dock.has_plague()) {
        return 0;
    }

    empire_city *city = empire_city_get(ship->empire_city_id);
    if (!city) {
        return 0;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (dock.accepts_good(r) && city->buys_resource[r]) {
            return 1;
        }
    }
    return 0;
}

void building_dock_enable_resource_in_all_docks(resource_type resource)
{
    for (Building b : Building::of_type(dock_type())) {
        b.set_accepted_good(resource, 1);
    }
}

// returns a list of goods that have been "handled" (i.e. the dock allowed for it to be traded)
// for each road network a ship has visited
static void get_already_handled_goods(handled_goods *handled, int ship_id)
{
    std::memset(handled->networks, 0, sizeof(handled_goods_by_road_network) * handled->max_networks);
    Figure *ship = Figure::get(ship_id);

    // loop through the docks
    for (Building dock : Building::of_type(dock_type())) {
        // check and see if the ship has visited this dock
        if (!dock.is_in_use() || !dock.is_working() ||
            !figure_visited_building_in_list(ship->last_visited_index, dock.id())) {
            continue;
        }

        // find the handled_good that is on this road network or find the next one that hasn't
        // been assigned to a road network yet
        handled_goods_by_road_network *network = nullptr;
        for (int j = 0; j < handled->max_networks; j++) {
            network = &handled->networks[j];
            if (!network->road_network_id ||
                network->road_network_id == dock.road_network_id()) {
                break;
            }
        }

        if (!network) {
            return;
        }

        // assign the road network (in case this is a new one) and add the goods this dock handles
        network->road_network_id = static_cast<unsigned char>(dock.road_network_id());
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
            if (dock.accepts_good(r)) {
                network->goods[r] = 1;
            }
        }
    }
}

static int all_dock_goods_already_handled(const handled_goods *handled, const Building &dock, const Figure *ship)
{
    empire_city *city = empire_city_get(ship->empire_city_id);
    if (!city) {
        return 1;
    }
    for (int i = 0; i < handled->max_networks; i++) {
        const handled_goods_by_road_network *network = &handled->networks[i];
        if (network->road_network_id != dock.road_network_id()) {
            continue;
        }
        // we've visited docks on this road network
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
            if (!city->sells_resource[r] && !city->buys_resource[r]) {
                // the ship doesn't buy or sell this good
                continue;
            }
            if (dock.accepts_good(r) && !network->goods[r]) {
                // this dock accepts a good that all previous docks on this road network did not accept
                return 0;
            }
        }
        // all goods at this dock have already been handled on this road network
        return 1;
    }
    // no matching road networks, assume unhandled
    return 0;
}

static Building get_free_destination(Figure &ship, const Building *exclude_dock, map_point *tile, const handled_goods *handled)
{
    Building importing_dock(nullptr);
    Building exporting_dock(nullptr);

    for (Building dock : Building::of_type(dock_type())) {
        if (!dock.is_in_use() || !dock.is_working()) {
            continue;
        }

        if ((exclude_dock && dock.id() == exclude_dock->id()) ||
            figure_visited_building_in_list(ship.last_visited_index, dock.id()) ||
            !building_dock_accepts_ship(ship, dock)) {
            continue;
        }

        if (dock.dock_trade_ship_id()) {
            continue;
        }

        if (all_dock_goods_already_handled(handled, dock, &ship)) {
            continue;
        }

        if (building_dock_can_import_from_ship(dock, ship.id())) {
            importing_dock = dock;
            // prioritize imports
            break;
        } else if (building_dock_can_export_to_ship(dock, ship.id())) {
            exporting_dock = dock;
        }
    }
    Building dock = importing_dock.id() ? importing_dock : exporting_dock;
    if (!dock.id()) {
        return Building(nullptr);
    }
    building_dock_get_ship_request_tile(dock, SHIP_DOCK_REQUEST_2_FIRST_QUEUE, tile);
    return dock;
}


static Building get_queue_destination(Figure &ship, const Building *exclude_dock, ship_dock_request_type request_type, map_point *tile,
    const handled_goods *handled)
{
    Building importing_dock(nullptr);
    Building exporting_dock(nullptr);

    for (Building dock : Building::of_type(dock_type())) {
        if (!dock.is_in_use() || !dock.is_working()) {
            continue;
        }
        if ((exclude_dock && dock.id() == exclude_dock->id()) ||
            figure_visited_building_in_list(ship.last_visited_index, dock.id()) ||
            !building_dock_accepts_ship(ship, dock)) {
            continue;
        }
        if (all_dock_goods_already_handled(handled, dock, &ship)) {
            continue;
        }

        map_point requested_tile;
        building_dock_get_ship_request_tile(dock, request_type, &requested_tile);

        int figure_at_offset = map_figure_at(map_grid_offset(requested_tile.x, requested_tile.y));
        if (figure_at_offset && figure_at_offset != (int) ship.id()) {
            Figure *ship_at_offset = Figure::get(figure_at_offset);
            if (ship_at_offset->action_state == FIGURE_ACTION_114_TRADE_SHIP_ANCHORED) {
                continue;
            }
        }

        if (figure_trader_ship_can_queue_for_import(&ship) && building_dock_can_import_from_ship(dock, ship.id())) {
            importing_dock = dock;
            map_point_store_result(requested_tile.x, requested_tile.y, tile);
            break;  // prioritize imports
        } else if (figure_trader_ship_can_queue_for_export(&ship) && building_dock_can_export_to_ship(dock, ship.id())) {
            map_point_store_result(requested_tile.x, requested_tile.y, tile);
            exporting_dock = dock;
        }
    }

    return importing_dock.id() ? importing_dock : exporting_dock;
}

static int destination_dock_ready_for_ship(Figure &ship)
{
    const Building &destination_dock = ship.destination_building;
    if (destination_dock.dock_trade_ship_id() &&
        destination_dock.dock_trade_ship_id() != (int) ship.id()) {
        return 0;
    }

    if (!building_dock_is_working(destination_dock) ||
        !building_dock_accepts_ship(ship, destination_dock)) {
        return 0;
    }

    if (!building_dock_can_import_from_ship(destination_dock, ship.id()) &&
        !building_dock_can_export_to_ship(destination_dock, ship.id())) {
        return 0;
    }
    return 1;
}

Building building_dock_get_destination(Figure &ship, const Building *exclude_dock, map_point *tile)
{
    int total_docks = 0;
    for (Building dock : Building::of_type(dock_type())) {
        if (dock.is_in_use() && dock.is_working()) {
            total_docks++;
        }
    }
    if (!total_docks) {
        return Building(nullptr);
    }

    handled_goods handled;
    handled.networks = static_cast<handled_goods_by_road_network *>(malloc(sizeof(handled_goods_by_road_network) * total_docks));
    if (!handled.networks) {
        return Building(nullptr);
    }
    handled.max_networks = total_docks;
    get_already_handled_goods(&handled, ship.id());

    Building dock = get_free_destination(ship, exclude_dock, tile, &handled);
    if (!dock.id()) {
        dock = get_queue_destination(ship, exclude_dock, SHIP_DOCK_REQUEST_2_FIRST_QUEUE, tile, &handled);
        if (!dock.id()) {
            dock = get_queue_destination(ship, exclude_dock, SHIP_DOCK_REQUEST_4_SECOND_QUEUE, tile, &handled);
        }
    }
    free(handled.networks);
    return dock;
}

Building building_dock_get_closer_free_destination(Figure &ship, ship_dock_request_type request_type, map_point *tile)
{
    int distance_to_destination = figure_trader_ship_get_distance_to_dock(&ship, ship.destination_building.id());
    int min_distance_import = -1, min_distance_export = -1;
    Building nearest_import_dock(nullptr);
    Building nearest_export_dock(nullptr);
    for (Building dock : Building::of_type(dock_type())) {
        if (!dock.is_in_use() || !dock.is_working()) {
            continue;
        }

        if (dock.dock_trade_ship_id() ||
            dock.id() == ship.destination_building.id() ||
            figure_visited_building_in_list(ship.last_visited_index, dock.id()) ||
            !building_dock_accepts_ship(ship, dock)) {
            continue;
        }

        int distance_to_dock = figure_trader_ship_get_distance_to_dock(&ship, dock.id());
        if (distance_to_dock > MAX_DISTANCE_FOR_REROUTING ||
            (figure_trader_ship_other_ship_closer_to_dock(dock.id(), distance_to_dock))) {
            continue;
        }

        if (ship.action_state == FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE &&
            destination_dock_ready_for_ship(ship) &&
            distance_to_destination < distance_to_dock) {
            continue;
        }

        if (building_dock_can_import_from_ship(dock, ship.id())) {
            if (min_distance_import == -1 || distance_to_dock < min_distance_import) {
                nearest_import_dock = dock;
                min_distance_import = distance_to_dock;
            }
        }

        if (building_dock_can_export_to_ship(dock, ship.id())) {
            if (min_distance_export == -1 || distance_to_dock < min_distance_export) {
                nearest_export_dock = dock;
                min_distance_export = distance_to_dock;
            }
        }
    }

    Building dock(nullptr);
    if (nearest_import_dock.id()) {
        if (nearest_export_dock.id() && min_distance_export < min_distance_import + MAX_DISTANCE_FOR_REROUTING) {
            dock = nearest_export_dock;
        } else {
            dock = nearest_import_dock;
        }
    } else if (nearest_export_dock.id()) {
        dock = nearest_export_dock;
    }

    if (dock.id()) {
        building_dock_get_ship_request_tile(dock, request_type, tile);
    }

    return dock;
}

int building_dock_can_trade_with_route(int route_id, const Building &dock)
{
    if (route_id < 0 || route_id >= 31) {
        return 0;
    }
    if (!dock.dock_has_accepted_route_ids()) {
        return 1;
    }
    return dock.dock_accepted_route_ids() & (1 << route_id);
}

void building_dock_set_can_trade_with_route(int route_id, Building &dock, int can_trade)
{
    if (route_id < 0 || route_id >= 31) {
        return;
    }
    int has_route_ids = dock.dock_has_accepted_route_ids();
    int accepted_route_ids = has_route_ids ? dock.dock_accepted_route_ids() : 0xffffffff;
    has_route_ids = 1;
    int mask = 1 << route_id;
    if (can_trade) {
        accepted_route_ids |= mask;
    } else {
        accepted_route_ids &= ~mask;
    }
    dock.set_dock_accepted_route_ids(has_route_ids, accepted_route_ids);
}

int building_dock_request_docking(Figure &ship, const Building &dock, map_point *tile)
{
    if ((!dock.dock_trade_ship_id() || dock.dock_trade_ship_id() == (int) ship.id())) {
        building_dock_get_ship_request_tile(dock, SHIP_DOCK_REQUEST_1_DOCKING, tile);
        return 1;
    }
    return 0;
}

void building_dock_get_ship_request_tile(const Building &dock, ship_dock_request_type request_type, map_point *tile)
{
    int dx, dy, grid_offset;
    switch (request_type) {
        case SHIP_DOCK_REQUEST_1_DOCKING:
            switch (dock.dock_orientation()) {
                case 0: dx = 1; dy = -1; break;
                case 1: dx = 3; dy = 1; break;
                case 2: dx = 1; dy = 3; break;
                default: dx = -1; dy = 1; break;
            }
            break;
        case SHIP_DOCK_REQUEST_2_FIRST_QUEUE:
            switch (dock.dock_orientation()) {
                case 0: dx = 2; dy = -2; break;
                case 1: dx = 4; dy = 2; break;
                case 2: dx = 2; dy = 4; break;
                default: dx = -2; dy = 2; break;
            }
            grid_offset = map_grid_offset(dock.x() + dx, dock.y() + dy);
            if (!map_terrain_is(grid_offset, TERRAIN_WATER) || terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
                // fallback 1
                switch (dock.dock_orientation()) {
                    case 0: dx = 0; dy = -1; break;
                    case 1: dx = 3; dy = 0; break;
                    case 2: dx = 2; dy = 3; break;
                    default: dx = -1; dy = 2; break;
                }
                grid_offset = map_grid_offset(dock.x() + dx, dock.y() + dy);
            }
            if (!map_terrain_is(grid_offset, TERRAIN_WATER) || terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
                // fallback 2
                switch (dock.dock_orientation()) {
                    case 0: dx = 1; dy = 0; break;
                    case 1: dx = 2; dy = 1; break;
                    case 2: dx = 1; dy = 4; break;
                    default: dx = -2; dy = 1; break;
                }
            }
            break;
        case SHIP_DOCK_REQUEST_4_SECOND_QUEUE:
        default:
            switch (dock.dock_orientation()) {
                case 0: dx = 2; dy = -3; break;
                case 1: dx = 5; dy = 2; break;
                case 2: dx = 2; dy = 5; break;
                default: dx = -3; dy = 2; break;
            }
            grid_offset = map_grid_offset(dock.x() + dx, dock.y() + dy);
            if (!map_terrain_is(grid_offset, TERRAIN_WATER) || terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
                // fallback 1
                switch (dock.dock_orientation()) {
                    case 0: dx = 2; dy = -1; break;
                    case 1: dx = 3; dy = 2; break;
                    case 2: dx = 0; dy = 3; break;
                    default: dx = -1; dy = 0; break;
                }
                grid_offset = map_grid_offset(dock.x() + dx, dock.y() + dy);
            }
            if (!map_terrain_is(grid_offset, TERRAIN_WATER) || terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
                // fallback 2
                switch (dock.dock_orientation()) {
                    case 0: dx = 0; dy = -3; break;
                    case 1: dx = 5; dy = 0; break;
                    case 2: dx = 0; dy = 4; break;
                    default: dx = -1; dy = 0; break;
                }
            }
            break;
    }
    map_point_store_result(dock.x() + dx, dock.y() + dy, tile);
}

int building_dock_is_working(const Building &b)
{
    return b.is_in_use() && is_dock(b) &&
        b.worker_count() > 0 && !b.has_plague();
}

Building building_dock_reposition_anchored_ship(Figure &ship, map_point *tile)
{
    const Building &dock = ship.destination_building;
    map_point tile_first_queue;
    map_point tile_second_queue;
    building_dock_get_ship_request_tile(dock, SHIP_DOCK_REQUEST_2_FIRST_QUEUE, &tile_first_queue);
    building_dock_get_ship_request_tile(dock, SHIP_DOCK_REQUEST_4_SECOND_QUEUE, &tile_second_queue);
    if (map_figure_at(ship.grid_offset) != (int) ship.id()) {
        if (ship.grid_offset == map_grid_offset(tile_first_queue.x, tile_first_queue.y) && !map_has_figure_at(map_grid_offset(tile_second_queue.x, tile_second_queue.y))) {
            map_point_store_result(tile_second_queue.x, tile_second_queue.y, tile);
            return ship.destination_building;
        } else if (ship.grid_offset == map_grid_offset(tile_second_queue.x, tile_second_queue.y) && !map_has_figure_at(map_grid_offset(tile_first_queue.x, tile_first_queue.y))) {
            map_point_store_result(tile_first_queue.x, tile_first_queue.y, tile);
            return ship.destination_building;
        } else {    // prefer emptier dock queue, free or not, as this one has more than two waiting
            return building_dock_get_destination(ship, &ship.destination_building, tile);
        }
    }
    return Building(nullptr);
}
