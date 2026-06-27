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

static int terrain_is_road_like(int grid_offset)
{
    // Building road access ignores highways. Figure roaming has figure-specific
    // terrain checks in movement.cpp so roads_highway profiles can opt in.
    return map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_ACCESS_RAMP) ? 1 : 0;
}

static int tile_accepts_storage_road_access(int grid_offset, int global_labor)
{
    const int valid_terrain = global_labor ?
        TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_ACCESS_RAMP :
        TERRAIN_ROAD | TERRAIN_ACCESS_RAMP;
    if (!map_terrain_is(grid_offset, valid_terrain)) {
        return 0;
    }
    return global_labor ||
        Roadblock(building_get(map_building_at(grid_offset))).kind() == ROADBLOCK_NONE;
}

static int first_storage_road_access_point(
    int x,
    int y,
    const int (*offsets)[2],
    int offset_count,
    int global_labor,
    int *x_road,
    int *y_road)
{
    for (int i = 0; i < offset_count; i++) {
        const int candidate_x = x + offsets[i][0];
        const int candidate_y = y + offsets[i][1];
        if (tile_accepts_storage_road_access(map_grid_offset(candidate_x, candidate_y), global_labor)) {
            *x_road = candidate_x;
            *y_road = candidate_y;
            return 1;
        }
    }
    return 0;
}

static int candidate_for_road_access_tile(int grid_offset, road_access_candidate *candidate)
{
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        Building adjacent(building_get(map_building_at(grid_offset)));
        if (adjacent.type && adjacent.type->roadblock().is_wall_gate()) {
            return 0;
        }
    }
    if (!terrain_is_road_like(grid_offset)) {
        return 0;
    }

    const int building_id = map_building_at(grid_offset);
    roadblock_type kind = building_id ? Roadblock(building_get(building_id)).kind() : ROADBLOCK_NONE;
    if (kind == ROADBLOCK_STANDARD || kind == ROADBLOCK_STORAGE) {
        return 0; // ignore non-bridge roadblocks
    }

    if (candidate) {
        const int network_id = map_road_network_get(grid_offset);
        *candidate = {
            grid_offset,
            { map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset) },
            network_id,
            city_map_road_network_index(network_id)
        };
    }
    return 1;
}

void map_road_access_visit_candidates(
    const road_access_area *areas,
    int area_count,
    RoadAccessCandidateVisitor &visitor)
{
    if (!areas || area_count <= 0) {
        return;
    }
    for (int i = 0; i < area_count; i++) {
        const road_access_area &area = areas[i];
        int base_offset = map_grid_offset(area.origin.x, area.origin.y);
        for (const int *tile_delta = map_grid_adjacent_offsets(area.size); *tile_delta; tile_delta++) {
            const int grid_offset = base_offset + *tile_delta;
            road_access_candidate candidate;
            if (candidate_for_road_access_tile(grid_offset, &candidate)) {
                visitor.visit(candidate);
            }
        }
    }
}

void map_road_access_visit_candidates(int x, int y, int size, RoadAccessCandidateVisitor &visitor)
{
    const road_access_area area = { { x, y }, size };
    map_road_access_visit_candidates(&area, 1, visitor);
}

static int fill_hippodrome_areas(road_access_area *areas, int x, int y, int rotation)
{
    areas[0] = { { x, y }, 5 };
    int x_offset = 0;
    int y_offset = 0;
    building_rotation_get_offset_with_rotation(5, rotation, &x_offset, &y_offset);
    areas[1] = { { x + x_offset, y + y_offset }, 5 };
    building_rotation_get_offset_with_rotation(10, rotation, &x_offset, &y_offset);
    areas[2] = { { x + x_offset, y + y_offset }, 5 };
    return 3;
}

void map_road_access_visit_hippodrome_candidates(
    int x,
    int y,
    int rotation,
    RoadAccessCandidateVisitor &visitor)
{
    road_access_area areas[3];
    const int area_count = fill_hippodrome_areas(areas, x, y, rotation);
    map_road_access_visit_candidates(areas, area_count, visitor);
}

static int fill_monument_construction_areas(road_access_area *areas, int x, int y, int size)
{
    if (size < 3) {
        areas[0] = { { x, y }, size };
        return 1;
    }

    const int half_size = size / 2;
    int area_count = 0;
    areas[area_count++] = { { x + half_size, y + size - 1 }, 1 };
    areas[area_count++] = { { x + size - 1, y + half_size }, 1 };
    areas[area_count++] = { { x + half_size, y }, 1 };
    areas[area_count++] = { { x, y + half_size }, 1 };
    if (size % 2 != 0) {
        areas[area_count++] = { { x + 1, y + size - 1 }, 1 };
        areas[area_count++] = { { x + size - 1, y + 1 }, 1 };
        areas[area_count++] = { { x + 1, y }, 1 };
        areas[area_count++] = { { x, y + 1 }, 1 };
    }
    return area_count;
}

void map_road_access_visit_monument_construction_candidates(
    int x,
    int y,
    int size,
    RoadAccessCandidateVisitor &visitor)
{
    road_access_area areas[8];
    const int area_count = fill_monument_construction_areas(areas, x, y, size);
    map_road_access_visit_candidates(areas, area_count, visitor);
}

class BestRoadAccessVisitor : public RoadAccessCandidateVisitor {
public:
    void visit(const road_access_candidate &candidate) override
    {
        if (!found_ || candidate.network_index < candidate_.network_index) {
            candidate_ = candidate;
            found_ = 1;
        }
    }

    int found() const { return found_; }
    const road_access_candidate &candidate() const { return candidate_; }

private:
    road_access_candidate candidate_;
    int found_ = 0;
};

template <typename VisitCandidates>
static int store_best_road_access(const VisitCandidates &visit_candidates, map_point *road)
{
    BestRoadAccessVisitor best;
    visit_candidates(best);
    if (!best.found()) {
        return 0;
    }

    if (road) {
        map_point_store_result(best.candidate().road.x, best.candidate().road.y, road);
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
    if (!warehouse) {
        return 0; //unable to map the building
    }
    x = warehouse->x;
    y = warehouse->y;
    int rx = x;
    int ry = y;
    const int access_offsets[][2] = {
        { 0, -1 },
        { 1, 0 },
        { 0, 1 },
        { -1, 0 },
    };
    if (first_storage_road_access_point(
        x,
        y,
        access_offsets,
        sizeof(access_offsets) / sizeof(access_offsets[0]),
        config_get(CONFIG_GP_CH_GLOBAL_LABOUR),
        &rx,
        &ry)) {
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
    return store_best_road_access(
        [x, y, size](RoadAccessCandidateVisitor &visitor) {
            map_road_access_visit_candidates(x, y, size, visitor);
        },
        road);
}

int map_has_road_access_hippodrome_rotation(int x, int y, map_point *road, int rotation)
{
    return store_best_road_access(
        [x, y, rotation](RoadAccessCandidateVisitor &visitor) {
            map_road_access_visit_hippodrome_candidates(x, y, rotation, visitor);
        },
        road);
}

int map_has_road_access_hippodrome(int x, int y, map_point *road)
{
    return map_has_road_access_hippodrome_rotation(x, y, road, building_rotation_get_rotation());
}

int map_has_road_access_granary(int x, int y, map_point *road)
{
    int rx = -1, ry = -1;
    const int access_offsets[][2] = {
        { 1, -1 },
        { 3, 1 },
        { 1, 3 },
        { -1, 1 },
    };
    if (first_storage_road_access_point(
        x,
        y,
        access_offsets,
        sizeof(access_offsets) / sizeof(access_offsets[0]),
        config_get(CONFIG_GP_CH_GLOBAL_LABOUR),
        &rx,
        &ry)) {
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
    return store_best_road_access(
        [x, y, size](RoadAccessCandidateVisitor &visitor) {
            map_road_access_visit_monument_construction_candidates(x, y, size, visitor);
        },
        nullptr);
}

static int road_within_radius(int x, int y, int size, int radius, int *x_road, int *y_road)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (terrain_is_road_like(map_grid_offset(xx, yy))) {
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
