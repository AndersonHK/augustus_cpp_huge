#pragma once

#include "building/building.h"

void map_building_tiles_add_remove(
    Building &building,
    int x,
    int y,
    int size,
    int image_id,
    int terrain_to_add,
    int terrain_to_remove);

void map_building_tiles_add(Building &building, int x, int y, int size, int image_id, int terrain);

void map_terrain_tiles_add(int x, int y, int size, int image_id, int terrain);

int map_building_tiles_add_aqueduct(int x, int y);

void map_building_tiles_remove(const Building *building, int x, int y);

void map_building_tiles_set_rubble(const Building *building, int x, int y, int size);

void map_building_tiles_add_rubble(Building &building, int x, int y, int image_id);

void map_building_tiles_add_bridge(Building &building, int x, int y);

void map_building_tiles_mark_deleting(int grid_offset);

int map_building_tiles_mark_construction(int x, int y, int size, int terrain, int absolute_xy);

int map_building_tiles_are_clear(int x, int y, int size, int terrain);
