#pragma once
#include "map/TerrainSet.h"

#include <array>
#include <optional>
#include <stdint.h>
#include <vector>

namespace building_type_registry_impl {

class FoundationDef;
class FoundationState;
struct FoundationTerrainDelta;

constexpr int FOUNDATION_SAVE_CELL_LIMIT = 64;
constexpr int FOUNDATION_SAVE_TERRAIN_BYTES = 1 + FOUNDATION_SAVE_CELL_LIMIT * 2 * sizeof(uint32_t);

// The wire payload contains a publication byte and archive set references per
// cell. Legacy versions decode those slots as masks. Geometry and definition
// values are never stored here.
struct FoundationTerrainSaveState {
    // Loader-only reconstruction bypasses wire IDs; never written to disk.
    std::optional<std::array<TerrainSet, FOUNDATION_SAVE_CELL_LIMIT>> recovered_added;
    uint8_t published = 0;
    std::array<uint32_t, FOUNDATION_SAVE_CELL_LIMIT> added = {};
    std::array<uint32_t, FOUNDATION_SAVE_CELL_LIMIT> removed = {};
};

FoundationTerrainSaveState foundation_terrain_state_for_save(
    const FoundationDef &definition,
    const FoundationState &state);

int foundation_terrain_deltas_from_save(
    const FoundationDef &definition,
    const FoundationTerrainSaveState &saved,
    std::vector<FoundationTerrainDelta> *deltas);

// Legacy waterside placement numbered the two quarter-turn orientations in
// the opposite direction from FoundationDef's canonical rotation transform.
// Keep that historical convention inside the load bridge rather than leaking
// it into runtime geometry or authored foundation data.
int foundation_rotation_from_save(
    const FoundationDef &definition,
    int saved_rotation,
    int save_version,
    int last_legacy_foundation_version);

} // namespace building_type_registry_impl
