#pragma once

#include "building/building.h"
#include "building/distribution.h"

class Market : public Building {
public:
    using Building::Building;
    explicit Market(Building building) : Building(building.legacy_record(), building.type_definition()) {}

    int max_food_stock() const;
    int max_supplier_distance() const;
    int needed_inventory(resource_storage_info info[RESOURCE_SLOT_COUNT]) const;
    int resource_storages_for_supplier(resource_storage_info info[RESOURCE_SLOT_COUNT], figure *supplier) const;
    resource_type fetch_inventory(resource_storage_info data[RESOURCE_SLOT_COUNT]) const;
    int storage_destination();

private:
    int handles_distribution(resource_type resource) const;
    int supply_search_distance() const;
    int wants_good(resource_type resource) const;
};
