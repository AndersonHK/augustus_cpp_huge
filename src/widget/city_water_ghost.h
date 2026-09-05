#pragma once


namespace building_type_registry_impl {
class BuildingType;
}

void city_water_ghost_draw_water_structure_ranges(void);
void city_water_ghost_draw_reservoir_ranges(void);
void city_water_ghost_draw_preview(
    const building_type_registry_impl::BuildingType *definition,
    int primary_grid_offset,
    int secondary_grid_offset,
    int primary_rotation,
    int secondary_rotation);
