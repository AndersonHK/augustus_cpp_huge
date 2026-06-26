#pragma once

#include "building/roadblock.h"
#include "core/buffer.h"
#include "figure/figure.h"
#include "map/point.h"

#include <optional>
#include <vector>

typedef struct building building;
struct road_access_area;

class Route {
public:
    struct RoadResult {
        int distance = 0;
        int grid_offset = 0;
        map_point road = { 0, 0 };

        explicit operator bool() const { return distance > 0; }
    };

    class DistanceQuery {
    public:
        static DistanceQuery fromRoad(const map_point &road, std::optional<roadblock_permission> permission = std::nullopt);
        static DistanceQuery fromPoint(const map_point &point, std::optional<roadblock_permission> permission = std::nullopt);
        static DistanceQuery fromFigure(const Figure &figure);

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
        DistanceQuery(const map_point &source, int sourceNetwork, std::optional<roadblock_permission> permission, bool valid);

        void seedDistanceGrid() const;
        int distanceFor(const RoadResult &candidate, int maxDistance) const;
        RoadResult findReachableAreaTile(int x, int y, int size, int radius, int maxDistance, bool requireRoad) const;
        RoadResult bestRoadToLargestNetwork(
            const std::vector<road_access_area> &areas,
            bool fallback_to_shortest_distance) const;

        map_point source_ = { 0, 0 };
        int sourceNetwork_ = 0;
        std::optional<roadblock_permission> permission_;
        bool valid_ = false;
        mutable int distanceGridGeneration_ = -1;
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
    static void remove(Figure &figure);
    static void remove(Figure *figure) { if (figure) remove(*figure); }
    static int currentDirection(const Figure &figure);
    static void advanceTile(Figure &figure);
    static int waterPathLength(const map_point &source, const map_point &destination, bool flotsam = false);
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
