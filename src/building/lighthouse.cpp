#include "building/building.h"
#include "lighthouse.h"

#include "assets/assets.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/monument.h"
#include "building/building_type_registry_internal.h"
#include "building/building_type.h"
#include "city/trade_policy.h"
#include "core/calc.h"
#include "map/building_tiles.h"
#include "map/terrain.h"

#define INFINITE 10000
#define TIMBER_CONSUMPTION 20
#define TIMBER_LOW 100

int building_lighthouse_enough_timber(Building lighthouse)
{
    return lighthouse.resource_amount(resource_timber()) > TIMBER_LOW;
}

int building_lighthouse_get_storage_destination(Building lighthouse)
{
    const building_type_registry_impl::Distribution *distribution = lighthouse.type().distribution();
    if (!distribution) {
        return 0;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    if (!distribution->needed_resources_for(lighthouse, info) ||
        !distribution->find_sources_for_building(info, lighthouse, INFINITE)) {
        return 0;
    }

    resource_type resource = distribution->fetch_resource(lighthouse, info, 0, 0, 1);
    if (resource == RESOURCE_NONE) {
        return 0;
    }
    lighthouse.set_fetch_inventory_id(resource);
    return info[resource].building_id;
}

int building_lighthouse_is_fully_functional(void)
{
    building_type lighthouse_type = building_type_registry_impl::runtime_id_from_text("lighthouse");
    if (!building_monument_working(lighthouse_type)) {
        return 0;
    }

    return building_lighthouse_enough_timber(Building::first_of_type(lighthouse_type));
}

static void set_lighthouse_graphic(Building lighthouse)
{
    if (!lighthouse.is_in_use()) {
        return;
    }
    building *record = lighthouse.legacy_record();
    if (lighthouse.type().has_phased_construction()) {
        lighthouse.refresh_graphic();
    } else {
        map_building_tiles_add(
            lighthouse.id(),
            lighthouse.x(),
            lighthouse.y(),
            lighthouse.size(),
            building_image_get(record),
            TERRAIN_BUILDING);
    }
}

void building_lighthouse_consume_timber(void)
{
    building_type lighthouse_type = building_type_registry_impl::runtime_id_from_text("lighthouse");
    if (building_monument_working(lighthouse_type)) {
        Building lighthouse = Building::first_of_type(lighthouse_type);
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
