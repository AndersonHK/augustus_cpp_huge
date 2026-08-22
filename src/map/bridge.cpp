#include "building/building_record.h"
#include "bridge.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "city/view.h"
#include "core/config.h"
#include "core/direction.h"
#include "core/log.h"
#include "figure/figure.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "figure/route.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/water_navigation.h"
#include "map/tiles.h"

#define MAX_DISTANCE_BETWEEN_PILLARS 12
#define MINIMUM_DISTANCE_FOR_PILLARS 9

static struct {
    int end_grid_offset;
    int length;
    int direction;
    int direction_grid_delta;
} bridge;

static int direction_delta(int direction)
{
    switch (direction) {
        case DIR_0_TOP: return map_grid_delta(0, -1);
        case DIR_2_RIGHT: return map_grid_delta(1, 0);
        case DIR_4_BOTTOM: return map_grid_delta(0, 1);
        case DIR_6_LEFT: return map_grid_delta(-1, 0);
        default: return 0;
    }
}

static int direction_axis(int direction)
{
    return direction == DIR_2_RIGHT || direction == DIR_6_LEFT ? 0 : 1;
}

static int direction_axis_sign(int direction)
{
    return direction == DIR_2_RIGHT || direction == DIR_4_BOTTOM ? 1 : -1;
}

static int view_relative_direction(int direction)
{
    int result = direction - city_view_orientation();
    if (result < 0) {
        result += 8;
    }
    return result;
}

static int direction_from_axis(int axis, int axis_direction)
{
    if (axis == 0) {
        return axis_direction >= 0 ? DIR_2_RIGHT : DIR_6_LEFT;
    }
    return axis_direction >= 0 ? DIR_4_BOTTOM : DIR_0_TOP;
}

int map_bridge_building_length(void)
{
    return bridge.length;
}

void map_bridge_reset_building_length(void)
{
    bridge.length = 0;
}

int map_bridge_calculate_length_direction(int x, int y, int *length, int *direction, grid_slice *blocking_tiles)
{
    int grid_offset = map_grid_offset(x, y);
    bridge.end_grid_offset = 0;
    bridge.direction_grid_delta = 0;
    bridge.length = *length = 0;
    bridge.direction = *direction = 0;

    if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_BUILDING)) {
        return 0;
    }
    if (map_terrain_count_directly_adjacent_with_type(grid_offset, TERRAIN_WATER) != 3) {
        return 0;
    }
    if (!map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(0, 1);
        bridge.direction = DIR_4_BOTTOM;
    } else if (!map_terrain_is(grid_offset + map_grid_delta(1, 0), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(-1, 0);
        bridge.direction = DIR_6_LEFT;
    } else if (!map_terrain_is(grid_offset + map_grid_delta(0, 1), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(0, -1);
        bridge.direction = DIR_0_TOP;
    } else if (!map_terrain_is(grid_offset + map_grid_delta(-1, 0), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(1, 0);
        bridge.direction = DIR_2_RIGHT;
    } else {
        return 0;
    }
    *direction = bridge.direction;
    bridge.length = 1;
    for (int i = 0; i < 64; i++) { //longer bridges
        grid_offset += bridge.direction_grid_delta;
        bridge.length++;
        if (i == 0) {
            //check for an inaccessible tile before the bridge starts
            int previous_offset = grid_offset - 2 * bridge.direction_grid_delta;
            if (map_terrain_is(previous_offset, TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_SHRUB | TERRAIN_BUILDING)) {
                blocking_tiles->grid_offsets[blocking_tiles->size++] = previous_offset;
                bridge.end_grid_offset = 0;
            }
        }
        int next_offset = grid_offset + bridge.direction_grid_delta;
        if (map_terrain_is(next_offset, TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_SHRUB | TERRAIN_BUILDING)) {
            blocking_tiles->grid_offsets[blocking_tiles->size++] = next_offset;
            bridge.end_grid_offset = 0;
            break;
        }
        if (!map_terrain_is(next_offset, TERRAIN_WATER)) {
            bridge.end_grid_offset = grid_offset;
            if (map_terrain_count_directly_adjacent_with_type(grid_offset, TERRAIN_WATER) != 3) {
                blocking_tiles->grid_offsets[blocking_tiles->size++] = grid_offset;
                bridge.end_grid_offset = 0;
            }
            *length = bridge.length;
            return bridge.end_grid_offset;
        }
        if (map_is_bridge(grid_offset)) {
            blocking_tiles->grid_offsets[blocking_tiles->size++] = grid_offset;
            bridge.end_grid_offset = 0;
            break;
        }
        if (map_terrain_count_diagonally_adjacent_with_type(grid_offset, TERRAIN_WATER) != 4) {
            blocking_tiles->grid_offsets[blocking_tiles->size++] = grid_offset;
            bridge.end_grid_offset = 0;
            break;
        }
    }
    // invalid bridge
    *length = bridge.length;
    return 0;
}

static int get_number_of_bridge_sections(int length)
{
    if (length < MINIMUM_DISTANCE_FOR_PILLARS) {
        return 1;
    }
    // Number of pillars is equal to length/max_distance, rounded up
    int number_of_pillars = (length + MAX_DISTANCE_BETWEEN_PILLARS - 1) / MAX_DISTANCE_BETWEEN_PILLARS;
    return number_of_pillars + 1;
}

static int get_pillar_distance(int length)
{
    if (length < MINIMUM_DISTANCE_FOR_PILLARS) {
        return length;
    }

    int number_of_sections = get_number_of_bridge_sections(length);
    int pillar_distance = length / (number_of_sections);

    return pillar_distance;
}

static int get_pillar_shift(int length)
{
    if (length < MINIMUM_DISTANCE_FOR_PILLARS) {
        return 0;
    }

    int pillar_distance = get_pillar_distance(length);
    int pillar_shift = (length % pillar_distance) / 2;

    return pillar_shift;
}


static int bridge_legacy_sprite_for_piece(int index, int length, int direction, int is_ship_bridge)
{
    if (is_ship_bridge) {
        int pillar_distance = get_pillar_distance(length);
        int pillar_shift = get_pillar_shift(length);
        if (index == 1 || index == length - 2) {
            // platform after ramp
            return 13;
        } else if (index == 0) {
            // ramp at start
            switch (direction) {
                case DIR_0_TOP:
                    return 7;
                case DIR_2_RIGHT:
                    return 8;
                case DIR_4_BOTTOM:
                    return 9;
                case DIR_6_LEFT:
                    return 10;
            }
        } else if (index == length - 1) {
            // ramp at end
            switch (direction) {
                case DIR_0_TOP:
                    return 9;
                case DIR_2_RIGHT:
                    return 10;
                case DIR_4_BOTTOM:
                    return 7;
                case DIR_6_LEFT:
                    return 8;
            }
        } else if ((index - pillar_shift) % pillar_distance == 0) {
            if (direction == DIR_0_TOP || direction == DIR_4_BOTTOM) {
                return 14;
            } else {
                return 15;
            }
        } else {
            // middle of the bridge
            if (direction == DIR_0_TOP || direction == DIR_4_BOTTOM) {
                return 11;
            } else {
                return 12;
            }
        }
    } else {
        if (index == 0) {
            // ramp at start
            switch (direction) {
                case DIR_0_TOP:
                    return 1;
                case DIR_2_RIGHT:
                    return 2;
                case DIR_4_BOTTOM:
                    return 3;
                case DIR_6_LEFT:
                    return 4;
            }
        } else if (index == length - 1) {
            // ramp at end
            switch (direction) {
                case DIR_0_TOP:
                    return 3;
                case DIR_2_RIGHT:
                    return 4;
                case DIR_4_BOTTOM:
                    return 1;
                case DIR_6_LEFT:
                    return 2;
            }
        } else {
            // middle part
            if (direction == DIR_0_TOP || direction == DIR_4_BOTTOM) {
                return 5;
            } else {
                return 6;
            }
        }
    }
    return 0;
}

static int bridge_graphics_variant_for_legacy_sprite(int sprite)
{
    if (sprite >= 1 && sprite <= 6) {
        return sprite - 1;
    }
    if (sprite >= 7 && sprite <= 15) {
        return sprite - 7;
    }
    return 0;
}

static int bridge_legacy_sprite_for_variant(const Building &building)
{
    building_runtime *runtime = building.runtime_instance();
    const int variant = runtime ? runtime->graphics_variant() : building.Graphics().variant();
    if (building.type && building.type->bridge().is_ship_bridge()) {
        return 7 + variant;
    }
    return 1 + variant;
}

int map_bridge_graphics_variant_for_piece(int index, int length, int direction, int is_ship_bridge)
{
    return bridge_graphics_variant_for_legacy_sprite(
        bridge_legacy_sprite_for_piece(index, length, direction, is_ship_bridge));
}

int map_bridge_legacy_section_at(int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        Building &bridge_piece = map_building_at(grid_offset);
        if (bridge_piece.type && bridge_piece.type->bridge().is_bridge()) {
            return bridge_legacy_sprite_for_variant(bridge_piece);
        }
    }
    return map_sprite_bridge_at(grid_offset);
}

static const building_type_registry_impl::BuildingType *bridge_definition(int is_ship_bridge)
{
    const building_type bridge_type = building_type_registry_impl::type_from_bridge(is_ship_bridge ?
        building_type_registry_impl::BridgeType::Ship :
        building_type_registry_impl::BridgeType::Low);
    return bridge_type == BUILDING_NONE ? nullptr : building_type_registry_impl::definition_for_type(bridge_type);
}

static int building_is_bridge_type(const Building &building, const building_type_registry_impl::BuildingType &definition)
{
    return building.type == &definition ||
        (building.type && building.type->type() == definition.type());
}

static void set_bridge_piece_graphics_variant(building *record, int variant)
{
    if (!record) {
        return;
    }
    BuildingGraphicsState state;
    state.set_variant(variant);
    building_runtime_stage_loaded_graphics_state(record->id, state);
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(record)) {
        runtime->restore_graphics_state(state);
    }
}

static void normalize_bridge_chain_tiles(Building &main)
{
    for (Building *part = &main; part; part = part->dynamic_bridge_next()) {
        const building *record = part->record();
        if (!record) {
            break;
        }
        map_building_tiles_add_bridge(*part, record->x, record->y);
        if (!part->has_dynamic_bridge_next()) {
            break;
        }
    }
}

static int restore_staged_bridge_chain_graphics(Building &main)
{
    int restored_every_piece = 1;
    for (Building *part = &main; part; part = part->dynamic_bridge_next()) {
        const building *record = part->record();
        BuildingGraphicsState state;
        building_runtime *runtime = record ?
            building_runtime_impl::get_or_create_instance(const_cast<building *>(record)) : nullptr;
        if (!record || !runtime || !building_runtime_loaded_graphics_state(record->id, &state)) {
            restored_every_piece = 0;
        } else {
            runtime->restore_graphics_state(state);
        }
        if (!record || !part->has_dynamic_bridge_next()) {
            break;
        }
    }
    return restored_every_piece;
}

static Building *reuse_loaded_bridge_main(
    int start_grid_offset,
    const building_type_registry_impl::BuildingType &definition)
{
    if (!map_building_exists_at(start_grid_offset)) {
        return nullptr;
    }
    Building &existing = map_building_at(start_grid_offset).dynamic_bridge_owner();
    return building_is_bridge_type(existing, definition) ? &existing : nullptr;
}

int map_bridge_create_native_chain(int start_grid_offset, int length, int direction, int is_ship_bridge, int loaded_migration)
{
    if (length <= 0) {
        return 0;
    }

    const int delta = direction_delta(direction);
    if (!delta) {
        return 0;
    }

    const building_type_registry_impl::BuildingType *definition = bridge_definition(is_ship_bridge);
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    if (!foundation || foundation->width() != 1 || foundation->height() != 1 ||
        foundation->cells().size() != 1) {
        return 0;
    }

    building *previous = nullptr;
    Building *first_piece = loaded_migration ? reuse_loaded_bridge_main(start_grid_offset, *definition) : nullptr;
    const int graphics_direction = view_relative_direction(direction);
    int current = start_grid_offset;

    for (int i = 0; i < length; i++, current += delta) {
        const int x = map_grid_offset_to_x(current);
        const int y = map_grid_offset_to_y(current);
        Building *piece = i == 0 ? first_piece : nullptr;
        if (!piece) {
            piece = &city_building_runtime().create(*definition, x, y);
            if (!loaded_migration) {
                game_undo_add_building(const_cast<building *>(piece->record()));
            }
        }

        building *record = const_cast<building *>(piece->record());
        if (!record) {
            continue;
        }

        record->type = definition->type();
        record->state = BUILDING_STATE_IN_USE;
        record->x = static_cast<unsigned char>(x);
        record->y = static_cast<unsigned char>(y);
        record->grid_offset = static_cast<short>(current);
        record->subtype.orientation = static_cast<short>(direction);
        // Dynamic bridge segments deliberately retain a record-owned chain;
        // fixed XML composition never publishes through these fields.
        record->prev_part_building_id = previous ? static_cast<short>(previous->id) : 0;
        record->next_part_building_id = 0;
        if (previous) {
            previous->next_part_building_id = static_cast<short>(record->id);
        }

        const int legacy_sprite = loaded_migration ? map_sprite_bridge_at(current) : 0;
        const int variant = legacy_sprite ?
            bridge_graphics_variant_for_legacy_sprite(legacy_sprite) :
            map_bridge_graphics_variant_for_piece(i, length, graphics_direction, is_ship_bridge);
        set_bridge_piece_graphics_variant(record, variant);
        map_building_tiles_add_bridge(*piece, x, y);
        previous = record;
    }

    return length;
}

int map_bridge_add(int x, int y, int is_ship_bridge)
{
    int min_length = is_ship_bridge ? 5 : 2;
    if (bridge.end_grid_offset <= 0 || bridge.length < min_length) {
        bridge.length = 0;
        return bridge.length;
    }

    const int grid_offset = map_grid_offset(x, y);
    if (!map_bridge_create_native_chain(grid_offset, bridge.length, bridge.direction, is_ship_bridge, 0)) {
        bridge.length = 0;
        return bridge.length;
    }

    Route::updateLandTerrain();
    water_navigation::invalidate_topology();
    map_tiles_update_region_water(
        x,
        y,
        map_grid_offset_to_x(bridge.end_grid_offset),
        map_grid_offset_to_y(bridge.end_grid_offset));
    return bridge.length;
}

int map_is_bridge(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_WATER) && map_terrain_is(grid_offset, TERRAIN_ROAD) && map_terrain_is(grid_offset, TERRAIN_BUILDING);
}

int map_bridge_is_ramp_sprite(int sprite)
{
    return (sprite >= 1 && sprite <= 4) || (sprite >= 7 && sprite <= 10);
}

int map_bridge_find_start_and_direction(int grid_offset, int *axis, int *axis_direction)
{
    if (!map_is_bridge(grid_offset)) {
        return -1;
    }

    if (!map_building_exists_at(grid_offset)) {
        return -1;
    }
    Building &building = map_building_at(grid_offset);
    Building &main = building.dynamic_bridge_owner();
    const ::building *main_record = main.record();
    if (!main_record) {
        return -1;
    }

    if (main.type && main.type->bridge().is_bridge() && main_record->next_part_building_id > 0) {
        const int direction = main_record->subtype.orientation;
        *axis = direction_axis(direction);
        *axis_direction = direction_axis_sign(direction);
        return main.grid_offset();
    }

    ::building *b = const_cast<::building *>(building.record());
    if (!b) {
        return -1;
    }

    int start = map_grid_offset(b->x, b->y);
    unsigned int building_id = b->id;

    static const int dirs[4][2] = {
        {  0, -1 }, // north
        { +1,  0 }, // east
        {  0, +1 }, // south
        { -1,  0 }  // west
    };

    for (int i = 0; i < 4; i++) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        int neighbor = start + map_grid_delta(dx, dy);
        if (map_building_exists_at(neighbor) && map_building_at(neighbor).id == building_id) {
            *axis = (dx != 0) ? 0 : 1;
            *axis_direction = (dx + dy > 0) ? +1 : -1;
            return start;
        }
    }

    // If no direction found, default to +1 vertical (shouldn't happen)
    *axis = 1;
    *axis_direction = 1;
    return start;
}

void map_bridge_remove(int grid_offset, int mark_deleted)
{
    if (!map_is_bridge(grid_offset)) {
        return;
    }

    if (!map_building_exists_at(grid_offset)) {
        return;
    }

    Building &main = map_building_at(grid_offset).dynamic_bridge_owner();
    const int start = main.grid_offset();
    int bridge_x_start = map_grid_offset_to_x(start);
    int bridge_y_start = map_grid_offset_to_y(start);
    int bridge_x_end = bridge_x_start;
    int bridge_y_end = bridge_y_start;

    for (Building *piece = &main; piece;) {
        Building *next_piece = piece->dynamic_bridge_next();
        const int had_next = piece->has_dynamic_bridge_next();
        building *record = const_cast<building *>(piece->record());
        if (!record) {
            break;
        }
        const int current = record->grid_offset;
        if (mark_deleted) {
            map_property_mark_deleted(current);
        } else {
            if (config_get(CONFIG_GP_CH_ALWAYS_DESTROY_BRIDGES)) {
                map_kill_figures_category_at(current, FIGURE_CATEGORY_ALL ^ FIGURE_CATEGORY_INACTIVE);
            }
            game_undo_add_building(record);
            record->state = BUILDING_STATE_DELETED_BY_PLAYER;
            record->is_deleted = 1;
            record->prev_part_building_id = 0;
            record->next_part_building_id = 0;
            map_sprite_clear_tile(current);
            map_terrain_remove(current, TERRAIN_ROAD);
            map_terrain_remove(current, TERRAIN_BUILDING);
            map_building_clear_at(current);
        }
        bridge_x_end = map_grid_offset_to_x(current);
        bridge_y_end = map_grid_offset_to_y(current);
        if (!had_next) {
            break;
        }
        piece = next_piece;
    }

    if (!mark_deleted) {
        water_navigation::invalidate_topology();
        game_undo_disable();
        map_tiles_update_region_water(bridge_x_start, bridge_y_start, bridge_x_end, bridge_y_end);
        map_tiles_update_region_empty_land(bridge_x_start, bridge_y_start, bridge_x_end, bridge_y_end);
    }
}


int map_bridge_has_figures(int grid_offset)
{
    if (!map_is_bridge(grid_offset)) {
        return 0;
    }

    if (!map_building_exists_at(grid_offset)) {
        return 0;
    }

    Building &main = map_building_at(grid_offset).dynamic_bridge_owner();
    for (Building *piece = &main; piece; piece = piece->dynamic_bridge_next()) {
        const building *record = piece->record();
        if (!record) {
            break;
        }
        if (map_has_figure_category_at(record->grid_offset, FIGURE_CATEGORY_ALL ^ FIGURE_CATEGORY_INACTIVE)) {
            return 1;
        }
        if (!piece->has_dynamic_bridge_next()) {
            break;
        }
    }
    return 0;
}

void map_bridge_update_after_rotate(int)
{
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (!map_is_bridge(grid_offset) || !map_building_exists_at(grid_offset)) {
                continue;
            }
            Building &piece = map_building_at(grid_offset);
            if (!piece.is_dynamic_bridge_owner()) {
                continue;
            }
            const building *record = piece.record();
            if (!record) {
                continue;
            }

            int length = 0;
            for (Building *part = &piece; part; part = part->dynamic_bridge_next()) {
                length++;
                if (!part->has_dynamic_bridge_next()) {
                    break;
                }
            }
            const int graphics_direction = view_relative_direction(record->subtype.orientation);
            int index = 0;
            for (Building *part = &piece; part; part = part->dynamic_bridge_next(), index++) {
                building *part_record = const_cast<building *>(part->record());
                if (!part_record) {
                    break;
                }
                set_bridge_piece_graphics_variant(
                    part_record,
                    map_bridge_graphics_variant_for_piece(
                        index,
                        length,
                        graphics_direction,
                        piece.type->bridge().is_ship_bridge()));
                if (!part->has_dynamic_bridge_next()) {
                    break;
                }
            }
        }
    }
}

int map_bridge_height(int grid_offset)
{
    int sprite = map_bridge_legacy_section_at(grid_offset);
    if (sprite <= 6) {
        // low bridge
        switch (sprite) {
            case 1:
            case 4:
                return 10;
            case 2:
            case 3:
                return 16;
            default:
                return 20;
        }
    } else {
        // ship bridge
        switch (sprite) {
            case 7:
            case 8:
            case 9:
            case 10:
                return 14;
            case 13:
                return 30;
            default:
                return 36;
        }
    }
}

static int bridge_chain_length_from_start(int start, int delta)
{
    int length = 0;
    for (int current = start; map_grid_is_valid_offset(current) && map_is_bridge(current); current += delta) {
        length++;
        if (length >= 64) {
            break;
        }
    }
    return length;
}

static void migrate_loaded_bridge_at(int grid_offset)
{
    int axis = 0;
    int axis_direction = 0;
    int start = map_bridge_find_start_and_direction_legacy(grid_offset, &axis, &axis_direction);
    if (start < 0 && map_building_exists_at(grid_offset)) {
        start = map_bridge_find_start_and_direction(grid_offset, &axis, &axis_direction);
    }
    if (start < 0) {
        return;
    }

    const int direction = direction_from_axis(axis, axis_direction);
    const int delta = direction_delta(direction);
    const int length = bridge_chain_length_from_start(start, delta);
    if (length <= 0) {
        return;
    }

    int is_ship_bridge = map_sprite_bridge_at(start) > 6 ? 1 : 0;
    if (!is_ship_bridge && map_building_exists_at(start)) {
        Building &building = map_building_at(start).dynamic_bridge_owner();
        is_ship_bridge = building.type && building.type->bridge().is_ship_bridge();
        if (building.has_dynamic_bridge_next()) {
            if (restore_staged_bridge_chain_graphics(building)) {
                normalize_bridge_chain_tiles(building);
                return;
            }
            const int graphics_direction = view_relative_direction(direction);
            int index = 0;
            for (Building *part = &building; part; part = part->dynamic_bridge_next(), index++) {
                set_bridge_piece_graphics_variant(
                    const_cast<::building *>(part->record()),
                    map_bridge_graphics_variant_for_piece(index, length, graphics_direction, is_ship_bridge));
                if (!part->has_dynamic_bridge_next()) {
                    break;
                }
            }
            normalize_bridge_chain_tiles(building);
            return;
        }
    }

    map_bridge_create_native_chain(start, length, direction, is_ship_bridge, 1);
}

void map_bridge_migrate_loaded_native_bridges(void)
{
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            const int grid_offset = map_grid_offset(x, y);
            if (!map_grid_is_valid_offset(grid_offset) || !map_is_bridge(grid_offset)) {
                continue;
            }
            if (map_building_exists_at(grid_offset)) {
                Building &piece = map_building_at(grid_offset);
                const building *record = piece.record();
                if (record && record->prev_part_building_id != 0) {
                    continue;
                }
            }
            migrate_loaded_bridge_at(grid_offset);
        }
    }
}

static int report_loaded_bridge_validation_failure(const char *reason, int grid_offset, unsigned int building_id)
{
    char detail[192];
    snprintf(detail, sizeof(detail), "grid_offset=%d building_id=%u reason=%s", grid_offset, building_id, reason ? reason : "<none>");
    log_error("Current save contains an invalid native bridge chain", detail, 0);
    return 0;
}

static int validate_loaded_bridge_piece_at(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return report_loaded_bridge_validation_failure("bridge terrain has no building record", grid_offset, 0);
    }
    Building &piece = map_building_at(grid_offset);
    const building *record = piece.record();
    if (!record || !record->id || !piece.type || !piece.type->bridge().is_bridge() || record->grid_offset != grid_offset || record->x != map_grid_offset_to_x(grid_offset) || record->y != map_grid_offset_to_y(grid_offset)) {
        return report_loaded_bridge_validation_failure("bridge record identity, type, or coordinates do not match its map cell", grid_offset, record ? record->id : 0);
    }
    const int delta = direction_delta(record->subtype.orientation);
    if (!delta) {
        return report_loaded_bridge_validation_failure("bridge record has an invalid chain orientation", grid_offset, record->id);
    }
    if (record->prev_part_building_id > 0) {
        const int previous_offset = grid_offset - delta;
        if (!map_grid_is_valid_offset(previous_offset) || !map_building_exists_at(previous_offset)) {
            return report_loaded_bridge_validation_failure("bridge predecessor map cell is missing", grid_offset, record->id);
        }
        const building *previous = map_building_at(previous_offset).record();
        if (!previous || previous->id != static_cast<unsigned int>(record->prev_part_building_id) || previous->next_part_building_id != static_cast<short>(record->id) || previous->type != record->type || previous->subtype.orientation != record->subtype.orientation) {
            return report_loaded_bridge_validation_failure("bridge predecessor link is not reciprocal and type-stable", grid_offset, record->id);
        }
    }
    if (record->next_part_building_id > 0) {
        const int next_offset = grid_offset + delta;
        if (!map_grid_is_valid_offset(next_offset) || !map_building_exists_at(next_offset)) {
            return report_loaded_bridge_validation_failure("bridge successor map cell is missing", grid_offset, record->id);
        }
        const building *next = map_building_at(next_offset).record();
        if (!next || next->id != static_cast<unsigned int>(record->next_part_building_id) || next->prev_part_building_id != static_cast<short>(record->id) || next->type != record->type || next->subtype.orientation != record->subtype.orientation) {
            return report_loaded_bridge_validation_failure("bridge successor link is not reciprocal and type-stable", grid_offset, record->id);
        }
    }
    int owner_offset = grid_offset;
    for (int guard = 0; guard < 64; ++guard) {
        const building *owner_candidate = map_building_at(owner_offset).record();
        if (!owner_candidate) {
            return report_loaded_bridge_validation_failure("bridge chain lost its owner record", grid_offset, record->id);
        }
        if (owner_candidate->prev_part_building_id == 0) {
            return 1;
        }
        owner_offset -= delta;
        if (!map_grid_is_valid_offset(owner_offset) || !map_building_exists_at(owner_offset)) {
            return report_loaded_bridge_validation_failure("bridge chain cannot reach a serialized owner", grid_offset, record->id);
        }
    }
    return report_loaded_bridge_validation_failure("bridge chain exceeds the supported 64-piece bound or is cyclic", grid_offset, record->id);
}

int map_bridge_validate_loaded_native_bridges(void)
{
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            const int grid_offset = map_grid_offset(x, y);
            if (map_grid_is_valid_offset(grid_offset) && map_is_bridge(grid_offset) && !validate_loaded_bridge_piece_at(grid_offset)) {
                return 0;
            }
        }
    }
    int valid = 1;
    Building::for_each([&valid](Building *candidate) {
        if (!valid || !candidate || !candidate->type || !candidate->type->bridge().is_bridge()) {
            return;
        }
        const building *record = candidate->record();
        if (!record || !map_grid_is_valid_offset(record->grid_offset) || !map_is_bridge(record->grid_offset) || !map_building_exists_at(record->grid_offset) || map_building_at(record->grid_offset).record() != record) {
            report_loaded_bridge_validation_failure("serialized bridge building is orphaned from bridge terrain", record ? record->grid_offset : -1, record ? record->id : 0);
            valid = 0;
        }
    });
    return valid;
}
