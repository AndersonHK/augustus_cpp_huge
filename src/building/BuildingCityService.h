#pragma once

#include "building/CityServiceDef.h"
#include "building/building_type.h"

class Building;

class BuildingCityService {
public:
    explicit BuildingCityService(Building &building) : building_(building) {}
    const building_type_registry_impl::CityServiceDefinition *definition() const;
    int infrastructure_units() const;
    int demand(resource_type resource) const;
    int available_input(resource_type resource) const;
    int stock_target(resource_type resource) const;
    int delivery_loads_needed(resource_type resource) const;
    int receive_load(resource_type resource);
    bool operational() const;
    void consume_monthly();
private:
    Building &building_;
};

int city_service_construction_cost(building_type target, int cost);
int city_service_monthly_infrastructure_levies();
void city_service_consume_monthly();
