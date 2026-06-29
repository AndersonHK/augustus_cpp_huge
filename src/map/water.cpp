#include "building/building_record.h"
#include "water.h"

#include "building/building.h"
#include "building/building_type.h"
#include "building/image.h"
#include "city/view.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/point.h"
#include "map/property.h"
#include "map/terrain.h"
#include "scenario/map.h"
#include "figure/action.h"
#include "figure/figure_runtime_api.h"
#include "figure/route.h"

#define OFFSET(x,y) ((x) + GRID_SIZE * (y))

void map_water_add_building(Building &building, int x, int y, int size, int image_id)
{
    if (!map_grid_is_inside(x, y, size)) {
        return;
    }
    if (!image_id) {
        image_id = building.image_id();
    }
    map_point leftmost;
    switch (city_view_orientation()) {
        case DIR_0_TOP:
            leftmost.x = 0;
            leftmost.y = size - 1;
            break;
        case DIR_2_RIGHT:
            leftmost.x = leftmost.y = 0;
            break;
        case DIR_4_BOTTOM:
            leftmost.x = size - 1;
            leftmost.y = 0;
            break;
        case DIR_6_LEFT:
            leftmost.x = leftmost.y = size - 1;
            break;
        default:
            return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            map_terrain_add(grid_offset, TERRAIN_BUILDING);
            if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
                map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
                map_terrain_add(grid_offset, TERRAIN_BUILDING);
            }
            map_building_set(grid_offset, building);
            map_property_clear_constructing(grid_offset);
            map_property_set_multi_tile_size(grid_offset, size);
            map_image_set(grid_offset, image_id);
            map_property_set_multi_tile_xy(grid_offset, dx, dy,
                dx == leftmost.x && dy == leftmost.y);
        }
    }
}

static int is_blocked_tile(int grid_offset)
{
    if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
        return map_terrain_is(grid_offset, TERRAIN_ROCK | TERRAIN_ROAD | TERRAIN_BUILDING);
    }
    return map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR);
}

int map_water_determine_orientation(int x, int y, int size, int adjust_xy,
    int *orientation_absolute, int *orientation_relative, int check_water_in_front, int *blocked)
{
    int edge = size - 1;
    int square = size * size;
    int unadjusted_base_offset;
    if (adjust_xy == 1) {
        unadjusted_base_offset = map_grid_offset(x, y);
        switch (city_view_orientation()) {
            default: break;
            case DIR_2_RIGHT: x -= edge; break;
            case DIR_6_LEFT: y -= edge; break;
            case DIR_4_BOTTOM: x -= edge; y -= edge; break;
        }
    } else {
        switch (city_view_orientation()) {
            default: unadjusted_base_offset = map_grid_offset(x, y); break;
            case DIR_2_RIGHT: unadjusted_base_offset = map_grid_offset(x + edge, y); break;
            case DIR_6_LEFT: unadjusted_base_offset = map_grid_offset(x, y + edge); break;
            case DIR_4_BOTTOM: unadjusted_base_offset = map_grid_offset(x + edge, y + edge); break;
        }
    }
    if (!map_grid_is_inside(x, y, size)) {
        return square;
    }

    int base_offset = map_grid_offset(x, y);
    int water_line_x[4] = { -1, edge, -1, 0 };
    int water_line_y[4] = { 0, -1, edge, -1 };

    for (int dir = 0; dir < 4; dir++) {
        int ok_tiles = 0;
        int blocked_tiles = 0;
        int index = 0;
        int column = 0;
        int row = 0;

        while (index < square) {
            for (column = 0; column < row; column++) {

                int should_be_water = water_line_y[dir] == row || water_line_x[dir] == column;
                ok_tiles += should_be_water == map_terrain_is(base_offset + OFFSET(column, row), TERRAIN_WATER);

                should_be_water = water_line_y[dir] == column || water_line_x[dir] == row;
                ok_tiles += should_be_water == map_terrain_is(base_offset + OFFSET(row, column), TERRAIN_WATER);

                index += 2;
            }
            int should_be_water = water_line_y[dir] == row || water_line_x[dir] == row;
            ok_tiles += should_be_water == map_terrain_is(base_offset + OFFSET(row, row), TERRAIN_WATER);

            index++;
            row++;
        }

        if (ok_tiles < square) {
            continue;
        }

        // water/land is OK in this orientation
        if (orientation_absolute) {
            *orientation_absolute = dir;
        }
        if (orientation_relative) {
            *orientation_relative = (4 + dir - city_view_orientation() / 2) % 4;
        }

        // Check for blocked tiles
        static const struct {
            int x;
            int y;
        } steps[4] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };

        int orientation = city_view_orientation() / 2;
        index = 0;
        column = 0;
        row = 0;
        int *x_offset = orientation & 1 ? &row : &column;
        int *y_offset = orientation & 1 ? &column : &row;

        while (index < square) {
            for (column = 0; column < row; column++) {
                int is_blocked[2];
                is_blocked[0] = is_blocked_tile(unadjusted_base_offset +
                    OFFSET(*x_offset * steps[orientation].x, *y_offset * steps[orientation].y));
                is_blocked[1] = is_blocked_tile(unadjusted_base_offset +
                    OFFSET(*y_offset * steps[orientation].x, *x_offset * steps[orientation].y));

                blocked_tiles += is_blocked[0] + is_blocked[1];
                if (blocked) {
                    blocked[index] = is_blocked[0];
                    blocked[index + 1] = is_blocked[1];
                }
                index += 2;
            }
            int is_blocked = is_blocked_tile(unadjusted_base_offset +
                OFFSET(row * steps[orientation].x, row * steps[orientation].y));
            blocked_tiles += is_blocked;

            if (blocked) {
                blocked[index] = is_blocked;
            }

            index++;
            row++;
        }

        if (!check_water_in_front ||
            map_water_has_water_in_front(x, y, 0, map_water_get_waterside_tile_loop(dir, size), 0)) {
            return blocked_tiles;
        }
    }
    // If the waterside building isn't properly positioned next to a shore, block everything
    if (blocked) {
        for (int i = 0; i < square; i++) {
            blocked[i] = 1;
        }
    }
    return square;
}

const waterside_tile_loop *map_water_get_waterside_tile_loop(int direction, int size)
{
    static waterside_tile_loop base_loops[4] = {
        { { -1,  0 }, {  0, -1 }, {  1,  0 } },
        { {  2, -1 }, {  1,  0 }, {  0,  1 } },
        { {  3,  2 }, {  0,  1 }, { -1,  0 } },
        { {  0,  3 }, { -1,  0 }, {  0, -1 } }
    };
    base_loops[direction].inner_length = size + 2;
    switch (direction) {
        case 1:
            base_loops[1].start.x = size - 1;
            return &base_loops[1];
        case 2:
            base_loops[2].start.x = size;
            base_loops[2].start.y = size - 1;
            return &base_loops[2];
        case 3:
            base_loops[3].start.y = size;
            return &base_loops[3];
        default:
            return &base_loops[0];
    }
}

int map_water_has_water_in_front(int x, int y, int adjust_xy, const waterside_tile_loop *loop, int *land_tiles)
{
    if (adjust_xy == 1) {
        int edge = loop->inner_length - 3;
        switch (city_view_orientation()) {
            case DIR_0_TOP: break;
            case DIR_2_RIGHT: x -= edge; break;
            case DIR_6_LEFT: y -= edge; break;
            case DIR_4_BOTTOM: x -= edge; y -= edge; break;
        }
    }
    int base_offset = map_grid_offset(x, y);

    // check three rows of water tiles in front
    int dx = loop->start.x;
    int dy = loop->start.y;
    int water_ok = 1;
    int index = 0;
    for (int outer = 0; outer < MAP_WATER_WATERSIDE_ROWS_NEEDED; outer++) {
        for (int inner = 0; inner < loop->inner_length; inner++) {
            if (!map_terrain_is(base_offset + OFFSET(dx, dy), TERRAIN_WATER)) {
                water_ok = 0;
                if (land_tiles) {
                    land_tiles[index] = 1;
                } else {
                    return 0;
                }
            } else if (land_tiles) {
                land_tiles[index] = 0;
            }
            dx += loop->inner_step.x;
            dy += loop->inner_step.y;
            index++;
        }
        if (loop->outer_step.y) {
            dx = loop->start.x;
            dy += loop->outer_step.y;
        } else {
            dy = loop->start.y;
            dx += loop->outer_step.x;
        }
    }
    return water_ok;
}

int map_water_is_connected_to_open_water(int x, int y, int size)
{
    map_point river_entry = scenario_map_river_entry();
    return Route::waterCanReachAdjacentOpenWater(river_entry, x, y, size);
}

static Figure *live_fishing_boat(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    Figure *figure = Figure::get(id);
    return figure && figure->state == FIGURE_STATE_ALIVE && figure->type == FIGURE_FISHING_BOAT ? figure : nullptr;
}

static int live_fishing_boat_id(unsigned int id)
{
    return live_fishing_boat(id) ? 1 : 0;
}

static int wharf_has_boat_id(const building *wharf, unsigned int boat_id)
{
    return wharf && boat_id &&
        (wharf->data.industry.fishing_boat_id == boat_id ||
            wharf->data.industry.second_fishing_boat_id == boat_id);
}

static void clear_stale_fishing_boat_slots(building *wharf)
{
    if (!wharf) {
        return;
    }
    if (wharf->data.industry.fishing_boat_id && !live_fishing_boat_id(wharf->data.industry.fishing_boat_id)) {
        wharf->data.industry.fishing_boat_id = 0;
    }
    if (wharf->data.industry.second_fishing_boat_id && !live_fishing_boat_id(wharf->data.industry.second_fishing_boat_id)) {
        wharf->data.industry.second_fishing_boat_id = 0;
    }
}

static int fishing_boat_capacity(Building wharf, building_type_registry_impl::SpawnSource source)
{
    int capacity = 0;
    const building_type_registry_impl::BuildingType *definition = wharf.type;
    if (!definition) {
        return 0;
    }
    for (const building_type_registry_impl::SpawnDelayGroup &group : definition->spawn_groups()) {
        for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
            if (policy.mode != building_type_registry_impl::SpawnMode::FishingBoat ||
                policy.spawn_figure != FIGURE_FISHING_BOAT) {
                continue;
            }
            if (source == building_type_registry_impl::SpawnSource::None || policy.spawn_source == source) {
                capacity += policy.capacity;
            }
        }
    }
    return capacity;
}

static int wharf_has_shipyard_fishing_boat_room(Building wharf, int live_boats)
{
    const int shipyard_capacity = fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Shipyard);
    if (shipyard_capacity <= 0) {
        return 0;
    }
    if (fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Self) > 0) {
        const building *record = wharf.record();
        return record && !live_fishing_boat_id(record->data.industry.second_fishing_boat_id);
    }
    return live_boats < shipyard_capacity;
}

static int wharf_live_fishing_boat_count(building *wharf)
{
    clear_stale_fishing_boat_slots(wharf);
    return live_fishing_boat_id(wharf->data.industry.fishing_boat_id) +
        live_fishing_boat_id(wharf->data.industry.second_fishing_boat_id);
}

static void wharf_tile(Building wharf, map_point *tile)
{
    int dx;
    int dy;
    switch (wharf.orientation()) {
        case 0: dx = 1; dy = -1; break;
        case 1: dx = 2; dy = 1; break;
        case 2: dx = 1; dy = 2; break;
        default: dx = -1; dy = 1; break;
    }
    map_point_store_result(wharf.x() + dx, wharf.y() + dy, tile);
}

int map_water_wharf_live_fishing_boats(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    return record ? wharf_live_fishing_boat_count(record) : 0;
}

Figure *map_water_wharf_live_fishing_boat(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    if (!record) {
        return nullptr;
    }
    clear_stale_fishing_boat_slots(record);
    if (Figure *boat = live_fishing_boat(record->data.industry.fishing_boat_id)) {
        return boat;
    }
    return live_fishing_boat(record->data.industry.second_fishing_boat_id);
}

int map_water_wharf_has_self_fishing_boat_room(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    return record && fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Self) > 0 &&
        !live_fishing_boat_id(record->data.industry.fishing_boat_id);
}

int map_water_assign_fishing_boat_to_wharf(Figure *boat, Building wharf, map_point *tile)
{
    building *record = const_cast<building *>(wharf.record());
    if (!boat || !record || record->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    clear_stale_fishing_boat_slots(record);
    if (!wharf_has_boat_id(record, boat->id())) {
        const int self_capacity = fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Self);
        const int self_boat = boat->building && boat->building->id == wharf.id;
        if (self_capacity > 0 && self_boat && !record->data.industry.fishing_boat_id) {
            record->data.industry.fishing_boat_id = boat->id();
        } else if (self_capacity > 0 && !self_boat &&
            fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Shipyard) > 0 &&
            !record->data.industry.second_fishing_boat_id) {
            record->data.industry.second_fishing_boat_id = boat->id();
        } else if (self_capacity <= 0 &&
            wharf_live_fishing_boat_count(record) <
                fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::None)) {
            if (!record->data.industry.fishing_boat_id) {
                record->data.industry.fishing_boat_id = boat->id();
            } else if (!record->data.industry.second_fishing_boat_id) {
                record->data.industry.second_fishing_boat_id = boat->id();
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }
    if (tile) {
        wharf_tile(wharf, tile);
    }
    return record->id;
}

void map_water_clear_fishing_boat_from_wharf(Building wharf, unsigned int boat_id)
{
    building *record = const_cast<building *>(wharf.record());
    if (!record || !boat_id) {
        return;
    }
    if (record->data.industry.fishing_boat_id == boat_id) {
        record->data.industry.fishing_boat_id = 0;
    }
    if (record->data.industry.second_fishing_boat_id == boat_id) {
        record->data.industry.second_fishing_boat_id = 0;
    }
}

static Building *find_wharf_for_new_fishing_boat(Figure *boat, map_point *tile, int assign)
{
    Building *wharf = nullptr;
    for (int pass = 0; pass < 2 && !wharf; pass++) {
        Building::for_each([&](Building *candidate) {
            if (wharf ||
                candidate->state_id() != BUILDING_STATE_IN_USE ||
                !candidate->matches("wharf")) {
                return;
            }
            building *record = const_cast<building *>(candidate->record());
            if (!record) {
                return;
            }
            clear_stale_fishing_boat_slots(record);
            const int live_boats = wharf_live_fishing_boat_count(record);
            if (wharf_has_boat_id(record, boat ? boat->id() : 0) ||
                (wharf_has_shipyard_fishing_boat_room(*candidate, live_boats) && (pass == 1 || live_boats == 0))) {
                wharf = candidate;
            }
        });
    }
    if (!wharf) {
        return nullptr;
    }
    if (assign) {
        return map_water_assign_fishing_boat_to_wharf(boat, *wharf, tile) ? wharf : nullptr;
    }
    wharf_tile(*wharf, tile);
    return wharf;
}

Building *map_water_assign_wharf_for_new_fishing_boat(Figure *boat, map_point *tile)
{
    return find_wharf_for_new_fishing_boat(boat, tile, 1);
}

int map_water_has_wharf_for_new_fishing_boat(void)
{
    map_point tile;
    return find_wharf_for_new_fishing_boat(nullptr, &tile, 0) != nullptr;
}

int map_water_shipyard_can_spawn_fishing_boat(Building shipyard)
{
    building *record = const_cast<building *>(shipyard.record());
    map_point tile;
    return record && !live_fishing_boat_id(record->figure_id) &&
        map_water_has_wharf_for_new_fishing_boat() &&
        map_water_can_spawn_fishing_boat(shipyard.x(), shipyard.y(), shipyard.size(), &tile);
}

int map_water_spawn_fishing_boat_from_shipyard(Building shipyard)
{
    building *record = const_cast<building *>(shipyard.record());
    map_point boat_tile;
    if (!record || live_fishing_boat_id(record->figure_id) ||
        !map_water_has_wharf_for_new_fishing_boat() ||
        !map_water_can_spawn_fishing_boat(shipyard.x(), shipyard.y(), shipyard.size(), &boat_tile)) {
        return 0;
    }
    Figure *boat =
        figure_runtime_create_profiled(FIGURE_FISHING_BOAT, boat_tile.x, boat_tile.y, DIR_0_TOP, shipyard, "fish_fetch");
    if (!boat) {
        return 0;
    }
    record->figure_id = boat->id();
    return 1;
}

int map_water_spawn_fishing_boat_from_wharf(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    map_point boat_tile;
    map_point wharf_point;
    if (!record || !map_water_wharf_has_self_fishing_boat_room(wharf) ||
        !map_water_can_spawn_fishing_boat(wharf.x(), wharf.y(), wharf.size(), &boat_tile)) {
        return 0;
    }

    Figure *boat =
        figure_runtime_create_profiled(FIGURE_FISHING_BOAT, boat_tile.x, boat_tile.y, DIR_0_TOP, wharf, "fish_fetch");
    if (!boat || !map_water_assign_fishing_boat_to_wharf(boat, wharf, &wharf_point)) {
        if (boat) {
            boat->state = FIGURE_STATE_DEAD;
        }
        return 0;
    }
    boat->action_state = FIGURE_ACTION_193_FISHING_BOAT_GOING_TO_WHARF;
    boat->destination_x = static_cast<unsigned char>(wharf_point.x);
    boat->destination_y = static_cast<unsigned char>(wharf_point.y);
    boat->source_x = static_cast<unsigned char>(wharf_point.x);
    boat->source_y = static_cast<unsigned char>(wharf_point.y);
    Route::remove(boat);
    return 1;
}

int map_water_find_alternative_fishing_boat_tile(Figure *boat, map_point *tile)
{
    if (map_figure_at(boat->grid_offset) == static_cast<int>(boat->id())) {
        return 0;
    }
    for (int radius = 1; radius <= 5; radius++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(boat->x, boat->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                int grid_offset = map_grid_offset(xx, yy);
                if (!map_has_figure_at(grid_offset) && map_terrain_is(grid_offset, TERRAIN_WATER)) {
                    map_point_store_result(xx, yy, tile);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int map_water_find_shipwreck_tile(Figure *wreck, map_point *tile)
{
    if (map_terrain_is(wreck->grid_offset, TERRAIN_WATER) &&
        map_figure_at(wreck->grid_offset) == static_cast<int>(wreck->id())) {
        return 0;
    }
    for (int radius = 1; radius <= 5; radius++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(wreck->x, wreck->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                int grid_offset = map_grid_offset(xx, yy);
                if (!map_has_figure_at(grid_offset) || map_figure_at(grid_offset) == static_cast<int>(wreck->id())) {
                    if (map_terrain_is(grid_offset, TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx, yy - 2), TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx, yy + 2), TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx - 2, yy), TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx + 2, yy), TERRAIN_WATER)) {
                        map_point_store_result(xx, yy, tile);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int num_surrounding_water_tiles(int grid_offset)
{
    int amount = 0;
    for (int i = 0; i < DIR_8_NONE; i++) {
        if (map_terrain_is(grid_offset + map_grid_direction_delta(i), TERRAIN_WATER)) {
            amount++;
        }
    }
    return amount;
}

int map_water_can_spawn_fishing_boat(int x, int y, int size, map_point *tile)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        int grid_offset = base_offset + *tile_delta;
        if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
            if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
                if (num_surrounding_water_tiles(grid_offset) >= 8) {
                    map_point_store_result(map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset), tile);
                    return 1;
                }
            }
        }
    }
    return 0;
}
