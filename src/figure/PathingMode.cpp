#include "figure/PathingMode.h"

#include "building/roadblock.h"
#include "figure/figure.h"
#include "map/routing.h"
#include "map/routing_data.h"

#include <cstring>

namespace figure_type_registry_impl {

// These declarations are the authoritative contract for XML pathing modes.
// Keep each requirement explicit here so adding a mode does not require
// duplicating its behavior in scattered helper functions.
const PathingMode VanillaRoaming(
    "vanilla_roaming",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode SmartService(
    "smart_service",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::RequiresServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode NearestUnemployed(
    "nearest_unemployed",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode VenueSeeker(
    "venue_seeker",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::RequiresVenueTargets);
const PathingMode StorageFetch(
    "storage_fetch",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode FollowLeader(
    "follow_leader",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode StandStill(
    "stand_still",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode TransientWander(
    "transient_wander",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode DepotOrderRoute(
    "depot_order_route",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode WaterRoute(
    "water_route",
    PathingMode::RoadRequirement::AllowsNonRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);

PathingMode::TerrainAccess PathingMode::terrainFromLegacyUsage(int terrain_usage)
{
    TerrainAccess terrain;
    terrain.legacy_usage = terrain_usage;
    switch (terrain_usage) {
        case TERRAIN_USAGE_ENEMY:
            terrain.enemy_land = true;
            break;
        case TERRAIN_USAGE_WALLS:
            terrain.wall_grid = true;
            break;
        case TERRAIN_USAGE_ANIMAL:
            terrain.animal_land = true;
            break;
        case TERRAIN_USAGE_PREFER_ROADS:
            terrain.prefers_roads = true;
            terrain.requires_roads = false;
            break;
        case TERRAIN_USAGE_ROADS:
            terrain.requires_roads = true;
            break;
        case TERRAIN_USAGE_PREFER_ROADS_HIGHWAY:
            terrain.prefers_roads = true;
            terrain.requires_roads = false;
            terrain.allows_highways = true;
            break;
        case TERRAIN_USAGE_ROADS_HIGHWAY:
            terrain.requires_roads = true;
            terrain.allows_highways = true;
            break;
        case TERRAIN_USAGE_ANY:
        default:
            terrain.legacy_usage = TERRAIN_USAGE_ANY;
            break;
    }
    return terrain;
}

bool PathingMode::terrainRequiresRoads(const TerrainAccess &terrain)
{
    return terrain.requires_roads || terrain.prefers_roads;
}

int PathingMode::canTravel(const Figure &figure, int direction_limit, const TerrainAccess &terrain) const
{
    const roadblock_permission permission = Roadblock::permission_for(figure);
    if (terrain.enemy_land) {
        int can_travel = map_routing_noncitizen_can_travel_over_land(
            figure.x,
            figure.y,
            figure.destination_x,
            figure.destination_y,
            direction_limit,
            figure.destination_building.id(),
            5000);
        if (!can_travel) {
            can_travel = map_routing_noncitizen_can_travel_over_land(
                figure.x,
                figure.y,
                figure.destination_x,
                figure.destination_y,
                direction_limit,
                0,
                25000);
        }
        return can_travel ? can_travel : map_routing_noncitizen_can_travel_through_everything(
            figure.x,
            figure.y,
            figure.destination_x,
            figure.destination_y,
            direction_limit);
    }

    if (terrain.wall_grid) {
        return map_routing_can_travel_over_walls(
            figure.x,
            figure.y,
            figure.destination_x,
            figure.destination_y,
            4);
    }

    if (terrain.animal_land) {
        return map_routing_noncitizen_can_travel_over_land(
            figure.x,
            figure.y,
            figure.destination_x,
            figure.destination_y,
            direction_limit,
            -1,
            5000);
    }

    if (terrain.requires_roads || terrain.prefers_roads) {
        const int can_travel_roads = terrain.allows_highways ?
            map_routing_citizen_can_travel_over_road_garden_highway(
                figure.x, figure.y, figure.destination_x, figure.destination_y, direction_limit, permission) :
            map_routing_citizen_can_travel_over_road_garden(
                figure.x, figure.y, figure.destination_x, figure.destination_y, direction_limit, permission);
        if (can_travel_roads || terrain.requires_roads) {
            return can_travel_roads;
        }
    }

    return map_routing_citizen_can_travel_over_land(
        figure.x,
        figure.y,
        figure.destination_x,
        figure.destination_y,
        direction_limit,
        permission);
}

int PathingMode::pathDirectionLimit(int default_direction_limit, const TerrainAccess &terrain) const
{
    return terrain.wall_grid ? 4 : default_direction_limit;
}

int PathingMode::citizenIsPassable(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] >= CITIZEN_0_ROAD &&
        terrain_land_citizen.items[grid_offset] <= CITIZEN_2_PASSABLE_TERRAIN;
}

int PathingMode::citizenIsRoad(int grid_offset)
{
    return terrain_land_citizen.items[grid_offset] == CITIZEN_0_ROAD;
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

const PathingMode *pathing_mode_from_xml_id(const char *xml_id)
{
    if (!xml_id) {
        return nullptr;
    }

    const PathingMode *modes[] = {
        &VanillaRoaming,
        &SmartService,
        &NearestUnemployed,
        &VenueSeeker,
        &StorageFetch,
        &FollowLeader,
        &StandStill,
        &TransientWander,
        &DepotOrderRoute,
        &WaterRoute
    };
    for (const PathingMode *mode : modes) {
        if (std::strcmp(xml_id, mode->xml_id) == 0) {
            return mode;
        }
    }
    return nullptr;
}

} // namespace figure_type_registry_impl
