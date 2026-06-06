#include "orientation.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_api.h"

extern "C" {
#include "assets/assets.h"
#include "building/connectable.h"
#include "building/house.h"
#include "building/image.h"
#include "building/properties.h"
#include "building/variant.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "figuretype/animal.h"
#include "figuretype/wall.h"
#include "game/undo.h"
#include "map/bridge.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/natives.h"
#include "map/property.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "map/water.h"
}

#include <math.h>
#include <stdlib.h>

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    building_type resolved = runtime_type(text_id);
    return resolved != BUILDING_NONE && type == resolved;
}

static int type_matches_any(building_type type, const char *const *text_ids, int count)
{
    for (int i = 0; i < count; i++) {
        if (type_matches(type, text_ids[i])) {
            return 1;
        }
    }
    return 0;
}

static int is_vacant_lot_starting_house(const building *b)
{
    building_type fill_type = building_type_registry_get_vacant_lot_fill_type();
    return b && b->house_population == 0 &&
        (type_matches(b->type, "house_vacant_lot") ||
            (fill_type != BUILDING_NONE && b->type == fill_type));
}

static void determine_leftmost_tile(void)
{
    int orientation = city_view_orientation();
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            int size = map_property_multi_tile_size(grid_offset);
            if (size == 1) {
                map_property_mark_draw_tile(grid_offset);
                continue;
            }
            map_property_clear_draw_tile(grid_offset);
            int dx = orientation == DIR_4_BOTTOM || orientation == DIR_6_LEFT ? size - 1 : 0;
            int dy = orientation == DIR_0_TOP || orientation == DIR_6_LEFT ? size - 1 : 0;
            if (map_property_is_multi_tile_xy(grid_offset, dx, dy)) {
                map_property_mark_draw_tile(grid_offset);
            }
        }
    }
}

void map_orientation_change(int quarter_turns)
{
    if (!quarter_turns) {
        return;
    }

    map_tiles_remove_entry_exit_flags();
    game_undo_disable();
    determine_leftmost_tile();

    map_tiles_update_all_elevation();
    map_tiles_update_all_water();
    map_tiles_update_all_earthquake();
    map_tiles_update_all_rocks();
    map_tiles_update_all_gardens();

    map_tiles_add_entry_exit_flags();

    map_tiles_update_all_empty_land();
    map_tiles_update_all_meadow();
    map_tiles_update_all_rubble();
    map_tiles_update_all_roads();
    map_tiles_update_all_highways();
    map_tiles_update_all_plazas();
    map_tiles_update_all_walls();
    map_tiles_update_all_aqueducts(0);
    building_connectable_update_connections();


    map_orientation_update_buildings();
    const int bridge_counter_clockwise = quarter_turns < 0 ? 1 : 0;
    for (int i = 0; i < abs(quarter_turns); i++) {
        map_bridge_update_after_rotate(bridge_counter_clockwise);
    }
    map_routing_update_walls();

    map_natives_check_land(0);

    figure_tower_sentry_reroute();
    figure_hippodrome_horse_reroute();
}

int map_orientation_for_gatehouse(int x, int y)
{
    switch (city_view_orientation()) {
        case DIR_2_RIGHT: x--; break;
        case DIR_4_BOTTOM: x--; y--; break;
        case DIR_6_LEFT: y--; break;
    }
    int grid_offset = map_grid_offset(x, y);
    int num_road_tiles_within = 0;
    int road_tiles_within_flags = 0;
    // tiles within gate, flags:
    // 1  2
    // 4  8
    if (map_terrain_is(map_grid_offset(x, y), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        road_tiles_within_flags |= 1;
        num_road_tiles_within++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(1, 0), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        road_tiles_within_flags |= 2;
        num_road_tiles_within++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(0, 1), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        road_tiles_within_flags |= 4;
        num_road_tiles_within++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(1, 1), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        road_tiles_within_flags |= 8;
        num_road_tiles_within++;
    }

    if (num_road_tiles_within != 2 && num_road_tiles_within != 4) {
        return 0;
    }
    if (num_road_tiles_within == 2) {
        if (road_tiles_within_flags == 6 || road_tiles_within_flags == 9) { // diagonals
            return 0;
        }
        if (road_tiles_within_flags == 5 || road_tiles_within_flags == 10) { // top to bottom
            return 1;
        }
        if (road_tiles_within_flags == 3 || road_tiles_within_flags == 12) { // left to right
            return 2;
        }
        return 0;
    }
    // all 4 tiles are road: check adjacent roads
    int num_road_tiles_top = 0;
    int num_road_tiles_right = 0;
    int num_road_tiles_bottom = 0;
    int num_road_tiles_left = 0;
    // top
    if (map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_top++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(1, -1), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_top++;
    }
    // bottom
    if (map_terrain_is(grid_offset + map_grid_delta(0, 2), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_bottom++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(1, 2), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_bottom++;
    }
    // left
    if (map_terrain_is(grid_offset + map_grid_delta(-1, 0), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_left++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(-1, 1), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_left++;
    }
    // right
    if (map_terrain_is(grid_offset + map_grid_delta(2, 0), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_right++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(2, 1), TERRAIN_ROAD | TERRAIN_HIGHWAY)) {
        num_road_tiles_right++;
    }
    // determine direction
    if (num_road_tiles_top || num_road_tiles_bottom) {
        if (num_road_tiles_left || num_road_tiles_right) {
            return 0;
        }
        return 1;
    } else if (num_road_tiles_left || num_road_tiles_right) {
        return 2;
    }
    return 0;
}

int map_orientation_for_triumphal_arch(int x, int y)
{
    switch (city_view_orientation()) {
        case DIR_2_RIGHT: x -= 2; break;
        case DIR_4_BOTTOM: x -= 2; y -= 2; break;
        case DIR_6_LEFT: y -= 2; break;
    }
    int num_road_tiles_top_bottom = 0;
    int num_road_tiles_left_right = 0;
    int num_blocked_tiles = 0;

    int grid_offset = map_grid_offset(x, y);
    // check corner tiles
    if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(2, 0), TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(0, 2), TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    if (map_terrain_is(grid_offset + map_grid_delta(2, 2), TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    // road tiles top to bottom
    int top_offset = grid_offset + map_grid_delta(1, 0);
    if ((map_terrain_get(top_offset) & TERRAIN_NOT_CLEAR) == TERRAIN_ROAD) {
        num_road_tiles_top_bottom++;
    } else if (map_terrain_is(top_offset, TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    int bottom_offset = grid_offset + map_grid_delta(1, 2);
    if ((map_terrain_get(bottom_offset) & TERRAIN_NOT_CLEAR) == TERRAIN_ROAD) {
        num_road_tiles_top_bottom++;
    } else if (map_terrain_is(bottom_offset, TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    // road tiles left to right
    int left_offset = grid_offset + map_grid_delta(0, 1);
    if ((map_terrain_get(left_offset) & TERRAIN_NOT_CLEAR) == TERRAIN_ROAD) {
        num_road_tiles_left_right++;
    } else if (map_terrain_is(left_offset, TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    int right_offset = grid_offset + map_grid_delta(2, 1);
    if ((map_terrain_get(right_offset) & TERRAIN_NOT_CLEAR) == TERRAIN_ROAD) {
        num_road_tiles_left_right++;
    } else if (map_terrain_is(right_offset, TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    // center tile
    int center_offset = grid_offset + map_grid_delta(2, 1);
    if ((map_terrain_get(center_offset) & TERRAIN_NOT_CLEAR) == TERRAIN_ROAD) {
        // do nothing
    } else if (map_terrain_is(center_offset, TERRAIN_NOT_CLEAR)) {
        num_blocked_tiles++;
    }
    // judgement time
    if (num_blocked_tiles) {
        return 0;
    }
    if (!num_road_tiles_left_right && !num_road_tiles_top_bottom) {
        return 0; // no road: can't determine direction
    }
    if (num_road_tiles_top_bottom == 2 && !num_road_tiles_left_right) {
        return 1;
    }
    if (num_road_tiles_left_right == 2 && !num_road_tiles_top_bottom) {
        return 2;
    }
    return 0;
}

void map_orientation_update_buildings(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_UNUSED || b->state == BUILDING_STATE_DELETED_BY_GAME ||
            b->state == BUILDING_STATE_RUBBLE) {
            continue;
        }
        if (Building(b).refresh_graphic_if_native()) {
            continue;
        }
        building_type type = b->type;
        if (type_matches(type, "gatehouse")) {
            map_building_tiles_add_remove(i, b->x, b->y, b->size, building_image_get(b),
                TERRAIN_GATEHOUSE | TERRAIN_BUILDING, TERRAIN_CLEARABLE & ~TERRAIN_HIGHWAY);
            map_terrain_add_gatehouse_roads(b->x, b->y, 0);
        } else if (type_matches(type, "triumphal_arch")) {
            map_building_tiles_add(i, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
            map_terrain_add_triumphal_arch_roads(b->x, b->y, b->subtype.orientation);
        } else if (type_matches(type, "hippodrome")) {
            map_building_tiles_add(i, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
        } else {
            static const char *const water_buildings[] = {
                "shipyard",
                "wharf",
                "dock",
            };
            static const char *const decorative_buildings[] = {
                "small_statue",
                "goddess_statue",
                "senator_statue",
                "gladiator_statue",
                "medium_statue",
                "legion_statue",
                "pavilion_blue",
                "horse_statue",
                "small_mausoleum",
                "large_mausoleum",
                "decorative_column",
            };
            Building current(b);
            if (type_matches_any(type, water_buildings, sizeof(water_buildings) / sizeof(water_buildings[0]))) {
                map_water_add_building(i, b->x, b->y, b->size);
            } else if (type_matches_any(type, decorative_buildings,
                    sizeof(decorative_buildings) / sizeof(decorative_buildings[0])) ||
                (current.has_type_definition() && current.type().is_watchtower())) {
                map_building_tiles_add(i, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
            } else if (current.has_type_definition() && current.type().has_roadblock()) {
                map_building_tiles_add(i, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
                map_terrain_add_roadblock_road(b->x, b->y);
            } else if (is_vacant_lot_starting_house(b)) {
                map_building_tiles_add(i, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
                building_house_vacant_lot_mark_draw(i);
            }
        }
    }
}
