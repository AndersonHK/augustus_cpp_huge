#pragma once

#include "building/building.h"

// Square legacy terrain art such as rocks and editor-authored map features.
void map_terrain_tiles_add(int x, int y, int size, int image_id, int terrain);

// Compatibility cleanup for ownerless map records decoded before runtime
// Building/Foundation materialization. Live buildings remove their Foundation.
void map_legacy_building_tiles_remove(int x, int y);

void map_building_tiles_add_rubble(Building &building, int x, int y, int image_id);

void map_building_tiles_add_bridge(Building &building, int x, int y);

void map_building_tiles_mark_deleting(int grid_offset);

int map_building_tiles_mark_construction(int x, int y, int size, int terrain, int absolute_xy);
