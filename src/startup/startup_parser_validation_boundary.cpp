#ifdef STARTUP_PARSER_TEST

#include "building/building_type.h"
#include "scenario/allowed_building.h"
#include "scenario/map.h"

int scenario_map_has_fishing_points(void)
{
    return 0;
}

int scenario_allowed_building(building_type type)
{
    (void) type;
    return 0;
}

int scenario_allowed_building(const building_type_registry_impl::BuildingType *type)
{
    (void) type;
    return 0;
}

// The parser process has no city or operational services; the executable gate tests dynamic pricing.
int city_service_construction_cost(building_type, int base_cost) { return base_cost; }

#endif // STARTUP_PARSER_TEST
