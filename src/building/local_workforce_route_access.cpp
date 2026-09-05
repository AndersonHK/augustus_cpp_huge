#include "building/local_workforce_route_access.h"

#include "building/local_workforce.h"
#include "game/performance_tracker.h"

#include <vector>

namespace building_local_workforce {

HouseRouteSelection::operator bool() const
{
    return house && house->id && access_road;
}

const map_point &HouseRouteSelection::road() const
{
    return access_road.road;
}

RouteAccessSelector RouteAccessSelector::fromRoad(
    const map_point &source_road,
    int max_distance,
    const RoutePolicy &route_policy,
    RouteAccessSelectorContext &context)
{
    return RouteAccessSelector(
        source_road,
        max_distance,
        route_policy.isCitizenRoadGardenHighway(),
        Route::DistanceQuery::fromRoad(
            source_road,
            route_policy,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_LOCAL_WORKFORCE),
        context);
}

RouteAccessSelector::operator bool() const
{
    return context_ && max_distance_ > 0 && route_query_;
}

RouteAccessSelector::RouteAccessSelector(
    const map_point &source_road,
    int max_distance,
    bool allow_highways,
    Route::DistanceQuery route_query,
    RouteAccessSelectorContext &context)
    : context_(&context),
      source_(source_road),
      max_distance_(max_distance),
      allow_highways_(allow_highways),
      route_query_(route_query)
{}

int RouteAccessSelector::houseAccessAreaTouchesSourceNetwork(const Building &house) const
{
    return house.access_area_touches_same_road_network(source_, kHouseAccessRadius, allow_highways_);
}

void RouteAccessSelector::recordNetworkPrune() const
{
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_LOCAL_WORKFORCE,
        1);
}

Route::RoadResult RouteAccessSelector::findHouseAccessRoad(const Building &house) const
{
    if (!*this) {
        return {};
    }
    if (!houseAccessAreaTouchesSourceNetwork(house)) {
        recordNetworkPrune();
        return {};
    }
    return route_query_.findAccessRoad(house, kHouseAccessRadius, max_distance_, true);
}

HouseRouteSelection RouteAccessSelector::bestSelection(
    const HouseRouteSelection &current,
    Building &house,
    const Route::RoadResult &access_road) const
{
    if (!access_road || (current && current.access_road.distance <= access_road.distance)) {
        return current;
    }
    return { &house, access_road };
}

HouseRouteSelection RouteAccessSelector::nearestUnemployedHouse() const
{
    HouseRouteSelection best;
    if (!*this) {
        return best;
    }

    context_->forEachPopulatedLaborSourceHouse([&best, this](Building &house) {
        if (!context_->houseHasUnemployedWorkers(house)) {
            return;
        }

        best = bestSelection(best, house, findHouseAccessRoad(house));
    });
    return best;
}

HouseRouteSelection RouteAccessSelector::nearestAssignedSourceReleasingUnreachable(Building &workplace) const
{
    HouseRouteSelection best;
    if (!*this || !context_->usesActiveWorkforce(workplace)) {
        return best;
    }

    std::vector<unsigned int> house_ids_to_release;
    context_->forEachAssignedSource(
        workplace.id,
        [this, &best, &house_ids_to_release](const AssignedWorkforceSource &source) {
            if (source.workers <= 0) {
                return;
            }

            Building *house = Building::get(source.house_id);
        if (!house || !house->is_labor_source_house()) {
                house_ids_to_release.push_back(source.house_id);
                return;
            }

            const Route::RoadResult house_road = findHouseAccessRoad(*house);
            if (!house_road) {
                house_ids_to_release.push_back(source.house_id);
                return;
            }

            best = bestSelection(best, *house, house_road);
        });

    for (unsigned int house_id : house_ids_to_release) {
        context_->releaseWorkplaceSource(workplace.id, house_id);
    }
    return best;
}

} // namespace building_local_workforce
