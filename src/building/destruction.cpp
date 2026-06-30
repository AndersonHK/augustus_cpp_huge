#include "building/data_transfer.h"
#include "building/image.h"
#include "building/local_workforce.h"
#include "figuretype/wall.h"
#include "game/undo.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/tiles.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "city/ratings.h"
#include "city/culture.h"
#include "destruction.h"

#include "city/message.h"
#include "city/population.h"
#include "core/image.h"
#include "figuretype/missile.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "figure/route.h"
#include "map/terrain.h"
#include "sound/effect.h"

#include <algorithm>
#include <vector>

static building_type burning_ruin_type()
{
    static building_type burning_ruin = BUILDING_NONE;
    if (burning_ruin == BUILDING_NONE) {
        burning_ruin = building_type_registry_impl::type_from_attr("burning_ruin");
    }
    return burning_ruin;
}

static building_type rubble_type()
{
    static building_type rubble = BUILDING_NONE;
    if (rubble == BUILDING_NONE) {
        rubble = building_type_registry_impl::type_from_attr("rubble");
    }
    return rubble;
}

struct RubbleOrigin {
    const building_type_registry_impl::BuildingType *type = nullptr;
    int grid_offset = 0;
    int size = 1;
    int orientation = 0;
};

struct RubbleTile {
    int x = 0;
    int y = 0;
    int grid_offset = 0;
};

static RubbleOrigin rubble_origin_for(Building &building_object)
{
    Building &main_building = building_object.main();
    const building *record = main_building.record();
    RubbleOrigin origin;
    origin.type = main_building.type;
    origin.grid_offset = record ? record->grid_offset : 0;
    origin.size = record && record->size > 0 ? record->size : 1;
    origin.orientation = record ? record->subtype.orientation : 0;
    if (origin.type && origin.type->is_warehouse()) {
        origin.size = 3;
    }
    return origin;
}

static int rubble_tile_is_captured(const std::vector<RubbleTile> &tiles, int grid_offset)
{
    return std::any_of(tiles.begin(), tiles.end(), [grid_offset](const RubbleTile &tile) {
        return tile.grid_offset == grid_offset;
    });
}

static void capture_rubble_tiles(Building &building_object, std::vector<RubbleTile> &tiles)
{
    const building *record = building_object.record();
    if (!record) {
        return;
    }
    const int size = record->size > 0 ? record->size : 1;
    if (!map_grid_is_inside(record->x, record->y, size)) {
        return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            const int x = record->x + dx;
            const int y = record->y + dy;
            const int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, TERRAIN_WATER) || rubble_tile_is_captured(tiles, grid_offset)) {
                continue;
            }
            if (map_building_exists_at(grid_offset) && map_building_at(grid_offset).id != building_object.id) {
                continue;
            }
            tiles.push_back({x, y, grid_offset});
        }
    }
}

static std::vector<Building *> collect_destroyed_parts(Building &building)
{
    std::vector<Building *> parts;
    Building &main_building = building.main();
    for (Building *part = &main_building; part; part = part->next()) {
        parts.push_back(part);
        if (!part->next_part_id() || parts.size() >= 64) {
            break;
        }
    }
    return parts;
}

static void retire_destroyed_part(Building &building_object)
{
    building *record = const_cast<building *>(building_object.record());
    if (!record) {
        return;
    }
    city_culture_remove_building_module_capacity(record);
    building_object.cleanup_figure_references_for_removal();
    building_local_workforce::remove_building(building_object);
    if (record->house_size && record->house_population) {
        city_population_remove_home_removed(record->house_population);
    }
    if (building_object.type && building_object.type->roadblock().is_bridge()) {
        map_bridge_remove(record->grid_offset, 0);
    }
    building_clear_related_data(record);
    map_building_tiles_remove(&building_object, record->x, record->y);
    record->prev_part_building_id = 0;
    record->next_part_building_id = 0;
    record->state = BUILDING_STATE_DELETED_BY_GAME;
}

static void bind_rubble_origin(Building &rubble, const RubbleOrigin &origin)
{
    if (!rubble.Rubble) {
        return;
    }
    RubbleState *state = rubble.Rubble->state();
    if (!state) {
        return;
    }
    state->original_grid_offset = static_cast<unsigned short>(origin.grid_offset);
    state->original_size = static_cast<unsigned char>(origin.size);
    state->original_orientation = static_cast<unsigned char>(origin.orientation);
    state->original_type = origin.type;
}

static building *create_rubble_piece(
    const building_type_registry_impl::BuildingType &definition,
    const RubbleOrigin &origin,
    const RubbleTile &tile,
    int burning,
    int plagued)
{
    Building &rubble = city_building_runtime().create(definition, tile.x, tile.y);
    building *record = const_cast<building *>(rubble.record());
    if (!record) {
        return nullptr;
    }
    record->state = static_cast<unsigned char>(burning ? BUILDING_STATE_IN_USE : BUILDING_STATE_RUBBLE);
    record->x = static_cast<unsigned char>(tile.x);
    record->y = static_cast<unsigned char>(tile.y);
    record->grid_offset = static_cast<short>(tile.grid_offset);
    record->size = 1;
    record->prev_part_building_id = 0;
    record->next_part_building_id = 0;
    record->figure_id4 = 0;
    record->tax_income_or_storage = 0;
    record->fire_duration = burning ? static_cast<short>((record->house_figure_generation_delay & 7) + 1) : 0;
    record->fire_proof = 1;
    record->has_plague = static_cast<unsigned char>(plagued);
    bind_rubble_origin(rubble, origin);
    map_building_tiles_add_rubble(rubble, record->x, record->y, building_image_get(&rubble));
    return record;
}

static void create_rubble_pieces(
    building_type rubble_building_type,
    const RubbleOrigin &origin,
    const std::vector<RubbleTile> &tiles,
    int burning,
    int plagued)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(rubble_building_type);
    if (!definition) {
        return;
    }
    building *previous = nullptr;
    for (const RubbleTile &tile : tiles) {
        building *record = create_rubble_piece(*definition, origin, tile, burning, plagued);
        if (!record) {
            continue;
        }
        if (previous) {
            previous->next_part_building_id = static_cast<short>(record->id);
            record->prev_part_building_id = static_cast<short>(previous->id);
        }
        previous = record;
    }
    if (origin.grid_offset) {
        const int x = map_grid_offset_to_x(origin.grid_offset);
        const int y = map_grid_offset_to_y(origin.grid_offset);
        map_tiles_update_region_rubble(x, y, x + origin.size, y + origin.size);
    }
}

static void destroy_with_rubble(building *b, building_type rubble_building_type, int burning, int plagued)
{
    game_undo_disable();
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return;
    }

    Building &destroyed = runtime->building;
    const RubbleOrigin origin = rubble_origin_for(destroyed);
    std::vector<Building *> parts = collect_destroyed_parts(destroyed);
    std::vector<RubbleTile> tiles;
    for (Building *part : parts) {
        if (part) {
            capture_rubble_tiles(*part, tiles);
        }
    }

    const int updates_water_route = destroyed.type &&
        destroyed.type->has_foundation() &&
        destroyed.type->foundation().has_water_requirement();
    for (Building *part : parts) {
        if (!part) {
            continue;
        }
        building *part_record = const_cast<building *>(part->record());
        if (part_record && building_type_registry_impl::type_attr_is(part_record->type, "tower")) {
            figure_kill_tower_sentries_in_building(part_record);
        }
        retire_destroyed_part(*part);
    }

    create_rubble_pieces(rubble_building_type, origin, tiles, burning, plagued);
    if (origin.grid_offset) {
        figure_create_explosion_cloud(
            map_grid_offset_to_x(origin.grid_offset),
            map_grid_offset_to_y(origin.grid_offset),
            origin.size,
            0);
    }
    if (updates_water_route) {
        Route::updateWaterTerrain();
    }
}

static void destroy_group_without_rubble(building *b)
{
    game_undo_disable();
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return;
    }
    std::vector<Building *> parts = collect_destroyed_parts(runtime->building);
    for (Building *part : parts) {
        if (part) {
            retire_destroyed_part(*part);
        }
    }
}

static void destroy_without_rubble(building *b)
{
    destroy_group_without_rubble(b);
}

static void destroy_on_fire(building *b, int plagued)
{
    const building_type burning_ruin = burning_ruin_type();
    destroy_with_rubble(b, burning_ruin == BUILDING_NONE ? rubble_type() : burning_ruin, burning_ruin != BUILDING_NONE, plagued);
}

void building_destroy_by_collapse(building *b)
{
    destroy_with_rubble(b, rubble_type(), 0, 0);
}

void building_destroy_by_fire(building *b)
{
    destroy_on_fire(b, 0);
}

void building_destroy_by_earthquake(building *b)
{
    destroy_with_rubble(b, rubble_type(), 0, 0);
}

void building_destroy_by_plague(building *b)
{
    destroy_on_fire(b, 1);
}

void building_destroy_without_rubble(building *b)
{
    destroy_without_rubble(b);
}

void building_destroy_by_rioter(building *b)
{
    destroy_on_fire(b, 0);
}

int building_destroy_first_of_type(building_type type)
{
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_MOTHBALLED) {
            continue;
        }
        int grid_offset = b->grid_offset;
        game_undo_disable();
        building_destroy_by_collapse(b);
        Route::updateLandTerrain();
        return grid_offset;
    }
    return 0;
}

void building_destroy_last_placed(void)
{
    int highest_sequence = 0;
    building *last_building = 0;
    Building::for_each([&](Building *runtime_building) {
        building *b = const_cast<::building *>(runtime_building->record());
        if (b->state == BUILDING_STATE_CREATED || b->state == BUILDING_STATE_IN_USE) {
            if (b->created_sequence > highest_sequence) {
                highest_sequence = b->created_sequence;
                last_building = b;
            }
        }
    });
    if (last_building) {
        city_message_post(1, MESSAGE_ROAD_TO_ROME_BLOCKED, 0, last_building->grid_offset);
        game_undo_disable();
        building_destroy_by_collapse(last_building);
        Route::updateLandTerrain();
    }
}

void building_destroy_increase_enemy_damage(int grid_offset, int max_damage)
{
    if (map_building_damage_increase(grid_offset) > max_damage) {
        building_destroy_by_enemy(map_grid_offset_to_x(grid_offset),
            map_grid_offset_to_y(grid_offset), grid_offset);
    }
}

void building_destroy_by_enemy(int x, int y, int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        Building &destroyed_building = map_building_at(grid_offset);
        building *b = const_cast<::building *>(destroyed_building.record());
        if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED) {
            city_ratings_peace_building_destroyed(destroyed_building);
            building_destroy_by_collapse(b);
        }
    } else {
        if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
            figure_kill_tower_sentries_at(x, y);
        }
        if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
            map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
            map_tiles_update_region_empty_land(x, y, x, y);
            map_property_clear_plaza_earthquake_or_overgrown_garden(grid_offset);
            map_tiles_update_all_gardens();
        } else {
            map_building_tiles_set_rubble(nullptr, x, y, 1);
        }
    }
    figure_tower_sentry_reroute();
    map_tiles_update_area_walls(x, y, 3);
    map_tiles_update_region_aqueducts(x - 3, y - 3, x + 3, y + 3);
    Route::updateLandTerrain();
    Route::updateWallTerrain();
}
