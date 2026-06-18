#include "building/building.h"
#include "building/list.h"
#include "map/building.h"

#include "water_supply.h"

#include "building/water_access_runtime.h"
#include "building/building_type_registry_internal.h"

#include <cstring>

extern "C" {
#include "building/building_record.h"
#include "map/grid.h"
#include "map/terrain.h"
}

static building_type building_type_from_definition_attr(const char *text_id)
{
    for (int type = 1; type < BUILDING_TYPE_MAX; type++) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(static_cast<building_type>(type));
        if (definition && definition->attr() && text_id && std::strcmp(definition->attr(), text_id) == 0) {
            return static_cast<building_type>(type);
        }
    }
    return BUILDING_NONE;
}

extern "C" void map_water_supply_update_buildings(void)
{
    water_access_runtime_refresh();
}

extern "C" void map_water_supply_update_reservoir_fountain(void)
{
    water_access_runtime_refresh();
}

extern "C" int map_water_supply_has_aqueduct_access(int grid_offset)
{
    return water_access_runtime_reservoir_has_network_access(grid_offset);
}

extern "C" void map_water_supply_refresh_building(building *b)
{
    water_access_runtime_refresh_building(b);
}

extern "C" int map_water_supply_is_building_unnecessary(int building_id, int radius)
{
    building *b = building_get(building_id);
    int num_houses = 0;
    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(b->x, b->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            unsigned int found_building_id = map_building_at(grid_offset);
            if (found_building_id && building_get(found_building_id)->house_size) {
                num_houses++;
                if (!water_access_runtime_tile_has_access(grid_offset, "fountain") &&
                    !map_terrain_is(grid_offset, TERRAIN_FOUNTAIN_RANGE)) {
                    return BUILDING_NECESSARY;
                }
            }
        }
    }
    return num_houses ? BUILDING_UNNECESSARY_FOUNTAIN : BUILDING_UNNECESSARY_NO_HOUSES;
}

extern "C" int map_water_supply_fountain_radius(void)
{
    return water_access_runtime_range_for_building(building_type_from_definition_attr("fountain"));
}

extern "C" int map_water_supply_reservoir_radius(void)
{
    return water_access_runtime_range_for_building(building_type_from_definition_attr("reservoir"));
}

extern "C" int map_water_supply_well_radius(void)
{
    return water_access_runtime_range_for_building(building_type_from_definition_attr("well"));
}

extern "C" int map_water_supply_latrines_radius(void)
{
    return water_access_runtime_range_for_building(building_type_from_definition_attr("latrines"));
}
