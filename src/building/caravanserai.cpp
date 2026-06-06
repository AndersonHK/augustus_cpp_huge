#include "building/building.h"
#include "caravanserai.h"

#include "building/distribution.h"
#include "building/building_type_registry_internal.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/trade.h"
#include "city/resource.h"
#include "empire/city.h"

#define INFINITE 10000

int building_caravanserai_enough_foods(Building caravanserai)
{
    int food_required_monthly = building_caravanserai_food_required_monthly();
    int total_food_in_caravanserai = 0;

    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        total_food_in_caravanserai += caravanserai.resource_amount(r);
    }

    return total_food_in_caravanserai >= food_required_monthly;
}

int building_caravanserai_food_required_monthly(void)
{
    int is_sea_trade = 0;
    int is_route_open = 1;
    int open_land_routes = empire_city_get_trade_routes_count(is_sea_trade, is_route_open);
    return open_land_routes * FOOD_PER_LAND_ROUTE_MONTHLY;

}

int building_caravanserai_get_storage_destination(Building caravanserai)
{
    const building_type_registry_impl::Distribution *distribution = caravanserai.type().distribution();
    if (!distribution) {
        return 0;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };

    if (!distribution->needed_resources_for(caravanserai, info) ||
        !distribution->find_sources_for_building(info, caravanserai, INFINITE)) {
        return 0;
    }
    // Prefer whichever food we don't have
    resource_type fetch_inventory = distribution->fetch_resource(caravanserai, info, 0, 0, 1);
    if (fetch_inventory != RESOURCE_NONE) {
        caravanserai.set_fetch_inventory_id(fetch_inventory);
        return info[fetch_inventory].building_id;
    }
    // Then prefer smallest stock below baseline stock
    fetch_inventory = distribution->fetch_resource(caravanserai, info, BASELINE_STOCK, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        caravanserai.set_fetch_inventory_id(fetch_inventory);
        return info[fetch_inventory].building_id;
    }
    // All items well stocked: use the XML stock target.
    fetch_inventory = distribution->fetch_resource(caravanserai, info, 0, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        caravanserai.set_fetch_inventory_id(fetch_inventory);
        return info[fetch_inventory].building_id;
    }
    return 0;
}

int building_caravanserai_is_fully_functional(void)
{
    if (!building_monument_working(building_type_registry_impl::runtime_id_from_text("caravanserai"))) {
        return 0;
    }

    return building_caravanserai_enough_foods(Building::from_id(city_buildings_get_caravanserai()));
}
