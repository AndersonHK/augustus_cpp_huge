#include "building/image.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/image.h"
#include "map/road_access.h"
#include "figure/figure.h"

#include "house.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/house_population.h"
#include "building/housing_type.h"
#include "building/local_workforce.h"

#include "city/population.h"
#include "core/config.h"
#include "core/image.h"
#include "game/resource.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/terrain.h"

#include <algorithm>
#include <vector>

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

static int find_best_corner_for_devolve(int x, int y, int old_size, int new_size);
static int split_blocking_houses(Building source, int x, int y, int num_tiles, int dry_run);

static Building *runtime_building(building *record)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record);
    return runtime ? &runtime->building : nullptr;
}

int building_house_is_active(Building house)
{
    return house.is_in_use() && house.has_house_size();
}

int building_house_legacy_level(Building house)
{
    if (!house.id) {
        return -1;
    }

    const auto *housing = house.type ? house.type->housing_type() : nullptr;
    const int level = housing ? housing->level() : -1;
    if (level >= HOUSE_MIN && level <= HOUSE_MAX) {
        return level;
    }
    if (!house.has_house_size() && !(house.type && house.type->has_housing())) {
        return -1;
    }

    const ::building *record = house.record();
    if (!record) {
        return -1;
    }
    if (record->subtype.house_level >= HOUSE_MIN && record->subtype.house_level <= HOUSE_MAX) {
        return record->subtype.house_level;
    }
    return -1;
}

const model_house *building_house_get_model(Building house)
{
    if (!house.id) {
        return nullptr;
    }

    const auto *housing = house.type ? house.type->housing_type() : nullptr;
    if (housing) {
        return &housing->model();
    }
    int level = building_house_legacy_level(house);
    return level >= 0 ? model_get_house(static_cast<house_level>(level)) : nullptr;
}

int building_house_has_plebeian_residents(Building house)
{
    if (!house.id) {
        return 0;
    }
    const auto *housing = house.type ? house.type->housing_type() : nullptr;
    if (housing) {
        return housing->resident_class() == building_type_registry_impl::HousingResidentClass::Plebeian;
    }

    int level = building_house_legacy_level(house);
    return level >= HOUSE_SMALL_TENT && level < HOUSE_SMALL_VILLA;
}

int building_house_has_patrician_residents(Building house)
{
    if (!house.id) {
        return 0;
    }
    const auto *housing = house.type ? house.type->housing_type() : nullptr;
    if (housing) {
        return housing->resident_class() == building_type_registry_impl::HousingResidentClass::Patrician;
    }

    int level = building_house_legacy_level(house);
    return level >= HOUSE_SMALL_VILLA && level <= HOUSE_MAX;
}

static void add_house_tiles(Building &house_object)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    Building *runtime_house = runtime_building(house);
    if (!runtime_house) {
        return;
    }
    // House evolution redraws often. Clamp the existing stable option here instead
    // of reseeding so a valid visual choice survives normal evolve/devolve cycles.
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(house)) {
        runtime->assign_graphic_variant(0);
    }
    if (!house_object.refresh_graphic_if_native()) {
        map_building_tiles_add(*runtime_house, house->x, house->y, house->size,
            building_image_get(runtime_house), TERRAIN_BUILDING);
    }
}

static int housing_level_for_type(building_type type)
{
    const auto *definition = building_type_registry_impl::definition_for_type(type);
    const auto *housing = definition ? definition->housing_type() : nullptr;
    return housing ? housing->level() : -1;
}

static int set_house_legacy_level_from_type(building *house, building_type type)
{
    if (!house) {
        return 0;
    }
    int level = housing_level_for_type(type);
    house->subtype.house_level = static_cast<short>(level);
    return level >= 0;
}

static int housing_model_size(building_type type)
{
    const auto *definition = building_type_registry_impl::definition_for_type(type);
    int size = definition ? definition->declared_model_size() : 0;
    return size > 0 ? size : 0;
}

static building_type one_tile_medium_insula_type()
{
    return building_type_registry_impl::building_type_for_housing_level(HOUSE_MEDIUM_INSULA, 1);
}

static building_type vacant_lot_fill_type()
{
    return building_type_registry_impl::vacant_lot_fill_type();
}

static int is_empty_vacant_lot(const building *house)
{
    return house && house->house_population == 0 && house->type == vacant_lot_fill_type();
}

static building_type split_type_for_house(building *house)
{
    Building *house_object = runtime_building(house);
    return house_object && house_object->type ?
        house_object->type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::SplitTo) :
        BUILDING_NONE;
}

void building_house_change_to(Building house_object, building_type type)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    int should_reseed_graphics = is_empty_vacant_lot(house);
    if (housing_level_for_type(type) < 0) {
        return;
    }
    house_object.change_type(type);
    set_house_legacy_level_from_type(house, house->type);
    if (house_object.type && house_object.type->has_housing()) {
        int size = house_object.type->declared_model_size();
        if (size > 0) {
            unsigned char model_size = static_cast<unsigned char>(size);
            house->size = house->house_size = model_size;
            house->house_is_merged = static_cast<unsigned char>(size > 1 ? 1 : 0);
        }
    }
    // Vacant lots do not carry a meaningful house visual variant, so first
    // occupation gets a fresh stable choice. Other transitions preserve it.
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(house)) {
        runtime->assign_graphic_variant(should_reseed_graphics);
    }
    add_house_tiles(house_object);
}

static void create_vacant_lot(int x, int y)
{
    building_type type = vacant_lot_fill_type();
    if (type == BUILDING_NONE || housing_level_for_type(type) < 0) {
        return;
    }
    building *b = building_create(type, x, y);
    b->house_population = 0;
    set_house_legacy_level_from_type(b, type);
    b->distance_from_entry = 0;
    if (Building *building = runtime_building(b)) {
        map_building_tiles_add(*building, b->x, b->y, 1, building_image_get(building), TERRAIN_BUILDING);
    }
}

void building_house_change_to_vacant_lot(Building house_object)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    house->house_population = 0;
    building_type type = vacant_lot_fill_type();
    if (type == BUILDING_NONE || housing_level_for_type(type) < 0) {
        return;
    }
    house_object.change_type(type);
    set_house_legacy_level_from_type(house, type);
    if (house->house_is_merged) {
        if (Building *building = runtime_building(house)) {
            map_building_tiles_remove(building, house->x, house->y);
        }
        house->house_is_merged = 0;
        house->size = house->house_size = 1;
        house->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(house));
        if (Building *building = runtime_building(house)) {
            map_building_tiles_add(*building, house->x, house->y, 1, building_image_get(building), TERRAIN_BUILDING);
        }
        create_vacant_lot(house->x + 1, house->y);
        create_vacant_lot(house->x, house->y + 1);
        create_vacant_lot(house->x + 1, house->y + 1);
    } else {
        map_image_set(house->grid_offset, building_image_get(&house_object));
    }
}

struct HouseMergePlan {
    building_type type = BUILDING_NONE;
    int x = 0;
    int y = 0;
    int size = 0;
    int merged = 0;
    Building *source = nullptr;
    std::vector<Building *> participants;
    int inventory[RESOURCE_SLOT_COUNT] = {};
    int population = 0;
    int happiness_weight = 0;
};

enum class HouseExpandMode {
    HousesOnly,
    ClearTerrain,
    Gardens,
};

static Building *building_at_tile(int grid_offset)
{
    return map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
}

static int plan_has_participant(const HouseMergePlan &plan, unsigned int building_id)
{
    for (const Building *participant : plan.participants) {
        if (participant && participant->id == building_id) {
            return 1;
        }
    }
    return 0;
}

static void add_participant(HouseMergePlan &plan, Building *house)
{
    if (!house || !house->id || plan_has_participant(plan, house->id)) {
        return;
    }
    plan.participants.push_back(house);
    plan.population += house->house_population();
    plan.happiness_weight += house->house_population() * house->house_happiness();
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        plan.inventory[r] += house->resource_amount(r);
    }
}

static int house_can_share_expand_footprint(Building source, Building *other)
{
    if (!other || !other->id) {
        return 0;
    }
    if (other->id == source.id) {
        return 1;
    }
    return other->is_in_use() && other->has_house_size() &&
        building_house_legacy_level(*other) <= building_house_legacy_level(source);
}

static int tile_can_expand_into(Building source, int tile_offset, HouseExpandMode mode)
{
    if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
        return house_can_share_expand_footprint(source, building_at_tile(tile_offset));
    }
    if (mode == HouseExpandMode::HousesOnly) {
        return 0;
    }
    if (!map_terrain_is(tile_offset, TERRAIN_NOT_CLEAR)) {
        return 1;
    }
    return mode == HouseExpandMode::Gardens &&
        !config_get(CONFIG_GP_CH_HOUSES_DONT_EXPAND_INTO_GARDENS) &&
        map_terrain_is(tile_offset, TERRAIN_GARDEN);
}

static int find_expand_origin(Building house, int num_tiles, int *out_x, int *out_y)
{
    const HouseExpandMode modes[] = {
        HouseExpandMode::HousesOnly,
        HouseExpandMode::ClearTerrain,
        HouseExpandMode::Gardens,
    };
    for (HouseExpandMode mode : modes) {
        for (int dir = 0; dir < MAX_DIR; dir++) {
            int base_offset = EXPAND_DIRECTION_DELTA[dir].offset + house.grid_offset();
            int ok_tiles = 0;
            for (int i = 0; i < num_tiles; i++) {
                if (tile_can_expand_into(house, base_offset + HOUSE_TILE_OFFSETS[i], mode)) {
                    ok_tiles++;
                }
            }
            if (ok_tiles == num_tiles) {
                int candidate_x = house.x() + EXPAND_DIRECTION_DELTA[dir].x;
                int candidate_y = house.y() + EXPAND_DIRECTION_DELTA[dir].y;
                if (!split_blocking_houses(house, candidate_x, candidate_y, num_tiles, 1)) {
                    continue;
                }
                *out_x = candidate_x;
                *out_y = candidate_y;
                return 1;
            }
        }
    }
    return 0;
}

static int tile_can_merge_into_2x2(Building source, int tile_offset)
{
    if (!map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
        return 0;
    }
    Building *other = building_at_tile(tile_offset);
    if (!other) {
        return 0;
    }
    if (other->id == source.id) {
        return 1;
    }
    return other->is_in_use() && other->has_house_size() &&
        building_house_legacy_level(*other) == building_house_legacy_level(source) &&
        !other->is_merged_house();
}

static int find_merge_origin(Building house, int *out_x, int *out_y)
{
    int valid_tiles = 0;
    for (int i = 0; i < 4; i++) {
        if (tile_can_merge_into_2x2(house, house.grid_offset() + HOUSE_TILE_OFFSETS[i])) {
            valid_tiles++;
        }
    }
    if (valid_tiles != 4) {
        return 0;
    }
    *out_x = house.x() + EXPAND_DIRECTION_DELTA[0].x;
    *out_y = house.y() + EXPAND_DIRECTION_DELTA[0].y;
    return 1;
}

static int collect_house_merge_plan(Building &source, building_type type, int x, int y, int size,
    int merged, int num_tiles, HouseMergePlan *plan)
{
    if (!source.id || type == BUILDING_NONE || !plan) {
        return 0;
    }
    *plan = {};
    plan->type = type;
    plan->x = x;
    plan->y = y;
    plan->size = size;
    plan->merged = merged;
    plan->source = &source;

    int base_offset = map_grid_offset(x, y);
    for (int i = 0; i < num_tiles; i++) {
        int tile_offset = base_offset + HOUSE_TILE_OFFSETS[i];
        if (!map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
            continue;
        }
        Building *participant = building_at_tile(tile_offset);
        if (!participant || !participant->id || !participant->is_in_use() || !participant->has_house_size()) {
            return 0;
        }
        add_participant(*plan, participant);
    }
    if (!plan_has_participant(*plan, source.id)) {
        add_participant(*plan, &source);
    }
    return !plan->participants.empty();
}

static void retarget_figures_for_house_merge(const HouseMergePlan &plan, Building &replacement)
{
    for (unsigned int figure_id = 1; figure_id < Figure::count(); figure_id++) {
        Figure *figure = Figure::get(figure_id);
        if (!figure || !figure->state) {
            continue;
        }
        for (Building *participant : plan.participants) {
            if (!participant) {
                continue;
            }
            const int was_immigrant = figure->immigrant_building && figure->immigrant_building->id == participant->id;
            const int retargeted = figure->retarget_building(*participant, replacement);
            if (retargeted) {
                if (was_immigrant && !replacement.immigrant_figure_id()) {
                    replacement.set_immigrant_figure_id(figure->id());
                }
                replacement.copy_house_figure_slot_from(*participant, figure->id());
            }
        }
    }
}

static unsigned int apply_house_merge_plan(const HouseMergePlan &plan)
{
    if (plan.type == BUILDING_NONE || !plan.source || !plan.source->id || plan.participants.empty() ||
        plan.size <= 0 || housing_level_for_type(plan.type) < 0) {
        return 0;
    }

    Building &source = *plan.source;
    Building &replacement = Building::create(plan.type, plan.x, plan.y);
    replacement.copy_house_data_from(source);
    if (!replacement.configure_house_replacement(plan.type, plan.x, plan.y, plan.size, plan.merged)) {
        replacement.retire_replaced_house();
        return 0;
    }
    replacement.set_house_population(plan.population);
    replacement.set_house_happiness(plan.population ? plan.happiness_weight / plan.population : source.house_happiness());
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        replacement.set_resource_amount(r, plan.inventory[r]);
    }
    int capacity = house_population_get_capacity(replacement);
    replacement.set_house_population_room(std::max(0, capacity - replacement.house_population()));

    retarget_figures_for_house_merge(plan, replacement);
    for (Building *participant : plan.participants) {
        if (participant) {
            building_local_workforce::replace_house(*participant, replacement);
        }
    }
    add_house_tiles(replacement);
    for (Building *participant : plan.participants) {
        if (participant) {
            participant->retire_replaced_house();
        }
    }
    return replacement.id;
}

unsigned int building_house_merge(Building house_object)
{
    if (!house_object.id || !house_object.has_house_size()) {
        return 0;
    }
    if (house_object.is_merged_house()) {
        return 0;
    }
    if (!config_get(CONFIG_GP_CH_ALL_HOUSES_MERGE)) {
        if ((map_random_get(house_object.grid_offset()) & 7) >= 5) {
            return 0;
        }
    }
    int x = 0;
    int y = 0;
    if (!find_merge_origin(house_object, &x, &y)) {
        return 0;
    }
    building_type merge_type = house_object.type ?
        house_object.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::MergeTo) :
        BUILDING_NONE;
    if (merge_type == BUILDING_NONE) {
        return 0;
    }
    HouseMergePlan plan;
    int merged_size = housing_model_size(merge_type);
    if (merged_size <= 0 || housing_level_for_type(merge_type) < 0) {
        return 0;
    }
    if (!collect_house_merge_plan(house_object, merge_type, x, y, merged_size, 1, 4, &plan)) {
        return 0;
    }
    game_undo_disable();
    return apply_house_merge_plan(plan);
}

int building_house_can_expand(Building house_object, int num_tiles)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return 0;
    }
    int x = 0;
    int y = 0;
    if (find_expand_origin(house_object, num_tiles, &x, &y)) {
        return split_blocking_houses(house_object, x, y, num_tiles, 1);
    }
    house->data.house.no_space_to_expand = 1;
    return 0;
}

static void create_splitted_house_tile(Building &source, building_type type,
    int x, int y, int population, const int *inventory)
{
    if (type == BUILDING_NONE || housing_level_for_type(type) < 0) {
        return;
    }
    if (!source.id) {
        return;
    }
    Building &house = Building::create(type, x, y);
    house.copy_house_data_from(source);
    house.set_house_population(population);
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        house.set_resource_amount(r, inventory[r]);
    }
    house.set_distance_from_entry(0);
    add_house_tiles(house);
}

static void split_size2(building *house, building_type new_type)
{
    if (new_type == BUILDING_NONE) {
        return;
    }
    if (housing_level_for_type(new_type) < 0) {
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

    if (Building *building = runtime_building(house)) {
        map_building_tiles_remove(building, house->x, house->y);
    }

    // main tile
    building_change_type(house, new_type);
    set_house_legacy_level_from_type(house, house->type);
    house->size = house->house_size = 1;
    house->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(house));
    house->house_is_merged = 0;
    house->house_population = static_cast<short>(population_per_tile + population_remainder);
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = static_cast<short>(inventory_per_tile[i] + inventory_remainder[i]);
    }
    house->distance_from_entry = 0;

    // the other tiles (new buildings)
    const building_type split_type = house->type;
    const int x = house->x;
    const int y = house->y;
    Building *source = runtime_building(house);
    if (!source) {
        return;
    }
    add_house_tiles(*source);
    create_splitted_house_tile(*source, split_type, x + 1, y, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x, y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x + 1, y + 1, population_per_tile, inventory_per_tile);
}

static void split_size3(building *house)
{
    building_type medium_insula_type = one_tile_medium_insula_type();
    if (medium_insula_type == BUILDING_NONE || housing_level_for_type(medium_insula_type) < 0) {
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

    if (Building *building = runtime_building(house)) {
        map_building_tiles_remove(building, house->x, house->y);
    }

    // main tile
    building_change_type(house, medium_insula_type);
    set_house_legacy_level_from_type(house, house->type);
    house->size = house->house_size = 1;
    house->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(house));
    house->house_is_merged = 0;
    house->house_population = static_cast<short>(population_per_tile + population_remainder);
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = static_cast<short>(inventory_per_tile[i] + inventory_remainder[i]);
    }
    house->distance_from_entry = 0;

    // the other tiles (new buildings)
    const building_type split_type = house->type;
    const int x = house->x;
    const int y = house->y;
    Building *source = runtime_building(house);
    if (!source) {
        return;
    }
    add_house_tiles(*source);
    create_splitted_house_tile(*source, split_type, x, y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x + 1, y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x + 2, y + 1, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x, y + 2, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x + 1, y + 2, population_per_tile, inventory_per_tile);
    create_splitted_house_tile(*source, split_type, x + 2, y + 2, population_per_tile, inventory_per_tile);
}

static int expansion_contains_house_footprint(int expansion_origin, int num_tiles, const building *house)
{
    if (!house || house->house_size <= 1) {
        return 1;
    }

    for (int y = 0; y < house->house_size; y++) {
        for (int x = 0; x < house->house_size; x++) {
            int house_tile = house->grid_offset + OFFSET(x, y);
            int contained = 0;
            for (int i = 0; i < num_tiles; i++) {
                if (expansion_origin + HOUSE_TILE_OFFSETS[i] == house_tile) {
                    contained = 1;
                    break;
                }
            }
            if (!contained) {
                return 0;
            }
        }
    }
    return 1;
}

static int split_blocking_houses(Building source, int x, int y, int num_tiles, int dry_run)
{
    const unsigned int source_id = source.id;
    int grid_offset = map_grid_offset(x, y);
    for (int i = 0; i < num_tiles; i++) {
        int tile_offset = grid_offset + HOUSE_TILE_OFFSETS[i];
        if (map_terrain_is(tile_offset, TERRAIN_BUILDING) && map_building_exists_at(tile_offset)) {
            building *other_house = const_cast<::building *>(map_building_at(tile_offset).record());
            if (other_house && other_house->id != source_id && other_house->house_size) {
                if (!expansion_contains_house_footprint(grid_offset, num_tiles, other_house)) {
                    return 0;
                }
                if (other_house->house_is_merged == 1) {
                    building_type split_type = split_type_for_house(other_house);
                    if (split_type == BUILDING_NONE || housing_level_for_type(split_type) < 0) {
                        return 0;
                    }
                    if (!dry_run) {
                        split_size2(other_house, split_type);
                    }
                } else if (other_house->house_size == 2) {
                    building_type split_type = one_tile_medium_insula_type();
                    if (split_type == BUILDING_NONE || housing_level_for_type(split_type) < 0) {
                        return 0;
                    }
                    if (!dry_run) {
                        split_size2(other_house, split_type);
                    }
                } else if (other_house->house_size == 3) {
                    building_type split_type = one_tile_medium_insula_type();
                    if (split_type == BUILDING_NONE || housing_level_for_type(split_type) < 0) {
                        return 0;
                    }
                    if (!dry_run) {
                        split_size3(other_house);
                    }
                }
            }
        }
    }
    return 1;
}

/// Runtime-size house transitions

int building_house_expand_to_type(Building house_object, building_type type)
{
    if (!house_object.id || !house_object.has_house_size()) {
        return 0;
    }
    int target_size = housing_model_size(type);
    if (target_size <= house_object.size()) {
        return 0;
    }

    int x = 0;
    int y = 0;
    int num_tiles = target_size * target_size;
    if (!find_expand_origin(house_object, num_tiles, &x, &y)) {
        ::building *house = const_cast<::building *>(house_object.record());
        if (house) {
            house->data.house.no_space_to_expand = 1;
        }
        return 0;
    }

    if (!split_blocking_houses(house_object, x, y, num_tiles, 1)) {
        return 0;
    }
    if (!split_blocking_houses(house_object, x, y, num_tiles, 0)) {
        return 0;
    }
    HouseMergePlan plan;
    if (!collect_house_merge_plan(house_object, type, x, y, target_size, 0, num_tiles, &plan)) {
        return 0;
    }
    return apply_house_merge_plan(plan) ? 1 : 0;
}

static void desize_house_to_type(building *house, building_type type)
{
    int target_size = housing_model_size(type);
    if (target_size <= 0 || target_size >= house->size || housing_level_for_type(type) < 0) {
        return;
    }

    if (Building *building = runtime_building(house)) {
        map_building_tiles_remove(building, house->x, house->y);
    }
    int road_tile_offset = find_best_corner_for_devolve(house->x, house->y, house->size, target_size);

    house->x = static_cast<unsigned char>(map_grid_offset_to_x(road_tile_offset));
    house->y = static_cast<unsigned char>(map_grid_offset_to_y(road_tile_offset));
    building_change_type(house, type);
    set_house_legacy_level_from_type(house, type);
    house->size = house->house_size = static_cast<unsigned char>(target_size);
    house->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(house));
    house->house_is_merged = 0;
    house->distance_from_entry = 0;

    Building *refreshed_house = runtime_building(house);
    if (!refreshed_house) {
        return;
    }
    add_house_tiles(*refreshed_house);
}

static void shrink_house_to_type(building *house, building_type type)
{
    int old_size = house->house_size;
    int target_size = housing_model_size(type);
    if (target_size <= 0 || target_size >= old_size || housing_level_for_type(type) < 0) {
        return;
    }
    int extra_tiles = old_size * old_size - target_size * target_size;
    if (extra_tiles <= 0) {
        if (Building *building = runtime_building(house)) {
            building_house_change_to(*building, type);
        }
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

    if (Building *building = runtime_building(house)) {
        map_building_tiles_remove(building, house->x, house->y);
    }

    building_change_type(house, type);
    set_house_legacy_level_from_type(house, type);
    house->size = house->house_size = static_cast<unsigned char>(target_size);
    house->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(house));
    house->house_is_merged = 0;
    house->house_population = static_cast<short>(population_per_tile + population_remainder);
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        house->resources[i] = static_cast<short>(inventory_per_tile[i] + inventory_remainder[i]);
    }
    house->distance_from_entry = 0;

    Building *refreshed_house = runtime_building(house);
    if (!refreshed_house) {
        return;
    }
    add_house_tiles(*refreshed_house);

    building_type extra_house_type = one_tile_medium_insula_type();
    if (extra_house_type == BUILDING_NONE || housing_level_for_type(extra_house_type) < 0) {
        return;
    }
    const int base_x = house->x;
    const int base_y = house->y;
    for (int y = 0; y < old_size; y++) {
        for (int x = 0; x < old_size; x++) {
            if (x < target_size && y < target_size) {
                continue;
            }
            create_splitted_house_tile(
                *refreshed_house,
                extra_house_type,
                base_x + x,
                base_y + y,
                population_per_tile,
                inventory_per_tile);
        }
    }
}

void building_house_devolve_to_type(Building house_object, building_type type)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    int current_size = house->house_size;
    int target_size = housing_model_size(type);
    int target_level = housing_level_for_type(type);
    if (target_size <= 0 || target_level < 0) {
        return;
    }

    int current_level = building_house_legacy_level(house_object);
    if (current_level == HOUSE_LARGE_INSULA && target_level == HOUSE_MEDIUM_INSULA &&
        current_size == 2 && target_size == 2) {
        building_type split_type = split_type_for_house(house);
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
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    int calc_grid_offset = map_grid_offset(house->x, house->y);
    house->data.house.no_space_to_expand = 0;
    if (house->grid_offset != calc_grid_offset ||
        !map_building_exists_at(house->grid_offset) ||
        map_building_at(house->grid_offset).id != house->id) {
        int map_width, map_height;
        map_grid_size(&map_width, &map_height);
        for (int y = 0; y < map_height; y++) {
            for (int x = 0; x < map_width; x++) {
                int grid_offset = map_grid_offset(x, y);
                if (map_building_exists_at(grid_offset) && map_building_at(grid_offset).id == house->id) {
                    house->grid_offset = static_cast<short>(grid_offset);
                    house->x = static_cast<unsigned char>(map_grid_offset_to_x(grid_offset));
                    house->y = static_cast<unsigned char>(map_grid_offset_to_y(grid_offset));
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
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    if (house->figure_id) {
        Figure *homeless = Figure::get(house->figure_id);
        if (homeless && homeless->building && homeless->building->id == house->id) {
            house->house_population = homeless->migrant_num_people;
            city_population_add_homeless(homeless->migrant_num_people);
            homeless->remove();
        }
    }
}
