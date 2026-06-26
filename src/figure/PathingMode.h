#pragma once

#include "figure/type.h"
#include "map/road_service_history.h"

namespace figure_type_registry_impl {

class PathingMode {
public:
    enum class RoadRequirement {
        AllowsNonRoadMovement,
        RequiresRoadMovement
    };

    enum class ServiceEffectRequirement {
        NoServiceEffect,
        RequiresServiceEffect
    };

    enum class VenueTargetRequirement {
        NoVenueTargets,
        RequiresVenueTargets
    };

    struct TerrainAccess {
        int legacy_usage = TERRAIN_USAGE_ANY;
        bool requires_roads = false;
        bool prefers_roads = false;
        bool allows_highways = false;
        bool enemy_land = false;
        bool wall_grid = false;
        bool animal_land = false;
    };

    constexpr PathingMode(
        const char *xml_id,
        RoadRequirement road_requirement,
        ServiceEffectRequirement service_effect_requirement,
        VenueTargetRequirement venue_target_requirement)
        : xml_id(xml_id),
          requires_road(road_requirement == RoadRequirement::RequiresRoadMovement),
          requires_service_effect(service_effect_requirement == ServiceEffectRequirement::RequiresServiceEffect),
          requires_venue_targets(venue_target_requirement == VenueTargetRequirement::RequiresVenueTargets)
    {
    }

    // These public attributes are immutable metadata for XML/runtime validation.
    // Profiles store a pointer to one of the declared mode objects below.
    const char *xml_id;
    bool requires_road;
    bool requires_service_effect;
    bool requires_venue_targets;

    static TerrainAccess terrainFromLegacyUsage(int terrain_usage);
    static bool terrainRequiresRoads(const TerrainAccess &terrain);
    static int citizenIsPassable(int grid_offset);
    static int citizenIsRoad(int grid_offset);
    static int citizenIsRoadLike(int grid_offset);
    static int citizenIsHighway(int grid_offset);
    static int citizenIsPassableTerrain(int grid_offset);
    static int gateIsTransformable(int grid_offset);
    static int noncitizenIsPassable(int grid_offset);
};

extern const PathingMode VanillaRoaming;
extern const PathingMode SmartService;
extern const PathingMode NearestUnemployed;
extern const PathingMode VenueSeeker;
extern const PathingMode StorageFetch;
extern const PathingMode FollowLeader;
extern const PathingMode StandStill;
extern const PathingMode TransientWander;
extern const PathingMode DepotOrderRoute;
extern const PathingMode WaterRoute;

struct PathingPolicy {
    const PathingMode *mode = &VanillaRoaming;
    PathingMode::TerrainAccess terrain = PathingMode::terrainFromLegacyUsage(TERRAIN_USAGE_ANY);
    road_service_effect effect = ROAD_SERVICE_EFFECT_NONE;
};

const PathingMode *pathing_mode_from_xml_id(const char *xml_id);

} // namespace figure_type_registry_impl
