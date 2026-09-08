#include "map/tile_runtime_graphics.h"
#include "building/building_record.h"
#include "map/TerrainMap.h"
#include <array>
#include <set>

#include "building/building.h"
#include "building/BuildingGeometry.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/water_access_runtime.h"
#include "city/map.h"
#include "core/direction.h"
#include "core/image.h"
#include "core/Logger.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/data.h"
#include <map>
#include "map/property.h"
#include "map/ring.h"
#include "map/sprite.h"
#include "map/water_navigation.h"

namespace {
bool sea_passable(const TerrainSet &terrain)
{
    bool water = false;
    for (const auto *entry : terrain.entries()) {
        if (!entry->allows_sea()) return false;
        water |= entry->is_water();
    }
    return water;
}

}

const TerrainSet &TerrainMap::intern(TerrainSet terrain)
{
    if (terrain.empty()) return empty_terrain;
    return *compositions.insert(std::move(terrain)).first;
}

void TerrainMap::count_changed(int offset, const TerrainSet &before, const TerrainSet &after)
{
    if (terrain_counts.empty() || !map_grid_is_inside(map_grid_offset_to_x(offset), map_grid_offset_to_y(offset), 1)) return;
    for (auto &[query, count] : terrain_counts) count += static_cast<int>(after.intersects(query)) - static_cast<int>(before.intersects(query));
}
TerrainMap &terrain_map() { static TerrainMap map; return map; }

const TerrainSet &TerrainMap::at(int offset) const
{
    return map_grid_is_valid_offset(offset) && tiles[offset] ? *tiles[offset] : empty_terrain;
}

int TerrainMap::distance_to_nearest(int grid_offset, const TerrainSet &terrain, int max_distance) const
{
    const int x = map_grid_offset_to_x(grid_offset), y = map_grid_offset_to_y(grid_offset);
    for (int radius = 0; radius <= max_distance; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) for (int dx = -radius; dx <= radius; ++dx) {
            if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
            if (map_grid_is_inside(x + dx, y + dy, 1) && at(map_grid_offset(x + dx, y + dy)).intersects(terrain)) return radius;
        }
    }
    return max_distance + 1;
}

int TerrainMap::contains(int offset, const TerrainSet &terrain) { return map_grid_is_valid_offset(offset) && at(offset).intersects(terrain); }
int TerrainMap::contains_all(int offset, const TerrainSet &terrain) { return map_grid_is_valid_offset(offset) && at(offset).contains_all(terrain); }
int TerrainMap::is_roadblock(int offset) { return contains(offset, terrain_types().building) && contains(offset, terrain_types().road); }

int TerrainMap::count(const TerrainSet &terrain)
{
    const auto found = terrain_counts.find(terrain);
    if (found != terrain_counts.end()) return found->second;
    int count = 0;
    for (int y = 0; y < map_data.height; ++y) for (int x = 0; x < map_data.width; ++x) count += contains(map_grid_offset(x, y), terrain);
    terrain_counts.emplace(terrain, count);
    return count;
}

void TerrainMap::set(int offset, const TerrainSet &terrain)
{
    if (!map_grid_is_valid_offset(offset)) return;
    TerrainSet complete = terrain;
    for (const auto *entry : terrain.entries()) complete |= entry->includes();
    const TerrainSet &before = at(offset);
    if (before == complete) return;
    const TerrainSet &after = intern(std::move(complete));
    tiles[offset] = &after;
    for (const auto *entry : before.entries()) if (!after.contains(*entry) && entry->has_water_images()) tile_runtime_clear_terrain_image(offset);
    count_changed(offset, before, after);
    if (sea_passable(before) != sea_passable(after) || before.intersects(terrain_types().water) != after.intersects(terrain_types().water)) water_navigation::invalidate_topology();
    water_access_runtime_terrain_changed(offset, before, after);
}

void TerrainMap::add(int offset, const TerrainSet &terrain) { set(offset, at(offset) | terrain); }

void TerrainMap::remove(int offset, const TerrainSet &terrain)
{
    TerrainSet remaining = at(offset) - terrain;
    const TerrainSet candidates = remaining;
    for (const auto *entry : candidates.entries()) if (entry->includes().intersects(terrain)) remaining -= *entry;
    set(offset, remaining);
}

void TerrainMap::remove_all(const TerrainSet &terrain)
{
    for (int offset = 0; offset < GRID_SIZE * GRID_SIZE; ++offset) if (contains(offset, terrain)) remove(offset, terrain);
}

void TerrainMap::backup() { backup_tiles = tiles; }
void TerrainMap::restore()
{
    tiles = backup_tiles;
    terrain_counts.clear();
    water_navigation::invalidate_topology();
}
void TerrainMap::clear()
{
    tiles.fill(nullptr);
    backup_tiles.fill(nullptr);
    terrain_counts.clear();
    compositions.clear();
    water_navigation::invalidate_topology();
}

void TerrainMap::init_outside_map()
{
    int width, height;
    map_grid_size(&width, &height);
    const int left = (GRID_SIZE - width) / 2, top = (GRID_SIZE - height) / 2;
    const TerrainSet &edge = intern(terrain_types().map_edge);
    for (int y = 0; y < GRID_SIZE; ++y) for (int x = 0; x < GRID_SIZE; ++x) {
        if (x < left || x >= left + width || y < top || y >= top + height) tiles[x + GRID_SIZE * y] = &edge;
    }
    terrain_counts.clear();
    water_navigation::invalidate_topology();
}

void TerrainMap::add_with_radius(int x, int y, int size, int radius, const TerrainSet &terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            terrain_map().add(map_grid_offset(xx, yy), terrain);
        }
    }
}

void TerrainMap::remove_with_radius(int x, int y, int size, int radius, const TerrainSet &terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            terrain_map().remove(map_grid_offset(xx, yy), terrain);
        }
    }
}

int TerrainMap::count_directly_adjacent_with_type(int grid_offset, const TerrainSet &terrain)
{
    int count = 0;
    if (terrain_map().contains(grid_offset + map_grid_delta(0, -1), terrain)) {
        count++;
    }
    if (terrain_map().contains(grid_offset + map_grid_delta(1, 0), terrain)) {
        count++;
    }
    if (terrain_map().contains(grid_offset + map_grid_delta(0, 1), terrain)) {
        count++;
    }
    if (terrain_map().contains(grid_offset + map_grid_delta(-1, 0), terrain)) {
        count++;
    }
    return count;
}

int TerrainMap::count_directly_adjacent_with_types(int grid_offset, const TerrainSet &terrain_sum)
{
    int count = 0;
    if (terrain_map().contains_all(grid_offset + map_grid_delta(0, -1), terrain_sum)) {
        count++;
    }
    if (terrain_map().contains_all(grid_offset + map_grid_delta(1, 0), terrain_sum)) {
        count++;
    }
    if (terrain_map().contains_all(grid_offset + map_grid_delta(0, 1), terrain_sum)) {
        count++;
    }
    if (terrain_map().contains_all(grid_offset + map_grid_delta(-1, 0), terrain_sum)) {
        count++;
    }
    return count;
}


int TerrainMap::count_diagonally_adjacent_with_type(int grid_offset, const TerrainSet &terrain)
{
    int count = 0;
    if (terrain_map().contains(grid_offset + map_grid_delta(1, -1), terrain)) {
        count++;
    }
    if (terrain_map().contains(grid_offset + map_grid_delta(1, 1), terrain)) {
        count++;
    }
    if (terrain_map().contains(grid_offset + map_grid_delta(-1, 1), terrain)) {
        count++;
    }
    if (terrain_map().contains(grid_offset + map_grid_delta(-1, -1), terrain)) {
        count++;
    }
    return count;
}

int TerrainMap::has_adjacent_x_with_type(int grid_offset, const TerrainSet &terrain)
{
    if (terrain_map().contains(grid_offset + map_grid_delta(0, -1), terrain) ||
        terrain_map().contains(grid_offset + map_grid_delta(0, 1), terrain)) {
        return 1;
    }
    return 0;
}

int TerrainMap::has_adjacent_y_with_type(int grid_offset, const TerrainSet &terrain)
{
    if (terrain_map().contains(grid_offset + map_grid_delta(-1, 0), terrain) ||
        terrain_map().contains(grid_offset + map_grid_delta(1, 0), terrain)) {
        return 1;
    }
    return 0;
}

int TerrainMap::exists_tile_in_area_with_type(int x, int y, int size, const TerrainSet &terrain)
{
    for (int yy = y; yy < y + size; yy++) {
        for (int xx = x; xx < x + size; xx++) {
            if (map_grid_is_inside(xx, yy, 1) && at(map_grid_offset(xx, yy)).intersects(terrain)) {
                return 1;
            }
        }
    }
    return 0;
}

int TerrainMap::exists_tile_in_radius_with_type(int x, int y, int size, int radius, const TerrainSet &terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (terrain_map().contains(map_grid_offset(xx, yy), terrain)) {
                return 1;
            }
        }
    }
    return 0;
}

int TerrainMap::exists_rock_in_radius(int x, int y, int size, int radius)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    int entry_flag_offset = map_grid_offset(city_map_entry_flag()->x, city_map_entry_flag()->y);
    int exit_flag_offset = map_grid_offset(city_map_exit_flag()->x, city_map_exit_flag()->y);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int offset = map_grid_offset(xx, yy);
            if (offset == entry_flag_offset || offset == exit_flag_offset) {
                continue;
            }
            if (terrain_map().contains(offset, terrain_types().rock)) {
                return 1;
            }
        }
    }
    return 0;
}

int TerrainMap::exists_clear_tile_in_radius(int x, int y, int size, int radius, int except_grid_offset,
    int *x_tile, int *y_tile)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (grid_offset != except_grid_offset && !(at(grid_offset).intersects(terrain_types().not_clear))) {
                *x_tile = xx;
                *y_tile = yy;
                return 1;
            }
        }
    }
    *x_tile = x_max;
    *y_tile = y_max;
    return 0;
}

int TerrainMap::all_tiles_in_radius_are(int x, int y, int size, int radius, const TerrainSet &terrain)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (!terrain_map().contains(map_grid_offset(xx, yy), terrain)) {
                return 0;
            }
        }
    }
    return 1;
}

int TerrainMap::has_only_rocks_trees_in_ring(int x, int y, int distance)
{
    int start = map_ring_start(1, distance);
    int end = map_ring_end(1, distance);
    int base_offset = map_grid_offset(x, y);
    for (int i = start; i < end; i++) {
        const ring_tile *tile = map_ring_tile(i);
        if (map_ring_is_inside_map(x + tile->x, y + tile->y)) {
            if (!terrain_map().contains(base_offset + tile->grid_offset,
                terrain_types().rock | terrain_types().tree | terrain_types().originally_tree)) {
                return 0;
            }
        }
    }
    return 1;
}

int TerrainMap::has_only_meadow_in_ring(int x, int y, int distance)
{
    int start = map_ring_start(1, distance);
    int end = map_ring_end(1, distance);
    int base_offset = map_grid_offset(x, y);
    for (int i = start; i < end; i++) {
        const ring_tile *tile = map_ring_tile(i);
        if (map_ring_is_inside_map(x + tile->x, y + tile->y)) {
            if (!terrain_map().contains(base_offset + tile->grid_offset, terrain_types().meadow)) {
                return 0;
            }
        }
    }
    return 1;
}

int TerrainMap::is_adjacent_to_wall(int x, int y, int size)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        if (terrain_map().contains(base_offset + *tile_delta, terrain_types().wall)) {
            return 1;
        }
    }
    return 0;
}

int TerrainMap::is_adjacent_to_water(int x, int y, int size)
{
    int base_offset = map_grid_offset(x, y);
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        if (terrain_map().contains(base_offset + *tile_delta, terrain_types().water)) {
            return 1;
        }
    }
    return 0;
}

int TerrainMap::get_adjacent_road_or_clear_land(
    const Building &building,
    int *x_tile,
    int *y_tile)
{
    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(building);
    for (const building_type_registry_impl::BuildingGeometryPoint &candidate :
        geometry.access_candidates()) {
        const int grid_offset = map_grid_offset(candidate.x, candidate.y);
        if (terrain_map().contains(grid_offset, terrain_types().road | terrain_types().rubble | terrain_types().garden | terrain_types().highway) ||
            !terrain_map().contains(grid_offset, terrain_types().not_clear)) {
            *x_tile = candidate.x;
            *y_tile = candidate.y;
            return 1;
        }
    }
    return 0;
}

static void add_road_if_no_highway(int grid_offset)
{
    if (!terrain_map().contains(grid_offset, terrain_types().highway)) {
        terrain_map().add(grid_offset, terrain_types().road);
    }
}

static void add_road_if_clear(int grid_offset)
{
    if (!terrain_map().contains(grid_offset, terrain_types().not_clear)) {
        terrain_map().add(grid_offset, terrain_types().road);
    }
}

void TerrainMap::add_roadblock_road(int x, int y)
{
    // roads under roadblock
    terrain_map().add(map_grid_offset(x, y), terrain_types().road);
}

void TerrainMap::add_gatehouse_roads(int x, int y, int orientation)
{
    // roads under gatehouse
    add_road_if_no_highway(map_grid_offset(x, y));
    add_road_if_no_highway(map_grid_offset(x + 1, y));
    add_road_if_no_highway(map_grid_offset(x, y + 1));
    add_road_if_no_highway(map_grid_offset(x + 1, y + 1));

    // free roads before/after gate
    if (orientation == 1) {
        add_road_if_clear(map_grid_offset(x, y - 1));
        add_road_if_clear(map_grid_offset(x + 1, y - 1));
        add_road_if_clear(map_grid_offset(x, y + 2));
        add_road_if_clear(map_grid_offset(x + 1, y + 2));
    } else if (orientation == 2) {
        add_road_if_clear(map_grid_offset(x - 1, y));
        add_road_if_clear(map_grid_offset(x - 1, y + 1));
        add_road_if_clear(map_grid_offset(x + 2, y));
        add_road_if_clear(map_grid_offset(x + 2, y + 1));
    }
}

void TerrainMap::add_triumphal_arch_roads(int x, int y, int orientation)
{
    if (orientation == 1) {
        // road in the middle
        terrain_map().add(map_grid_offset(x + 1, y), terrain_types().road);
        terrain_map().add(map_grid_offset(x + 1, y + 1), terrain_types().road);
        terrain_map().add(map_grid_offset(x + 1, y + 2), terrain_types().road);
        // no roads on other tiles
        terrain_map().remove(map_grid_offset(x, y), terrain_types().road);
        terrain_map().remove(map_grid_offset(x, y + 1), terrain_types().road);
        terrain_map().remove(map_grid_offset(x, y + 2), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 2, y), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 2, y + 1), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 2, y + 2), terrain_types().road);
    } else if (orientation == 2) {
        // road in the middle
        terrain_map().add(map_grid_offset(x, y + 1), terrain_types().road);
        terrain_map().add(map_grid_offset(x + 1, y + 1), terrain_types().road);
        terrain_map().add(map_grid_offset(x + 2, y + 1), terrain_types().road);
        // no roads on other tiles
        terrain_map().remove(map_grid_offset(x, y), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 1, y), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 2, y), terrain_types().road);
        terrain_map().remove(map_grid_offset(x, y + 2), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 1, y + 2), terrain_types().road);
        terrain_map().remove(map_grid_offset(x + 2, y + 2), terrain_types().road);
    }
}

