#pragma once

#include "figure/route_policy.h"
#include "figure/type.h"
#include "map/road_service_history.h"

class Figure;

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

    enum class RoadblockRule {
        UseFigurePermission,
        IgnoreRoadblocks
    };

    struct TerrainAccess {
        int legacy_usage = TERRAIN_USAGE_ANY;
        bool requires_roads = false;
        bool prefers_roads = false;
        bool allows_highways = false;
        bool enemy_land = false;
        bool wall_grid = false;
        bool animal_land = false;

        bool usesRoadAccess() const
        {
            return requires_roads || prefers_roads;
        }

        bool usesNonCitizenPolicy() const
        {
            return enemy_land || animal_land;
        }

        bool usesEnemyLandRoute() const
        {
            return enemy_land;
        }

        bool usesAnimalLandRoute() const
        {
            return animal_land;
        }

        bool allowsRoadAccessFallback() const
        {
            return usesRoadAccess() && !requires_roads;
        }

        bool operator==(const TerrainAccess &other) const
        {
            return legacy_usage == other.legacy_usage &&
                requires_roads == other.requires_roads &&
                prefers_roads == other.prefers_roads &&
                allows_highways == other.allows_highways &&
                enemy_land == other.enemy_land &&
                wall_grid == other.wall_grid &&
                animal_land == other.animal_land;
        }
    };

    struct RoutePolicySelection {
        TerrainAccess terrain;
        RoutePolicy policy;
    };

    constexpr PathingMode(
        const char *xml_id,
        RoadRequirement road_requirement,
        ServiceEffectRequirement service_effect_requirement,
        VenueTargetRequirement venue_target_requirement,
        RoadblockRule roadblock_rule = RoadblockRule::UseFigurePermission)
        : xml_id(xml_id),
          requires_road(road_requirement == RoadRequirement::RequiresRoadMovement),
          requires_service_effect(service_effect_requirement == ServiceEffectRequirement::RequiresServiceEffect),
          requires_venue_targets(venue_target_requirement == VenueTargetRequirement::RequiresVenueTargets),
          roadblock_rule(roadblock_rule)
    {
    }

    // These public attributes are immutable metadata for XML/runtime validation.
    // Profiles store a pointer to one of the declared mode objects below.
    const char *xml_id;
    bool requires_road;
    bool requires_service_effect;
    bool requires_venue_targets;
    RoadblockRule roadblock_rule;

    roadblock_permission roadblockPermissionFor(const Figure &figure) const;
    static TerrainAccess terrainFromLegacyUsage(int terrain_usage);
    static RoutePolicy routePolicyForTerrain(
        const TerrainAccess &terrain,
        std::optional<roadblock_permission> permission = std::nullopt,
        RouteNeighborhood neighborhood = RouteNeighborhood::FourWay);
    static int citizenIsPassable(int grid_offset);
    static int citizenIsRoad(int grid_offset);
    static int citizenIsRoadLike(int grid_offset);
    static int citizenRoadNetworkAt(int grid_offset);
    static bool citizenIsInRoadNetwork(int grid_offset, int road_network);
    static bool citizenAreaTouchesRoadNetwork(
        int x_min,
        int y_min,
        int x_max,
        int y_max,
        int road_network);
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
extern const PathingMode CrossCountry;

struct PathingPolicy {
    const PathingMode *mode = &VanillaRoaming;
    PathingMode::TerrainAccess terrain = PathingMode::terrainFromLegacyUsage(TERRAIN_USAGE_ANY);
    road_service_effect effect = ROAD_SERVICE_EFFECT_NONE;

    bool hasRequiredServiceEffect() const;
    bool hasRequiredTerrainAccess() const;
    roadblock_permission roadblockPermissionFor(const Figure &figure) const;
    PathingMode::RoutePolicySelection routePolicySelection(
        roadblock_permission permission,
        RouteNeighborhood neighborhood) const;
};

const PathingMode *pathing_mode_from_xml_id(const char *xml_id);

} // namespace figure_type_registry_impl
