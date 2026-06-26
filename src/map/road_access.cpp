#include "figure/figure.h"
#include "building/building_record.h"
#include "road_access.h"

#include "building/building.h"
#include "building/building_type.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "core/config.h"
#include "city/map.h"
#include "figure/PathingMode.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/terrain.h"
#include "map/tiles.h"

#include <vector>

static int building_is_wall_gate(building *b)
{
    if (!b) {
        return 0;
    }
    Building current(b);
    return current.type && current.type->roadblock().is_wall_gate();
}

std::vector<road_access_candidate> map_road_access_candidates(
    const std::vector<road_access_area> &areas)
{
    std::vector<road_access_candidate> candidates;
    for (const road_access_area &area : areas) {
        int base_offset = map_grid_offset(area.origin.x, area.origin.y);
        for (const int *tile_delta = map_grid_adjacent_offsets(area.size); *tile_delta; tile_delta++) {
            const int grid_offset = base_offset + *tile_delta;
            building *adjacent_building = map_terrain_is(grid_offset, TERRAIN_BUILDING) ?
                building_get(map_building_at(grid_offset)) : nullptr;
            if (map_terrain_is(grid_offset, TERRAIN_BUILDING) &&
                building_is_wall_gate(adjacent_building)) {
                continue;
            }
            if (!map_terrain_is(grid_offset, TERRAIN_ROAD)) {
                continue;
            }

            const int building_id = map_building_at(grid_offset);
            roadblock_type kind = building_id ? Roadblock(building_get(building_id)).kind() : ROADBLOCK_NONE;
            if (kind == ROADBLOCK_STANDARD || kind == ROADBLOCK_STORAGE) {
                continue; // ignore non-bridge roadblocks
            }

            const int network_id = map_road_network_get(grid_offset);
            candidates.push_back({
                grid_offset,
                { map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset) },
                network_id,
                city_map_road_network_index(network_id)
            });
        }
    }
    return candidates;
}

std::vector<road_access_area> map_road_access_hippodrome_areas(int x, int y, int rotation)
{
    std::vector<road_access_area> areas = { { { x, y }, 5 } };
    int x_offset = 0;
    int y_offset = 0;
    building_rotation_get_offset_with_rotation(5, rotation, &x_offset, &y_offset);
    areas.push_back({ { x + x_offset, y + y_offset }, 5 });
    building_rotation_get_offset_with_rotation(10, rotation, &x_offset, &y_offset);
    areas.push_back({ { x + x_offset, y + y_offset }, 5 });
    return areas;
}

std::vector<road_access_area> map_road_access_monument_construction_areas(int x, int y, int size)
{
    if (size < 3) {
        return { { { x, y }, size } };
    }

    const int half_size = size / 2;
    std::vector<road_access_area> areas = {
        { { x + half_size, y + size - 1 }, 1 },
        { { x + size - 1, y + half_size }, 1 },
        { { x + half_size, y }, 1 },
        { { x, y + half_size }, 1 }
    };
    if (size % 2 != 0) {
        areas.push_back({ { x + 1, y + size - 1 }, 1 });
        areas.push_back({ { x + size - 1, y + 1 }, 1 });
        areas.push_back({ { x + 1, y }, 1 });
        areas.push_back({ { x, y + 1 }, 1 });
    }
    return areas;
}

static int store_best_road_access(const std::vector<road_access_area> &areas, map_point *road)
{
    const std::vector<road_access_candidate> candidates = map_road_access_candidates(areas);
    if (candidates.empty()) {
        return 0;
    }

    const road_access_candidate *best = &candidates.front();
    for (const road_access_candidate &candidate : candidates) {
        if (candidate.network_index < best->network_index) {
            best = &candidate;
        }
    }
    if (road) {
        map_point_store_result(best->road.x, best->road.y, road);
    }
    return 1;
}

int map_has_road_access(int x, int y, int size, map_point *road)
{
    return map_has_road_access_rotation(0, x, y, size, road);
}

void map_update_granary_internal_roads(const building *b)
{
    int cx = b->x + 1; // Center of the granary
    int cy = b->y + 1;
    int center_grid_offset = map_grid_offset(cx, cy);

    map_terrain_add(center_grid_offset, TERRAIN_ROAD);

    static const int edge_checks[4][2] = {
        { 0,  2},
        { 2,  0},
        {-2,  0},
        { 0, -2}
    };

    for (int i = 0; i < 4; ++i) {
        int ex = cx + edge_checks[i][0];
        int ey = cy + edge_checks[i][1];
        int edge_grid_offset = map_grid_offset(ex, ey);
        int ix = cx + edge_checks[i][0] / 2;
        int iy = cy + edge_checks[i][1] / 2;
        int inner_grid_offset = map_grid_offset(ix, iy);
        if (map_terrain_is(edge_grid_offset, TERRAIN_ROAD)) {
            map_terrain_add(inner_grid_offset, TERRAIN_ROAD);
        } else {
            map_terrain_remove(inner_grid_offset, TERRAIN_ROAD);
        }
    }
    map_tiles_update_area_roads(b->x, b->y, 5);
}

int map_has_road_access_warehouse(int x, int y, map_point *road)
{
    building *warehouse = building_main(building_get(map_building_at(map_grid_offset(x, y))));
    int rx = x = (warehouse->x);
    int ry = y = (warehouse->y);
    int glp = config_get(CONFIG_GP_CH_GLOBAL_LABOUR);
    int valid_terrain = glp ? TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_ACCESS_RAMP : TERRAIN_ROAD | TERRAIN_ACCESS_RAMP;
    int has_road = 0;
    if (!warehouse) {
        return 0; //unable to map the building
    }
    // Check the 4 non-diagonal adjacent tiles for a 1x1 warehouse
    if (map_terrain_is(map_grid_offset(x, y - 1), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x, y - 1)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x;
        ry = y - 1;
        has_road = 1;
    } else if (map_terrain_is(map_grid_offset(x + 1, y), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x + 1, y)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x + 1;
        ry = y;
        has_road = 1;
    } else if (map_terrain_is(map_grid_offset(x, y + 1), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x, y + 1)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x;
        ry = y + 1;
        has_road = 1;
    } else if (map_terrain_is(map_grid_offset(x - 1, y), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x - 1, y)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x - 1;
        ry = y;
        has_road = 1;
    }

    if (has_road) {
        warehouse->road_access_x = rx;
        warehouse->road_access_y = ry;
        if (road) {
            map_point_store_result(rx, ry, road);
        }
        return 1;
    }
    return 0;
}

int map_has_road_access_rotation(int rotation, int x, int y, int size, map_point *road)
{
    // do not use for warehouses or granaries, they have their own access checks
    switch (rotation) {
        case 3:
            x = x - size + 1;
            break;
        case 2:
            x = x - size + 1;
            y = y - size + 1;
            break;
        case 1:
            y = y - size + 1;
            break;
        default:
            break;
    }
    return store_best_road_access({ { { x, y }, size } }, road);
}

int map_has_road_access_hippodrome_rotation(int x, int y, map_point *road, int rotation)
{
    return store_best_road_access(map_road_access_hippodrome_areas(x, y, rotation), road);
}

int map_has_road_access_hippodrome(int x, int y, map_point *road)
{
    return map_has_road_access_hippodrome_rotation(x, y, road, building_rotation_get_rotation());
}

int map_has_road_access_granary(int x, int y, map_point *road)
{
    int rx = -1, ry = -1;
    int glp = config_get(CONFIG_GP_CH_GLOBAL_LABOUR);
    int valid_terrain = glp ? TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_ACCESS_RAMP : TERRAIN_ROAD | TERRAIN_ACCESS_RAMP;
    if (map_terrain_is(map_grid_offset(x + 1, y - 1), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x + 1, y - 1)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x + 1;
        ry = y - 1;
    } else if (map_terrain_is(map_grid_offset(x + 3, y + 1), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x + 3, y + 1)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x + 3;
        ry = y + 1;
    } else if (map_terrain_is(map_grid_offset(x + 1, y + 3), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x + 1, y + 3)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x + 1;
        ry = y + 3;
    } else if (map_terrain_is(map_grid_offset(x - 1, y + 1), valid_terrain) &&
        (Roadblock(building_get(map_building_at(map_grid_offset(x - 1, y + 1)))).kind() == ROADBLOCK_NONE || glp)) {
        rx = x - 1;
        ry = y + 1;
    }
    if (rx >= 0 && ry >= 0) {
        building *b = building_get(map_building_at(map_grid_offset(x, y)));
        if (b) {
            b->road_access_x = rx;
            b->road_access_y = ry;
        }
        if (road) {
            map_point_store_result(rx, ry, road);
        }
        return 1;
    }
    return 0;
}

int map_has_road_access_monument_construction(int x, int y, int size)
{
    return store_best_road_access(map_road_access_monument_construction_areas(x, y, size), nullptr);
}

static int road_within_radius(int x, int y, int size, int radius, int *x_road, int *y_road)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_ROAD)) {
                // Don't spawn walkers on roadblocks
                if (Roadblock(building_get(map_building_at(map_grid_offset(xx, yy)))).kind() != ROADBLOCK_NONE) {
                    continue;
                }
                if (x_road && y_road) {
                    *x_road = xx;
                    *y_road = yy;
                }
                return 1;
            }
        }
    }
    return 0;
}

int map_closest_road_within_radius(int x, int y, int size, int radius, int *x_road, int *y_road)
{
    for (int r = 1; r <= radius; r++) {
        if (road_within_radius(x, y, size, r, x_road, y_road)) {
            return 1;
        }
    }
    return 0;
}

static int terrain_is_road_like(int grid_offset)
{
    // Building road access ignores highways. Figure roaming has figure-specific
    // terrain checks in movement.cpp so roads_highway profiles can opt in.
    return map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_ACCESS_RAMP) ? 1 : 0;
}

int map_road_get_granary_inner_road_tiles_count(building *b)
{
    int count = 0;
    int base_x = b->x;
    int base_y = b->y;

    for (int y_offset = 0; y_offset < 3; y_offset++) {
        for (int x_offset = 0; x_offset < 3; x_offset++) {
            int tile_offset = map_grid_offset(base_x + x_offset, base_y + y_offset);
            if (map_terrain_is(tile_offset, TERRAIN_ROAD)) {
                count++;
            }
        }
    }
    return count;
}

static int get_adjacent_road_tile_for_roaming(int grid_offset, roadblock_permission perm)
{
    int is_road = terrain_is_road_like(grid_offset);
    int no_permissions = 0;
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {

        building *b = building_get(map_building_at(grid_offset));
        Building current(b);
        if (Roadblock(b).kind() != ROADBLOCK_NONE) {
            if (!Roadblock(b).has_permission(perm)) {
                no_permissions = 1;
            }
        }
        if (current.type && current.type->is_granary()) {
            if (figure_type_registry_impl::PathingMode::citizenIsRoad(grid_offset)) {
                if (map_road_get_granary_inner_road_tiles_count(b) >= 3) {
                    is_road = 1; //edges of the granary that connect to another road become roads
                    //not including passable terrain helps deal with roaming inside the granary
                } else {
                    is_road = 0; // dont roam into dead-end granaries
                }
            }
        } else if (current.type && current.type->is_warehouse()) {
            if (figure_type_registry_impl::PathingMode::citizenIsPassableTerrain(grid_offset) || figure_type_registry_impl::PathingMode::citizenIsRoad(grid_offset)) {
                is_road = 1;
            }
        }
    }
    is_road = is_road && !no_permissions;
    return is_road;
}

int map_get_adjacent_road_tiles_for_roaming(int grid_offset, int *road_tiles, int perm)
{
    road_tiles[1] = road_tiles[3] = road_tiles[5] = road_tiles[7] = 0;

    road_tiles[0] = get_adjacent_road_tile_for_roaming(grid_offset + map_grid_delta(0, -1),
        static_cast<roadblock_permission>(perm));
    road_tiles[2] = get_adjacent_road_tile_for_roaming(grid_offset + map_grid_delta(1, 0),
        static_cast<roadblock_permission>(perm));
    road_tiles[4] = get_adjacent_road_tile_for_roaming(grid_offset + map_grid_delta(0, 1),
        static_cast<roadblock_permission>(perm));
    road_tiles[6] = get_adjacent_road_tile_for_roaming(grid_offset + map_grid_delta(-1, 0),
        static_cast<roadblock_permission>(perm));

    return road_tiles[0] + road_tiles[2] + road_tiles[4] + road_tiles[6];
}
