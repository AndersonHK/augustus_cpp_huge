#include "building/local_workforce_route_access.h"

#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/local_workforce_runtime_lists.h"
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
    RouteAccessSelectorContext &context)
{
    return RouteAccessSelector(
        source_road,
        max_distance,
        Route::DistanceQuery::fromRoad(
            source_road,
            PERMISSION_LABOR_SEEKER,
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
    Route::DistanceQuery route_query,
    RouteAccessSelectorContext &context)
    : context_(&context),
      source_(source_road),
      max_distance_(max_distance),
      route_query_(route_query)
{}

int RouteAccessSelector::houseAccessAreaTouchesSourceNetwork(const Building &house) const
{
    return house.access_area_touches_same_road_network(source_, kHouseAccessRadius);
}

int RouteAccessSelector::houseRecordIsLiveLaborSource(const building *house) const
{
    const auto *definition = house ? building_type_registry_impl::definition_for_type(house->type) : nullptr;
    return house && house->id && house->state == BUILDING_STATE_IN_USE && definition && definition->has_housing();
}

void RouteAccessSelector::recordNetworkPrune() const
{
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_LOCAL_WORKFORCE,
        1);
}

Route::RoadResult RouteAccessSelector::findHouseAccessRoad(
    const Building &house,
    const building &house_record) const
{
    if (!*this) {
        return {};
    }
    if (!houseAccessAreaTouchesSourceNetwork(house)) {
        recordNetworkPrune();
        return {};
    }
    return route_query_.findAccessRoad(house_record, kHouseAccessRadius, max_distance_, true);
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

    context_->runtimeLists().forEachPopulatedLaborSourceHouse([&best, this](Building &house_object, building &house) {
        if (!context_->houseHasUnemployedWorkers(house_object, house)) {
            return;
        }

        best = bestSelection(best, house_object, findHouseAccessRoad(house_object, house));
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

            Building *house_object = Building::get(source.house_id);
            building *house = house_object ? const_cast<::building *>(house_object->record()) : nullptr;
            if (!houseRecordIsLiveLaborSource(house)) {
                house_ids_to_release.push_back(source.house_id);
                return;
            }

            const Route::RoadResult house_road = findHouseAccessRoad(*house_object, *house);
            if (!house_road) {
                house_ids_to_release.push_back(source.house_id);
                return;
            }

            best = bestSelection(best, *house_object, house_road);
        });

    for (unsigned int house_id : house_ids_to_release) {
        context_->releaseWorkplaceSource(workplace.id, house_id);
    }
    return best;
}

} // namespace building_local_workforce
