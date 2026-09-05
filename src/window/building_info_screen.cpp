#include "window/building_info_screen.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/roadblock.h"
#include "window/building/common.h"

BuildingInfoScreenSelection BuildingInfoScreenSelection::resolve(
    const Building &building,
    int special_orders)
{
    // Storage is a presentation capability with its own roadblock sub-screen.
    // It must win over the more general owner-controlled foundation capability.
    if (building.type && building.type->is_storage()) {
        if (special_orders == SPECIAL_ORDERS_ROADBLOCK) {
            return BuildingInfoScreenSelection(BuildingInfoScreenId::StorageRoadblockOrders);
        }
        if (special_orders == SPECIAL_ORDERS_STORAGE || special_orders == SPECIAL_ORDERS_GENERIC) {
            return BuildingInfoScreenSelection(BuildingInfoScreenId::StorageOrders);
        }
        return BuildingInfoScreenSelection(BuildingInfoScreenId::Storage);
    }

    if (Roadblock(building).kind() == ROADBLOCK_FOUNDATION) {
        return BuildingInfoScreenSelection(
            special_orders ? BuildingInfoScreenId::FoundationRoadblockOrders : BuildingInfoScreenId::FoundationRoadblock);
    }

    return BuildingInfoScreenSelection(BuildingInfoScreenId::Other);
}

BuildingInfoScreenSelection::BuildingInfoScreenSelection(BuildingInfoScreenId id)
    : id_(id)
{
}

BuildingInfoScreenId BuildingInfoScreenSelection::id() const
{
    return id_;
}

bool BuildingInfoScreenSelection::isStorageFamily() const
{
    return id_ == BuildingInfoScreenId::Storage || id_ == BuildingInfoScreenId::StorageOrders ||
        id_ == BuildingInfoScreenId::StorageRoadblockOrders;
}

bool BuildingInfoScreenSelection::isFoundationRoadblockFamily() const
{
    return id_ == BuildingInfoScreenId::FoundationRoadblock ||
        id_ == BuildingInfoScreenId::FoundationRoadblockOrders;
}
