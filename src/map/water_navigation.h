#pragma once

#include "map/point.h"

#include <cstdint>
#include <vector>


enum class WaterNavigationProfile : std::uint8_t {
    Boat,
    Flotsam,
};

namespace water_navigation {

// Invalidations only mark derived state dirty. Work is coalesced until the
// next query, or performed synchronously when world loading finishes.
void invalidate_topology();
void invalidate_dock_endpoints();
void invalidate_river_anchors();

void begin_world_load();
void finish_world_load();

bool is_passable(int grid_offset, WaterNavigationProfile profile);

// Returns uncompressed map directions (0..7), one entry per traversed tile.
// The caller retains ownership of the result and may compress it for storage.
bool find_path(
    const map_point &source,
    const map_point &destination,
    WaterNavigationProfile profile,
    int direction_limit,
    std::vector<std::uint8_t> *directions);

int path_length(
    const map_point &source,
    const map_point &destination,
    WaterNavigationProfile profile);

bool can_reach_adjacent(
    const map_point &source,
    const std::vector<int> &foundation_water_cells,
    WaterNavigationProfile profile = WaterNavigationProfile::Boat);

} // namespace water_navigation
