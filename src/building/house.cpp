#include "house.h"

#include "building/building.h"
#include "building/building_record.h"

extern "C" {
#include "building/image.h"
#include "building/building_type_api.h"
#include "city/population.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_access.h"
#include "map/terrain.h"
}

#define MAX_DIR 4

#define OFFSET(x,y) (x + GRID_SIZE * y)

static const int HOUSE_TILE_OFFSETS[] = {
    OFFSET(0,0), OFFSET(1,0), OFFSET(0,1), OFFSET(1,1), // 2x2
    OFFSET(2,0), OFFSET(2,1), OFFSET(2,2), OFFSET(1,2), OFFSET(0,2), // 3x3
    OFFSET(3,0), OFFSET(3,1), OFFSET(3,2), OFFSET(3,3), OFFSET(2,3), OFFSET(1,3), OFFSET(0,3) // 4x4
};

static const struct {
    int x;
    int y;
    int offset;
} EXPAND_DIRECTION_DELTA[MAX_DIR] = { {0, 0, 0}, {-1, -1, -GRID_SIZE - 1}, {-1, 0, -1}, {0, -1, -GRID_SIZE} };

static struct {
    int x;
    int y;
    int inventory[RESOURCE_SLOT_COUNT];
    int sentiment;
    int population;
} merge_data;

static int find_best_corner_for_devolve(int x, int y, int old_size, int new_size);

int building_house_is_active(Building house)
{
    return house.is_in_use() && house.has_house_size();
}

int building_house_legacy_level(Building house)
{
    const building *record = house.legacy_record();
    if (!record) {
        return -1;
    }

    int level = building_type_registry_get_housing_level(house.type_id());
    if (level >= HOUSE_MIN && level <= HOUSE_MAX) {
        return level;
    }
    if (!record->house_size && !building_type_registry_has_housing(house.type_id())) {
        return -1;
    }
    if (record->subtype.house_level >= HOUSE_MIN && record->subtype.house_level <= HOUSE_MAX) {
        return record->subtype.house_level;
    }
    return -1;
}

const model_house *building_house_get_model(Building house)
{
    if (!house.legacy_record()) {
        return nullptr;
    }

    const model_house *model = building_type_registry_get_housing_model(house.type_id());
    if (model) {
        return model;
    }
    int level = building_house_legacy_level(house);
    return level >= 0 ? model_get_house(static_cast<house_level>(level)) : nullptr;
}

int building_house_has_plebeian_residents(Building house)
{
    if (!house.legacy_record()) {
        return 0;
    }
    if (building_type_registry_has_housing(house.type_id())) {
        return building_type_registry_housing_has_resident_class(
            house.type_id(), BUILDING_TYPE_HOUSING_RESIDENT_PLEBEIAN);
    }

    int level = building_house_legacy_level(house);
    return level >= HOUSE_SMALL_TENT && level < HOUSE_SMALL_VILLA;
}

int building_house_has_patrician_residents(Building house)
{
    if (!house.legacy_record()) {
        return 0;
    }
    if (building_type_registry_has_housing(house.type_id())) {
        return building_type_registry_housing_has_resident_class(
            house.type_id(), BUILDING_TYPE_HOUSING_RESIDENT_PATRICIAN);
    }

    int level = building_house_legacy_level(house);
    return level >= HOUSE_SMALL_VILLA && level <= HOUSE_MAX;
}

static void add_house_tiles(Building &house_object)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    // House evolution redraws often. Clamp the existing stable option here instead
    // of reseeding so a valid visual choice survives normal evolve/devolve cycles.
    house_object.assign_graphic_variant(0);
    if (!house_object.refresh_graphic_if_native()) {
        map_building_tiles_add(house->id, house->x, house->y, house->size, building_image_get(house), TERRAIN_BUILDING);
    }
}

static void set_house_legacy_level_from_type(building *house, building_type type)
{
    int level = building_type_registry_get_housing_level(type);
    if (level >= 0) {
        house->subtype.house_level = level;
    }
}

static int housing_model_size(building_type type, int fallback_size)
{
    int size = building_type_registry_get_model_size(type);
    return size > 0 ? size : fallback_size;
}

static building_type one_tile_medium_insula_type()
{
    return building_type_registry_get_housing_type_for_level(HOUSE_MEDIUM_INSULA, 1);
}

static building_type vacant_lot_fill_type()
{
    return building_type_registry_get_vacant_lot_fill_type();
}

static int is_empty_vacant_lot(const building *house)
{
    return house && house->house_population == 0 && house->type == vacant_lot_fill_type();
}

static building_type split_type_for_house(building *house, building_type fallback_type)
{
    building_type split_type = building_type_registry_get_housing_transition(
        house->type, BUILDING_TYPE_HOUSING_TRANSITION_SPLIT_TO);
    return split_type == BUILDING_NONE ? fallback_type : split_type;
}

void building_house_change_to(Building house_object, building_type type)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    int should_reseed_graphics = is_empty_vacant_lot(house);
    house_object.change_type(type);
    set_house_legacy_level_from_type(house, house->type);
    if (building_type_registry_has_housing(house->type)) {
        int size = building_type_registry_get_model_size(house->type);
        if (size > 0) {
            house->size = house->house_size = size;
            house->house_is_merged = size > 1 ? 1 : 0;
        }
    }
    // Vacant lots do not carry a meaningful house visual variant, so first
    // occupation gets a fresh stable choice. Other transitions preserve it.
    house_object.assign_graphic_variant(should_reseed_graphics);
    add_house_tiles(house_object);
}

static void create_vacant_lot(int x, int y)
{
    building_type type = vacant_lot_fill_type();
    if (type == BUILDING_NONE) {
        return;
    }
    building *b = building_create(type, x, y);
    b->house_population = 0;
    set_house_legacy_level_from_type(b, type);
    b->distance_from_entry = 0;
    map_building_tiles_add(b->id, b->x, b->y, 1, building_image_get(b), TERRAIN_BUILDING);
}

void building_house_vacant_lot_mark_draw(int building_id)
{
    grid_slice *slice = map_grid_get_grid_slice_house(building_id, 0);
    for (int i = 0; i < slice->size; i++) {
        int grid_offset = slice->grid_offsets[i];
        map_property_mark_draw_tile(grid_offset);
    }
}

void building_house_change_to_vacant_lot(Building house_object)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    house->house_population = 0;
    building_type type = vacant_lot_fill_type();
    if (type == BUILDING_NONE) {
        return;
    }
    house_object.change_type(type);
    set_house_legacy_level_from_type(house, type);
    if (house->house_is_merged) {
        map_building_tiles_remove(house->id, house->x, house->y);
        house->house_is_merged = 0;
        house->size = house->house_size = 1;
        house->is_close_to_water = building_is_close_to_water(house);
        map_building_tiles_add(house->id, house->x, house->y, 1, building_image_get(house), TERRAIN_BUILDING);
        create_vacant_lot(house->x + 1, house->y);
        create_vacant_lot(house->x, house->y + 1);
        create_vacant_lot(house->x + 1, house->y + 1);
    } else {
        map_image_set(house->grid_offset, building_image_get(house));
    }
}

static void prepare_for_merge(unsigned int building_id, int num_tiles)
{
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        merge_data.inventory[r] = 0;
    }
    merge_data.population = 0;
    merge_data.sentiment = 0;
    int grid_offset = map_grid_offset(merge_data.x, merge_data.y);
    for (int i = 0; i < num_tiles; i++) {
        int house_offset = grid_offset + HOUSE_TILE_OFFSETS[i];
        if (map_terrain_is(house_offset, TERRAIN_BUILDING)) {
            building *house = building_get(map_building_at(house_offset));
            if (house->id != building_id && house->house_size) {
                merge_data.population += house->house_population;
                merge_data.sentiment += house->house_population * house->sentiment.house_happiness;
                for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
                    merge_data.inventory[r] += house->resources[r];
                }
                house->house_population = 0;
                house->state = BUILDING_STATE_DELETED_BY_GAME;
            }
        }
    }
}

static void merge(building *b)
{
    prepare_for_merge(b->id, 4);

    building_type merge_type = building_type_registry_get_housing_transition(
        b->type, BUILDING_TYPE_HOUSING_TRANSITION_MERGE_TO);
    if (merge_type != BUILDING_NONE) {
        building_change_type(b, merge_type);
        int level = building_type_registry_get_housing_level(merge_type);
        if (level >= 0) {
            b->subtype.house_level = level;
        }
    }

    int merged_size = building_type_registry_get_model_size(b->type);
    b->size = b->house_size = merged_size > 0 ? merged_size : 2;
    b->is_close_to_water = building_is_close_to_water(b);
    merge_data.sentiment += b->house_population * b->sentiment.house_happiness;
    b->house_population += merge_data.population;
    if (b->house_population) {
        b->sentiment.house_happiness = merge_data.sentiment / b->house_population;
    }
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        b->resources[r] += merge_data.inventory[r];
    }
    map_building_tiles_remove(b->id, b->x, b->y);
    b->x = merge_data.x;
    b->y = merge_data.y;
    b->grid_offset = map_grid_offset(b->x, b->y);
    b->house_is_merged = 1;
    Building refreshed_house(b);
    add_house_tiles(refreshed_house);
    if (config_get(CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS)) {
        if (is_empty_vacant_lot(b)) {
            grid_slice *slice = map_grid_get_grid_slice_house(b->id, 0);
            for (int i = 0; i < slice->size; i++) {
                int offset = slice->grid_offsets[i];
                map_property_mark_draw_tile(offset); // re-mark the tiles for redraw
            }
        }
    }

}

void building_house_merge(Building house_object)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    if (house->house_is_merged) {
        return;
    }
    if (!config_get(CONFIG_GP_CH_ALL_HOUSES_MERGE)) {
        if ((map_random_get(house->grid_offset) & 7) >= 5) {
            return;
        }
    }
    int num_house_tiles = 0;
    for (int i = 0; i < 4; i++) {
        int tile_offset = house->grid_offset + HOUSE_TILE_OFFSETS[i];
        if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
            building *other_house = building_get(map_building_at(tile_offset));
            if (other_house->id == house->id) {
                num_house_tiles++;
            } else if (other_house->state == BUILDING_STATE_IN_USE && other_house->house_size &&
                other_house->subtype.house_level == house->subtype.house_level &&
                !other_house->house_is_merged) {
                num_house_tiles++;
            }
        }
    }
    if (num_house_tiles == 4) {
        game_undo_disable();
        merge_data.x = house->x + EXPAND_DIRECTION_DELTA[0].x;
        merge_data.y = house->y + EXPAND_DIRECTION_DELTA[0].y;
        merge(house);
    }
}

int building_house_can_expand(Building house_object, int num_tiles)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return 0;
    }
    // merge with other houses
    for (int dir = 0; dir < MAX_DIR; dir++) {
        int base_offset = EXPAND_DIRECTION_DELTA[dir].offset + house->grid_offset;
        int ok_tiles = 0;
        for (int i = 0; i < num_tiles; i++) {
            int tile_offset = base_offset + HOUSE_TILE_OFFSETS[i];
            if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
                building *other_house = building_get(map_building_at(tile_offset));
                if (other_house->id == house->id) {
                    ok_tiles++;
                } else if (other_house->state == BUILDING_STATE_IN_USE && other_house->house_size) {
                    if (other_house->subtype.house_level <= house->subtype.house_level) {
                        ok_tiles++;
                    }
                }
            }
        }
        if (ok_tiles == num_tiles) {
            merge_data.x = house->x + EXPAND_DIRECTION_DELTA[dir].x;
            merge_data.y = house->y + EXPAND_DIRECTION_DELTA[dir].y;
            return 1;
        }
    }
    // merge with houses and empty terrain
    for (int dir = 0; dir < MAX_DIR; dir++) {
        int base_offset = EXPAND_DIRECTION_DELTA[dir].offset + house->grid_offset;
        int ok_tiles = 0;
        for (int i = 0; i < num_tiles; i++) {
            int tile_offset = base_offset + HOUSE_TILE_OFFSETS[i];
            if (!map_terrain_is(tile_offset, TERRAIN_NOT_CLEAR)) {
                ok_tiles++;
            } else if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
                building *other_house = building_get(map_building_at(tile_offset));
                if (other_house->id == house->id) {
                    ok_tiles++;
                } else if (other_house->state == BUILDING_STATE_IN_USE && other_house->house_size) {
                    if (other_house->subtype.house_level <= house->subtype.house_level) {
                        ok_tiles++;
                    }
                }
            }
        }
        if (ok_tiles == num_tiles) {
            merge_data.x = house->x + EXPAND_DIRECTION_DELTA[dir].x;
            merge_data.y = house->y + EXPAND_DIRECTION_DELTA[dir].y;
            return 1;
        }
    }
    // merge with houses, empty terrain and gardens
    for (int dir = 0; dir < MAX_DIR; dir++) {
        int base_offset = EXPAND_DIRECTION_DELTA[dir].offset + house->grid_offset;
        int ok_tiles = 0;
        for (int i = 0; i < num_tiles; i++) {
            int tile_offset = base_offset + HOUSE_TILE_OFFSETS[i];
            if (!map_terrain_is(tile_offset, TERRAIN_NOT_CLEAR)) {
                ok_tiles++;
            } else if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
                building *other_house = building_get(map_building_at(tile_offset));
                if (other_house->id == house->id) {
                    ok_tiles++;
                } else if (other_house->state == BUILDING_STATE_IN_USE && other_house->house_size) {
                    if (other_house->subtype.house_level <= house->subtype.house_level) {
                        ok_tiles++;
                    }
                }
            } else if (map_terrain_is(tile_offset, TERRAIN_GARDEN) &&
                !config_get(CONFIG_GP_CH_HOUSES_DONT_EXPAND_INTO_GARDENS)) {
                ok_tiles++;
            }
        }
        if (ok_tiles == num_tiles) {
            merge_data.x = house->x + EXPAND_DIRECTION_DELTA[dir].x;
            merge_data.y = house->y + EXPAND_DIRECTION_DELTA[dir].y;
            return 1;
        }
    }
    house->data.house.no_space_to_expand = 1;
    return 0;
}

static void copy_house_data(building *house, const building *main_house)
{
    house->data.house.academy = main_house->data.house.academy;
    house->data.house.amphitheater_actor = main_house->data.house.amphitheater_actor;
    house->data.house.amphitheater_gladiator = main_house->data.house.amphitheater_gladiator;
    house->data.house.barber = main_house->data.house.barber;
    house->data.house.bathhouse = main_house->data.house.bathhouse;
    house->data.house.clinic = main_house->data.house.clinic;
    house->data.house.colosseum_gladiator = main_house->data.house.colosseum_gladiator;
    house->data.house.colosseum_lion = main_house->data.house.colosseum_lion;
    house->data.house.education = main_house->data.house.education;
    house->data.house.entertainment = main_house->data.house.entertainment;
    house->data.house.health = main_house->data.house.health;
    house->data.house.hippodrome = main_house->data.house.hippodrome;
    house->data.house.hospital = main_house->data.house.hospital;
    house->data.house.library = main_house->data.house.library;
    house->data.house.num_foods = main_house->data.house.num_foods;
    house->data.house.num_gods = main_house->data.house.num_gods;
    house->data.house.school = main_house->data.house.school;
    house->data.house.temple_ceres = main_house->data.house.temple_ceres;
    house->data.house.temple_mars = main_house->data.house.temple_mars;
    house->data.house.temple_mercury = main_house->data.house.temple_mercury;
    house->data.house.temple_neptune = main_house->data.house.temple_neptune;
    house->data.house.temple_venus = main_house->data.house.temple_venus;
    house->data.house.theater = main_house->data.house.theater;
    house->sentiment.house_happiness = main_house->sentiment.house_happiness;
}

static void create_splitted_house_tile(building *main_house, building_type type,
    int x, int y, int population, const int *inventory)
{
    building *house = building_create(type, x, y);
    house->house_population = population;
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = inventory[i];
    }
    set_house_legacy_level_from_type(house, type);
    copy_house_data(house, main_house);
    house->distance_from_entry = 0;
    Building refreshed_house(house);
    add_house_tiles(refreshed_house);
}

static void split_size2(building *house, building_type new_type)
{
    if (new_type == BUILDING_NONE) {
        return;
    }
    int inventory_per_tile[RESOURCE_SLOT_COUNT];
    int inventory_remainder[RESOURCE_SLOT_COUNT];
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        inventory_per_tile[i] = house->resources[i] / 4;
        inventory_remainder[i] = house->resources[i] % 4;
    }
    int population_per_tile = house->house_population / 4;
    int population_remainder = house->house_population % 4;

    map_building_tiles_remove(house->id, house->x, house->y);

    // main tile
    building_change_type(house, new_type);
    set_house_legacy_level_from_type(house, house->type);
    house->size = house->house_size = 1;
    house->is_close_to_water = building_is_close_to_water(house);
    house->house_is_merged = 0;
    house->house_population = population_per_tile + population_remainder;
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = inventory_per_tile[i] + inventory_remainder[i];
    }
    house->distance_from_entry = 0;

    Building refreshed_house(house);
    add_house_tiles(refreshed_house);

    // the other tiles (new buildings)
    create_splitted_house_tile(house, house->type, house->x + 1, house->y, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x, house->y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x + 1, house->y + 1, population_per_tile, inventory_per_tile);
}

static void split_size3(building *house)
{
    building_type medium_insula_type = one_tile_medium_insula_type();
    if (medium_insula_type == BUILDING_NONE) {
        return;
    }
    int inventory_per_tile[RESOURCE_SLOT_COUNT];
    int inventory_remainder[RESOURCE_SLOT_COUNT];
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        inventory_per_tile[i] = house->resources[i] / 9;
        inventory_remainder[i] = house->resources[i] % 9;
    }
    int population_per_tile = house->house_population / 9;
    int population_remainder = house->house_population % 9;

    map_building_tiles_remove(house->id, house->x, house->y);

    // main tile
    building_change_type(house, medium_insula_type);
    set_house_legacy_level_from_type(house, house->type);
    house->size = house->house_size = 1;
    house->is_close_to_water = building_is_close_to_water(house);
    house->house_is_merged = 0;
    house->house_population = population_per_tile + population_remainder;
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = inventory_per_tile[i] + inventory_remainder[i];
    }
    house->distance_from_entry = 0;

    Building refreshed_house(house);
    add_house_tiles(refreshed_house);

    // the other tiles (new buildings)
    create_splitted_house_tile(house, house->type, house->x, house->y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x + 1, house->y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x + 2, house->y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x, house->y + 2, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x + 1, house->y + 2, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(house, house->type, house->x + 2, house->y + 2, population_per_tile, inventory_per_tile);
}

static void split(building *house, int num_tiles)
{
    int grid_offset = map_grid_offset(merge_data.x, merge_data.y);
    for (int i = 0; i < num_tiles; i++) {
        int tile_offset = grid_offset + HOUSE_TILE_OFFSETS[i];
        if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
            building *other_house = building_get(map_building_at(tile_offset));
            if (other_house->id != house->id && other_house->house_size) {
                if (other_house->house_is_merged == 1) {
                    split_size2(other_house, split_type_for_house(other_house, other_house->type));
                } else if (other_house->house_size == 2) {
                    split_size2(other_house, one_tile_medium_insula_type());
                } else if (other_house->house_size == 3) {
                    split_size3(other_house);
                }
            }
        }
    }
}

/// Runtime-size house transitions

int building_house_expand_to_type(Building house_object, building_type type)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return 0;
    }
    int target_size = housing_model_size(type, house->house_size);
    if (target_size <= house->house_size) {
        return 0;
    }

    split(house, target_size * target_size);
    prepare_for_merge(house->id, target_size * target_size);

    building_change_type(house, type);
    set_house_legacy_level_from_type(house, type);
    house->size = house->house_size = target_size;
    house->is_close_to_water = building_is_close_to_water(house);
    house->house_is_merged = 0;
    house->house_population += merge_data.population;
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] += merge_data.inventory[i];
    }
    map_building_tiles_remove(house->id, house->x, house->y);
    house->x = merge_data.x;
    house->y = merge_data.y;
    house->grid_offset = map_grid_offset(house->x, house->y);
    Building refreshed_house(house);
    add_house_tiles(refreshed_house);
    return 1;
}

static void desize_house_to_type(building *house, building_type type)
{
    int target_size = housing_model_size(type, house->size - 1);

    map_building_tiles_remove(house->id, house->x, house->y);
    int road_tile_offset = find_best_corner_for_devolve(house->x, house->y, house->size, target_size);

    house->x = map_grid_offset_to_x(road_tile_offset);
    house->y = map_grid_offset_to_y(road_tile_offset);
    building_change_type(house, type);
    set_house_legacy_level_from_type(house, type);
    house->size = house->house_size = static_cast<unsigned char>(target_size);
    house->is_close_to_water = building_is_close_to_water(house);
    house->house_is_merged = 0;
    house->distance_from_entry = 0;

    Building refreshed_house(house);
    add_house_tiles(refreshed_house);
}

static void shrink_house_to_type(building *house, building_type type)
{
    int old_size = house->house_size;
    int target_size = housing_model_size(type, old_size - 1);
    int extra_tiles = old_size * old_size - target_size * target_size;
    if (extra_tiles <= 0) {
        building_house_change_to(Building(house), type);
        return;
    }

    int inventory_per_tile[RESOURCE_SLOT_COUNT];
    int inventory_remainder[RESOURCE_SLOT_COUNT];
    int shares = extra_tiles + 1;
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        inventory_per_tile[i] = house->resources[i] / shares;
        inventory_remainder[i] = house->resources[i] % shares;
    }
    int population_per_tile = house->house_population / shares;
    int population_remainder = house->house_population % shares;

    map_building_tiles_remove(house->id, house->x, house->y);

    building_change_type(house, type);
    set_house_legacy_level_from_type(house, type);
    house->size = house->house_size = static_cast<unsigned char>(target_size);
    house->is_close_to_water = building_is_close_to_water(house);
    house->house_is_merged = 0;
    house->house_population = population_per_tile + population_remainder;
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = inventory_per_tile[i] + inventory_remainder[i];
    }
    house->distance_from_entry = 0;

    Building refreshed_house(house);
    add_house_tiles(refreshed_house);

    building_type extra_house_type = one_tile_medium_insula_type();
    if (extra_house_type == BUILDING_NONE) {
        return;
    }
    for (int y = 0; y < old_size; y++) {
        for (int x = 0; x < old_size; x++) {
            if (x < target_size && y < target_size) {
                continue;
            }
            create_splitted_house_tile(
                house,
                extra_house_type,
                house->x + x,
                house->y + y,
                population_per_tile,
                inventory_per_tile);
        }
    }
}

void building_house_devolve_to_type(Building house_object, building_type type)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    int current_size = house->house_size;
    int target_size = housing_model_size(type, current_size);

    int current_level = building_type_registry_get_housing_level(house->type);
    int target_level = building_type_registry_get_housing_level(type);
    if (current_level == HOUSE_LARGE_INSULA && target_level == HOUSE_MEDIUM_INSULA &&
        current_size == 2 && target_size == 2) {
        building_type split_type = split_type_for_house(house, BUILDING_NONE);
        if (split_type != BUILDING_NONE &&
            !config_get(CONFIG_GP_CH_ALL_HOUSES_MERGE) &&
            (map_random_get(house->grid_offset) & 7) >= 5) {
            split_size2(house, split_type);
        } else {
            house->house_is_merged = 1;
            building_house_change_to(house_object, type);
        }
        return;
    }

    if (target_size < current_size && current_size >= 3) {
        if (config_get(CONFIG_GP_CH_PATRICIAN_DEVOLUTION_FIX)) {
            desize_house_to_type(house, type);
        } else {
            shrink_house_to_type(house, type);
        }
        return;
    }

    if (target_size < current_size && current_size == 2) {
        split_size2(house, type);
        return;
    }

    building_house_change_to(house_object, type);
}

static int find_best_corner_for_devolve(int x, int y, int old_size, int new_size)
{
    int max_offset = old_size - new_size;
    int x_one_tile = -1, y_one_tile = -1;  // road found 1 tile away (radius 2)

    for (int oy = 0; oy <= max_offset; oy++) {
        for (int ox = 0; ox <= max_offset; ox++) {
            int base_x = x + ox;
            int base_y = y + oy;
            int rx, ry;
            if (map_closest_road_within_radius(base_x, base_y, new_size, 1, &rx, &ry)) {
                return map_grid_offset(base_x, base_y);
            }
            // fallback: store 1 tile-away candidate if not already stored
            if (x_one_tile == -1 &&
                map_closest_road_within_radius(base_x, base_y, new_size, 2, &rx, &ry)) {
                x_one_tile = base_x;
                y_one_tile = base_y;
            }
        }
    }

    if (x_one_tile != -1) { //only found position 1 tile away from road
        return map_grid_offset(x_one_tile, y_one_tile);
    }
    // not found
    return map_grid_offset(x, y);
}

void building_house_check_for_corruption(Building house_object)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    int calc_grid_offset = map_grid_offset(house->x, house->y);
    house->data.house.no_space_to_expand = 0;
    if (house->grid_offset != calc_grid_offset || map_building_at(house->grid_offset) != house->id) {
        int map_width, map_height;
        map_grid_size(&map_width, &map_height);
        for (int y = 0; y < map_height; y++) {
            for (int x = 0; x < map_width; x++) {
                int grid_offset = map_grid_offset(x, y);
                if (map_building_at(grid_offset) == house->id) {
                    house->grid_offset = grid_offset;
                    house->x = map_grid_offset_to_x(grid_offset);
                    house->y = map_grid_offset_to_y(grid_offset);
                    building_totals_add_corrupted_house(0);
                    return;
                }
            }
        }
        building_totals_add_corrupted_house(1);
        house->state = BUILDING_STATE_RUBBLE;
    }
}

void building_house_restore_population_after_undo(Building house_object)
{
    building *house = house_object.legacy_record();
    if (!house) {
        return;
    }
    if (house->figure_id) {
        figure *homeless = figure_get(house->figure_id);
        if (homeless->building_id == house->id) {
            house->house_population = homeless->migrant_num_people;
            city_population_add_homeless(homeless->migrant_num_people);
            figure_delete(homeless);
        }
    }
}
