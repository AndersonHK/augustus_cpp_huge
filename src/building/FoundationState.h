#pragma once

#include "building/FoundationDef.h"
#include "building/RoadblockState.h"

#include <stdint.h>

#include <vector>

namespace building_type_registry_impl {

struct FoundationTerrainDelta {
    int cell_index = -1;
    int grid_offset = -1;
    uint32_t added_terrain = 0;
    uint32_t removed_terrain = 0;
    int bound_building = 0;
};

struct FoundationTerrainMutation {
    uint32_t terrain_after = 0;
    FoundationTerrainDelta delta;
};

FoundationTerrainMutation foundation_apply_terrain_cell(
    const FoundationCellDefinition &cell,
    int cell_index,
    int grid_offset,
    uint32_t terrain_before);
uint32_t foundation_restore_terrain_cell(
    uint32_t terrain_after,
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

private:
    int published_ = 0;
    int origin_x_ = 0;
    int origin_y_ = 0;
    int rotation_ = 0;
    RoadblockState roadblock_;
    std::vector<FoundationTerrainDelta> terrain_deltas_;
};

} // namespace building_type_registry_impl
