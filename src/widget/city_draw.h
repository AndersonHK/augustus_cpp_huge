#pragma once

#include "city/view_render.h"
#include "graphics/color.h"

class Building;

void city_draw_grid_overlay(int x, int y, float scale);
void city_draw_prepare_render_tile_rows(CityViewRenderCommandBuffer &commands);
void city_draw_render_tile_rows(const CityViewRenderCommandBuffer &commands, const CityViewRenderPhase *phases, int phase_count);
void city_draw_depot_resource(const Building &building, int x, int y, float scale);
int city_draw_building_as_deleted(const Building &building);
int city_draw_is_multi_tile_terrain(int grid_offset);
int city_draw_should_draw_top_before_deletion(int grid_offset);
int city_draw_terrain_foundation_footprint(
    int grid_offset, int x, int y, color_t color_mask, float scale);

// Input: a terrain grid offset plus the tile draw origin.
// Output: draws the native runtime terrain footprint for that tile and returns 1 when a native tile slice exists.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale);
int city_draw_runtime_tile_top(int grid_offset, int x, int y, color_t color_mask, float scale);
