#include "building/building.h"
#include "building/building_record.h"
#include "graphics/image.h"
#include "graphics/runtime_texture.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/tile_runtime_api.h"
#include "map/tile_runtime_graphics.h"
#include "map/tiles.h"

#include "city_draw.h"
#include "city_draw_highway.h"

#include "city/view.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/terrain.h"


static int highway_barrier_direction_offsets[4] = { 1, -GRID_SIZE, -1, GRID_SIZE };

static int has_adjacent_road(int adjacent_grid_offset, int direction_index)
{
    int right_direction = highway_barrier_direction_offsets[(direction_index + 3) % 4];
    int left_direction = highway_barrier_direction_offsets[(direction_index + 1) % 4];
    int left_has_road = map_terrain_is(adjacent_grid_offset + left_direction, TERRAIN_ROAD);
    int right_has_road = map_terrain_is(adjacent_grid_offset + right_direction, TERRAIN_ROAD);
    if (left_has_road && right_has_road) {
        return 1;
    } else if (left_has_road && map_terrain_is(adjacent_grid_offset + left_direction * 2, TERRAIN_ROAD)) {
        return 1;
    } else if (right_has_road && map_terrain_is(adjacent_grid_offset + right_direction * 2, TERRAIN_ROAD)) {
        return 1;
    }
    return 0;
}

static int is_highway_access(int grid_offset, int direction_index)
{
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY | TERRAIN_GATEHOUSE | TERRAIN_ACCESS_RAMP)) {
        return 1;
    }
    if (map_terrain_is(grid_offset, TERRAIN_ROAD) && !has_adjacent_road(grid_offset, direction_index)) {
        return 1;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        Building &building_object = map_building_at(grid_offset);
        const building *b = building_object.record();
        if (b && building_object.type && building_object.type->is_granary()) {
            return grid_offset == b->grid_offset + map_grid_delta(1, 0) ||
                grid_offset == b->grid_offset + map_grid_delta(0, 1) ||
                grid_offset == b->grid_offset + map_grid_delta(2, 1) ||
                grid_offset == b->grid_offset + map_grid_delta(1, 2);
        }
    }
    return 0;
}

static void draw_highway_role(const char *role, int option_index, int x, int y, float scale, color_t color_mask)
{
    const RuntimeDrawSlice *slice = tile_runtime_get_role_footprint_slice("highway", role, option_index);
    if (slice && slice->is_valid()) {
        runtime_texture_draw(*slice, x, y, color_mask, scale);
    }
}

static void draw_barrier_image(int grid_offset, int direction_index, int x, int y, float scale, color_t color_mask)
{
    int direction = highway_barrier_direction_offsets[direction_index];

    int direction_offset = grid_offset + direction;
    if (is_highway_access(direction_offset, direction_index)) {
        return;
    }

    int last_direction_index = (direction_index + 3) % 4;
    int last_direction_offset = grid_offset + highway_barrier_direction_offsets[last_direction_index];
    // last barrier was a corner and will handle the rendering
    if (!is_highway_access(last_direction_offset, last_direction_index)) {
        return;
    }

    int barrier_offset = (direction_index + city_view_orientation() / 2) % 4;
    int next_direction_index = (direction_index + 1) % 4;
    int next_direction_offset = grid_offset + highway_barrier_direction_offsets[next_direction_index];
    // is this a corner?
    if (!is_highway_access(next_direction_offset, next_direction_index)) {
        // increment by 4 to get the corner image
        barrier_offset += 4;
    }
    draw_highway_role("tile_barrier", barrier_offset, x, y, scale, color_mask);
}

void city_draw_highway_footprint(int x, int y, float scale, int grid_offset, color_t color_mask)
{
    if (!city_draw_runtime_tile_footprint(grid_offset, x, y, color_mask, scale)) {
        int option_count = tile_runtime_role_option_count("highway", "tile_base");
        int option_index = option_count > 0 ? map_random_get(grid_offset) % option_count : 0;
        draw_highway_role("tile_base", option_index, x, y, scale, color_mask);
    }
    draw_barrier_image(grid_offset, 1, x, y, scale, color_mask);
    draw_barrier_image(grid_offset, 2, x, y, scale, color_mask);
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        int aqueduct_image_id = map_tiles_highway_get_aqueduct_image(grid_offset);
        Image::from_id(aqueduct_image_id).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale);
    }
    draw_barrier_image(grid_offset, 0, x, y, scale, color_mask);
    draw_barrier_image(grid_offset, 3, x, y, scale, color_mask);
}
