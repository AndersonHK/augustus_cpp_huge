#include "building/building_record.h"
#include "road_access.h"

#include "building/building.h"
#include "building/BuildingFoundation.h"
#include "building/BuildingGeometry.h"
#include "building/building_type.h"
#include "core/config.h"
#include "city/map.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/road_network.h"
#include "map/terrain.h"
#include "map/tiles.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <vector>

static int terrain_is_road_like(int grid_offset)
{
    // Building road access ignores highways. Figure roaming has figure-specific
    // terrain checks in movement.cpp so roads_highway profiles can opt in.
    return map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_ACCESS_RAMP) ? 1 : 0;
}

static int tile_has_controlled_passage(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return 0;
    }
    Building existing = map_building_at(grid_offset);
    if (existing.type && existing.type->bridge().is_bridge()) {
        return 0;
    }
    return existing.Foundation &&
        existing.Foundation->passage_at(grid_offset) ==
            building_type_registry_impl::FoundationPassage::OwnerControlled;
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
        !tile_has_controlled_passage(grid_offset);
}

static int candidate_for_road_access_tile(int grid_offset, road_access_candidate *candidate)
{
    if (!terrain_is_road_like(grid_offset)) {
        return 0;
    }

    if (tile_has_controlled_passage(grid_offset)) {
        return 0;
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

static int candidate_for_building_road_access(
    const Building &building,
    int grid_offset,
    road_access_candidate *candidate)
{
    const int storage_rules = building.type && building.type->is_storage();
    if (!storage_rules) {
        return candidate_for_road_access_tile(grid_offset, candidate);
    }
    const int global_labor = config_get(CONFIG_GP_CH_GLOBAL_LABOUR);
    if (!tile_accepts_storage_road_access(grid_offset, global_labor)) {
        return 0;
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
        if (area.width <= 0 || area.height <= 0) {
            continue;
        }
        const int x_min = area.origin.x - 1;
        const int x_max = area.origin.x + area.width;
        const int y_min = area.origin.y - 1;
        const int y_max = area.origin.y + area.height;
        const auto visit_tile = [&visitor](int x, int y) {
            const int grid_offset = map_grid_offset(x, y);
            road_access_candidate candidate;
            if (candidate_for_road_access_tile(grid_offset, &candidate)) {
                visitor.visit(candidate);
            }
        };
        for (int x = area.origin.x; x < area.origin.x + area.width; ++x) {
            visit_tile(x, y_min);
        }
        for (int y = area.origin.y; y < area.origin.y + area.height; ++y) {
            visit_tile(x_max, y);
        }
        for (int x = area.origin.x + area.width - 1; x >= area.origin.x; --x) {
            visit_tile(x, y_max);
        }
        for (int y = area.origin.y + area.height - 1; y >= area.origin.y; --y) {
            visit_tile(x_min, y);
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

void RoadAccessQuery::addArea(int x, int y, int width, int height)
{
    areas_[area_count_++] = { { x, y }, width, height };
}

RoadAccessQuery RoadAccessQuery::fromFootprint(int x, int y, int size)
{
    return fromRectangle(x, y, size, size);
}

RoadAccessQuery RoadAccessQuery::fromRectangle(int x, int y, int width, int height)
{
    RoadAccessQuery query;
    query.addArea(x, y, width, height);
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

void map_road_access_visit_building_candidates(
    const Building &building,
    RoadAccessCandidateVisitor &visitor)
{
    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(building);
    if (!geometry.valid()) {
        return;
    }
    const Building &owner = geometry.owner() ? *geometry.owner() : building;
    for (const building_type_registry_impl::BuildingGeometryPoint &point :
        geometry.access_candidates()) {
        road_access_candidate candidate;
        if (candidate_for_building_road_access(
                owner, map_grid_offset(point.x, point.y), &candidate)) {
            visitor.visit(candidate);
        }
    }
}

int map_has_road_access(int x, int y, int size, map_point *road)
{
    return map_has_road_access_rotation(0, x, y, size, road);
}

int map_has_road_access_rectangle(int x, int y, int width, int height, map_point *road)
{
    return RoadAccessQuery::fromRectangle(x, y, width, height).hasRoadAccess(road);
}

namespace {

int has_internal_passage(const Building &building)
{
    if (!building.Foundation || !building.Foundation->state().is_published()) {
        return 0;
    }
    const auto &state = building.Foundation->state();
    std::set<int> occupied;
    for (const auto &cell : building.Foundation->cells(state.rotation())) {
        occupied.insert(map_grid_offset(state.origin_x() + cell.x, state.origin_y() + cell.y));
    }
    static const int neighbors[][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    for (const auto &cell : building.Foundation->cells(state.rotation())) {
        if (!cell.definition ||
            cell.definition->passage == building_type_registry_impl::FoundationPassage::None) {
            continue;
        }
        const int x = state.origin_x() + cell.x;
        const int y = state.origin_y() + cell.y;
        if (std::all_of(std::begin(neighbors), std::end(neighbors),
            [&occupied, x, y](const int (&neighbor)[2]) {
                const int adjacent_x = x + neighbor[0];
                const int adjacent_y = y + neighbor[1];
                return map_grid_is_inside(adjacent_x, adjacent_y, 1) &&
                    occupied.count(map_grid_offset(adjacent_x, adjacent_y)) != 0;
            })) {
            return 1;
        }
    }
    return 0;
}

} // namespace

int map_building_has_internal_passage(const building *b)
{
    Building *building = b ? Building::get(b->id) : nullptr;
    return building && has_internal_passage(*building);
}

void map_update_building_internal_roads(const building *b)
{
    if (!b) {
        return;
    }
    Building *building = Building::get(b->id);
    if (!building || !building->Foundation || !building->Foundation->state().is_published()) {
        return;
    }
    const auto &state = building->Foundation->state();
    std::set<int> occupied;
    for (const auto &cell : building->Foundation->cells(state.rotation())) {
        occupied.insert(map_grid_offset(state.origin_x() + cell.x, state.origin_y() + cell.y));
    }
    static const int neighbors[][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    if (!has_internal_passage(*building)) {
        return;
    }
    for (const auto &cell : building->Foundation->cells(state.rotation())) {
        if (!cell.definition ||
            cell.definition->passage == building_type_registry_impl::FoundationPassage::None) {
            continue;
        }
        const int x = state.origin_x() + cell.x;
        const int y = state.origin_y() + cell.y;
        bool touches_exterior = false;
        bool touches_road = false;
        for (const auto &neighbor : neighbors) {
            const int adjacent_x = x + neighbor[0];
            const int adjacent_y = y + neighbor[1];
            if (!map_grid_is_inside(adjacent_x, adjacent_y, 1)) {
                continue;
            }
            const int adjacent = map_grid_offset(adjacent_x, adjacent_y);
            if (!occupied.count(adjacent)) {
                touches_exterior = true;
                touches_road |= terrain_is_road_like(adjacent) != 0;
            }
        }
        const int grid_offset = map_grid_offset(x, y);
        if (!touches_exterior || touches_road) {
            map_terrain_add(grid_offset, TERRAIN_ROAD);
        } else {
            map_terrain_remove(grid_offset, TERRAIN_ROAD);
        }
    }
    map_tiles_update_area_roads(
        state.origin_x() - 1,
        state.origin_y() - 1,
        std::max(building->Foundation->width(state.rotation()), building->Foundation->height(state.rotation())) + 2);
}

int map_has_road_access_building(int x, int y, map_point *road)
{
    int grid_offset = map_grid_offset(x, y);
    if (!map_building_exists_at(grid_offset)) {
        return 0;
    }
    Building building = map_building_at(grid_offset);
    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(building);
    if (!geometry.valid() || !geometry.owner()) {
        return 0;
    }
    const Building &owner = *geometry.owner();
    ::building *owner_record = const_cast<::building *>(owner.record());
    if (!owner_record) {
        return 0;
    }
    const int storage_rules = owner.type && owner.type->is_storage();
    const int global_labor = storage_rules && config_get(CONFIG_GP_CH_GLOBAL_LABOUR);
    road_access_candidate best;
    bool found = false;
    for (const building_type_registry_impl::BuildingGeometryPoint &point : geometry.access_candidates()) {
        const int candidate = map_grid_offset(point.x, point.y);
        if (storage_rules) {
            if (!tile_accepts_storage_road_access(candidate, global_labor)) {
                continue;
            }
            const int rx = map_grid_offset_to_x(candidate);
            const int ry = map_grid_offset_to_y(candidate);
            owner_record->road_access_x = static_cast<unsigned char>(rx);
            owner_record->road_access_y = static_cast<unsigned char>(ry);
            if (road) {
                map_point_store_result(rx, ry, road);
            }
            return 1;
        }
        road_access_candidate current;
        if (candidate_for_road_access_tile(candidate, &current) &&
            (!found || current.network_index < best.network_index)) {
            best = current;
            found = true;
        }
    }
    if (found) {
        owner_record->road_access_x = static_cast<unsigned char>(best.road.x);
        owner_record->road_access_y = static_cast<unsigned char>(best.road.y);
        if (road) {
            map_point_store_result(best.road.x, best.road.y, road);
        }
        return 1;
    }
    return 0;
}

int map_has_road_access_rotation(int rotation, int x, int y, int size, map_point *road)
{
    return RoadAccessQuery::fromRotatedFootprint(rotation, x, y, size).hasRoadAccess(road);
}

static int road_within_radius(
    int x,
    int y,
    int width,
    int height,
    int radius,
    int *x_road,
    int *y_road)
{
    int x_min = x - radius;
    int y_min = y - radius;
    int x_max = x + width - 1 + radius;
    int y_max = y + height - 1 + radius;
    map_grid_bound_area(&x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (terrain_is_road_like(map_grid_offset(xx, yy))) {
                const int grid_offset = map_grid_offset(xx, yy);
                if (tile_has_controlled_passage(grid_offset)) {
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
    return map_closest_road_within_radius_rectangle(x, y, size, size, radius, x_road, y_road);
}

int map_closest_road_within_radius_building(
    const Building &building,
    int radius,
    int *x_road,
    int *y_road)
{
    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(building);
    if (!geometry.valid() || radius <= 0) {
        return 0;
    }
    for (int distance = 1; distance <= radius; ++distance) {
        for (const building_type_registry_impl::BuildingGeometryPoint &point :
            geometry.access_points_at_distance(distance)) {
            if (!map_grid_is_inside(point.x, point.y, 1)) {
                continue;
            }
            const int grid_offset = map_grid_offset(point.x, point.y);
            if (!terrain_is_road_like(grid_offset) || tile_has_controlled_passage(grid_offset)) {
                continue;
            }
            if (x_road && y_road) {
                *x_road = point.x;
                *y_road = point.y;
            }
            return 1;
        }
    }
    return 0;
}

int map_closest_road_within_radius_rectangle(
    int x,
    int y,
    int width,
    int height,
    int radius,
    int *x_road,
    int *y_road)
{
    for (int r = 1; r <= radius; r++) {
        if (road_within_radius(x, y, width, height, r, x_road, y_road)) {
            return 1;
        }
    }
    return 0;
}

int map_road_get_internal_passage_tiles_count(building *b)
{
    if (!b) {
        return 0;
    }
    Building *building = Building::get(b->id);
    if (!building || !building->Foundation || !building->Foundation->state().is_published()) {
        return 0;
    }
    int count = 0;
    const auto &state = building->Foundation->state();
    for (const auto &cell : building->Foundation->cells(state.rotation())) {
        if (cell.definition &&
            cell.definition->passage != building_type_registry_impl::FoundationPassage::None &&
            map_terrain_is(map_grid_offset(state.origin_x() + cell.x, state.origin_y() + cell.y), TERRAIN_ROAD)) {
            ++count;
        }
    }
    return count;
}
