#include "building/building_record.h"
#include "building_tiles.h"

#include "building/building.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/tiles.h"

static void map_legacy_terrain_tiles_add_remove(
    int x,
    int y,
    int size,
    int image_id,
    int terrain_to_add,
    int terrain_to_remove)
{
    if (!map_grid_is_inside(x, y, size)) {
        return;
    }
    int x_leftmost, y_leftmost;
    switch (city_view_orientation()) {
        case DIR_0_TOP:
            x_leftmost = 0;
            y_leftmost = size - 1;
            break;
        case DIR_2_RIGHT:
            x_leftmost = y_leftmost = 0;
            break;
        case DIR_4_BOTTOM:
            x_leftmost = size - 1;
            y_leftmost = 0;
            break;
        case DIR_6_LEFT:
            x_leftmost = y_leftmost = size - 1;
            break;
        default:
            return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            map_terrain_remove(grid_offset, terrain_to_remove);
            map_terrain_add(grid_offset, terrain_to_add);
            map_building_clear_at(grid_offset);
            map_property_clear_constructing(grid_offset);
            map_property_set_legacy_multi_tile_size(grid_offset, size);
            map_image_set(grid_offset, image_id);
            map_property_set_multi_tile_xy(grid_offset, dx, dy,
                dx == x_leftmost && dy == y_leftmost);
        }
    }
}

void map_terrain_tiles_add(int x, int y, int size, int image_id, int terrain)
{
    map_legacy_terrain_tiles_add_remove(x, y, size, image_id, terrain, TERRAIN_CLEARABLE);
}

static int legacy_north_tile_grid_offset(int x, int y, int *size)
{
    int grid_offset = map_grid_offset(x, y);
    *size = map_property_legacy_multi_tile_size(grid_offset);
    for (int i = 0; i < *size && map_property_multi_tile_x(grid_offset); i++) {
        grid_offset += map_grid_delta(-1, 0);
    }
    for (int i = 0; i < *size && map_property_multi_tile_y(grid_offset); i++) {
        grid_offset += map_grid_delta(0, -1);
    }
    return grid_offset;
}

void map_legacy_building_tiles_remove(int x, int y)
{
    if (!map_grid_is_inside(x, y, 1)) {
        return;
    }
    int size;
    int base_grid_offset = legacy_north_tile_grid_offset(x, y, &size);
    x = map_grid_offset_to_x(base_grid_offset);
    y = map_grid_offset_to_y(base_grid_offset);
    if (map_terrain_get(base_grid_offset) == TERRAIN_ROCK) {
        return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            map_property_clear_constructing(grid_offset);
            map_property_set_legacy_multi_tile_size(grid_offset, 1);
            map_property_clear_multi_tile_xy(grid_offset);
            map_property_mark_draw_tile(grid_offset);
            map_aqueduct_remove(grid_offset);
            map_building_clear_at(grid_offset);
            map_building_damage_clear(grid_offset);
            map_sprite_clear_tile(grid_offset);
            if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
                map_terrain_set(grid_offset, TERRAIN_WATER); // clear other flags
                map_tiles_set_water(x + dx, y + dy);
            } else {
                map_image_set(grid_offset,
                    image_group(GROUP_TERRAIN_UGLY_GRASS) +
                    (map_random_get(grid_offset) & 7));
                map_terrain_remove(grid_offset, TERRAIN_CLEARABLE & ~TERRAIN_HIGHWAY);
            }
        }
    }
    map_tiles_update_region_empty_land(x, y, x + size, y + size);
    map_tiles_update_region_meadow(x, y, x + size, y + size);
    map_tiles_update_region_rubble(x, y, x + size, y + size);
}


void map_building_tiles_add_rubble(Building &building, int x, int y)
{
    if (!map_grid_is_inside(x, y, 1)) {
        return;
    }

    int grid_offset = map_grid_offset(x, y);
    if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
        map_terrain_set(grid_offset, TERRAIN_WATER);
        map_tiles_set_water(x, y);
        return;
    }

    map_property_clear_constructing(grid_offset);
    map_property_set_legacy_multi_tile_size(grid_offset, 1);
    map_property_set_multi_tile_xy(grid_offset, 0, 0, 1);
    map_property_mark_draw_tile(grid_offset);
    map_aqueduct_remove(grid_offset);
    map_building_damage_clear(grid_offset);
    map_sprite_clear_tile(grid_offset);
    map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
    map_terrain_add(grid_offset, TERRAIN_RUBBLE | TERRAIN_BUILDING);
    map_building_set(grid_offset, building);
    map_building_set_rubble_grid_building_id(grid_offset, building.id, 1);
}

void map_building_tiles_add_bridge(Building &building, int x, int y)
{
    if (!map_grid_is_inside(x, y, 1)) {
        return;
    }

    const int grid_offset = map_grid_offset(x, y);
    map_property_clear_constructing(grid_offset);
    map_property_set_legacy_multi_tile_size(grid_offset, 1);
    map_property_set_multi_tile_xy(grid_offset, 0, 0, 1);
    map_terrain_add(grid_offset, TERRAIN_WATER | TERRAIN_ROAD | TERRAIN_BUILDING);
    map_building_set(grid_offset, building);
    map_tiles_set_water(x, y);
    map_sprite_clear_tile(grid_offset);
}

static void adjust_to_absolute_xy(int *x, int *y, int size)
{
    switch (city_view_orientation()) {
        case DIR_2_RIGHT:
            *x = *x - size + 1;
            break;
        case DIR_4_BOTTOM:
            *x = *x - size + 1;
            // fall-through
        case DIR_6_LEFT:
            *y = *y - size + 1;
            break;
    }
}

int map_building_tiles_mark_construction(int x, int y, int size, int terrain, int absolute_xy)
{
    if (!absolute_xy) {
        adjust_to_absolute_xy(&x, &y, size);
    }
    if (!map_grid_is_inside(x, y, size)) {
        return 0;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            if (map_terrain_is(grid_offset, terrain & TERRAIN_NOT_CLEAR) || map_has_figure_at(grid_offset)) {
                return 0;
            }
        }
    }
    // mark as being constructed
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            map_property_mark_constructing(grid_offset);
        }
    }
    return 1;
}

void map_building_tiles_mark_deleting(int grid_offset)
{
    if (map_is_bridge(grid_offset)) {
        // previous version triggered map_bridge_remove with an early exit condition for regular terrain.
        map_bridge_remove(grid_offset, 1);
    } else if (map_building_exists_at(grid_offset)) {
        Building &selected = map_building_at(grid_offset);
        Building *owner = selected.Composition ? selected.Composition->owner() : &selected;
        grid_offset = owner ? owner->grid_offset() : grid_offset;
    }
    map_property_mark_deleted(grid_offset);
}
