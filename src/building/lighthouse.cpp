#include "building/building.h"
#include "lighthouse.h"

#include "assets/assets.h"
#include "building/distribution.h"
#include "building/monument.h"
#include "building/building_type_registry_internal.h"
#include "building/building_type.h"
#include "city/trade_policy.h"
#include "core/calc.h"

#define INFINITE 10000
#define TIMBER_CONSUMPTION 20
#define TIMBER_LOW 100

static building_type lighthouse_type()
{
    return building_type_registry_impl::type_from_attr("lighthouse");
}

int building_lighthouse_enough_timber(Building lighthouse)
{
    return lighthouse.resource_amount(resource_timber()) > TIMBER_LOW;
}

Building building_lighthouse_first(void)
{
    return *Building::first_of_type(lighthouse_type());
}

Building *building_lighthouse_get_storage_destination(Building lighthouse)
{
    const building_type_registry_impl::Distribution *distribution =
        lighthouse.type ? lighthouse.type->distribution() : nullptr;
    if (!distribution) {
        return nullptr;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    if (!distribution->needed_resources_for(lighthouse, info) ||
        !distribution->find_sources_for_building(info, lighthouse, INFINITE)) {
        return nullptr;
    }

    resource_type resource = distribution->fetch_resource(lighthouse, info, 0, 0, 1);
    if (resource == RESOURCE_NONE) {
        return nullptr;
    }
    Building *destination = info[resource].source;
    if (destination) {
        lighthouse.set_fetch_inventory_id(resource);
    }
    return destination;
}

int building_lighthouse_is_fully_functional(void)
{
    const building_type type = lighthouse_type();
    if (!building_monument_working(type)) {
        return 0;
    }

    return building_lighthouse_enough_timber(building_lighthouse_first());
}

static void set_lighthouse_graphic(Building lighthouse)
{
    if (!lighthouse.is_in_use()) {
        return;
    }
    if (lighthouse.type && lighthouse.type->has_phased_construction()) {
        lighthouse.refresh_graphic();
    } else if (lighthouse.Foundation) {
        lighthouse.Foundation->refresh();
        lighthouse.refresh_graphic();
    }
}

void building_lighthouse_consume_timber(void)
{
    const building_type type = lighthouse_type();
    if (building_monument_working(type)) {
        Building lighthouse = building_lighthouse_first();
        int timber = lighthouse.resource_amount(resource_timber());
        if (timber > 0) {
            trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);
            int consume = TIMBER_CONSUMPTION;

            if (policy == TRADE_POLICY_3) { // consume 20% more
                consume = calc_adjust_with_percentage(consume, 100 + POLICY_3_MALUS_PERCENT);
            }

            lighthouse.set_resource_amount(resource_timber(), timber - consume < 0 ? 0 : timber - consume);
        }
        set_lighthouse_graphic(lighthouse);
    }
}
