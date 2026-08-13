#

#include "city_water_ghost.h"

#include "building/water_access_runtime.h"
#include "building/building_type.h"
#include "widget/city_building_ghost.h"

#include "city/view.h"

#include <cstring>

namespace {

const char *g_preview_access_type = "fountain";

void draw_water_structure_access(int x, int y, int grid_offset)
{
    if (!water_access_runtime_should_draw_overlay_at(grid_offset)) {
        return;
    }
    if (water_access_runtime_tile_has_access(grid_offset, "fountain")) {
        city_building_ghost_draw_fountain_range(x, y, grid_offset);
    } else if (water_access_runtime_tile_has_access(grid_offset, "well")) {
        city_building_ghost_draw_well_range(x, y, grid_offset);
    }
}

void draw_reservoir_access(int x, int y, int grid_offset)
{
    if (!water_access_runtime_should_draw_overlay_at(grid_offset)) {
        return;
    }
    if (water_access_runtime_tile_has_access(grid_offset, "reservoir")) {
        city_building_ghost_draw_reservoir_range(x, y, grid_offset);
    }
}

void draw_preview_access(int x, int y, int grid_offset)
{
    if (!water_access_runtime_should_draw_overlay_at(grid_offset)) {
        return;
    }
    if (water_access_runtime_tile_has_preview_highlight(grid_offset)) {
        if (g_preview_access_type && strcmp(g_preview_access_type, "reservoir") == 0) {
            city_building_ghost_draw_reservoir_range(x, y, grid_offset);
        } else if (g_preview_access_type && strcmp(g_preview_access_type, "aqueduct") == 0) {
            city_building_ghost_draw_aqueduct_range(x, y, grid_offset);
        } else if (g_preview_access_type && strcmp(g_preview_access_type, "well") == 0) {
            city_building_ghost_draw_well_range(x, y, grid_offset);
        } else if (g_preview_access_type && strcmp(g_preview_access_type, "latrines") == 0) {
            city_building_ghost_draw_latrines_range(x, y, grid_offset);
        } else {
            city_building_ghost_draw_fountain_range(x, y, grid_offset);
        }
    }
}

const char *preview_access_type_for_building(const building_type_registry_impl::BuildingType *definition)
{
    const char *access_type = water_access_runtime_primary_provider_access_text(definition);
    return access_type && *access_type ? access_type : "fountain";
}

} // namespace

void city_water_ghost_draw_water_structure_ranges(void)
{
    city_view_foreach_valid_map_tile(draw_water_structure_access);
}

void city_water_ghost_draw_reservoir_ranges(void)
{
    city_view_foreach_valid_map_tile(draw_reservoir_access);
}

void city_water_ghost_draw_preview(
    const building_type_registry_impl::BuildingType *definition,
    int primary_grid_offset,
    int secondary_grid_offset,
    int primary_rotation,
    int secondary_rotation)
{
    g_preview_access_type = preview_access_type_for_building(definition);
    water_access_runtime_begin_preview(
        definition, primary_grid_offset, secondary_grid_offset, primary_rotation, secondary_rotation);
    city_view_foreach_valid_map_tile(draw_preview_access);
    water_access_runtime_end_preview();
}
