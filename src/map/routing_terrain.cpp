#include "figure/figure.h"
#include "building/building_record.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "figure/route.h"
#include "figure/movement.h"
#include "map/building.h"
#include "map/data.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/routing.h"
#include "map/routing_data.h"
#include "map/TerrainMap.h"

static void update_land_terrain_noncitizen(void);
static int get_land_type_citizen_aqueduct(int grid_offset);

bool Route::groundPositionIsPassable(int cross_country_x, int cross_country_y)
{
    // Cross-country zero is drawn at the tile center, not its top corner.
    // Check the actual occupied tile and the local segment from the routing anchor.
    const int x = figure_movement_cross_country_to_tile(cross_country_x);
    const int y = figure_movement_cross_country_to_tile(cross_country_y);
    const int occupied_x = figure_movement_cross_country_to_tile(cross_country_x + FIGURE_CROSS_COUNTRY_TILE_UNITS / 2);
    const int occupied_y = figure_movement_cross_country_to_tile(cross_country_y + FIGURE_CROSS_COUNTRY_TILE_UNITS / 2);
    for (int yy = y; yy <= occupied_y; ++yy) for (int xx = x; xx <= occupied_x; ++xx) {
        if (!map_grid_is_inside(xx, yy, 1) || terrain_land_citizen.items[map_grid_offset(xx, yy)] < CITIZEN_0_ROAD) return false;
    }
    return true;
}

bool Route::herdCanEnter(int from_offset, int to_offset, int building_clearance)
{
    const int x = map_grid_offset_to_x(to_offset), y = map_grid_offset_to_y(to_offset);
    if (!map_grid_is_inside(x, y, 1) || terrain_map().contains(to_offset, terrain_types().impassable_herd)) return false;
    const int from_x = map_grid_offset_to_x(from_offset), from_y = map_grid_offset_to_y(from_offset);
    if (x != from_x && y != from_y &&
        (terrain_map().contains(map_grid_offset(x, from_y), terrain_types().impassable_herd) ||
         terrain_map().contains(map_grid_offset(from_x, y), terrain_types().impassable_herd))) return false;
    if (building_clearance <= 0) return true;
    const int next_distance = terrain_map().distance_to_nearest(to_offset, terrain_types().building, building_clearance);
    // Construction can surround an existing herd: allow it to leave the buffer,
    // but never let it move closer to buildings while escaping.
    return next_distance > building_clearance || next_distance >= terrain_map().distance_to_nearest(from_offset, terrain_types().building, building_clearance);
}

static int is_road_surface(const TerrainSet &terrain)
{
    return terrain.intersects(terrain_types().road) || terrain.intersects(terrain_types().access_ramp);
}

static int is_noncitizen_clearable_surface(const TerrainSet &terrain)
{
    return terrain.intersects(terrain_types().garden) || terrain.intersects(terrain_types().rubble) || terrain.intersects(terrain_types().aqueduct);
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
    TerrainSet terrain = terrain_map().at(grid_offset);
    int type = CITIZEN_N1_BLOCKED;
    if ((terrain & terrain_types().aqueduct) && (terrain & terrain_types().highway)) {
        // The road/highway surface below an aqueduct remains traversable.
        type = CITIZEN_1_HIGHWAY;
    } else if ((terrain & terrain_types().aqueduct) && is_road_surface(terrain)) {
        type = CITIZEN_0_ROAD;
    } else if (terrain & terrain_types().aqueduct) {
        type = get_land_type_citizen_aqueduct(grid_offset);
    } else if ((terrain & terrain_types().rubble) && current.Rubble && current.Rubble->is_rubble()) {
        // Runtime-backed rubble also carries terrain_types().building so that each piece can
        // retain its origin and burning state. It remains citizen-passable terrain.
        type = CITIZEN_2_PASSABLE_TERRAIN;
    } else if (current.Foundation && current.Foundation->passage_at(grid_offset) !=
            building_type_registry_impl::FoundationPassage::None) {
        if (terrain & terrain_types().highway) {
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
            TerrainSet terrain = terrain_map().at(grid_offset);
            if (terrain & (terrain_types().building | terrain_types().gatehouse)) {
                if (!map_building_exists_at(grid_offset)) {
                    // shouldn't happen
                    terrain_land_noncitizen.items[grid_offset] = CITIZEN_4_CLEAR_TERRAIN; // BUG: should be citizen?
                    terrain_map().remove(grid_offset, terrain_types().building);
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
            } else if (terrain & terrain_types().highway) {
                terrain_land_citizen.items[grid_offset] = CITIZEN_1_HIGHWAY;
            } else if (terrain & terrain_types().aqueduct) {
                terrain_land_citizen.items[grid_offset] = static_cast<int8_t>(get_land_type_citizen_aqueduct(grid_offset));
            } else if (terrain & (terrain_types().rubble | terrain_types().garden)) {
                terrain_land_citizen.items[grid_offset] = CITIZEN_2_PASSABLE_TERRAIN;
            } else if (terrain.intersects(terrain_types().impassable)) {
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
    const TerrainSet &terrain = terrain_map().at(grid_offset);
    if (((terrain & terrain_types().aqueduct) &&
            (is_road_surface(terrain) || (terrain & terrain_types().highway))) ||
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
            TerrainSet terrain = terrain_map().at(grid_offset);
            if (terrain & terrain_types().gatehouse) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_4_GATEHOUSE;
            } else if (terrain & terrain_types().building) {
                terrain_land_noncitizen.items[grid_offset] = static_cast<int8_t>(get_land_type_noncitizen(grid_offset));
            } else if (is_road_surface(terrain)) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_0_PASSABLE;
            } else if (terrain & terrain_types().highway) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_0_PASSABLE;
            } else if (is_noncitizen_clearable_surface(terrain)) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_2_CLEARABLE;
            } else if (terrain & terrain_types().wall) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_3_WALL;
            } else if (terrain.intersects(terrain_types().impassable_enemy)) {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_N1_BLOCKED;
            } else {
                terrain_land_noncitizen.items[grid_offset] = NONCITIZEN_0_PASSABLE;
            }
        }
    }
}

static int is_wall_tile(int grid_offset)
{
    return terrain_map().contains(grid_offset, terrain_types().wall_or_gatehouse) ? 1 : 0;
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
            if (terrain_map().contains(grid_offset, terrain_types().wall)) {
                if (count_adjacent_wall_tiles(grid_offset) == 3) {
                    terrain_walls.items[grid_offset] = WALL_0_PASSABLE;
                } else {
                    terrain_walls.items[grid_offset] = WALL_N1_BLOCKED;
                }
            } else if (terrain_map().contains(grid_offset, terrain_types().gatehouse)) {
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
