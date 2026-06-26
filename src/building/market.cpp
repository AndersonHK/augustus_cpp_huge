#include "market.h"

#include "core/config.h"
#include "map/data.h"

int Market::max_food_stock() const
{
    int max_stock = 0;
    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    if (!distribution) {
        return 0;
    }
    for (const building_type_registry_impl::DistributionResourceRule &rule : distribution->resources()) {
        if (!resource_is_food(rule.resource)) {
            continue;
        }
        int stock = resource_amount(rule.resource);
        if (stock > max_stock) {
            max_stock = stock;
        }
    }
    return max_stock;
}

int Market::max_supplier_distance() const
{
    return type ? type->market().max_distance() : 0;
}

int Market::needed_inventory(resource_storage_info info[RESOURCE_SLOT_COUNT]) const
{
    int needed = 0;
    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    if (!distribution) {
        return 0;
    }
    for (const building_type_registry_impl::DistributionResourceRule &rule : distribution->resources()) {
        const resource_type resource = rule.resource;
        info[resource].needed = handles_distribution(resource) && (accepts_good(resource) > 0 || wants_good(resource));
        if (!needed && info[resource].needed) {
            needed = 1;
        }
    }
    return needed;
}

int Market::resource_storages_for_supplier(resource_storage_info info[RESOURCE_SLOT_COUNT], Figure *supplier) const
{
    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    return distribution ? distribution->find_sources_for_figure(
        info, type ? type->type() : BUILDING_NONE, road_network_id(), supplier, supply_search_distance()) : 0;
}

resource_type Market::fetch_inventory(resource_storage_info info[RESOURCE_SLOT_COUNT]) const
{
    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    if (!distribution) {
        return RESOURCE_NONE;
    }

    // Prefer whichever good we don't have
    resource_type fetch_inventory = distribution->fetch_resource(*this, info, 0, 0, 1);
    if (fetch_inventory != RESOURCE_NONE) {
        return fetch_inventory;
    }
    // Then prefer smallest stock below baseline stock
    fetch_inventory = distribution->fetch_resource(*this, info, BASELINE_STOCK, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        return fetch_inventory;
    }

    fetch_inventory = distribution->fetch_resource(*this, info, 0, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        return fetch_inventory;
    }

    return RESOURCE_NONE;
}

int Market::storage_destination()
{
    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    if (!needed_inventory(info) ||
        !distribution ||
        !distribution->find_sources_for_building(info, *this, supply_search_distance())) {
        return 0;
    }
    resource_type resource = fetch_inventory(info);
    set_fetch_inventory_id(resource);
    return info[resource].building_id;
}

int Market::handles_distribution(resource_type resource) const
{
    const building_type_registry_impl::Distribution *distribution = type ? type->distribution() : nullptr;
    return distribution && distribution->handles_resource(resource);
}

int Market::supply_search_distance() const
{
    return config_get(CONFIG_GP_CH_MARKET_RANGE) ? max_supplier_distance() : map_data.width;
}

int Market::wants_good(resource_type resource) const
{
    return accepts_good(resource) > 1;
}
