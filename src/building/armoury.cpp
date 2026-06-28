#include "armoury.h"

#include "building/building.h"
#include "building/barracks.h"
#include "building/building_type_registry_internal.h"
#include "building/count.h"
#include "building/distribution.h"
#include "building/monument.h"
#include "city/resource.h"
#include "map/point.h"

int Armoury::is_needed() const
{
    if (!building_count_active(building_type_registry_impl::type_from_attr("barracks")) &&
        !grand_temple_for_god(GOD_MARS, false)) {
        return 0;
    }

    if (city_resource_is_stockpiled(resource_weapons()) ||
        !Barracks::for_weapon(x(), y(), resource_weapons(), road_network_id(), nullptr)) {
        return 0;
    }

    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    if (!distribution) {
        return 0;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    return distribution->needed_resources_for(*this, info) &&
        distribution->find_sources_for_building(info, *this, 10000);
}
