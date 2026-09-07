#include "building/FoundationDef.h"
#include "city/map.h"
#include "map/TerrainMap.h"
#include "map/grid.h"
#include "map/water_navigation.h"
#include "scenario/map.h"
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace building_type_registry_impl {
bool FoundationDef::meets_proximity(const FoundationProximityRequirement &requirement, int origin_x, int origin_y, int rotation) const
{
    const auto footprint = rotated_cells(rotation);
    const int width = rotated_width(rotation), height = rotated_height(rotation);
    int count = 0;
    for (int y = origin_y - requirement.max_distance; y < origin_y + height + requirement.max_distance; ++y) {
        for (int x = origin_x - requirement.max_distance; x < origin_x + width + requirement.max_distance; ++x) {
            if (!map_grid_is_inside(x, y, 1)) continue;
            if (requirement.exclude_map_flags && ((x == city_map_entry_flag()->x && y == city_map_entry_flag()->y) ||
                (x == city_map_exit_flag()->x && y == city_map_exit_flag()->y))) continue;
            int distance = std::numeric_limits<int>::max();
            for (const auto &cell : footprint) {
                const int dx = std::abs(x - origin_x - cell.x), dy = std::abs(y - origin_y - cell.y);
                distance = std::min(distance, requirement.orthogonal_distance ? dx + dy : std::max(dx, dy));
            }
            if (distance < requirement.min_distance || distance > requirement.max_distance) continue;
            const int offset = map_grid_offset(x, y);
            const auto &terrain = terrain_map().at(offset);
            if (requirement.match_any ? !terrain.intersects(requirement.terrain) : !terrain.contains_all(requirement.terrain)) continue;
            if (requirement.navigable && !water_navigation::is_passable(offset, WaterNavigationProfile::Boat)) continue;
            if (requirement.sea) {
                const map_point entry = scenario_map_river_entry();
                if ((x != entry.x || y != entry.y) && water_navigation::path_length({x, y}, entry, WaterNavigationProfile::Boat) <= 0) continue;
            }
            if (++count >= requirement.min_count) return true;
        }
    }
    return false;
}
}
