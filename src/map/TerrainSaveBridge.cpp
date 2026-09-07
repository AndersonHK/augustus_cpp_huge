#include "building/building_record.h"
#include "map/TerrainMap.h"
#include <array>
#include <set>

#include "building/building.h"
#include "building/BuildingGeometry.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/water_access_runtime.h"
#include "city/map.h"
#include "core/direction.h"
#include "core/image.h"
#include "core/log.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/data.h"
#include <map>
#include "map/property.h"
#include "map/ring.h"
#include "map/sprite.h"
#include "map/water_navigation.h"

#include "map/TerrainSaveBridge.h"
#include "map/TerrainRegistry.h"
#include <sstream>
#include <stdexcept>

static void determine_original_trees(buffer *images, int legacy_buffer)
{
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (terrain_map().contains(x + GRID_SIZE * y, terrain_types().tree) &&
                !(terrain_map().contains(x + GRID_SIZE * y, terrain_types().water))) {
                terrain_map().add(x + GRID_SIZE * y, terrain_types().originally_tree);
                if (images) {
                    buffer_set(images, (x + GRID_SIZE * y) * (legacy_buffer ? 2 : 4));
                    int image_id = legacy_buffer ? buffer_read_u16(images) : buffer_read_u32(images);
                    int image_tree_group = image_group(GROUP_TERRAIN_TREE);
                    int ring;
                    if (image_id >= image_tree_group + 8 && image_id < image_tree_group + 16) {
                        ring = 1;
                    } else if (image_id >= image_tree_group + 16 && image_id < image_tree_group + 24) {
                        ring = 2;
                    } else if (image_id >= image_tree_group + 24 && image_id < image_tree_group + 32) {
                        ring = 3;
                    } else {
                        continue;
                    }
                    int start = map_ring_start(1, ring);
                    int end = map_ring_end(1, ring);
                    int base_offset = x + GRID_SIZE * y;
                    for (int i = start; i < end; i++) {
                        int current_offset = base_offset + map_ring_tile(i)->grid_offset;
                        if (map_grid_is_valid_offset(current_offset) &&
                            !terrain_map().contains_all(current_offset, terrain_types().map_edge)) {
                            terrain_map().add(current_offset, terrain_types().originally_tree);
                        }
                    }
                }
            }
        }
    }
}

static int old_save_bridge_tile(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) &&
        map_sprite_bridge_at(grid_offset) &&
        terrain_map().contains(grid_offset, terrain_types().water);
}

static int old_save_bridge_tile_has_segment_chain_record(int grid_offset)
{
    if (!old_save_bridge_tile(grid_offset) || !map_is_bridge(grid_offset) || !map_building_exists_at(grid_offset)) {
        return 0;
    }

    Building &building = map_building_at(grid_offset);
    const ::building *record = building.record();
    // Dynamic bridges are the sole live runtime owner of record-chain links.
    return building.type && building.type->bridge().is_bridge() && record &&
        (record->prev_part_building_id || record->next_part_building_id);
}

static int legacy_bridge_direction_from_axis(int axis, int dir)
{
    if (axis == 0) {
        return dir > 0 ? DIR_2_RIGHT : DIR_6_LEFT;
    }
    return dir > 0 ? DIR_4_BOTTOM : DIR_0_TOP;
}

static int materialize_legacy_bridge_span(
    int start,
    int delta,
    int axis,
    int dir,
    int is_ship_bridge)
{
    int length = 0;
    for (int current = start; old_save_bridge_tile(current); current += delta) {
        length++;
    }
    return map_bridge_create_native_chain(
        start,
        length,
        legacy_bridge_direction_from_axis(axis, dir),
        is_ship_bridge,
        1);
}

int map_bridge_find_start_and_direction_legacy(int grid_offset, int *axis, int *axis_direction)
{
    if (!old_save_bridge_tile(grid_offset)) {
        return -1;
    }

    static const int dirs[4][2] = {
        {  0, -1 }, // north
        { +1,  0 }, // east
        {  0, +1 }, // south
        { -1,  0 }  // west
    };
    // Scan in all 4 directions until we find a ramp
    for (int i = 0; i < 4; ++i) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        int delta = map_grid_delta(dx, dy);

        int current = grid_offset;
        while (old_save_bridge_tile(current)) {
            int sprite = map_sprite_bridge_at(current);
            if (map_bridge_is_ramp_sprite(sprite)) {
                int next = current + delta;
                int next_sprite = map_sprite_bridge_at(next);

                if (map_bridge_is_ramp_sprite(next_sprite)) {
                    // If both are ramps, check for low bridge
                    if (sprite <= 6 && next_sprite <= 6) { // ship bridge sprites > 6
                        *axis = (dx != 0) ? 0 : 1;
                        *axis_direction = (dx + dy);
                        return current;
                    }
                    break; // invalid for ship bridge
                }

                if (next_sprite >= 5 && next_sprite <= 15 && !map_bridge_is_ramp_sprite(next_sprite)) {
                    *axis = (dx != 0) ? 0 : 1;
                    *axis_direction = (dx + dy);
                    return current;
                }
            }

            current -= delta;
        }
    }

    return -1; // No valid bridge start found
}

void TerrainMap::migrate_old_bridges(void)
{
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_grid_is_valid_offset(grid_offset)) {
                continue;
            }
            if (old_save_bridge_tile(grid_offset) && !old_save_bridge_tile_has_segment_chain_record(grid_offset)) {
                // Find true start of the old bridge
                // Only process tiles that are part of a legacy bridge and haven't been upgraded yet 
                int axis, dir;
                int start = map_bridge_find_start_and_direction_legacy(grid_offset, &axis, &dir);
                if (start < 0) {
                    continue;
                }
                int delta = (axis == 0)
                    ? map_grid_delta(dir, 0)
                    : map_grid_delta(0, dir);

                int is_ship_bridge = map_sprite_bridge_at(start) > 6 ? 1 : 0;
                materialize_legacy_bridge_span(start, delta, axis, dir, is_ship_bridge);
            }
        }
    }
    water_navigation::invalidate_topology();
}

void TerrainMap::migrate_old_walls(void)
{
    building_type wall_type = building_type_registry_impl::type_from_attr("wall");
    if (wall_type == BUILDING_NONE) {
        return;
    }
    const building_type_registry_impl::BuildingType *wall_definition =
        building_type_registry_impl::definition_for_type(wall_type);
    if (!wall_definition) {
        return;
    }
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_grid_is_valid_offset(grid_offset)) {
                continue;
            }
            if (terrain_map().contains(grid_offset, terrain_types().wall)) {
                if (!terrain_map().contains(grid_offset, terrain_types().building)) {
                    // Create wall building for each wall tile
                    Building &wall = city_building_runtime().create(*wall_definition, x, y);
                    map_building_set(grid_offset, wall);
                    terrain_map().add(grid_offset, terrain_types().building);
                } else {
                    // Recreate the wall if pointing to a wrong building
                    if (!map_building_exists_at(grid_offset) || !map_building_at(grid_offset).matches("wall")) {
                        Building &wall = city_building_runtime().create(*wall_definition, x, y);
                        map_building_set(grid_offset, wall);
                    }
                }
                map_property_clear_multi_tile_xy(grid_offset);
            }
        }
    }
}

int TerrainMap::validate_loaded_walls(void)
{
    for (int grid_offset = 0; grid_offset < GRID_SIZE * GRID_SIZE; ++grid_offset) {
        if (!map_grid_is_valid_offset(grid_offset) || !terrain_map().contains(grid_offset, terrain_types().wall)) {
            continue;
        }
        if (!map_building_exists_at(grid_offset)) {
            log_error("Current save wall terrain has no building record", 0, grid_offset);
            return 0;
        }
        Building &wall = map_building_at(grid_offset);
        const building *record = wall.record();
        if (!record || !record->id || !wall.matches("wall") || record->grid_offset != grid_offset || record->x != map_grid_offset_to_x(grid_offset) || record->y != map_grid_offset_to_y(grid_offset)) {
            log_error("Current save wall terrain does not exactly match its wall building record", 0, grid_offset);
            return 0;
        }
    }
    int valid = 1;
    Building::for_each([&valid](Building *candidate) {
        if (!valid || !candidate || !candidate->matches("wall")) {
            return;
        }
        const building *record = candidate->record();
        if (!record || !map_grid_is_valid_offset(record->grid_offset) || !terrain_map().contains(record->grid_offset, terrain_types().wall) || !map_building_exists_at(record->grid_offset) || map_building_at(record->grid_offset).record() != record) {
            log_error("Current save contains an orphaned wall building record", 0, record ? record->id : 0);
            valid = 0;
        }
    });
    return valid;
}


void TerrainMap::save_state(buffer *destination)
{
    for (int offset = 0; offset < GRID_SIZE * GRID_SIZE; ++offset) buffer_write_u32(destination, terrain_save::encode(at(offset)));
}

void TerrainMap::save_state_legacy(buffer *destination)
{
    for (int offset = 0; offset < GRID_SIZE * GRID_SIZE; ++offset) {
        const uint32_t value = terrain_save::encode_legacy(at(offset));
        if (value > UINT16_MAX) throw std::runtime_error("Terrain cannot be represented by the original scenario format");
        buffer_write_u16(destination, static_cast<uint16_t>(value));
    }
}

void TerrainMap::load_state(buffer *source, int wide, buffer *images, int legacy_image_buffer)
{
    clear();
    for (int offset = 0; offset < GRID_SIZE * GRID_SIZE; ++offset) set(offset, terrain_save::read_at(source, offset, wide != 0));
    if (images) determine_original_trees(images, legacy_image_buffer);
    water_navigation::invalidate_topology();
}
