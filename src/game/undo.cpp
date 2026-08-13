#include "building/connectable.h"
#include "building/construction.h"
#include "building/storage.h"
#include "figure/roamer_preview.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/image.h"
#include "scenario/earthquake.h"
#include "undo.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"

#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/population.h"
#include "core/image.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "map/property.h"
#include "figure/route.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/tile_runtime_api.h"
#include "map/water_navigation.h"

#include <algorithm>
#include <string.h>
#include <vector>

#define MAX_UNDO_BUILDINGS 50
#define MAX_UNDO_TYPE_CHANGES 50

static struct {
    int available;
    int ready;
    int timeout_ticks;
    int building_cost;
    int num_buildings;
    building_type type;
    building buildings[MAX_UNDO_BUILDINGS];
    std::vector<unsigned int> created_building_ids;
    std::vector<building> replaced_buildings;
    struct {
        int num;
        struct {
            unsigned int building_id;
            building_type original_type;
        } items[MAX_UNDO_TYPE_CHANGES];
    } type_changes;
} data;

int game_can_undo(void)
{
    return data.ready && data.available;
}

void game_undo_disable(void)
{
    data.available = 0;
}

void game_undo_add_building(building *b)
{
    if (b->id <= 0) {
        return;
    }
    data.num_buildings = 0;
    int is_on_list = 0;
    for (int i = 0; i < MAX_UNDO_BUILDINGS; i++) {
        if (data.buildings[i].id) {
            data.num_buildings++;
        }
        if (data.buildings[i].id == b->id) {
            is_on_list = 1;
        }
    }
    if (!is_on_list) {
        for (int i = 0; i < MAX_UNDO_BUILDINGS; i++) {
            if (!data.buildings[i].id) {
                data.num_buildings++;
                memcpy(&data.buildings[i], b, sizeof(building));
                return;
            }
        }
        data.available = 0;
    }
}

void game_undo_adjust_building(building *b)
{
    for (int i = 0; i < MAX_UNDO_BUILDINGS; i++) {
        if (data.buildings[i].id == b->id) {
            // found! update the building now
            memcpy(&data.buildings[i], b, sizeof(building));
        }
    }
}

int game_undo_contains_building(int building_id)
{
    if (building_id <= 0 || !game_can_undo()) {
        return 0;
    }
    if (data.num_buildings <= 0) {
        return 0;
    }
    for (int i = 0; i < MAX_UNDO_BUILDINGS; i++) {
        if (data.buildings[i].id == (unsigned int) building_id) {
            return 1;
        }
    }
    return 0;
}

static void clear_buildings(void)
{
    data.num_buildings = 0;
    memset(data.buildings, 0, MAX_UNDO_BUILDINGS * sizeof(building));
    data.type_changes.num = 0;
    data.created_building_ids.clear();
    data.replaced_buildings.clear();
}

void game_undo_add_created_building(building *b)
{
    if (!b || !b->id) {
        return;
    }
    if (std::find(data.created_building_ids.begin(), data.created_building_ids.end(), b->id) ==
        data.created_building_ids.end()) {
        data.created_building_ids.push_back(b->id);
    }
}

void game_undo_add_replaced_building(building *b)
{
    if (!b || !b->id) {
        return;
    }
    const auto existing = std::find_if(
        data.replaced_buildings.begin(), data.replaced_buildings.end(),
        [b](const building &saved) { return saved.id == b->id; });
    if (existing == data.replaced_buildings.end()) {
        data.replaced_buildings.push_back(*b);
    }
}

static building *undo_building_by_id(unsigned int building_id)
{
    Building *building = Building::get(building_id);
    return building ? const_cast<::building *>(building->record()) : nullptr;
}

static void discard_created_buildings()
{
    if (data.created_building_ids.empty()) {
        return;
    }
    for (unsigned int id : data.created_building_ids) {
        if (building *b = undo_building_by_id(id)) {
            b->state = BUILDING_STATE_UNDO;
        }
    }
    data.created_building_ids.clear();
    building_update_state();
}

static void restore_replaced_buildings()
{
    for (const building &saved : data.replaced_buildings) {
        building *record = undo_building_by_id(saved.id);
        if (!record) {
            continue;
        }
        record->state = saved.state;
        record->is_deleted = saved.is_deleted;
        if (Building *restored = Building::get(saved.id)) {
            if (restored->Foundation) {
                restored->Foundation->restore_unbound_ownership();
            }
        }
    }
    data.replaced_buildings.clear();
}

void game_undo_record_building_type(building *b)
{
    if (!b || b->id <= 0 || data.type_changes.num >= MAX_UNDO_TYPE_CHANGES) {
        return;
    }
    // Only record the first snapshot (before any change this build action)
    for (int i = 0; i < data.type_changes.num; i++) {
        if (data.type_changes.items[i].building_id == b->id) {
            return;
        }
    }
    data.type_changes.items[data.type_changes.num].building_id = b->id;
    data.type_changes.items[data.type_changes.num].original_type = b->type;
    data.type_changes.num++;
}

void game_undo_restore_building_types(void)
{
    for (int i = 0; i < data.type_changes.num; i++) {
        building *b = undo_building_by_id(data.type_changes.items[i].building_id);
        if (b && b->id > 0) {
            if (!building_change_type(b, data.type_changes.items[i].original_type)) {
                data.available = 0;
            }
        }
    }
    data.type_changes.num = 0;
}

int game_undo_start_build(building_type type)
{
    data.ready = 0;
    data.available = 1;
    data.timeout_ticks = 0;
    data.building_cost = 0;
    data.type = type;
    clear_buildings();
    Building::for_each([&](Building *building_object) {
        building *b = building_object ? const_cast<building *>(building_object->record()) : nullptr;
        if (!b) {
            return;
        }
        if (b->state == BUILDING_STATE_UNDO) {
            data.available = 0;
            return;
        }
        if (b->state == BUILDING_STATE_DELETED_BY_PLAYER) {
            data.available = 0;
        }
    });
    if (!data.available) {
        return 0;
    }

    map_image_backup();
    map_terrain_backup();
    map_aqueduct_backup();
    map_property_backup();
    map_sprite_backup();
    map_building_backup();
    building_runtime_backup_graphics_state();
    tile_runtime_backup();

    return 1;
}

void game_undo_set_build_type(building_type type)
{
    data.type = type;
}

void game_undo_restore_building_state(void)
{
    for (int i = 0; i < data.num_buildings; i++) {
        if (data.buildings[i].id) {
            building *b = undo_building_by_id(data.buildings[i].id);
            if (!b) {
                continue;
            }
            if (b->state == BUILDING_STATE_DELETED_BY_PLAYER) {
                b->state = BUILDING_STATE_IN_USE;
            }
            b->is_deleted = 0;
            if (Building *restored = Building::get(b->id)) {
                if (restored->Foundation) {
                    restored->Foundation->restore_unbound_ownership();
                }
            }
        }
    }
    clear_buildings();
}

static void restore_map_images(void)
{
    int map_width, map_height;
    map_grid_size(&map_width, &map_height);
    for (int y = 0; y < map_height; y++) {
        for (int x = 0; x < map_width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_building_exists_at(grid_offset) ||
                map_terrain_is(grid_offset, TERRAIN_AQUEDUCT) ||
                map_building_at(grid_offset).is_surface_terrain_tile()) {
                map_image_restore_at(grid_offset);
            }
        }
    }
}

void game_undo_restore_map(int include_properties)
{
    map_terrain_restore();
    map_aqueduct_restore();
    map_building_restore();
    if (include_properties) {
        map_property_restore();
    }
    restore_map_images();
    building_runtime_restore_graphics_state();
    tile_runtime_restore();
}

void game_undo_finish_build(int cost)
{
    data.ready = 1;
    data.timeout_ticks = 500;
    data.building_cost = cost;
    window_invalidate();
}

static Building *add_building_to_terrain(building *b)
{
    if (!b || b->id <= 0) {
        return nullptr;
    }
    b->state = BUILDING_STATE_IN_USE;
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return nullptr;
    }
    if (!runtime->building.refresh_graphic_if_native()) {
        runtime->building.add_map_tiles(0);
    }
    if (building_type_registry_impl::type_attr_is(b->type, "wharf")) {
        b->data.industry.fishing_boat_id = 0;
        b->data.industry.second_fishing_boat_id = 0;
    }
    return &runtime->building;
}

void game_undo_perform(void)
{
    if (!game_can_undo()) {
        return;
    }
    data.available = 0;
    city_finance_process_construction(-data.building_cost);
    restore_replaced_buildings();
    if (building_type_registry_impl::type_attr_is_any(data.type, {"clear_land", "clear_trees"})) {
        for (int i = 0; i < data.num_buildings; i++) {
            if (data.buildings[i].id) {
                building *b = building_restore_from_undo(&data.buildings[i]);
                Building *restored = add_building_to_terrain(b);
                if (!restored) {
                    continue;
                }
                if (restored->type && (restored->type->is_warehouse() || restored->type->is_granary())) {
                    if (!building_storage_restore(b->storage_id)) {
                        building_storage_reset_building_ids();
                    }
                } else if (building_type_registry_impl::type_attr_is(b->type, "triumphal_arch")) {
                    const building_type arch = b->type;
                    city_buildings_build_triumphal_arch();
                    building_menu_update();
                    if (building_construction_type() == arch &&
                        !building_menu_is_enabled(building_type_registry_impl::definition_for_type(arch))) {
                        building_construction_clear_type();
                    }
                }
                if (restored->Housing) {
                    Figure *homeless = b->figure_id ? Figure::get(b->figure_id) : nullptr;
                    if (homeless && homeless->building && homeless->building->id == b->id) {
                        restored->Housing->state().population = homeless->migrant_num_people;
                        city_population_add_homeless(homeless->migrant_num_people);
                        homeless->remove();
                    }
                }
            }
        }
        map_terrain_restore();
        map_aqueduct_restore();
        map_sprite_restore();
        map_image_restore();
        map_property_restore();
        map_building_restore();
        map_property_clear_constructing_and_deleted();
    } else if (building_type_registry_impl::type_attr_is_any(data.type, {"aqueduct", "road", "wall", "highway"})) {
        discard_created_buildings();
        map_terrain_restore();
        map_aqueduct_restore();
        restore_map_images();
        game_undo_restore_building_types();
        building_connectable_update_connections();

    } else if (building_type_registry_impl::type_attr_is_any(data.type, {"low_bridge", "ship_bridge"})) {
        map_terrain_restore();
        map_sprite_restore();
        restore_map_images();
    } else if (building_type_registry_impl::type_attr_is_any(data.type, {"plaza", "gardens", "overgrown_gardens"})) {
        map_terrain_restore();
        map_aqueduct_restore();
        map_property_restore();
        restore_map_images();
    } else if (data.num_buildings) {
        if (building_type_registry_impl::type_attr_is(data.type, "reservoir")) {
            map_terrain_restore();
            map_aqueduct_restore();
            restore_map_images();
        }
        for (int i = 0; i < data.num_buildings; i++) {
            if (data.buildings[i].id) {
                if (building *b = undo_building_by_id(data.buildings[i].id)) {
                    b->state = BUILDING_STATE_UNDO;
                }
            }
        }
        building_update_state();
    }
    building_runtime_restore_graphics_state();
    tile_runtime_restore();
    Route::updateLandTerrain();
    Route::updateWallTerrain();
    bool dock_changed = building_type_registry_impl::type_attr_is(data.type, "dock");
    bool bridge_changed = building_type_registry_impl::type_attr_is_any(
        data.type, {"low_bridge", "ship_bridge"});
    for (int index = 0; index < data.num_buildings; ++index) {
        const building_type restored_type = data.buildings[index].type;
        dock_changed = dock_changed ||
            building_type_registry_impl::type_attr_is(restored_type, "dock");
        bridge_changed = bridge_changed ||
            building_type_registry_impl::type_attr_is_any(restored_type, {"low_bridge", "ship_bridge"});
    }
    if (dock_changed) {
        water_navigation::invalidate_dock_endpoints();
    }
    if (bridge_changed) {
        water_navigation::invalidate_topology();
    }
    figure_roamer_preview_reset(building_construction_type());
    data.num_buildings = 0;
}

void game_undo_reduce_time_available(void)
{
    if (!game_can_undo()) {
        return;
    }
    if (data.timeout_ticks <= 0 || scenario_earthquake_is_in_progress()) {
        data.available = 0;
        clear_buildings();
        window_invalidate();
        return;
    }
    data.timeout_ticks--;
    if (building_type_registry_impl::type_attr_is_any(data.type, {
        "clear_land",
        "clear_trees",
        "aqueduct",
        "road",
        "highway",
        "wall",
        "low_bridge",
        "ship_bridge",
        "plaza",
        "gardens",
        "overgrown_gardens"
    })) {
        return;
    }
    if (data.num_buildings <= 0) {
        data.available = 0;
        window_invalidate();
        return;
    }
    if (data.type == building_type_registry_impl::vacant_lot_fill_type()) {
        for (int i = 0; i < data.num_buildings; i++) {
            Building *building = Building::get(data.buildings[i].id);
            if (building && building->Housing && building->Housing->state().population) {
                // no undo on a new house where people moved in
                data.available = 0;
                window_invalidate();
                return;
            }
        }
    }
    for (int i = 0; i < data.num_buildings; i++) {
        if (data.buildings[i].id) {
            building *b = undo_building_by_id(data.buildings[i].id);
            if (!b) {
                data.available = 0;
                window_invalidate();
                return;
            }
            if (b->state == BUILDING_STATE_UNDO ||
                b->state == BUILDING_STATE_RUBBLE ||
                b->state == BUILDING_STATE_DELETED_BY_GAME) {
                data.available = 0;
                window_invalidate();
                return;
            }
            if (b->type != data.buildings[i].type || b->grid_offset != data.buildings[i].grid_offset) {
                data.available = 0;
                window_invalidate();
                return;
            }
        }
    }
}
