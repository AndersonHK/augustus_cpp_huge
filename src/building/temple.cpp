#include "building/building.h"
#include "temple.h"

#include "building/distribution.h"
#include "building/monument.h"
#include "city/resource.h"
#include "game/resource.h"

#define INFINITE 10000
#define MAX_FOOD 600
#define MAX_MESS_HALL_FOOD 1600

Building *building_temple_get_storage_destination(Building &temple)
{
    auto building_for_id = [](unsigned int building_id) -> Building * {
        if (!building_id) {
            return nullptr;
        }
        Building *destination = nullptr;
        Building::for_each([&](Building *building) {
            if (!destination && building->id == building_id) {
                destination = building;
            }
        });
        return destination;
    };
    auto destination_for_resource = [&](resource_type resource, unsigned int building_id) -> Building * {
        Building *destination = building_for_id(building_id);
        if (destination) {
            temple.set_fetch_inventory_id(resource);
        }
        return destination;
    };

    if (temple.type->is_temple(GOD_VENUS)) {
        if (!temple.accepts_good(resource_wine()) || !temple.distribution_demand(resource_wine())) {
            return nullptr;
        }
        Building *grand_temple = grand_temple_for_god(GOD_VENUS, false);
        if (grand_temple && grand_temple->road_network_id() == temple.road_network_id() &&
            temple.resource_amount(resource_wine()) < BASELINE_STOCK &&
            grand_temple->resource_amount(resource_wine()) > 0) {
            temple.set_fetch_inventory_id(resource_wine());
            return grand_temple;
        }
        return nullptr;
    }

    if (!temple.type->is_temple(GOD_CERES)) { // Ceres module 2
        return nullptr;
    }

    resource_type food = city_resource_ceres_temple_food();

    if (food == RESOURCE_NONE) {
        return nullptr;
    }

    const building_type_registry_impl::Distribution *distribution = temple.type->distribution();
    if (!distribution) {
        return nullptr;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    if (!distribution->needed_resources_for(temple, info)) {
        return nullptr;
    }
    info[resource_oil()].needed = temple.distribution_demand(resource_oil()) > 0;

    if (!distribution->find_sources_for_building(info, temple, INFINITE)) {
        return nullptr;
    }

    // Get food if below threshold
    if (info[food].building_id && temple.resource_amount(food) < MAX_FOOD) {
        return destination_for_resource(food, info[food].building_id);
    }

    // Otherwise get allowed oil depending on stock
    resource_type fetch_resource = distribution->fetch_resource(temple, info, BASELINE_STOCK, 0, 0);
    if (fetch_resource == RESOURCE_NONE) {
        return nullptr;
    }
    return destination_for_resource(fetch_resource, info[fetch_resource].building_id);
}

int building_temple_mars_food_to_deliver(Building temple, Building mess_hall)
{
    int most_stocked_food_id = -1;
    int next;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        next = temple.resource_amount(r);
        if (next > most_stocked_food_id && next >= 100 && mess_hall.resource_amount(r) <= MAX_MESS_HALL_FOOD) {
            most_stocked_food_id = r;
        }
    }
    return most_stocked_food_id;
}
