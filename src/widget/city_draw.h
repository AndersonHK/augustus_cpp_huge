#pragma once

#include "city/view_render.h"
#include "graphics/color.h"

class Building;
struct image;

void city_draw_grid_overlay(int x, int y, float scale);
void city_draw_main_render_tile_row(
    city_view_render_tile_callback *callback1,
    city_view_render_tile_callback *callback2,
    city_view_render_tile_callback *callback3,
    performance_tracker_bucket bucket1,
    performance_tracker_bucket bucket2,
    performance_tracker_bucket bucket3);
void city_draw_depot_resource(const Building &building, int x, int y, float scale);
void city_draw_warehouse_ornaments(int x, int y, color_t color_mask, float scale);
void city_draw_granary_stores(const image &image, Building &building, int x, int y, color_t color_mask, float scale);
int city_draw_building_as_deleted(const Building &building);
int city_draw_is_multi_tile_terrain(int grid_offset);
int city_draw_should_draw_top_before_deletion(int grid_offset);

// Input: a terrain grid offset plus the tile draw origin.
// Output: draws the native runtime terrain footprint for that tile and returns 1 when a native tile slice exists.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale);
int city_draw_runtime_tile_top(int grid_offset, int x, int y, color_t color_mask, float scale);
int city_draw_storage_permission_flag(Building &building, int x, int y, color_t color_mask, float scale);
