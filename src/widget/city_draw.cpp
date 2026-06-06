#include "widget/city_draw.h"

#include "graphics/runtime_texture.h"
#include "map/tile_runtime_graphics.h"

int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale)
{
    const RuntimeDrawSlice *slice = tile_runtime_get_graphic_footprint_slice(grid_offset);
    if (!slice) {
        return 0;
    }

    runtime_texture_draw(*slice, x, y, color_mask, scale);
    return 1;
}
