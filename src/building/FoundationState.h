#pragma once

#include "map/TerrainSet.h"

#include "building/FoundationDef.h"
#include "building/RoadblockState.h"

#include <stdint.h>

#include <vector>

namespace building_type_registry_impl {

struct FoundationTerrainDelta {
    int cell_index = -1;
    int grid_offset = -1;
    TerrainSet added_terrain;
    TerrainSet removed_terrain;
    int bound_building = 0;
};

struct FoundationTerrainMutation {
    TerrainSet terrain_after;
    FoundationTerrainDelta delta;
};

FoundationTerrainMutation foundation_apply_terrain_cell(
    const FoundationCellDefinition &cell,
    int cell_index,
    int grid_offset,
    TerrainSet terrain_before);
TerrainSet foundation_restore_terrain_cell(
    TerrainSet terrain_after,
    const FoundationTerrainDelta &delta);

class FoundationState {
public:
    const std::vector<FoundationTerrainDelta> &terrain_deltas() const;
    int is_published() const;
    int origin_x() const;
    int origin_y() const;
    int rotation() const;
    RoadblockState &roadblock();
    const RoadblockState &roadblock() const;
    void clear();
    void begin_publication(int origin_x, int origin_y, int rotation);
    void record_delta(FoundationTerrainDelta delta);
    bool release_added_terrain(int cell_index, TerrainSet terrain);

private:
    int published_ = 0;
    int origin_x_ = 0;
    int origin_y_ = 0;
    int rotation_ = 0;
    RoadblockState roadblock_;
    std::vector<FoundationTerrainDelta> terrain_deltas_;
};

} // namespace building_type_registry_impl
