#include "figure/PathingMode.h"

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
    PathingMode::VenueTargetRequirement::RequiresVenueTargets,
    PathingMode::RoadblockRule::IgnoreRoadblocks);
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
    PathingMode::RoadRequirement::AllowsNonRoadMovement,
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
const PathingMode CrossCountry(
    "cross_country",
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
    } else if (terrain.usesNonCitizenPolicy()) {
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

bool PathingPolicy::hasRequiredServiceEffect() const
{
    return mode && (!mode->requires_service_effect || effect != ROAD_SERVICE_EFFECT_NONE);
}

bool PathingPolicy::hasRequiredTerrainAccess() const
{
    return mode && (!mode->requires_road || terrain.usesRoadAccess());
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
        &WaterRoute,
        &CrossCountry
    };
    for (const PathingMode *mode : modes) {
        if (std::strcmp(xml_id, mode->xml_id) == 0) {
            return mode;
        }
    }
    return nullptr;
}

} // namespace figure_type_registry_impl
