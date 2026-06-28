#pragma once

#include "building/building.h"
#include "building/building_type.h"
#include "core/buffer.h"
#include "game/save_version.h"

/**
 * Returns whether the map has a runtime building at the given offset.
 * @param grid_offset Map offset
 */
int map_building_exists_at(int grid_offset);

/**
 * Returns the runtime building at the given offset.
 * @param grid_offset Map offset
 * @return Building object bound to runtime state; caller must check map_building_exists_at first
 */
Building &map_building_at(int grid_offset);

/**
 * Returns the building type at the given offset
 * @param grid_offset Map offset
 * @return Building type of building at offset, BUILDING_NONE means no building or unknown building type
 */
building_type map_building_type_at(int grid_offset);

unsigned int map_building_from_buffer_16(buffer *buildings, int grid_offset);
unsigned int map_building_from_buffer_32(buffer *buildings, int grid_offset);

void map_building_set(int grid_offset, Building &building);
void map_building_clear_at(int grid_offset);

/**
 * Increases building damage by 1
 * @param grid_offset Map offset
 * @return New damage amount
 */
int map_building_damage_increase(int grid_offset);

void map_building_damage_clear(int grid_offset);

unsigned int map_building_rubble_building_id(int grid_offset);

void map_building_set_rubble_grid_building_id(int grid_offset, unsigned int building_id, int size);

int map_building_ruins_left(const Building &building);

void map_building_backup(void);

/**
 * Clears the maps related to buildings
 */
void map_building_clear(void);

void map_building_restore(void);

void map_building_clear_backup(void);

void map_building_save_state(buffer *buildings, buffer *damage, buffer *rubble);

void map_building_load_state(buffer *buildings, buffer *damage, buffer *rubble, savegame_version_t version);

unsigned int map_building_loaded_id_at(int grid_offset);
void map_building_set_loaded_id(int grid_offset, unsigned int building_id);

void map_building_rebind_runtime_references(void);
void map_building_remove_invalid_references(void);

int map_building_is_reservoir(int x, int y);

