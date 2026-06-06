#include "map/routing_distance.h"

extern "C" {
#include "building/building.h"
#include "building/building_record.h"
#include "map/grid.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
}

#include <limits>

namespace {

int g_prepared = 0;
int g_source_network = 0;

} // namespace

namespace routing_distance {

bool prepare_from_road(const map_point &road)
{
    const int source_offset = map_grid_offset(road.x, road.y);
    if (!map_grid_is_valid_offset(source_offset) ||
        !map_routing_citizen_is_road(source_offset)) {
        g_prepared = 0;
        g_source_network = 0;
        return false;
    }

    map_routing_calculate_distances(road.x, road.y);
    g_prepared = 1;
    g_source_network = map_road_network_get(source_offset);
    return true;
}

BuildingRoadResult find_access_road_to_building(
    const building *target,
    int radius,
    int max_distance,
    int require_same_network)
{
    BuildingRoadResult result;
    if (!g_prepared || !target || !target->id || radius <= 0) {
        return result;
    }

    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(target->x, target->y, target->size, radius, &x_min, &y_min, &x_max, &y_max);

    int best_distance = std::numeric_limits<int>::max();
    map_point best_road = { 0, 0 };
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            const int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, TERRAIN_ROAD)) {
                continue;
            }
            if (require_same_network &&
                g_source_network > 0 &&
                map_road_network_get(grid_offset) != g_source_network) {
                continue;
            }

            const int distance = map_routing_distance(grid_offset);
            if (distance <= 0 ||
                (max_distance > 0 && distance > max_distance) ||
                distance >= best_distance) {
                continue;
            }

            best_distance = distance;
            best_road = { x, y };
        }
    }

    if (best_distance == std::numeric_limits<int>::max()) {
        return result;
    }

    // Destination selection should use route distance: this is the same routing
    // grid path walkers follow, unlike calc_maximum_distance's local heuristic.
    result.reachable = 1;
    result.distance = best_distance;
    result.road = best_road;
    return result;
}

} // namespace routing_distance
