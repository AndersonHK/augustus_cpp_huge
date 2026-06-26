#include "building/building.h"
#include "mess_hall.h"

#include "building/distribution.h"
#include "game/resource.h"

#define MAX_DISTANCE 40
int building_mess_hall_get_storage_destination(Building mess_hall)
{
    const building_type_registry_impl::Distribution *distribution =
        mess_hall.type ? mess_hall.type->distribution() : nullptr;
    if (!distribution) {
        return 0;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };

    if (!distribution->needed_resources_for(mess_hall, info) ||
        !distribution->find_sources_for_building(info, mess_hall, MAX_DISTANCE)) {
        return 0;
    }
    // Prefer whichever food we don't have
    resource_type fetch_inventory = distribution->fetch_resource(mess_hall, info, 0, 0, 1);
    if (fetch_inventory != RESOURCE_NONE) {
        mess_hall.set_fetch_inventory_id(fetch_inventory);
        return info[fetch_inventory].building_id;
    }
    // Then prefer smallest stock below baseline stock
    fetch_inventory = distribution->fetch_resource(mess_hall, info, BASELINE_STOCK, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        mess_hall.set_fetch_inventory_id(fetch_inventory);
        return info[fetch_inventory].building_id;
    }    
    // All items well stocked: use the XML stock target.
    fetch_inventory = distribution->fetch_resource(mess_hall, info, 0, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        mess_hall.set_fetch_inventory_id(fetch_inventory);
        return info[fetch_inventory].building_id;
    }
    return 0;
}
