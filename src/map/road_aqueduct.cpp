#include "road_aqueduct.h"

#include "building/building.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/connectable.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "figure/route.h"
#include "figure/PathingMode.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/road_aqueduct_rules.h"
#include "map/terrain.h"

static road_aqueduct_axis straight_road_axis_for_aqueduct(int grid_offset);

static int axis_has_construction_conflict(int grid_offset, road_aqueduct_axis axis, int include_existing_roads)
{
    const int dx = axis == road_aqueduct_axis::x ? 1 : 0;
    const int dy = axis == road_aqueduct_axis::y ? 1 : 0;
    if (!dx && !dy) {
        return 1;
    }

    const int before = grid_offset + map_grid_delta(-dx, -dy);
    const int after = grid_offset + map_grid_delta(dx, dy);
    return (include_existing_roads &&
            (map_terrain_is(before, TERRAIN_ROAD) || map_terrain_is(after, TERRAIN_ROAD))) ||
        Route::constructionDistanceTo(before) > 0 ||
        Route::constructionDistanceTo(after) > 0;
}

int map_can_place_road_under_aqueduct(int grid_offset)
{
    const int view_swaps_axes =
        city_view_orientation() == DIR_6_LEFT || city_view_orientation() == DIR_2_RIGHT;
    const road_aqueduct_axis aqueduct_axis = road_aqueduct_axis_from_connectable_option(
        building_connectable_get_aqueduct_offset(grid_offset), view_swaps_axes);
    return !axis_has_construction_conflict(grid_offset, aqueduct_axis, 1);
}

road_preview_graphic map_road_preview_graphic_at(int grid_offset)
{
    road_preview_graphic preview;
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        preview.image_id = image_group(GROUP_BUILDING_AQUEDUCT);
        if (map_can_place_road_under_aqueduct(grid_offset)) {
            preview.image_id += map_get_aqueduct_with_road_image(grid_offset);
        } else {
            preview.blocked = 1;
        }
        return preview;
    }

    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) &&
        figure_type_registry_impl::PathingMode::gateIsTransformable(grid_offset)) {
        const building_type source_type = map_building_type_at(grid_offset);
        preview.building = static_cast<building_type>(building_connectable_gate_type(source_type));
        return preview;
    }

    if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
        preview.blocked = 1;
        return preview;
    }

    preview.image_id = image_group(GROUP_TERRAIN_ROAD);
    if (!map_terrain_has_adjacent_x_with_type(grid_offset, TERRAIN_ROAD) &&
        map_terrain_has_adjacent_y_with_type(grid_offset, TERRAIN_ROAD)) {
        preview.image_id++;
    }
    return preview;
}

int map_can_place_aqueduct_on_road(int grid_offset)
{
    const road_aqueduct_axis road_axis = straight_road_axis_for_aqueduct(grid_offset);
    if (road_axis == road_aqueduct_axis::none) {
        return 0;
    }
    if (map_terrain_count_directly_adjacent_with_types(grid_offset, TERRAIN_ROAD | TERRAIN_AQUEDUCT)) {
        return 0;
    }

    return !axis_has_construction_conflict(grid_offset, road_axis, 0);
}

int map_get_aqueduct_with_road_image(int grid_offset)
{
    int image_id = map_image_at(grid_offset) - image_group(GROUP_BUILDING_AQUEDUCT);
    switch (image_id) {
        case 2:
            return 8;
        case 17:
            return 23;
        case 3:
            return 9;
        case 18:
            return 24;
        case 0:
        case 1:
        case 8:
        case 9:
        case 15:
        case 16:
        case 23:
        case 24:
            // unchanged
            return image_id;
        default:
            // shouldn't happen
            return 8;
    }
}

static int is_road_tile_for_aqueduct(int grid_offset, int gate_orientation)
{
    int is_road = map_terrain_is(grid_offset, TERRAIN_ROAD) ? 1 : 0;
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && map_building_exists_at(grid_offset)) {
        Building &current = map_building_at(grid_offset);
        if (current.Foundation &&
            current.Foundation->passage_at(grid_offset) != building_type_registry_impl::FoundationPassage::None) {
            if (!map_terrain_is(grid_offset, TERRAIN_GATEHOUSE) ||
                current.Foundation->passage_axis() == gate_orientation) {
                is_road = 1;
            }
        }
    }
    return is_road;
}

int map_is_straight_road_for_aqueduct(int grid_offset)
{
    return straight_road_axis_for_aqueduct(grid_offset) != road_aqueduct_axis::none;
}

static road_aqueduct_axis straight_road_axis_for_aqueduct(int grid_offset)
{
    int road_tiles_x =
        is_road_tile_for_aqueduct(grid_offset + map_grid_delta(1, 0), 2) +
        is_road_tile_for_aqueduct(grid_offset + map_grid_delta(-1, 0), 2);
    int road_tiles_y =
        is_road_tile_for_aqueduct(grid_offset + map_grid_delta(0, -1), 1) +
        is_road_tile_for_aqueduct(grid_offset + map_grid_delta(0, 1), 1);

    return road_aqueduct_axis_from_opposite_neighbors(road_tiles_x, road_tiles_y);
}

static int is_highway(int x, int y, int check_routing)
{
    int grid_offset = map_grid_offset(x, y);
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    } else if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        return 1;
    } else if (check_routing) {
        for (int xx = x - 1; xx <= x; xx++) {
            for (int yy = y - 1; yy <= y; yy++) {
                int routing_grid_offset = map_grid_offset(xx, yy);
                if (Route::constructionDistanceTo(routing_grid_offset) > 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int is_aqueduct(int x, int y, int check_routing)
{
    int grid_offset = map_grid_offset(x, y);
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        return 1;
    }
    if (map_building_exists_at(grid_offset) && map_building_at(grid_offset).matches("reservoir")) {
        return 1;
    } else if (check_routing && Route::constructionDistanceTo(grid_offset) > 0) {
        return 1;
    }
    return 0;
}

static int is_highway_and_aqueduct(int x, int y, int check_highway_routing, int check_aqueduct_routing)
{
    if (is_highway(x, y, check_highway_routing) && is_aqueduct(x, y, check_aqueduct_routing)) {
        return 1;
    }
    return 0;
}

// check to see if placing an aqueduct here would create an aqueduct corner on a highway
// note: this tile does NOT need to be on a highway to create a corner on one
static int aqueduct_placement_creates_corner_from_edge(int x, int y, int corner_x, int corner_y, int check_aqueduct_routing)
{
    if (!is_highway_and_aqueduct(corner_x, corner_y, 0, check_aqueduct_routing)) {
        return 0;
    }
    int c1x, c1y, c2x, c2y;
    map_grid_get_corner_tiles(x, y, corner_x, corner_y, &c1x, &c1y, &c2x, &c2y);
    if (is_aqueduct(c1x, c1y, check_aqueduct_routing) || is_aqueduct(c2x, c2y, check_aqueduct_routing)) {
        return 1;
    }
    return 0;
}

// check to see if placing an aqueduct here would create a corner due to two adjacent tiles having aqueducts
// note: this tile DOES need to be on a highway
static int aqueduct_placement_creates_corner_from_center(int x, int y)
{
    if (is_aqueduct(x - 1, y, 0) && is_aqueduct(x, y - 1, 0)) {
        return 1;
    } else if (is_aqueduct(x, y - 1, 0) && is_aqueduct(x + 1, y, 0)) {
        return 1;
    } else if (is_aqueduct(x + 1, y, 0) && is_aqueduct(x, y + 1, 0)) {
        return 1;
    } else if (is_aqueduct(x, y + 1, 0) && is_aqueduct(x - 1, y, 0)) {
        return 1;
    }
    return 0;
}

// check to see if placing an aqueduct here would create a line (at least two aqueduct tiles) along a highway
static int aqueduct_highway_line(int x, int y, int is_check_x, int check_highway_routing, int check_aqueduct_routing)
{
    int x_offs = 1;
    int y_offs = 0;
    if (!is_check_x) {
        x_offs = 0;
        y_offs = 1;
    }
    int left_occupied = is_highway_and_aqueduct(x - x_offs, y - y_offs, check_highway_routing, check_aqueduct_routing);
    int right_occupied = is_highway_and_aqueduct(x + x_offs, y + y_offs, check_highway_routing, check_aqueduct_routing);
    if (left_occupied && right_occupied) {
        return 1;
    } else if (left_occupied && is_highway(x - x_offs * 2, y - y_offs * 2, check_highway_routing)) {
        return 1;
    } else if (right_occupied && is_highway(x + x_offs * 2, y + y_offs * 2, check_highway_routing)) {
        return 1;
    }
    return 0;
}

int map_can_place_aqueduct_on_highway(int grid_offset, int check_aqueduct_routing)
{
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);

    if (aqueduct_placement_creates_corner_from_edge(x, y, x + 1, y, check_aqueduct_routing)) {
        return 0;
    } else if (aqueduct_placement_creates_corner_from_edge(x, y, x - 1, y, check_aqueduct_routing)) {
        return 0;
    } else if (aqueduct_placement_creates_corner_from_edge(x, y, x, y + 1, check_aqueduct_routing)) {
        return 0;
    } else if (aqueduct_placement_creates_corner_from_edge(x, y, x, y - 1, check_aqueduct_routing)) {
        return 0;
    }

    if (!map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        return 1;
    }

    if (aqueduct_placement_creates_corner_from_center(x, y)) {
        return 0;
    } else if (aqueduct_highway_line(x, y, 1, 0, check_aqueduct_routing)) {
        return 0;
    } else if (aqueduct_highway_line(x, y, 0, 0, check_aqueduct_routing)) {
        return 0;
    }

    return 1;
}

int map_can_place_highway_under_aqueduct(int grid_offset, int check_highway_routing)
{
    if (!map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        return 1;
    }

    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    if (aqueduct_placement_creates_corner_from_center(x, y)) {
        return 0;
    } else if (aqueduct_highway_line(x, y, 1, check_highway_routing, 0)) {
        return 0;
    } else if (aqueduct_highway_line(x, y, 0, check_highway_routing, 0)) {
        return 0;
    }

    return 1;
}
