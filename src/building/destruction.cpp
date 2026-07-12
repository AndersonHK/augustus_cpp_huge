#include "building/image.h"
#include "building/local_workforce.h"
#include "building/connectable.h"
#include "figuretype/wall.h"
#include "game/defines.h"
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

struct RubbleTile {
    int x = 0;
    int y = 0;
    int grid_offset = 0;
};

static const building_type_registry_impl::BuildingType *surface_type_at(int grid_offset)
{
    const char *type_attr = nullptr;
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        type_attr = "aqueduct";
    } else if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
        type_attr = "wall";
    } else if (map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        type_attr = "gatehouse";
    }
    return type_attr ? building_type_registry_impl::definition_for_type(
        building_type_registry_impl::type_from_attr(type_attr)) : nullptr;
}

static RubbleState rubble_origin_for_surface(int grid_offset)
{
    RubbleState origin;
    origin.original_grid_offset = static_cast<unsigned short>(grid_offset);
    origin.original_type = surface_type_at(grid_offset);
    return origin;
}

static void refresh_after_destruction(const building_type_registry_impl::BuildingType *type)
{
    if (!type) {
        return;
    }
    if (type->tool().is_aqueduct() ||
        (type->has_foundation() && type->foundation().has_water_requirement())) {
        Route::updateWaterTerrain();
    }
    if (building_is_connectable(type->type())) {
        building_connectable_update_connections_for_type(type->type());
    }
}

static RubbleState rubble_origin_for(Building &building_object)
{
    Building &main_building = building_object.main();
    const building *record = main_building.record();
    RubbleState origin;
    origin.original_type = main_building.type;
    origin.original_orientation = record ? static_cast<unsigned char>(record->subtype.orientation) : 0;
    int x = record ? record->x : 0;
    int y = record ? record->y : 0;
    if (origin.original_type && origin.original_type->has_composition()) {
        const int rotation = (origin.original_orientation % 4 + 4) % 4;
        const building_type_registry_impl::ComposedBuildingDefinition &composition =
            origin.original_type->composition();
        const building_type_registry_impl::ComposedPartOffset main_offset =
            composition.main_offset_for_rotation(rotation);
        x -= main_offset.x;
        y -= main_offset.y;
    }
    origin.original_grid_offset = static_cast<unsigned short>(map_grid_offset(x, y));
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
    building_object.remove_map_tiles();
    record->prev_part_building_id = 0;
    record->next_part_building_id = 0;
    record->state = BUILDING_STATE_DELETED_BY_GAME;
}

static int create_rubble_pieces(
    building_type rubble_building_type,
    const RubbleState &origin,
    const std::vector<RubbleTile> &tiles,
    int burning,
    int plagued)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(rubble_building_type);
    if (!definition) {
        return 0;
    }
    int created = 0;
    for (const RubbleTile &tile : tiles) {
        Building &rubble = city_building_runtime().create(*definition, tile.x, tile.y);
        building *record = const_cast<building *>(rubble.record());
        if (!record || !rubble.Rubble || !rubble.Rubble->state()) {
            continue;
        }
        record->state = static_cast<unsigned char>(burning ? BUILDING_STATE_IN_USE : BUILDING_STATE_RUBBLE);
        record->size = 1;
        record->prev_part_building_id = 0;
        record->next_part_building_id = 0;
        record->figure_id4 = 0;
        record->tax_income_or_storage = 0;
        record->fire_duration = burning ? static_cast<short>((record->house_figure_generation_delay & 7) + 1) : 0;
        record->fire_proof = 1;
        record->has_plague = static_cast<unsigned char>(plagued);
        *rubble.Rubble->state() = origin;
        map_building_tiles_add_rubble(rubble, tile.x, tile.y, building_image_get(&rubble));
        created++;
    }
    return created;
}

static int destroy_with_rubble(building *b, building_type rubble_building_type, int burning, int plagued)
{
    game_undo_disable();
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return 0;
    }

    Building &destroyed = runtime->building;
    std::vector<Building *> parts = collect_destroyed_parts(destroyed);
    std::vector<RubbleTile> tiles;
    for (Building *part : parts) {
        if (part) {
            capture_rubble_tiles(*part, tiles);
        }
    }
    const RubbleState origin = rubble_origin_for(destroyed);

    const building_type_registry_impl::BuildingType *destroyed_type = destroyed.type;
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
    if (!tiles.empty() && origin.original_type) {
        figure_create_explosion_cloud(
            tiles.front().x,
            tiles.front().y,
            std::max(
                origin.original_type->placement_width(origin.original_orientation),
                origin.original_type->placement_height(origin.original_orientation)),
            0);
    }
    refresh_after_destruction(destroyed_type);
    return 1;
}

static void destroy_group_without_rubble(building *b)
{
    game_undo_disable();
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return;
    }
    const building_type_registry_impl::BuildingType *destroyed_type = runtime->building.type;
    std::vector<Building *> parts = collect_destroyed_parts(runtime->building);
    for (Building *part : parts) {
        if (part) {
            retire_destroyed_part(*part);
        }
    }
    refresh_after_destruction(destroyed_type);
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
        building_destroy_by_collapse(last_building);
        Route::updateLandTerrain();
    }
}

static int hit_points_at(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return game_defines_default_building_hit_points();
    }
    const Building &target = map_building_at(grid_offset);
    return target.type && target.type->model().has_hit_points() ?
        target.type->model().hit_points() :
        game_defines_default_building_hit_points();
}

void building_apply_enemy_damage(int grid_offset)
{
    if (map_building_damage_increase(grid_offset) > hit_points_at(grid_offset)) {
        building_destroy_by_enemy(grid_offset);
    }
}

static int destroy_surface_by_enemy(int grid_offset)
{
    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
        map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
        map_tiles_update_region_empty_land(x, y, x, y);
        map_property_clear_plaza_earthquake_or_overgrown_garden(grid_offset);
        map_tiles_update_all_gardens();
        return 1;
    }

    if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
        figure_kill_tower_sentries_at(x, y);
    }
    if (!map_terrain_is(grid_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL | TERRAIN_GATEHOUSE)) {
        return 0;
    }
    const RubbleState origin = rubble_origin_for_surface(grid_offset);
    const std::vector<RubbleTile> tiles = {{x, y, grid_offset}};
    if (!create_rubble_pieces(rubble_type(), origin, tiles, 0, 0)) {
        return 0;
    }
    refresh_after_destruction(origin.original_type);
    return 1;
}

void building_destroy_by_enemy(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }

    int destroyed = 0;
    if (map_building_exists_at(grid_offset)) {
        Building &destroyed_building = map_building_at(grid_offset);
        building *b = const_cast<::building *>(destroyed_building.record());
        if (b && !destroyed_building.Rubble) {
            city_ratings_peace_building_destroyed(destroyed_building);
            destroyed = destroy_with_rubble(b, rubble_type(), 0, 0);
        }
    } else {
        destroyed = destroy_surface_by_enemy(grid_offset);
    }
    if (!destroyed) {
        return;
    }

    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    figure_tower_sentry_reroute();
    map_tiles_update_area_walls(x, y, 3);
    map_tiles_update_region_aqueducts(x - 3, y - 3, x + 3, y + 3);
    Route::updateLandTerrain();
    Route::updateWallTerrain();
}
