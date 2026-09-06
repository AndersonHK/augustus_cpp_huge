#include "building/resource_consumption.h"
#include "building/building.h"
#include "building/warehouse.h"
#include "building/granary.h"
#include "city/trade_ledger.h"
#include "core/log.h"
#include <algorithm>
#include <limits>
#include <map>

int resource_stockpile_amount(resource_type resource, bool include_granaries)
{
    if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) return 0;
    int64_t amount = 0;
    Building::for_each(BuildingRuntimeList::Warehouses, [&](Building *warehouse) {
        if (warehouse->is_in_use() && !warehouse->has_plague()) amount += building_warehouse_get_amount(*warehouse, resource);
    });
    if (include_granaries && resource_is_food(resource)) Building::for_each(BuildingRuntimeList::Granaries, [&](Building *granary) {
        if (granary->is_in_use() && !granary->has_plague()) amount += building_granary_get_amount(*granary, resource);
    });
    return static_cast<int>(std::min<int64_t>(amount, std::numeric_limits<int>::max()));
}

static bool collect_requirements(const std::vector<ResourceConsumptionAmount> &inputs, std::map<resource_type, int64_t> &totals, bool include_granaries)
{
    for (const auto &input : inputs) {
        if (input.resource <= RESOURCE_NONE || input.resource >= RESOURCE_SLOT_COUNT || input.amount < 0) return false;
        totals[input.resource] += input.amount;
    }
    for (const auto &[resource, amount] : totals) if (amount > resource_stockpile_amount(resource, include_granaries)) return false;
    return true;
}

bool resource_stockpile_has(const std::vector<ResourceConsumptionAmount> &inputs, bool include_granaries)
{
    std::map<resource_type, int64_t> totals;
    return collect_requirements(inputs, totals, include_granaries);
}

bool resource_stockpile_consume(const std::vector<ResourceConsumptionAmount> &inputs, bool include_granaries)
{
    std::map<resource_type, int64_t> totals;
    if (!collect_requirements(inputs, totals, include_granaries)) return false;
    // Simulation-thread transaction: preflight every input before debiting any.
    // Availability and withdrawal use the same in-use/non-plague warehouse owners.
    for (const auto &[resource, amount] : totals) {
        int remaining = building_warehouses_remove_resource(resource, static_cast<int>(amount));
        if (remaining && include_granaries && resource_is_food(resource)) Building::for_each(BuildingRuntimeList::Granaries, [&](Building *granary) {
            if (remaining && granary->is_in_use() && !granary->has_plague()) remaining -= building_granary_try_remove_resource(*granary, resource, remaining);
        });
        if (remaining) { log_error("Global stockpile changed during consumption", resource_text_id(resource), remaining); return false; }
        city_trade_ledger_consumed(resource, static_cast<int>(amount));
    }
    return true;
}
