#include "building/building.h"
#include "temple.h"

#include "building/distribution.h"
#include "building/monument.h"
#include "city/resource.h"
#include "game/resource.h"

#define INFINITE 10000
#define MAX_FOOD 600
#define MAX_MESS_HALL_FOOD 1600

Building building_temple_get_storage_destination(Building temple)
{
    if (temple.type && temple.type->is_venus_temple()) {
        if (!temple.accepts_good(resource_wine()) || temple.accepts_good(resource_wine()) <= 1) {
            return Building(nullptr);
        }
        Building grand_temple(building_get(building_monument_get_venus_gt()));
        if (grand_temple.id != 0 && grand_temple.road_network_id() == temple.road_network_id() &&
            temple.resource_amount(resource_wine()) < BASELINE_STOCK &&
            grand_temple.resource_amount(resource_wine()) > 0) {
            temple.set_fetch_inventory_id(resource_wine());
            return grand_temple;
        }
        return Building(nullptr);
    }

    if (!temple.type || !temple.type->is_ceres_temple()) { // Ceres module 2
        return Building(nullptr);
    }

    resource_type food = city_resource_ceres_temple_food();

    if (food == RESOURCE_NONE) {
        return Building(nullptr);
    }

    const building_type_registry_impl::Distribution *distribution = temple.type->distribution();
    if (!distribution) {
        return Building(nullptr);
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    if (!distribution->needed_resources_for(temple, info)) {
        return Building(nullptr);
    }
    info[resource_oil()].needed = temple.accepts_good(resource_oil()) > 1;

    if (!distribution->find_sources_for_building(info, temple, INFINITE)) {
        return Building(nullptr);
    }

    // Get food if below threshold
    if (info[food].building_id && temple.resource_amount(food) < MAX_FOOD) {
        temple.set_fetch_inventory_id(food);
        return Building(building_get(info[food].building_id));
    }

    // Otherwise get allowed oil depending on stock
    resource_type fetch_resource = distribution->fetch_resource(temple, info, BASELINE_STOCK, 0, 0);
    if (fetch_resource == RESOURCE_NONE) {
        return Building(nullptr);
    }
    temple.set_fetch_inventory_id(fetch_resource);
    return Building(building_get(info[fetch_resource].building_id));
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
