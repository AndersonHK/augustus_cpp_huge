#pragma once

#include "building/roadblock.h"
#include "core/buffer.h"
#include "figure/figure.h"
#include "figure/route_policy.h"
#include "game/performance_tracker.h"
#include "map/point.h"

#include <optional>

typedef struct building building;

class Route {
public:
    struct Request {
        map_point source = { 0, 0 };
        map_point destination = { 0, 0 };
        RoutePolicy policy;
        performance_tracker_route_purpose purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_DISTANCE_QUERY;
        int max_tiles = 0;
        int only_through_building_id = 0;
        bool require_same_road_network = false;
        bool has_destination = false;

        static Request between(
            const map_point &source,
            const map_point &destination,
            RoutePolicy policy,
            performance_tracker_route_purpose purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_DISTANCE_QUERY);
        int sourceOffset() const;
        int destinationOffset() const;
        bool hasValidEndpoints() const;
        bool acceptsDestinationDistance(int distance) const;
        bool canReachOverSurface() const;
        bool canReachWithBoundedRoadGardenDistanceField() const;
        bool prunesByRoadNetwork() const;
    };

    class Planner {
    public:
        static bool canReach(const Request &request);
    };

    struct RoadResult {
        int distance = 0;
        int grid_offset = 0;
        map_point road = { 0, 0 };

        explicit operator bool() const { return distance > 0; }
    };

    class DistanceQuery {
    public:
        static DistanceQuery fromRoad(
            const map_point &road,
            std::optional<roadblock_permission> permission = std::nullopt,
            performance_tracker_route_purpose purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_DISTANCE_QUERY);
        static DistanceQuery fromPoint(
            const map_point &point,
            std::optional<roadblock_permission> permission = std::nullopt,
            performance_tracker_route_purpose purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_DISTANCE_QUERY);
        static DistanceQuery fromFigure(
            Figure &figure,
            performance_tracker_route_purpose purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_DISTANCE_QUERY);

        explicit operator bool() const { return valid_; }

        int distanceTo(int gridOffset, int maxDistance = 0) const;
        RoadResult findRoad(const map_point &road, int maxDistance = 0) const;
        RoadResult findReachableRoad(int x, int y, int size, int radius, int maxDistance = 0) const;
        RoadResult findReachableTile(int x, int y, int size, int radius, int maxDistance = 0) const;
        RoadResult findRoadToLargestNetwork(int x, int y, int size) const;
        RoadResult findHippodromeRoadToLargestNetwork(int x, int y, bool rotated) const;
        RoadResult findMonumentConstructionRoadToLargestNetwork(int x, int y, int size) const;
        RoadResult findAccessRoad(const building &target, int radius, int maxDistance = 0, bool requireSameNetwork = false) const;

    private:
        class CostMapHandle {
        public:
            void seed(
                const map_point &source,
                const std::optional<roadblock_permission> &permission,
                performance_tracker_route_purpose purpose);
            int distanceAt(int gridOffset) const;
            int reachableDistanceAt(int gridOffset, int maxDistance = 0) const;

        private:
            int generation_ = -1;
        };

        DistanceQuery(
            const map_point &source,
            int sourceNetwork,
            std::optional<roadblock_permission> permission,
            performance_tracker_route_purpose purpose,
            bool valid);

        const CostMapHandle &costMap() const;
        RoadResult findBestReachableAreaTile(
            int x_min,
            int y_min,
            int x_max,
            int y_max,
            const CostMapHandle &cost_map,
            int maxDistance,
            bool requireRoad,
            bool requireSameNetwork) const;
        RoadResult findReachableAreaTile(int x, int y, int size, int radius, int maxDistance, bool requireRoad) const;

        map_point source_ = { 0, 0 };
        int sourceNetwork_ = 0;
        std::optional<roadblock_permission> permission_;
        performance_tracker_route_purpose purpose_ = PERFORMANCE_TRACKER_ROUTE_PURPOSE_DISTANCE_QUERY;
        bool valid_ = false;
        mutable CostMapHandle costMap_;
    };

    class TerrainQuery {
    public:
        static TerrainQuery enemyLandFrom(const map_point &source, int maxTiles, int onlyThroughBuildingId = 0, int directions = 8);

        explicit operator bool() const { return valid_; }
        int distanceTo(int gridOffset) const;
        bool canReach(int gridOffset) const { return distanceTo(gridOffset) > 0; }

    private:
        explicit TerrainQuery(bool valid) : valid_(valid) {}

        bool valid_ = false;
    };

    static void clearAll();
    static void clean();
    static void add(Figure &figure);
    static void add(Figure *figure) { if (figure) add(*figure); }
    static bool hasReusablePath(Figure &figure);
    static void remove(Figure &figure);
    static void remove(Figure *figure) { if (figure) remove(*figure); }
    static int currentDirection(const Figure &figure);
    static void advanceTile(Figure &figure);
    static bool calculateConstructionDistances(RoutePolicyKind kind, const map_point &source);
    static int constructionDistanceTo(int gridOffset);
    static bool waterCanReachAdjacentOpenWater(const map_point &source, int x, int y, int size);
    static int waterPathLength(const map_point &source, const map_point &destination, bool flotsam = false);
    static void blockDistanceArea(int x, int y, int size);
    static void deleteFirstWallOrAqueduct(int x, int y);
    static void updateAllTerrain();
    static void updateLandTerrain();
    static void updateCitizenLandTerrain();
    static void updateWaterTerrain();
    static void updateWallTerrain();
    static int wallIsPassable(int gridOffset);
    static int findWallTileInRadius(int x, int y, int radius, int *xWall, int *yWall);
    static void saveState(buffer *figures, buffer *paths);
    static void loadState(buffer *figures, buffer *paths, int version);
};
