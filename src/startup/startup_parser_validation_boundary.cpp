#ifdef STARTUP_PARSER_TEST

#include "building/building_type.h"
#include "scenario/allowed_building.h"
#include "scenario/map.h"

int building_is_house(building_type type)
{
    (void) type;
    return 0;
}

int scenario_map_has_fishing_points(void)
{
    return 0;
}

int scenario_allowed_building(building_type type)
{
    (void) type;
    return 0;
}

#endif // STARTUP_PARSER_TEST
