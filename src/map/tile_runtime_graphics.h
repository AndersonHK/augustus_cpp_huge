#pragma once

#include "graphics/runtime_texture.h"

int tile_runtime_has_graphic(int grid_offset);
const RuntimeDrawSlice *tile_runtime_get_graphic_footprint_slice(int grid_offset);
const RuntimeDrawSlice *tile_runtime_get_graphic_top_slice(int grid_offset);
const RuntimeDrawSlice *tile_runtime_get_role_footprint_slice(const char *tile_kind, const char *role, int option_index);
