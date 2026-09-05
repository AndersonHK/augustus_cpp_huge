#include "figure/figure.h"
#include "building/building_record.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "figure/route.h"
#include "map/building.h"
#include "map/data.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/routing.h"
#include "map/routing_data.h"
#include "map/terrain.h"

static void update_land_terrain_noncitizen(void);
static int get_land_type_citizen_aqueduct(int grid_offset);

static int is_road_surface(int terrain)
{
    return terrain & (TERRAIN_ROAD | TERRAIN_ACCESS_RAMP);
}

static int is_noncitizen_clearable_surface(int terrain)
{
    return terrain & (TERRAIN_GARDEN | TERRAIN_RUBBLE | TERRAIN_AQUEDUCT);
}

static int is_reservoir_connector_tile(int grid_offset)
{
    switch (map_property_multi_tile_xy(grid_offset)) {
        case EDGE_X1Y0:
        case EDGE_X0Y1:
        case EDGE_X2Y1:
        case EDGE_X1Y2:
            return 1;
    }
    return 0;
}

static int is_transformable_gate_wall(building_type type)
{
    static const char *const types[] = {
        "roofed_garden_wall",
        "looped_garden_wall",
        "panelled_garden_wall",
        "hedge_dark",
        "hedge_light",
    };
    return building_type_registry_impl::type_attr_is_any(type, types, sizeof(types) / sizeof(types[0]));
}

static int is_native_blocker(building_type type)
{
    static const char *const types[] = {
        "burning_ruin",
        "native_hut",
        "native_hut_alt",
        "native_meeting",
        "native_crops",
        "native_decor",
        "native_monument",
        "native_watchtower",
    };
    return building_type_registry_impl::type_attr_is_any(type, types, sizeof(types) / sizeof(types[0]));
}

void Route::updateAllTerrain(void)
{
    Route::updateLandTerrain();
    Route::updateWallTerrain();
}

void Route::updateLandTerrain(void)
{
    Route::updateCitizenLandTerrain();
    update_land_terrain_noncitizen();
}

static int get_land_type_citizen_building(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return CITIZEN_4_CLEAR_TERRAIN;
    }
    Building current = map_building_at(grid_offset);
    building *b = const_cast<::building *>(current.record());
    int terrain = map_terrain_get(grid_offset);
    int type = CITIZEN_N1_BLOCKED;
    if ((terrain & TERRAIN_AQUEDUCT) && (terrain & TERRAIN_HIGHWAY)) {
        // The road/highway surface below an aqueduct remains traversable.
        type = CITIZEN_1_HIGHWAY;
    } else if ((terrain & TERRAIN_AQUEDUCT) && is_road_surface(terrain)) {
        type = CITIZEN_0_ROAD;
    } else if (terrain & TERRAIN_AQUEDUCT) {
        type = get_land_type_citizen_aqueduct(grid_offset);
    } else if ((terrain & TERRAIN_RUBBLE) && current.Rubble && current.Rubble->is_rubble()) {
        // Runtime-backed rubble also carries TERRAIN_BUILDING so that each piece can
        // retain its origin and burning state. It remains citizen-passable terrain.
        type = CITIZEN_2_PASSABLE_TERRAIN;
    } else if (current.Foundation && current.Foundation->passage_at(grid_offset) !=
            building_type_registry_impl::FoundationPassage::None) {
        if (terrain & TERRAIN_HIGHWAY) {
            type = CITIZEN_1_HIGHWAY;
        } else {
            type = CITIZEN_0_ROAD;
        }
    } else if (is_transformable_gate_wall(b->type)) {
        // colonnade can be enabled if we add a gate variant
        type = GATE_0_TRANSFORMABLE;
    } else if (building_type_registry_impl::type_attr_is(b->type, "fort_ground")) {
        type = CITIZEN_2_PASSABLE_TERRAIN;
    } else if (building_type_registry_impl::type_attr_is(b->type, "reservoir")) {
        if (is_reservoir_connector_tile(grid_offset)) {
            type = CITIZEN_N4_RESERVOIR_CONNECTOR; // aqueduct connect points
        }
    }
    return type;
}

static int get_land_type_citizen_aqueduct(int grid_offset)
{
    int image_id = map_image_at(grid_offset) - image_group(GROUP_BUILDING_AQUEDUCT);
    if (image_id <= 3) {
        return CITIZEN_N3_AQUEDUCT;
    } else if (image_id <= 7) {
        return CITIZEN_N1_BLOCKED;
    } else if (image_id <= 9) {
        return CITIZEN_N3_AQUEDUCT;
    } else if (image_id <= 14) {
        return CITIZEN_N1_BLOCKED;
    } else if (image_id <= 18) {
        return CITIZEN_N3_AQUEDUCT;
    } else if (image_id <= 22) {
        return CITIZEN_N1_BLOCKED;
    } else if (image_id <= 24) {
        return CITIZEN_N3_AQUEDUCT;
    } else {
        return CITIZEN_N1_BLOCKED;
    }
}

void Route::updateCitizenLandTerrain(void)
{
    map_grid_init_i8(terrain_land_citizen.items, -1);
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            int terrain = map_terrain_get(grid_offset);
            if (terrain & (TERRAIN_BUILDING | TERRAIN_GATEHOUSE)) {
                if (!map_building_exists_at(grid_offset)) {
                    // shouldn't happen
                    terrain_land_noncitizen.items[grid_offset] = CITIZEN_4_CLEAR_TERRAIN; // BUG: should be citizen?
                    map_terrain_remove(grid_offset, TERRAIN_BUILDING);
                    map_image_set(grid_offset, (map_random_get(grid_offset) & 7) + image_group(GROUP_TERRAIN_GRASS_1));
                    map_property_mark_draw_tile(grid_offset);
                    map_property_set_legacy_multi_tile_size(grid_offset, 1);
                    continue;
                }
                // A building's declared foundation passage is authoritative.
                // This also keeps old saves with stale underlying ROAD bits
                // from making non-passage building tiles traversable.
                terrain_land_citizen.items[grid_offset] = static_cast<int8_t>(get_land_type_citizen_building(grid_offset));
            } else if (is_road_surface(terrain)) {
                terrain_land_citizen.items[grid_offset] = CITIZEN_0_ROAD;
            } else if (terrain & TERRAIN_HIGHWAY) {
                terrain_land_citizen.items[grid_offset] = CITIZEN_1_HIGHWAY;
            } else if (terrain & TERRAIN_AQUEDUCT) {
                terrain_land_citizen.items[grid_offset] = static_cast<int8_t>(get_land_type_citizen_aqueduct(grid_offset));
            } else if (terrain & (TERRAIN_RUBBLE | TERRAIN_GARDEN)) {
                terrain_land_citizen.items[grid_offset] = CITIZEN_2_PASSABLE_TERRAIN;
            } else if (terrain & TERRAIN_NOT_CLEAR) {
                terrain_land_citizen.items[grid_offset] = CITIZEN_N1_BLOCKED;
            } else {
                terrain_land_citizen.items[grid_offset] = CITIZEN_4_CLEAR_TERRAIN;
            }
        }
    }
}

static int get_land_type_noncitizen(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return NONCITIZEN_2_CLEARABLE;
    }
    int type = NONCITIZEN_1_BUILDING;
    Building current = map_building_at(grid_offset);
    building *b = const_cast<::building *>(current.record());
    const int terrain = map_terrain_get(grid_offset);
    if (((terrain & TERRAIN_AQUEDUCT) &&
            (is_road_surface(terrain) || (terrain & TERRAIN_HIGHWAY))) ||
        (current.Foundation && current.Foundation->passage_at(grid_offset) !=
            building_type_registry_impl::FoundationPassage::None) ||
        building_type_registry_impl::type_attr_is(b->type, "fort_ground")) {
        type = NONCITIZEN_0_PASSABLE;
    } else if (is_native_blocker(b->type)) {
        type = NONCITIZEN_N1_BLOCKED;
    } else if (building_is_fort(b->type)) {
        type = NONCITIZEN_5_FORT;
    } else if (is_transformable_gate_wall(b->type)) {
        // colonnade can be enabled if we add a gate variant
        type = GATE_0_TRANSFORMABLE;
    } else if (building_type_registry_impl::type_attr_is(b->type, "wall")) {
        type = NONCITIZEN_3_WALL;
    }
    return type;
}

static void update_land_terrain_noncitizen(void)
{
    map_grid_init_i8(terrain_land_noncitizen.items, -1);
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            int terrain = map_terrain_get(grid_offset);
            if (terrain & TERRAIN_GATEHOUSE) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_4_GATEHOUSE;
            } else if (terrain & TERRAIN_BUILDING) {
                terrain_land_noncitizen.items[grid_offset] = static_cast<int8_t>(get_land_type_noncitizen(grid_offset));
            } else if (is_road_surface(terrain)) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_0_PASSABLE;
            } else if (terrain & TERRAIN_HIGHWAY) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_0_PASSABLE;
            } else if (is_noncitizen_clearable_surface(terrain)) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_2_CLEARABLE;
            } else if (terrain & TERRAIN_WALL) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_3_WALL;
            } else if (terrain & TERRAIN_NOT_CLEAR) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_N1_BLOCKED;
            } else {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_0_PASSABLE;
            }
        }
    }
}

static int is_wall_tile(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_WALL_OR_GATEHOUSE) ? 1 : 0;
}

static int count_adjacent_wall_tiles(int grid_offset)
{
    int adjacent = 0;
    switch (city_view_orientation()) {
        case DIR_0_TOP:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 0));
            break;
        case DIR_2_RIGHT:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 0));
            break;
        case DIR_4_BOTTOM:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 0));
            break;
        case DIR_6_LEFT:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 0));
            break;
    }
    return adjacent;
}

void Route::updateWallTerrain(void)
{
    map_grid_init_i8(terrain_walls.items, -1);
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
                if (count_adjacent_wall_tiles(grid_offset) == 3) {
                    terrain_walls.items[grid_offset] = WALL_0_PASSABLE;
                } else {
                    terrain_walls.items[grid_offset] = WALL_N1_BLOCKED;
                }
            } else if (map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
                terrain_walls.items[grid_offset] = WALL_0_PASSABLE;
            } else {
                terrain_walls.items[grid_offset] = WALL_N1_BLOCKED;
            }
        }
    }
}

int Route::wallIsPassable(int grid_offset)
{
    return terrain_walls.items[grid_offset] == WALL_0_PASSABLE;
}

static int wall_tile_in_radius(int x, int y, int radius, int *x_wall, int *y_wall)
{
    int size = 1;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (Route::wallIsPassable(map_grid_offset(xx, yy))) {
                *x_wall = xx;
                *y_wall = yy;
                return 1;
            }
        }
    }
    return 0;
}

int Route::findWallTileInRadius(int x, int y, int radius, int *x_wall, int *y_wall)
{
    for (int i = 1; i <= radius; i++) {
        if (wall_tile_in_radius(x, y, i, x_wall, y_wall)) {
            return 1;
        }
    }
    return 0;
}

int building_destroyable_at(int grid_offset)
{
    return terrain_land_noncitizen.items[grid_offset] > NONCITIZEN_0_PASSABLE &&
        terrain_land_noncitizen.items[grid_offset] != NONCITIZEN_5_FORT;
}
