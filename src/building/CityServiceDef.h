#pragma once

#include "game/resource.h"
#include "building/resource_consumption.h"
#include <cstdint>
#include <string>
#include <vector>

namespace building_type_registry_impl {

struct InfrastructureDefinition {
    uint32_t terrain_mask = 0;
    int tiles_per_unit = 1;
    int monthly_levy = 0;
};

struct CityServiceInput {
    resource_type resource = RESOURCE_NONE;
    int loads = 1;
};

struct CityServiceDefinition {
    ResourceConsumptionSource input_source = ResourceConsumptionSource::Building;
    std::string infrastructure;
    int units_per_input = 1;
    int stock_periods = 1;
    int construction_percent = 100;
    int levy_percent = 100;
    std::vector<CityServiceInput> inputs;
    bool enabled() const { return !infrastructure.empty(); }
};

}
