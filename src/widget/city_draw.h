#pragma once

#include "graphics/color.h"

class Building;

// Input: a terrain grid offset plus the tile draw origin.
// Output: draws the native runtime terrain footprint for that tile and returns 1 when a native tile slice exists.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale);
int city_draw_runtime_tile_top(int grid_offset, int x, int y, color_t color_mask, float scale);
int city_draw_storage_permission_flag(Building &building, int x, int y, color_t color_mask, float scale);
