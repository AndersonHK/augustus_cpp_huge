#include "figure/PathingMode.h"

#include "map/grid.h"
#include "map/road_network.h"
#include "map/routing_data.h"
#include "map/terrain.h"

namespace figure_type_registry_impl {

int PathingMode::citizenIsPassable(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] >= CITIZEN_0_ROAD &&
        terrain_land_citizen.items[grid_offset] <= CITIZEN_2_PASSABLE_TERRAIN;
}

int PathingMode::citizenIsRoad(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] == CITIZEN_0_ROAD;
}

int PathingMode::citizenIsRoadLike(int grid_offset)
{
    return citizenIsRoad(grid_offset) || map_terrain_is(grid_offset, TERRAIN_ACCESS_RAMP);
}

int PathingMode::citizenRoadNetworkAt(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset) || !citizenIsRoadLike(grid_offset)) {
        return 0;
    }
    return map_road_network_get(grid_offset);
}

bool PathingMode::citizenIsInRoadNetwork(int grid_offset, int road_network)
{
    return road_network > 0 && citizenRoadNetworkAt(grid_offset) == road_network;
}

bool PathingMode::citizenAreaTouchesRoadNetwork(
    int x_min,
    int y_min,
    int x_max,
    int y_max,
    int road_network)
{
    if (road_network <= 0) {
        return false;
    }
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            if (citizenIsInRoadNetwork(map_grid_offset(x, y), road_network)) {
                return true;
            }
        }
    }
    return false;
}

int PathingMode::citizenIsHighway(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] == CITIZEN_1_HIGHWAY;
}

int PathingMode::citizenIsPassableTerrain(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] == CITIZEN_2_PASSABLE_TERRAIN;
}

int PathingMode::gateIsTransformable(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] == GATE_0_TRANSFORMABLE ||
        terrain_land_noncitizen.items[grid_offset] == GATE_0_TRANSFORMABLE;
}

int PathingMode::noncitizenIsPassable(int grid_offset)
{
    return terrain_land_noncitizen.items[grid_offset] >= NONCITIZEN_0_PASSABLE;
}

roadblock_permission PathingMode::roadblockPermissionFor(const Figure &figure) const
{
    if (roadblock_rule == RoadblockRule::IgnoreRoadblocks) {
        return PERMISSION_NONE;
    }
    return Roadblock::permission_for(figure);
}

roadblock_permission PathingPolicy::roadblockPermissionFor(const Figure &figure) const
{
    return mode ? mode->roadblockPermissionFor(figure) : Roadblock::permission_for(figure);
}

PathingMode::RoutePolicySelection PathingPolicy::routePolicySelection(
    roadblock_permission permission,
    RouteNeighborhood neighborhood) const
{
    PathingMode::RoutePolicySelection selection;
    selection.terrain = terrain;
    selection.policy = PathingMode::routePolicyForTerrain(terrain, permission, neighborhood);
    return selection;
}

} // namespace figure_type_registry_impl
