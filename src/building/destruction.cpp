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
#include "building/BuildingGeometry.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/building_type_registry_internal.h"
#include "city/ratings.h"
#include "city/culture.h"
#include "destruction.h"

#include "city/message.h"
#include "city/population.h"
#include "core/image.h"
#include "core/random.h"
#include "figuretype/missile.h"
#include "map/grid.h"
#include "map/property.h"
#include "figure/route.h"
#include "map/terrain.h"
#include "map/water_navigation.h"
#include "sound/effect.h"

#include <algorithm>
#include <utility>
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

static building_type_registry_impl::BuildingGeometry geometry_from_rubble_tiles(
    const std::vector<RubbleTile> &tiles)
{
    std::vector<building_type_registry_impl::BuildingGeometryCell> cells;
    cells.reserve(tiles.size());
    for (const RubbleTile &tile : tiles) {
        cells.push_back({tile.x, tile.y});
    }
    return building_type_registry_impl::BuildingGeometry::from_world_cells(std::move(cells));
}

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

static void refresh_after_destruction(
    const building_type_registry_impl::BuildingType *type,
    const std::vector<RubbleTile> &tiles)
{
    if (!type) {
        return;
    }
    if (type->bridge().is_bridge()) {
        water_navigation::invalidate_topology();
    }
    if (type->attr_is("dock")) {
        water_navigation::invalidate_dock_endpoints();
    }
    if (building_is_connectable(type->type())) {
        building_connectable_update_connections_for_type(type->type());
    }
    Route::updateLandTerrain();
    Route::updateWallTerrain();
    if (tiles.empty()) {
        return;
    }
    int min_x = tiles.front().x;
    int min_y = tiles.front().y;
    int max_x = min_x;
    int max_y = min_y;
    for (const RubbleTile &tile : tiles) {
        min_x = std::min(min_x, tile.x);
        min_y = std::min(min_y, tile.y);
        max_x = std::max(max_x, tile.x);
        max_y = std::max(max_y, tile.y);
    }
    map_tiles_update_region_empty_land(min_x, min_y, max_x, max_y);
    map_tiles_update_region_meadow(min_x, min_y, max_x, max_y);
    map_tiles_update_region_rubble(min_x, min_y, max_x, max_y);
    map_tiles_update_area_roads(min_x, min_y, std::max(max_x - min_x, max_y - min_y) + 3);
    map_tiles_update_area_highways(min_x, min_y, std::max(max_x - min_x, max_y - min_y) + 3);
}

static Building &destruction_owner(Building &building_object)
{
    if (building_object.type && building_object.type->bridge().is_bridge()) {
        return building_object.dynamic_bridge_owner();
    }
    return building_object.Composition ? *building_object.Composition->owner() : building_object;
}

static RubbleState rubble_origin_for(Building &building_object)
{
    Building &owner = destruction_owner(building_object);
    RubbleState origin;
    origin.original_type = owner.type;
    origin.original_orientation = static_cast<unsigned char>(owner.orientation());
    origin.original_grid_offset = static_cast<unsigned short>(
        map_grid_offset(owner.x(), owner.y()));
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
    if (!building_object.Foundation) {
        return;
    }
    const int rotation = building_object.Foundation->definition().rotates() ? building_object.orientation() : 0;
    for (const building_type_registry_impl::RotatedFoundationCell &cell :
        building_object.Foundation->cells(rotation)) {
        const int x = building_object.x() + cell.x;
        const int y = building_object.y() + cell.y;
        if (!map_grid_is_inside(x, y, 1)) {
            continue;
        }
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

static std::vector<Building *> collect_destroyed_parts(Building &building)
{
    std::vector<Building *> parts;
    Building &owner = destruction_owner(building);
    if (owner.type && owner.type->bridge().is_bridge()) {
        Building *part = &owner;
        for (int guard = 0; part && guard < 64; ++guard) {
            Building *next = part->dynamic_bridge_next();
            if (Building *stable = Building::get(part->id)) {
                parts.push_back(stable);
            }
            part = next;
        }
        return parts;
    }
    if (owner.Composition) {
        owner.Composition->for_each_member([&parts](Building &part) {
            if (Building *stable = Building::get(part.id)) {
                parts.push_back(stable);
            }
        });
    } else {
        parts.push_back(&owner);
    }
    return parts;
}

void Building::retire_for_destruction()
{
    city_culture_remove_building_module_capacity(record_);
    building_local_workforce::remove_building(*this);
    if (Housing && Housing->state().population) {
        city_population_remove_home_removed(Housing->state().population);
    }
    const bool is_dynamic_bridge = type && type->bridge().is_bridge();
    if (is_dynamic_bridge) {
        map_bridge_remove(record_->grid_offset, 0);
    }
    building_clear_related_data(record_);
    remove_map_tiles();
    if (is_dynamic_bridge) {
        record_->prev_part_building_id = 0;
        record_->next_part_building_id = 0;
    }
    record_->state = BUILDING_STATE_DELETED_BY_GAME;
}

void Building::initialize_destruction_rubble(
    const RubbleState &origin,
    bool burning,
    bool plagued)
{
    record_->state = static_cast<unsigned char>(burning ? BUILDING_STATE_IN_USE : BUILDING_STATE_RUBBLE);
    record_->figure_id4 = 0;
    record_->fire_proof = 1;
    record_->has_plague = static_cast<unsigned char>(plagued);
    *Rubble->state() = origin;
    Rubble->state()->random_seed = random_byte();
    record_->fire_duration = burning
        ? static_cast<short>((Rubble->state()->random_seed & 7) + 1)
        : 0;
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
        rubble.initialize_destruction_rubble(origin, burning != 0, plagued != 0);
        map_building_tiles_add_rubble(rubble, tile.x, tile.y, building_image_get(&rubble));
        created++;
    }
    return created;
}

void Building::destroy_by_collapse()
{
    game_undo_disable();
    Building &destroyed = destruction_owner(*this);
    std::vector<Building *> parts = collect_destroyed_parts(destroyed);
    std::vector<RubbleTile> tiles;
    for (Building *part : parts) {
        capture_rubble_tiles(*part, tiles);
    }
    const RubbleState origin = rubble_origin_for(destroyed);
    const building_type_registry_impl::BuildingGeometry destroyed_geometry =
        geometry_from_rubble_tiles(tiles);
    const building_type_registry_impl::BuildingType *destroyed_type = destroyed.type;
    for (Building *part : parts) {
        if (part->matches("tower")) {
            figure_kill_tower_sentries_in_building(*part);
        }
        part->retire_for_destruction();
    }
    create_rubble_pieces(rubble_type(), origin, tiles, 0, 0);
    const building_type_registry_impl::BuildingGeometrySquareExtent explosion =
        destroyed_geometry.centered_square_extent(5);
    if (explosion.size) {
        figure_create_explosion_cloud(explosion.x, explosion.y, explosion.size, 0);
    }
    refresh_after_destruction(destroyed_type, tiles);
}

void Building::destroy_by_fire()
{
    game_undo_disable();
    Building &destroyed = destruction_owner(*this);
    std::vector<Building *> parts = collect_destroyed_parts(destroyed);
    std::vector<RubbleTile> tiles;
    for (Building *part : parts) {
        capture_rubble_tiles(*part, tiles);
    }
    const RubbleState origin = rubble_origin_for(destroyed);
    const building_type_registry_impl::BuildingGeometry destroyed_geometry =
        geometry_from_rubble_tiles(tiles);
    const building_type_registry_impl::BuildingType *destroyed_type = destroyed.type;
    for (Building *part : parts) {
        if (part->matches("tower")) {
            figure_kill_tower_sentries_in_building(*part);
        }
        part->retire_for_destruction();
    }
    const building_type burning_ruin = burning_ruin_type();
    create_rubble_pieces(
        burning_ruin == BUILDING_NONE ? rubble_type() : burning_ruin,
        origin,
        tiles,
        burning_ruin != BUILDING_NONE,
        0);
    const building_type_registry_impl::BuildingGeometrySquareExtent explosion =
        destroyed_geometry.centered_square_extent(5);
    if (explosion.size) {
        figure_create_explosion_cloud(explosion.x, explosion.y, explosion.size, 0);
    }
    refresh_after_destruction(destroyed_type, tiles);
}

void Building::destroy_by_plague()
{
    game_undo_disable();
    Building &destroyed = destruction_owner(*this);
    std::vector<Building *> parts = collect_destroyed_parts(destroyed);
    std::vector<RubbleTile> tiles;
    for (Building *part : parts) {
        capture_rubble_tiles(*part, tiles);
    }
    const RubbleState origin = rubble_origin_for(destroyed);
    const building_type_registry_impl::BuildingGeometry destroyed_geometry =
        geometry_from_rubble_tiles(tiles);
    const building_type_registry_impl::BuildingType *destroyed_type = destroyed.type;
    for (Building *part : parts) {
        if (part->matches("tower")) {
            figure_kill_tower_sentries_in_building(*part);
        }
        part->retire_for_destruction();
    }
    const building_type burning_ruin = burning_ruin_type();
    create_rubble_pieces(
        burning_ruin == BUILDING_NONE ? rubble_type() : burning_ruin,
        origin,
        tiles,
        burning_ruin != BUILDING_NONE,
        1);
    const building_type_registry_impl::BuildingGeometrySquareExtent explosion =
        destroyed_geometry.centered_square_extent(5);
    if (explosion.size) {
        figure_create_explosion_cloud(explosion.x, explosion.y, explosion.size, 0);
    }
    refresh_after_destruction(destroyed_type, tiles);
}

void Building::destroy_without_rubble()
{
    game_undo_disable();
    Building &destroyed = destruction_owner(*this);
    const building_type_registry_impl::BuildingType *destroyed_type = destroyed.type;
    std::vector<Building *> parts = collect_destroyed_parts(destroyed);
    std::vector<RubbleTile> tiles;
    for (Building *part : parts) {
        capture_rubble_tiles(*part, tiles);
    }
    for (Building *part : parts) {
        part->retire_for_destruction();
    }
    refresh_after_destruction(destroyed_type, tiles);
}

int building_destroy_first_of_type(building_type type)
{
    if (type == BUILDING_NONE) {
        return 0;
    }
    for (Building &target : Building::of_type(type)) {
        if (!target.is_in_use() && !target.is_mothballed()) {
            continue;
        }
        const int grid_offset = target.grid_offset();
        target.destroy_by_collapse();
        Route::updateLandTerrain();
        return grid_offset;
    }
    return 0;
}

void building_destroy_last_placed(void)
{
    int highest_sequence = 0;
    Building *last_building = nullptr;
    Building::for_each([&](Building *runtime_building) {
        const building *b = runtime_building->record();
        if (b->state == BUILDING_STATE_CREATED || b->state == BUILDING_STATE_IN_USE) {
            if (b->created_sequence > highest_sequence) {
                highest_sequence = b->created_sequence;
                last_building = runtime_building;
            }
        }
    });
    if (last_building) {
        city_message_post(1, MESSAGE_ROAD_TO_ROME_BLOCKED, 0, last_building->grid_offset());
        last_building->destroy_by_collapse();
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
    refresh_after_destruction(origin.original_type, tiles);
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
        if (!destroyed_building.Rubble) {
            city_ratings_peace_building_destroyed(destroyed_building);
            destroyed_building.destroy_by_collapse();
            destroyed = 1;
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
