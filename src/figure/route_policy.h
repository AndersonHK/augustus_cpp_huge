#pragma once

#include "building/roadblock.h"
#include "game/performance_tracker.h"
#include "map/routing.h"

#include <cstdint>
#include <optional>

enum class RoutePolicyKind : std::uint8_t {
    CitizenLand,
    CitizenRoadGarden,
    CitizenRoadGardenHighway,
    NonCitizenLand,
    Walls,
    WaterBoat,
    WaterFlotsam,
    ConstructionRoad,
    ConstructionHighway,
    ConstructionWall,
    ConstructionAqueduct,
    ConstructionDraggableReservoir
};

enum class RouteNeighborhood : std::uint8_t {
    FourWay = 4,
    EightWay = 8
};

inline RouteNeighborhood route_neighborhood_from_direction_limit(int direction_limit)
{
    return direction_limit == 8 ? RouteNeighborhood::EightWay : RouteNeighborhood::FourWay;
}

struct RoutePolicy {
    RoutePolicyKind kind = RoutePolicyKind::CitizenLand;
    RouteNeighborhood neighborhood = RouteNeighborhood::FourWay;
    std::optional<roadblock_permission> permission;

    static RoutePolicy fromKind(
        RoutePolicyKind kind,
        RouteNeighborhood neighborhood = RouteNeighborhood::FourWay)
    {
        RoutePolicy policy;
        policy.kind = kind;
        policy.neighborhood = neighborhood;
        return policy;
    }

    static RoutePolicy water(
        bool flotsam,
        RouteNeighborhood neighborhood = RouteNeighborhood::FourWay)
    {
        return fromKind(flotsam ? RoutePolicyKind::WaterFlotsam : RoutePolicyKind::WaterBoat, neighborhood);
    }

    static RoutePolicy nonCitizenLand(RouteNeighborhood neighborhood = RouteNeighborhood::FourWay)
    {
        return fromKind(RoutePolicyKind::NonCitizenLand, neighborhood);
    }

    int directionLimit() const
    {
        return static_cast<int>(neighborhood);
    }

    bool isConstruction() const
    {
        return kind == RoutePolicyKind::ConstructionRoad ||
            kind == RoutePolicyKind::ConstructionHighway ||
            kind == RoutePolicyKind::ConstructionWall ||
            kind == RoutePolicyKind::ConstructionAqueduct ||
            kind == RoutePolicyKind::ConstructionDraggableReservoir;
    }

    routed_building_type constructionBuildingType() const
    {
        switch (kind) {
            case RoutePolicyKind::ConstructionHighway:
                return ROUTED_BUILDING_HIGHWAY;
            case RoutePolicyKind::ConstructionWall:
                return ROUTED_BUILDING_WALL;
            case RoutePolicyKind::ConstructionAqueduct:
                return ROUTED_BUILDING_AQUEDUCT;
            case RoutePolicyKind::ConstructionDraggableReservoir:
                return ROUTED_BUILDING_DRAGGABLE_RESERVOIR;
            case RoutePolicyKind::ConstructionRoad:
            default:
                return ROUTED_BUILDING_ROAD;
        }
    }

    bool isNonCitizenLand() const
    {
        return kind == RoutePolicyKind::NonCitizenLand;
    }

    RoutePolicy asNonCitizenLand() const
    {
        RoutePolicy policy = *this;
        policy.kind = RoutePolicyKind::NonCitizenLand;
        return policy;
    }

    bool isWalls() const
    {
        return kind == RoutePolicyKind::Walls;
    }

    bool isCitizenRoadGarden() const
    {
        return kind == RoutePolicyKind::CitizenRoadGarden;
    }

    bool isCitizenRoadGardenHighway() const
    {
        return kind == RoutePolicyKind::CitizenRoadGardenHighway;
    }

    RoutePolicy withoutRoadAccess() const
    {
        RoutePolicy policy = *this;
        policy.kind = RoutePolicyKind::CitizenLand;
        return policy;
    }

    int pathDirectionLimit() const
    {
        return isWalls() || isConstruction() ?
            4 :
            directionLimit();
    }

    roadblock_permission roadblockPermission() const
    {
        return permission.value_or(PERMISSION_NONE);
    }

    bool isWater() const
    {
        return kind == RoutePolicyKind::WaterBoat ||
            kind == RoutePolicyKind::WaterFlotsam;
    }

    bool isWaterFlotsam() const
    {
        return kind == RoutePolicyKind::WaterFlotsam;
    }

    performance_tracker_route_purpose performancePurpose() const
    {
        if (isWater()) {
            return PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER;
        }
        if (isWalls()) {
            return PERFORMANCE_TRACKER_ROUTE_PURPOSE_WALL;
        }
        return PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT;
    }

    bool operator==(const RoutePolicy &other) const
    {
        return kind == other.kind &&
            neighborhood == other.neighborhood &&
            permission == other.permission;
    }
};
