#include "tavern.h"

int Tavern::storage_destination()
{
    const building_type_registry_impl::Distribution *distribution = type().distribution();
    if (!distribution) {
        return 0;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    if (!distribution->needed_resources_for(*this, info) ||
        !distribution->find_sources_for_building(info, *this, 10000)) {
        return 0;
    }

    resource_type fetch_resource = distribution->fetch_resource(*this, info, BASELINE_STOCK, 1, 0);
    if (fetch_resource == RESOURCE_NONE) {
        fetch_resource = distribution->fetch_resource(*this, info, 0, 0, 0);
    }
    if (fetch_resource == RESOURCE_NONE) {
        return 0;
    }

    set_fetch_inventory_id(fetch_resource);
    return info[fetch_resource].building_id;
}
