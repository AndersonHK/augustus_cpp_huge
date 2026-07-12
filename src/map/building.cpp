#include "building/building_record.h"
#include "building.h"

#include "building/building.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "core/config.h"
#include "core/crash_context.h"
#include "core/log.h"
#include "game/save_version.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "platform/screen.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <vector>

static grid_u32 buildings_grid;
static grid_u8 damage_grid;
static grid_u32 rubble_info_grid;

static grid_u32 buildings_grid_backup;
static grid_u8 damage_grid_backup;
static grid_u32 rubble_info_grid_backup;

static Building *building_objects_grid[GRID_SIZE * GRID_SIZE];
static Building *building_objects_grid_backup[GRID_SIZE * GRID_SIZE];

[[noreturn]] static void report_missing_runtime_building(int grid_offset)
{
    log_error("map_building_at called without a runtime building", 0, grid_offset);
    std::terminate();
}

int map_building_exists_at(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) && building_objects_grid[grid_offset];
}

Building &map_building_at(int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        return *building_objects_grid[grid_offset];
    }
    report_missing_runtime_building(grid_offset);
}

building_type map_building_type_at(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return BUILDING_NONE;
    }
    const Building &building = map_building_at(grid_offset);
    return building.type ? building.type->type() : BUILDING_NONE;
}

unsigned int map_building_from_buffer_16(buffer *buildings, int grid_offset)
{
    buffer_set(buildings, grid_offset * sizeof(uint16_t));
    return buffer_read_u16(buildings);
}

unsigned int map_building_from_buffer_32(buffer *buildings, int grid_offset)
{
    buffer_set(buildings, grid_offset * sizeof(uint32_t));
    return buffer_read_u32(buildings);
}

void map_building_set(int grid_offset, Building &building)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    building_runtime *runtime = building.runtime_instance();
    if (!runtime) {
        report_missing_runtime_building(grid_offset);
    }
    building_objects_grid[grid_offset] = &runtime->building;
    buildings_grid.items[grid_offset] = runtime->building.id;
}

void map_building_clear_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    building_objects_grid[grid_offset] = nullptr;
    buildings_grid.items[grid_offset] = 0;
}

void map_building_damage_clear(int grid_offset)
{
    damage_grid.items[grid_offset] = 0;
}

int map_building_damage_increase(int grid_offset)
{
    return ++damage_grid.items[grid_offset];
}

unsigned int map_building_rubble_building_id(int grid_offset)
{
    return rubble_info_grid.items[grid_offset];
}

void map_building_set_rubble_grid_building_id(int grid_offset, unsigned int building_id, int size)
{
    if (size == 1) {
        if (!building_id || !map_terrain_is(grid_offset, TERRAIN_WATER)) {
            rubble_info_grid.items[grid_offset] = building_id;
        }
        return;
    }
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int offset = map_grid_offset(x + i, y + j);
            if (!building_id || !map_terrain_is(offset, TERRAIN_WATER)) {
                rubble_info_grid.items[offset] = building_id;
            }
        }
    }
}

void map_building_backup(void)
{
    map_grid_copy_u32(buildings_grid.items, buildings_grid_backup.items);
    map_grid_copy_u8(damage_grid.items, damage_grid_backup.items);
    map_grid_copy_u32(rubble_info_grid.items, rubble_info_grid_backup.items);
    std::memcpy(building_objects_grid_backup, building_objects_grid, sizeof(building_objects_grid));
}

void map_building_restore(void)
{
    map_grid_copy_u32(buildings_grid_backup.items, buildings_grid.items);
    map_grid_copy_u8(damage_grid_backup.items, damage_grid.items);
    map_grid_copy_u32(rubble_info_grid_backup.items, rubble_info_grid.items);
    std::memcpy(building_objects_grid, building_objects_grid_backup, sizeof(building_objects_grid));
}

void map_building_clear_backup(void)
{
    map_grid_clear_u32(buildings_grid_backup.items);
    map_grid_clear_u8(damage_grid_backup.items);
    map_grid_clear_u32(rubble_info_grid_backup.items);
    std::memset(building_objects_grid_backup, 0, sizeof(building_objects_grid_backup));
}

void map_building_clear(void)
{
    map_grid_clear_u32(buildings_grid.items);
    map_grid_clear_u8(damage_grid.items);
    map_grid_clear_u32(rubble_info_grid.items);
    std::memset(building_objects_grid, 0, sizeof(building_objects_grid));
}

void map_building_save_state(buffer *buildings, buffer *damage, buffer *rubble)
{
    map_grid_save_state_u32(buildings_grid.items, buildings);
    map_grid_save_state_u8(damage_grid.items, damage);
    map_grid_save_state_u32(rubble_info_grid.items, rubble);
}

void map_building_load_state(buffer *buildings, buffer *damage, buffer *rubble, savegame_version_t version)
{
    if (version <= SAVE_GAME_LAST_U16_GRIDS) {
        map_grid_load_state_u16_to_u32(buildings_grid.items, buildings);
        map_grid_load_state_u8(damage_grid.items, damage);
    } else {
        map_grid_load_state_u32(buildings_grid.items, buildings);
        map_grid_load_state_u8(damage_grid.items, damage);
        map_grid_load_state_u32(rubble_info_grid.items, rubble);
    }
    std::memset(building_objects_grid, 0, sizeof(building_objects_grid));
}

unsigned int map_building_loaded_id_at(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? buildings_grid.items[grid_offset] : 0;
}

void map_building_set_loaded_id(int grid_offset, unsigned int building_id)
{
    if (map_grid_is_valid_offset(grid_offset)) {
        buildings_grid.items[grid_offset] = building_id;
    }
}

static int map_building_reference_is_live(Building *building)
{
    if (!building) {
        return 0;
    }
    const ::building *record = building->record();
    return record && record->state != BUILDING_STATE_UNUSED && building->type;
}

void map_building_rebind_runtime_references(void)
{
    std::memset(building_objects_grid, 0, sizeof(building_objects_grid));

    std::vector<building *> records_by_id(static_cast<size_t>(building_count()), nullptr);
    building_for_each_loaded_record([&](building *record) {
        if (record && record->id < records_by_id.size()) {
            records_by_id[record->id] = record;
        }
    });

    for (int grid_offset = 0; grid_offset < GRID_SIZE * GRID_SIZE; grid_offset++) {
        const unsigned int building_id = buildings_grid.items[grid_offset];
        if (!building_id || building_id >= records_by_id.size()) {
            continue;
        }

        building *record = records_by_id[building_id];
        if (!record) {
            continue;
        }

        if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(record)) {
            building_objects_grid[grid_offset] = &runtime->building;
        }
    }
}

static void clear_single_invalid_building_reference(int grid_offset)
{
    map_building_clear_at(grid_offset);
    map_building_damage_clear(grid_offset);
    map_sprite_clear_tile(grid_offset);
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        map_terrain_remove(grid_offset, TERRAIN_BUILDING);
    }
}

static void report_malformed_terrain_building_tiles_after_load(int count, int first_grid_offset)
{
    if (count <= 0) {
        return;
    }

    char detail[256];
    snprintf(detail, sizeof(detail),
        "tiles=%d first_grid_offset=%d first_x=%d first_y=%d action=removed TERRAIN_BUILDING",
        count,
        first_grid_offset,
        map_grid_offset_to_x(first_grid_offset),
        map_grid_offset_to_y(first_grid_offset));

    ErrorContextScope scope("Save load building map normalization", detail);
    error_context_report_error(
        "Save load found TERRAIN_BUILDING tiles without live building records.",
        detail);

    char message[512];
    snprintf(message, sizeof(message),
        "This save contained %d tile%s marked as a building without a valid building record.\n\n"
        "Vespasian repaired the map by removing the invalid building terrain flag and continued loading.\n\n"
        "%s\n\nMore details were written to augustus-log.txt.",
        count,
        count == 1 ? "" : "s",
        detail);
    platform_screen_show_error_message_box("Vespasian Save Load Error", message);
}

void map_building_remove_invalid_references(void)
{
    map_building_rebind_runtime_references();

    int removed = 0;
    int malformed_terrain_building_tiles = 0;
    int first_malformed_terrain_building_offset = 0;
    for (int grid_offset = 0; grid_offset < GRID_SIZE * GRID_SIZE; grid_offset++) {
        Building *building = building_objects_grid[grid_offset];
        if (map_building_reference_is_live(building)) {
            continue;
        }
        const int has_terrain_building = map_terrain_is(grid_offset, TERRAIN_BUILDING);
        if (!building) {
            if (has_terrain_building) {
                if (!malformed_terrain_building_tiles) {
                    first_malformed_terrain_building_offset = grid_offset;
                }
                malformed_terrain_building_tiles++;
                clear_single_invalid_building_reference(grid_offset);
                removed++;
            } else if (buildings_grid.items[grid_offset]) {
                clear_single_invalid_building_reference(grid_offset);
                removed++;
            }
            continue;
        }
        if (has_terrain_building) {
            if (!malformed_terrain_building_tiles) {
                first_malformed_terrain_building_offset = grid_offset;
            }
            malformed_terrain_building_tiles++;
        }
        clear_single_invalid_building_reference(grid_offset);
        removed++;
    }
    report_malformed_terrain_building_tiles_after_load(
        malformed_terrain_building_tiles,
        first_malformed_terrain_building_offset);
    if (removed) {
        log_warning("Removed invalid building references from map grid after save load", 0, removed);
    }
}

int map_building_is_reservoir(int x, int y)
{
    if (!map_grid_is_inside(x, y, 3)) {
        return 0;
    }
    int grid_offset = map_grid_offset(x, y);
    if (!map_building_exists_at(grid_offset)) {
        return 0;
    }
    const Building &reservoir = map_building_at(grid_offset);
    const unsigned int building_id = reservoir.id;
    if (!reservoir.matches("reservoir")) {
        return 0;
    }
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            const int offset = grid_offset + map_grid_delta(dx, dy);
            if (!map_building_exists_at(offset) || map_building_at(offset).id != building_id) {
                return 0;
            }
        }
    }
    return 1;
}
