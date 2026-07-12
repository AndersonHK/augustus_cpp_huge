#include "building/building_record.h"
#include "road_access.h"

#include "building/building.h"
#include "building/building_type.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "core/config.h"
#include "city/map.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/road_network.h"
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
        !map_building_exists_at(grid_offset) ||
        Roadblock(map_building_at(grid_offset)).kind() == ROADBLOCK_NONE;
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
        Building *adjacent = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
        if (adjacent && adjacent->type && adjacent->type->roadblock().is_wall_gate()) {
            return 0;
        }
    }
    if (!terrain_is_road_like(grid_offset)) {
        return 0;
    }

    roadblock_type kind = map_building_exists_at(grid_offset) ?
        Roadblock(map_building_at(grid_offset)).kind() :
        ROADBLOCK_NONE;
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
    RoadAccessQuery::fromFootprint(x, y, size).visitCandidates(visitor);
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

void RoadAccessQuery::addArea(int x, int y, int size)
{
    areas_[area_count_++] = { { x, y }, size };
}

RoadAccessQuery RoadAccessQuery::fromFootprint(int x, int y, int size)
{
    RoadAccessQuery query;
    query.addArea(x, y, size);
    return query;
}

RoadAccessQuery RoadAccessQuery::fromRotatedFootprint(int rotation, int x, int y, int size)
{
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
    return fromFootprint(x, y, size);
}

RoadAccessQuery RoadAccessQuery::hippodrome(int x, int y, int rotation)
{
    RoadAccessQuery query;
    query.addArea(x, y, 5);
    int x_offset = 0;
    int y_offset = 0;
    building_rotation_get_offset_with_rotation(5, rotation, &x_offset, &y_offset);
    query.addArea(x + x_offset, y + y_offset, 5);
    building_rotation_get_offset_with_rotation(10, rotation, &x_offset, &y_offset);
    query.addArea(x + x_offset, y + y_offset, 5);
    return query;
}

RoadAccessQuery RoadAccessQuery::monumentConstruction(int x, int y, int size)
{
    RoadAccessQuery query;
    if (size < 3) {
        query.addArea(x, y, size);
        return query;
    }

    const int half_size = size / 2;
    query.addArea(x + half_size, y + size - 1, 1);
    query.addArea(x + size - 1, y + half_size, 1);
    query.addArea(x + half_size, y, 1);
    query.addArea(x, y + half_size, 1);
    if (size % 2 != 0) {
        query.addArea(x + 1, y + size - 1, 1);
        query.addArea(x + size - 1, y + 1, 1);
        query.addArea(x + 1, y, 1);
        query.addArea(x, y + 1, 1);
    }
    return query;
}

void RoadAccessQuery::visitCandidates(RoadAccessCandidateVisitor &visitor) const
{
    map_road_access_visit_candidates(areas_, area_count_, visitor);
}

int RoadAccessQuery::hasRoadAccess(map_point *road) const
{
    BestRoadAccessVisitor best;
    visitCandidates(best);
    if (!best.found()) {
        return 0;
    }

    if (road) {
        map_point_store_result(best.candidate().road.x, best.candidate().road.y, road);
    }
    return 1;
}

void map_road_access_visit_hippodrome_candidates(
    int x,
    int y,
    int rotation,
    RoadAccessCandidateVisitor &visitor)
{
    RoadAccessQuery::hippodrome(x, y, rotation).visitCandidates(visitor);
}

void map_road_access_visit_monument_construction_candidates(
    int x,
    int y,
    int size,
    RoadAccessCandidateVisitor &visitor)
{
    RoadAccessQuery::monumentConstruction(x, y, size).visitCandidates(visitor);
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
    int grid_offset = map_grid_offset(x, y);
    if (!map_building_exists_at(grid_offset)) {
        return 0;
    }
    Building warehouse_building = map_building_at(grid_offset).main();
    building *warehouse = const_cast<::building *>(warehouse_building.record());
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
        warehouse->road_access_x = static_cast<unsigned char>(rx);
        warehouse->road_access_y = static_cast<unsigned char>(ry);
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
    return RoadAccessQuery::fromRotatedFootprint(rotation, x, y, size).hasRoadAccess(road);
}

int map_has_road_access_hippodrome_rotation(int x, int y, map_point *road, int rotation)
{
    return RoadAccessQuery::hippodrome(x, y, rotation).hasRoadAccess(road);
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
        int grid_offset = map_grid_offset(x, y);
        building *b = map_building_exists_at(grid_offset) ?
            const_cast<::building *>(map_building_at(grid_offset).record()) :
            nullptr;
        if (b) {
            b->road_access_x = static_cast<unsigned char>(rx);
            b->road_access_y = static_cast<unsigned char>(ry);
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
    return RoadAccessQuery::monumentConstruction(x, y, size).hasRoadAccess(nullptr);
}

static int road_within_radius(int x, int y, int size, int radius, int *x_road, int *y_road)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (terrain_is_road_like(map_grid_offset(xx, yy))) {
                // Don't spawn walkers on roadblocks
                const int grid_offset = map_grid_offset(xx, yy);
                if (map_building_exists_at(grid_offset) &&
                    Roadblock(map_building_at(grid_offset)).kind() != ROADBLOCK_NONE) {
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
