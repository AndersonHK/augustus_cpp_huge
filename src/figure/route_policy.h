#pragma once

#include "building/roadblock.h"

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
    ConstructionAqueduct
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

    int directionLimit() const
    {
        return static_cast<int>(neighborhood);
    }

    int pathDirectionLimit() const
    {
        switch (kind) {
            case RoutePolicyKind::Walls:
            case RoutePolicyKind::ConstructionRoad:
            case RoutePolicyKind::ConstructionHighway:
            case RoutePolicyKind::ConstructionWall:
            case RoutePolicyKind::ConstructionAqueduct:
                return 4;
            default:
                return directionLimit();
        }
    }

    bool isWater() const
    {
        return kind == RoutePolicyKind::WaterBoat ||
            kind == RoutePolicyKind::WaterFlotsam;
    }

    bool operator==(const RoutePolicy &other) const
    {
        return kind == other.kind &&
            neighborhood == other.neighborhood &&
            permission == other.permission;
    }
};
