#include "building/FoundationStateSaveBridge.h"

#include "building/FoundationDef.h"
#include "building/FoundationState.h"
#include "map/terrain.h"

#include <algorithm>

namespace building_type_registry_impl {

namespace {

int normalize_rotation(int rotation)
{
    return (rotation % 4 + 4) % 4;
}

int uses_legacy_waterside_rotation(const FoundationDef &definition)
{
    if (!definition.rotates()) {
        return 0;
    }
    int has_water_cell = 0;
    int has_non_water_cell = 0;
    for (const FoundationCellDefinition &cell : definition.cells()) {
        if (cell.required_terrain & TERRAIN_WATER) {
            has_water_cell = 1;
        } else {
            has_non_water_cell = 1;
        }
    }
    return has_water_cell && has_non_water_cell;
}

} // namespace

FoundationTerrainSaveState foundation_terrain_state_for_save(
    const FoundationDef &definition,
    const FoundationState &state)
{
    FoundationTerrainSaveState saved;
    if (!state.is_published()) {
        return saved;
    }
    saved.published = 1;
    const int cell_count = std::min(
        static_cast<int>(definition.cells().size()), FOUNDATION_SAVE_CELL_LIMIT);
    for (const FoundationTerrainDelta &delta : state.terrain_deltas()) {
        if (delta.cell_index < 0 || delta.cell_index >= cell_count) {
            continue;
        }
        saved.added[delta.cell_index] = delta.added_terrain;
        saved.removed[delta.cell_index] = delta.removed_terrain;
    }
    return saved;
}

int foundation_terrain_deltas_from_save(
    const FoundationDef &definition,
    const FoundationTerrainSaveState &saved,
    std::vector<FoundationTerrainDelta> *deltas)
{
    if (!deltas || saved.published != 1 || definition.cells().empty() ||
        definition.cells().size() > FOUNDATION_SAVE_CELL_LIMIT) {
        return 0;
    }

    deltas->clear();
    deltas->reserve(definition.cells().size());
    for (int cell_index = 0; cell_index < static_cast<int>(definition.cells().size()); ++cell_index) {
        const FoundationCellDefinition &cell = definition.cells()[cell_index];
        if ((saved.added[cell_index] & ~cell.added_terrain) ||
            (saved.removed[cell_index] & ~cell.removed_terrain)) {
            deltas->clear();
            return 0;
        }
        FoundationTerrainDelta delta;
        delta.cell_index = cell_index;
        delta.added_terrain = saved.added[cell_index];
        delta.removed_terrain = saved.removed[cell_index];
        delta.bound_building = cell.binds_building;
        deltas->push_back(delta);
    }
    return 1;
}

int foundation_rotation_from_save(
    const FoundationDef &definition,
    int saved_rotation,
    int save_version,
    int last_legacy_foundation_version)
{
    const int rotation = normalize_rotation(saved_rotation);
    if (save_version <= last_legacy_foundation_version &&
        uses_legacy_waterside_rotation(definition)) {
        return (4 - rotation) % 4;
    }
    return rotation;
}

} // namespace building_type_registry_impl
