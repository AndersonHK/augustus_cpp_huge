#include "city/trade_ledger.h"
#include "building/BuildingCityService.h"
#include "building/building.h"
#include "building/monument.h"
#include "building/building_type_registry_internal.h"
#include "map/terrain.h"
#include <algorithm>
#include <limits>

using namespace building_type_registry_impl;

const CityServiceDefinition *BuildingCityService::definition() const
{
    const auto *type = building_.type;
    return type && type->city_service().enabled() ? &type->city_service() : nullptr;
}

int BuildingCityService::infrastructure_units() const
{
    const auto *service = definition();
    const auto *target = service ? definition_for_type(type_from_attr(service->infrastructure)) : nullptr;
    if (!target || !target->infrastructure().terrain_mask) return 0;
    return map_terrain_count(target->infrastructure().terrain_mask) / target->infrastructure().tiles_per_unit;
}

int BuildingCityService::demand(resource_type resource) const
{
    const auto *service = definition();
    if (!service) return 0;
    const int units = infrastructure_units();
    for (const auto &input : service->inputs) {
        if (input.resource == resource) {
            const int64_t periods = (static_cast<int64_t>(units) + service->units_per_input - 1) / service->units_per_input;
            return static_cast<int>(std::min<int64_t>(periods * input.loads * resource_units_per_load(), std::numeric_limits<int>::max()));
        }
    }
    return 0;
}

int BuildingCityService::stock_target(resource_type resource) const
{
    const auto *service = definition();
    // Building inventories use signed 16-bit units in the save ABI. Never wrap a large city's reserve.
    const int maximum = std::numeric_limits<short>::max() / resource_units_per_load() * resource_units_per_load();
    return service && service->input_source == ResourceConsumptionSource::Building ? static_cast<int>(std::min<int64_t>(static_cast<int64_t>(demand(resource)) * service->stock_periods, maximum)) : 0;
}

int BuildingCityService::available_input(resource_type resource) const
{
    const auto *service = definition();
    // Retain and use stock already stored by an older save or an in-flight delivery.
    const int64_t local = building_.resource_amount(resource);
    const int64_t global = service && service->input_source == ResourceConsumptionSource::GlobalStockpile ? resource_stockpile_amount(resource) : 0;
    return static_cast<int>(std::min<int64_t>(local + global, std::numeric_limits<int>::max()));
}

int BuildingCityService::delivery_loads_needed(resource_type resource) const
{
    const int reserved = building_monument_resource_in_delivery(const_cast<building *>(building_.record()), resource);
    return std::max(0, (stock_target(resource) - building_.resource_amount(resource)) / resource_units_per_load() - reserved);
}

int BuildingCityService::receive_load(resource_type resource)
{
    const auto *service = definition();
    if (!service || !building_.is_in_use()) return 0;
    if (std::none_of(service->inputs.begin(), service->inputs.end(), [resource](const auto &input) { return input.resource == resource; })) return 0;
    if (building_.resource_amount(resource) > std::numeric_limits<short>::max() - resource_units_per_load()) return 0;
    building_.add_resource(resource, resource_units_per_load());
    building_.invalidate_graphic();
    return 1;
}

bool BuildingCityService::operational() const
{
    const auto *service = definition();
    if (!service || !building_.is_in_use() || building_.worker_count() <= 0) return false;
    for (const auto &input : service->inputs) {
        if (available_input(input.resource) < demand(input.resource)) return false;
    }
    return true;
}

void BuildingCityService::consume_monthly()
{
    const auto *service = definition();
    if (!service || !building_.is_in_use()) return;
    if (service->input_source == ResourceConsumptionSource::GlobalStockpile) {
        std::vector<ResourceConsumptionAmount> inputs;
        for (const auto &input : service->inputs) inputs.push_back({input.resource, std::max(0, demand(input.resource) - building_.resource_amount(input.resource))});
        if (!resource_stockpile_consume(inputs)) return;
        for (const auto &input : service->inputs) {
            const int local = std::min(demand(input.resource), building_.resource_amount(input.resource));
            building_.add_resource(input.resource, -local);
            city_trade_ledger_consumed(input.resource, local);
        }
        building_.invalidate_graphic();
        return;
    }
    // A complete period is consumed atomically, even when an existing staffed service has lost its workers.
    for (const auto &input : service->inputs) if (building_.resource_amount(input.resource) < demand(input.resource)) return;
    for (const auto &input : service->inputs) {
        const int amount = demand(input.resource);
        building_.add_resource(input.resource, -amount);
        city_trade_ledger_consumed(input.resource, amount);
    }
    building_.invalidate_graphic();
}

static int service_percent(building_type target, bool construction)
{
    int percent = 100;
    Building::for_each(BuildingRuntimeList::CityServices, [&](Building *building) {
        BuildingCityService service(*building);
        const auto *definition = service.definition();
        if (definition && type_from_attr(definition->infrastructure) == target && service.operational()) {
            percent = std::min(percent, construction ? definition->construction_percent : definition->levy_percent);
        }
    });
    return percent;
}

int city_service_construction_cost(building_type target, int cost)
{
    return static_cast<int>(static_cast<int64_t>(cost) * service_percent(target, true) / 100);
}

int city_service_monthly_infrastructure_levies()
{
    int64_t levies = 0;
    for (const auto &type : g_building_types) {
        if (!type || !type->infrastructure().terrain_mask || !type->infrastructure().monthly_levy) continue;
        const auto &infrastructure = type->infrastructure();
        const int units = map_terrain_count(infrastructure.terrain_mask) / infrastructure.tiles_per_unit;
        levies += static_cast<int64_t>(units) * infrastructure.monthly_levy * service_percent(type->type(), false) / 100;
    }
    return static_cast<int>(std::min<int64_t>(levies, std::numeric_limits<int>::max()));
}

void city_service_consume_monthly()
{
    Building::for_each(BuildingRuntimeList::CityServices, [](Building *building) { BuildingCityService(*building).consume_monthly(); });
}
