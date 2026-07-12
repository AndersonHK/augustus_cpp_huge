#include "building/building.h"
#include "building/list.h"
#include "map/building.h"

#include "water_supply.h"

#include "building/water_access_runtime.h"
#include "building/building_type_registry_internal.h"

#include "map/grid.h"
#include "map/terrain.h"

void map_water_supply_update_buildings(void)
{
    water_access_runtime_refresh();
}

void map_water_supply_update_reservoir_fountain(void)
{
    water_access_runtime_refresh();
}

int map_water_supply_has_aqueduct_access(int grid_offset)
{
    return water_access_runtime_reservoir_has_network_access(grid_offset);
}

void map_water_supply_refresh_building(Building *building)
{
    water_access_runtime_refresh_building(building);
}

int map_water_supply_is_building_unnecessary(Building *building, int radius)
{
    if (!building) {
        return BUILDING_UNNECESSARY_NO_HOUSES;
    }
    int num_houses = 0;
    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(building->x(), building->y(), 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            Building *found_building = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
            if (found_building && found_building->has_house_size()) {
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

int map_water_supply_fountain_radius(void)
{
    return water_access_runtime_range_for_building(
        building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr("fountain")));
}

int map_water_supply_reservoir_radius(void)
{
    return water_access_runtime_range_for_building(
        building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr("reservoir")));
}

int map_water_supply_well_radius(void)
{
    return water_access_runtime_range_for_building(
        building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr("well")));
}

int map_water_supply_latrines_radius(void)
{
    return water_access_runtime_range_for_building(
        building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr("latrines")));
}
