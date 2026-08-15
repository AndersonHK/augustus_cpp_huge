#pragma once

#include "building/building.h"
#include "figure/route.h"
#include "map/point.h"

namespace building_local_workforce {

class RouteAccessSelectorContext;

struct HouseRouteSelection {
    Building *house = nullptr;
    Route::RoadResult access_road;

    explicit operator bool() const;
    const map_point &road() const;
};

class RouteAccessSelector {
public:
    static RouteAccessSelector fromRoad(
        const map_point &source_road,
        int max_distance,
        const RoutePolicy &route_policy,
        RouteAccessSelectorContext &context);

    explicit operator bool() const;
    HouseRouteSelection nearestUnemployedHouse() const;
    HouseRouteSelection nearestAssignedSourceReleasingUnreachable(Building &workplace) const;

private:
    static constexpr int kHouseAccessRadius = 2;

    RouteAccessSelector(
        const map_point &source_road,
        int max_distance,
        bool allow_highways,
        Route::DistanceQuery route_query,
        RouteAccessSelectorContext &context);

    int houseAccessAreaTouchesSourceNetwork(const Building &house) const;
    int houseRecordIsLiveLaborSource(const building *house) const;
    void recordNetworkPrune() const;
    Route::RoadResult findHouseAccessRoad(const Building &house, const building &house_record) const;
    HouseRouteSelection bestSelection(
        const HouseRouteSelection &current,
        Building &house,
        const Route::RoadResult &access_road) const;

    RouteAccessSelectorContext *context_ = nullptr;
    map_point source_ = { 0, 0 };
    int max_distance_ = 0;
    bool allow_highways_ = false;
    Route::DistanceQuery route_query_;
};

} // namespace building_local_workforce
