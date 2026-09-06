#pragma once

#include "game/resource.h"
#include <vector>

enum class ResourceConsumptionSource { Building, GlobalStockpile };

struct ResourceConsumptionAmount {
    resource_type resource = RESOURCE_NONE;
    int amount = 0;
};

// Global stockpile means actual warehouse contents, including maintaining stocks.
// Plague-closed warehouses cannot provide resources. Quantities are resource units.
int resource_stockpile_amount(resource_type resource, bool include_granaries = false);
bool resource_stockpile_has(const std::vector<ResourceConsumptionAmount> &inputs, bool include_granaries = false);
bool resource_stockpile_consume(const std::vector<ResourceConsumptionAmount> &inputs, bool include_granaries = false);
