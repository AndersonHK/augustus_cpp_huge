#include "building/FoundationState.h"

namespace building_type_registry_impl {

FoundationTerrainMutation foundation_apply_terrain_cell(
    const FoundationCellDefinition &cell,
    int cell_index,
    int grid_offset,
    uint32_t terrain_before)
{
    FoundationTerrainMutation mutation;
    mutation.delta.cell_index = cell_index;
    mutation.delta.grid_offset = grid_offset;
    mutation.delta.removed_terrain = terrain_before & cell.removed_terrain;
    const uint32_t after_removal = terrain_before & ~mutation.delta.removed_terrain;
    mutation.delta.added_terrain = cell.added_terrain & ~after_removal;
    mutation.delta.bound_building = cell.binds_building;
    mutation.terrain_after = (after_removal | cell.added_terrain);
    return mutation;
}

uint32_t foundation_restore_terrain_cell(
    uint32_t terrain_after,
    const FoundationTerrainDelta &delta)
{
    return (terrain_after & ~delta.added_terrain) | delta.removed_terrain;
}

const std::vector<FoundationTerrainDelta> &FoundationState::terrain_deltas() const
{
    return terrain_deltas_;
}

int FoundationState::is_published() const { return published_; }
int FoundationState::origin_x() const { return origin_x_; }
int FoundationState::origin_y() const { return origin_y_; }
int FoundationState::rotation() const { return rotation_; }
RoadblockState &FoundationState::roadblock() { return roadblock_; }
const RoadblockState &FoundationState::roadblock() const { return roadblock_; }

void FoundationState::clear()
{
    published_ = 0;
    origin_x_ = 0;
    origin_y_ = 0;
    rotation_ = 0;
    terrain_deltas_.clear();
}

void FoundationState::begin_publication(int origin_x, int origin_y, int rotation)
{
    clear();
    published_ = 1;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    rotation_ = (rotation % 4 + 4) % 4;
}

void FoundationState::record_delta(FoundationTerrainDelta delta)
{
    terrain_deltas_.push_back(delta);
}

} // namespace building_type_registry_impl
