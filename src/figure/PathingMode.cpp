#include "figure/PathingMode.h"

#include "map/routing_data.h"
#include "map/terrain.h"

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
    return terrain.usesRoadAccess();
}

RoutePolicy PathingMode::routePolicyForTerrain(
    const TerrainAccess &terrain,
    std::optional<roadblock_permission> permission,
    RouteNeighborhood neighborhood)
{
    RoutePolicy policy;
    policy.permission = permission;
    policy.neighborhood = terrain.wall_grid ? RouteNeighborhood::FourWay : neighborhood;

    if (terrain.wall_grid) {
        policy.kind = RoutePolicyKind::Walls;
    } else if (terrain.enemy_land || terrain.animal_land) {
        policy.kind = RoutePolicyKind::NonCitizenLand;
    } else if (terrain.usesRoadAccess()) {
        policy.kind = terrain.allows_highways ?
            RoutePolicyKind::CitizenRoadGardenHighway :
            RoutePolicyKind::CitizenRoadGarden;
    } else {
        policy.kind = RoutePolicyKind::CitizenLand;
    }
    return policy;
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

int PathingMode::citizenIsRoadLike(int grid_offset)
{
    return citizenIsRoad(grid_offset) || map_terrain_is(grid_offset, TERRAIN_ACCESS_RAMP);
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
