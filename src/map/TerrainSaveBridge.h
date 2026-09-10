#pragma once

#include "map/TerrainSet.h"
#include "core/buffer.h"
#include <cstdint>

// Archive IDs exist only in this bridge. Runtime cells and rollback records
// contain references, and new archives never encode terrain membership masks.
namespace terrain_save {
void reset();
void prepare();
void write_ledger(buffer *destination);
bool load_ledger(buffer *source, bool has_ledger);
uint32_t encode(const TerrainSet &terrain);
TerrainSet decode(uint32_t archive_value);
TerrainSet decode_legacy(uint32_t mask);
uint32_t encode_legacy(const TerrainSet &terrain);
TerrainSet read_at(buffer *source, int offset, bool wide);
}
