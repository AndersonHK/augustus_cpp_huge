#include "building/building.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/construction_area_tile.h"
#include "building/building_type_registry_internal.h"
#include "building/connectable.h"
#include "building/construction_building.h"
#include "building/construction_plan.h"
#include "building/construction_clear.h"
#include "building/construction_routed.h"
#include "building/construction_warning.h"
#include "building/count.h"
#include "building/rotation.h"
#include "building/tool_mode.h"
#include "building/variant.h"
#include "city/warning.h"
#include "construction.h"
#include "game/undo.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/image.h"
#include "map/tiles.h"
#include "map/water.h"
#include "building/water_access_runtime.h"

#include "building/construction_session.h"
#include "figure/figure.h"
#include "game/performance_tracker.h"
#include "translation/translation.h"

#include <algorithm>
#include <cstring>
#include <vector>


#include "assets/assets.h"
#include "building/building_record.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/finance.h"
#include "city/resource.h"
#include "city/view.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/formation.h"
#include "graphics/window.h"
#include "input/hotkey.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/point.h"
#include "map/property.h"
#include "figure/route.h"
#include "map/terrain.h"
#include "scenario/allowed_building.h"

struct reservoir_info {
    int cost;
    int place_reservoir_at_start;
    int place_reservoir_at_end;
};

struct reservoir_endpoint {
    int center_x;
    int center_y;
    int origin_x;
    int origin_y;
    int origin_grid_offset;
};

enum {
    PLACE_RESERVOIR_BLOCKED = -1,
    PLACE_RESERVOIR_NO = 0,
    PLACE_RESERVOIR_YES = 1,
    PLACE_RESERVOIR_EXISTS = 2
};

static struct {
    ConstructionToolSession tool;
    int in_progress;
    int cost_preview;
    int force_place_clear_cost;
    int can_place;
    int has_reservoir_aqueduct_preview_route;
    building_type reservoir_aqueduct_preview_type;
    grid_slice reservoir_aqueduct_preview_route;
    struct {
        int meadow;
        int rock;
        int tree;
        int water;
        int wall;
        int distant_water;
    } required_terrain;
    int draw_as_constructing;
    int start_offset_x_view;
    int start_offset_y_view;
    int cycle_step;
    int auto_cycling;
} data;

static int last_items_cleared;

static void clear_reservoir_aqueduct_preview_route(void)
{
    data.has_reservoir_aqueduct_preview_route = 0;
    data.reservoir_aqueduct_preview_type = BUILDING_NONE;
    data.reservoir_aqueduct_preview_route.size = 0;
}

static reservoir_endpoint reservoir_endpoint_from_center(int center_x, int center_y)
{
    reservoir_endpoint endpoint = {};
    endpoint.center_x = center_x;
    endpoint.center_y = center_y;
    endpoint.origin_x = center_x - 1;
    endpoint.origin_y = center_y - 1;
    endpoint.origin_grid_offset = map_grid_offset(endpoint.origin_x, endpoint.origin_y);
    return endpoint;
}

static int reservoir_endpoint_has_existing_reservoir(const reservoir_endpoint &endpoint)
{
    return map_building_is_reservoir(endpoint.origin_x, endpoint.origin_y);
}

static int building_type_allows_out_of_money_construction(building_type type)
{
    const building_type_registry_impl::BuildingType &definition =
        *building_type_registry_impl::definition_for_type(type);
    int limit = definition.construction().free_when_broke_limit();
    return limit > 0 && building_count_total(type) < limit;
}

static const building_type_registry_impl::ConstructionToolDefinition &construction_tool_for_type(building_type type)
{
    static const building_type_registry_impl::ConstructionToolDefinition empty_tool;
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition ? definition->tool() : empty_tool;
}

static int draggable_tool_rotation(
    building_type type,
    const building_type_registry_impl::ConstructionToolDefinition &tool)
{
    switch (tool.drag_rotation()) {
        case building_type_registry_impl::ConstructionDragRotation::Path:
            return building_rotation_get_rotation_with_limit(BUILDING_CONNECTABLE_ROTATION_LIMIT_PATHS);
        case building_type_registry_impl::ConstructionDragRotation::Hedge:
            return building_rotation_get_rotation_with_limit(BUILDING_CONNECTABLE_ROTATION_LIMIT_HEDGES);
        case building_type_registry_impl::ConstructionDragRotation::Variant: {
            const int variants = building_variant_get_number_of_variants(type);
            return variants > 0 ? building_rotation_get_rotation_with_limit(variants) : 0;
        }
        case building_type_registry_impl::ConstructionDragRotation::Pair:
            return building_rotation_get_rotation() % 2;
        case building_type_registry_impl::ConstructionDragRotation::None:
        default:
            return 0;
    }
}

static void construction_requirements_snapshot(
    const building_type_registry_impl::ConstructionDefinition &construction,
    int available[RESOURCE_SLOT_COUNT])
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
         resource = static_cast<resource_type>(resource + 1)) {
        if (construction.instant_requirement_amount(resource) > 0) {
            available[resource] = city_resource_count_warehouses_amount(resource);
        }
    }
}

static int construction_requirements_available(
    const building_type_registry_impl::ConstructionDefinition &construction,
    int available[RESOURCE_SLOT_COUNT])
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
         resource = static_cast<resource_type>(resource + 1)) {
        int amount = construction.instant_requirement_amount(resource);
        if (amount > 0 && available[resource] < amount) {
            return 0;
        }
    }
    return 1;
}

static void construction_requirements_reserve(
    const building_type_registry_impl::ConstructionDefinition &construction,
    int available[RESOURCE_SLOT_COUNT])
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
         resource = static_cast<resource_type>(resource + 1)) {
        int amount = construction.instant_requirement_amount(resource);
        if (amount > 0) {
            available[resource] -= amount;
        }
    }
}

static void construction_requirements_remove(
    const building_type_registry_impl::ConstructionDefinition &construction)
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
         resource = static_cast<resource_type>(resource + 1)) {
        int amount = construction.instant_requirement_amount(resource);
        if (amount > 0) {
            building_warehouses_remove_resource(resource, amount);
        }
    }
}

static resource_type construction_missing_requirement(
    const building_type_registry_impl::ConstructionDefinition &construction)
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
         resource = static_cast<resource_type>(resource + 1)) {
        int amount = construction.instant_requirement_amount(resource);
        if (amount > 0 && city_resource_count_warehouses_amount(resource) < amount) {
            return resource;
        }
    }
    return RESOURCE_NONE;
}

static int is_vacant_lot_type(building_type type)
{
    building_type vacant_lot = building_type_registry_impl::vacant_lot_fill_type();
    return vacant_lot != BUILDING_NONE && type == vacant_lot;
}

static void set_required_terrain(building_type type)
{
    data.required_terrain.wall = 0;
    data.required_terrain.water = 0;
    data.required_terrain.tree = 0;
    data.required_terrain.rock = 0;
    data.required_terrain.meadow = 0;
    data.required_terrain.distant_water = 0;

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    const unsigned int required_terrain = foundation ? foundation->site_requirements() : 0;
    if (required_terrain) {
        data.required_terrain.meadow = (required_terrain & building_type_registry_impl::FOUNDATION_SITE_MEADOW) != 0;
        data.required_terrain.rock = (required_terrain & building_type_registry_impl::FOUNDATION_SITE_ROCK) != 0;
        data.required_terrain.tree = (required_terrain & building_type_registry_impl::FOUNDATION_SITE_TREE) != 0;
        data.required_terrain.water = (required_terrain & building_type_registry_impl::FOUNDATION_SITE_WATER) != 0;
        data.required_terrain.wall = (required_terrain & building_type_registry_impl::FOUNDATION_SITE_WALL) != 0;
        data.required_terrain.distant_water =
            (required_terrain & building_type_registry_impl::FOUNDATION_SITE_DISTANT_WATER) != 0;
        return;
    }

    data.required_terrain.wall = foundation && foundation->requires_terrain(TERRAIN_WALL);
}

static void sync_construction_type(int construction_in_progress)
{
    building_type old_type = data.tool.type;
    if (!data.tool.sync_type(hotkey_get_modifiers(), construction_in_progress)) {
        return;
    }
    if (data.tool.type != old_type) {
        building_rotation_remove_rotation();
    }
    set_required_terrain(data.tool.type);
    data.cost_preview = 0;
    data.force_place_clear_cost = 0;
    data.can_place = 0;
    if (data.in_progress) {
        game_undo_set_build_type(data.tool.type);
    }
}

int building_construction_is_land_work_type(building_type type)
{
    return construction_tool_for_type(type).is_land_work();
}

int building_construction_force_place_active(void)
{
    if (!(hotkey_get_modifiers() & KEY_MOD_SHIFT)) {
        return 0;
    }

    const building_type type = building_construction_type();
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (type == BUILDING_NONE || is_vacant_lot_type(type) ||
        ConstructionAreaTilePlacement::is_area_tile_type(type) ||
        (definition && definition->bridge().is_bridge()) ||
        (definition && definition->foundation_def() &&
            definition->foundation_def()->requires_terrain(TERRAIN_WALL)) ||
        (definition && definition->foundation_def() &&
            definition->foundation_def()->adds_terrain(TERRAIN_GATEHOUSE)) ||
        construction_tool_for_type(type).has_any()) {
        return 0;
    }
    return !building_construction_is_updatable();
}

struct construction_cycle_entry {
    building_type type;
    int order;
};

static std::vector<construction_cycle_entry> construction_cycle_entries_for_type(building_type type, int *steps)
{
    std::vector<construction_cycle_entry> entries;
    const building_type_registry_impl::BuildingType *source =
        building_type_registry_impl::definition_for_type(type);
    if (!source || !source->has_cycle()) {
        return entries;
    }

    const building_type_registry_impl::ConstructionCycleDefinition &cycle = source->cycle();
    if (steps) {
        *steps = cycle.steps();
    }
    for (const auto &definition : building_type_registry_impl::g_building_types) {
        if (!definition || !definition->has_cycle() ||
            std::strcmp(definition->cycle().group(), cycle.group()) != 0) {
            continue;
        }
        entries.push_back({ definition->type(), definition->cycle().order() });
    }

    std::sort(entries.begin(), entries.end(), [](const construction_cycle_entry &left,
        const construction_cycle_entry &right) {
        if (left.order != right.order) {
            return left.order < right.order;
        }
        return left.type < right.type;
    });
    return entries;
}

static unsigned int count_enabled_buildings_for_cycling(const std::vector<construction_cycle_entry> &entries)
{
    unsigned int count = 0;
    for (const construction_cycle_entry &entry : entries) {
        count += scenario_allowed_building(building_type_registry_impl::definition_for_type(entry.type)) ? 1 : 0;
    }
    return count;
}

int building_construction_type_can_cycle(building_type type)
{
    std::vector<construction_cycle_entry> entries = construction_cycle_entries_for_type(type, nullptr);
    if (entries.empty()) {
        return 0;
    }
    for (const construction_cycle_entry &entry : entries) {
        if (entry.type == type) {
            return count_enabled_buildings_for_cycling(entries) > 1;
        }
    }
    return 0;
}

int building_construction_type_num_cycles(building_type type)
{
    int steps = 1;
    std::vector<construction_cycle_entry> entries = construction_cycle_entries_for_type(type, &steps);
    if (entries.empty()) {
        return 1;
    }
    return count_enabled_buildings_for_cycling(entries) * steps;
}

int building_construction_type_cycle_steps(building_type type)
{
    int steps = 1;
    construction_cycle_entries_for_type(type, &steps);
    return steps;
}

int building_construction_cycle_forward(void)
{
    if (data.tool.type == BUILDING_NONE) {
        return 0;
    }

    int steps = 1;
    std::vector<construction_cycle_entry> entries =
        construction_cycle_entries_for_type(building_construction_type(), &steps);
    const int size = static_cast<int>(entries.size());
    for (int j = 0; j < size; j++) {
        if (entries[j].type == building_construction_type()) {
            data.cycle_step += 1;
            if (data.cycle_step < steps) {
                return 0;
            }
            data.cycle_step = 0;
            for (int attempts = 0; attempts < size; attempts++) {
                j = (j + 1) % size;
                building_type new_type = entries[j].type;
                if (scenario_allowed_building(building_type_registry_impl::definition_for_type(new_type))) {
                    data.tool.force_type(new_type);
                    return 1;
                }
            }
            return 0;
        }
    }
    return 0;
}

int building_construction_cycle_back(void)
{
    if (data.tool.type == BUILDING_NONE) {
        return 0;
    }

    int steps = 1;
    std::vector<construction_cycle_entry> entries =
        construction_cycle_entries_for_type(building_construction_type(), &steps);
    const int size = static_cast<int>(entries.size());
    for (int j = 0; j < size; j++) {
        if (entries[j].type == building_construction_type()) {
            data.cycle_step -= 1;
            if (data.cycle_step >= 0) {
                return 0;
            }
            data.cycle_step = steps - 1;
            for (int attempts = 0; attempts < size; attempts++) {
                j = j - 1 < 0 ? size - 1 : j - 1;
                building_type new_type = entries[j].type;
                if (scenario_allowed_building(building_type_registry_impl::definition_for_type(new_type))) {
                    data.tool.force_type(new_type);
                    return 1;
                }
            }
            return 0;
        }
    }
    return 0;
}

int building_construction_is_auto_cycling(void)
{
    return data.auto_cycling;
}

void building_construction_toggle_auto_cycle(void)
{
    data.auto_cycling ^= 1;
}

static void mark_construction(int x, int y, int size, int terrain, int absolute_xy)
{
    if (map_building_tiles_mark_construction(x, y, size, terrain, absolute_xy)) {
        data.draw_as_constructing = 1;
    }
}

static void mark_placement_construction(
    const building_construction::ConstructionPlacementPlan &placement,
    int terrain)
{
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        for (const building_construction::ConstructionPlacementTile &tile : part.tiles) {
            mark_construction(tile.x, tile.y, 1, terrain, 1);
        }
    }
}

static int place_houses(int measure_only, int x_start, int y_start, int x_end, int y_end)
{
    int x_min, x_max, y_min, y_max;
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min, &y_min, &x_max, &y_max);

    const building_type vacant_lot_type = building_type_registry_impl::vacant_lot_fill_type();
    const building_type_registry_impl::BuildingType *vacant_lot =
        building_type_registry_impl::definition_for_type(vacant_lot_type);
    if (!vacant_lot) {
        return 0;
    }

    int needs_road_warning = 0;
    int items_placed = 0;
    game_undo_restore_building_state();
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                continue;
            }
            if (measure_only) {
                building_construction_assessment assessment =
                    building_construction_assess_placement(*vacant_lot, x, y, 1, 0);
                if (!assessment.can_place) {
                    continue;
                }
                mark_placement_construction(assessment.placement, TERRAIN_ALL);
                items_placed++;
            } else {
                if (building_construction_place_building(vacant_lot_type, x, y, 1)) {
                    items_placed++;
                    if (!map_terrain_exists_tile_in_radius_with_type(x, y, 1, 2, TERRAIN_ROAD)) {
                        needs_road_warning = 1;
                    }
                }
            }
        }
    }
    if (!measure_only) {
        building_construction_warning_check_food_stocks(vacant_lot_type);
        if (needs_road_warning) {
            city_warning_show_translated(WARNING_HOUSE_TOO_FAR_FROM_ROAD);
        }
        Route::updateLandTerrain();
        window_invalidate();
    }
    return items_placed;
}

static int place_area_tile(int x_start, int y_start, int x_end, int y_end, building_type type, int preview)
{
    return ConstructionAreaTilePlacement(x_start, y_start, x_end, y_end, type, preview != 0).place();
}

static int place_wall(int x_start, int y_start, int x_end, int y_end, int measure_only, int construction_mode, building_type wall_type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(wall_type);
    if (!definition || !definition->tool().is_wall()) {
        return 0;
    }

    if (construction_mode) {
        game_undo_restore_map(0); // map_tiles_set_wall places wall terrain, even during preview.
        //the restoration is done to go back to the terrain state before measuring.
        //It's not needed if not using regular construction mode, e.g. repairs
    }
    int x_min, y_min, x_max, y_max;
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min, &y_min, &x_max, &y_max);

    int items_placed = 0;
    int resource_blocked_placement = 0;
    int available_resources[RESOURCE_SLOT_COUNT] = { 0 };
    const building_type_registry_impl::ConstructionDefinition &construction = definition->construction();
    construction_requirements_snapshot(construction, available_resources);
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                if (!construction_requirements_available(construction, available_resources)) {
                    resource_blocked_placement = 1;
                    continue;
                }
                construction_requirements_reserve(construction, available_resources);
                items_placed++;
                if (measure_only) {
                    map_tiles_set_wall(x, y);
                } else {
                    Building &wall = city_building_runtime().create(*definition, x, y);
                    wall.add_map_tiles();
                    wall.refresh_graphic();
                    map_tiles_set_wall(x, y);
                    map_property_clear_multi_tile_xy(grid_offset);
                    construction_requirements_remove(construction);
                }
            }
        }
    }
    Route::updateLandTerrain();
    Route::updateWallTerrain();
    map_tiles_update_all_walls();
    if (!measure_only && items_placed == 0 && resource_blocked_placement) {
        resource_type missing_resource = construction_missing_requirement(construction);
        if (missing_resource != RESOURCE_NONE) {
            building_construction_warning_show_missing_resource(missing_resource);
        }
    }
    return items_placed;
}

int building_construction_place_wall(int grid_offset)
{
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    if (map_has_figure_at(grid_offset)) {
        return 0;
    }
    return place_wall(x, y, x, y, 0, 0, data.tool.type);
}

static int plot_draggable_building(int x_start, int y_start, int x_end, int y_end, int allow_roads)
{
    int x_min, y_min, x_max, y_max;
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min, &y_min, &x_max, &y_max);
    map_image_restore();
    map_image_backup();

    int terrain = TERRAIN_NOT_CLEAR;
    if (allow_roads) {
        terrain = TERRAIN_NOT_CLEAR_EXCEPT_ROAD;
    }

    int items_placed = 0;
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, terrain)) {
                map_property_mark_constructing(grid_offset);
                items_placed++;
                continue;
            }
        }
    }
    return items_placed;
}

static int place_draggable_building(int x_start, int y_start, int x_end, int y_end, building_type type, int rotation)
{
    int x_min, y_min, x_max, y_max;
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min, &y_min, &x_max, &y_max);
    map_image_restore();

    int items_placed = 0;
    int gates_placed = 0;
    building_type gate_type = static_cast<building_type>(building_connectable_gate_type(type));
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    const building_type_registry_impl::BuildingType *gate_definition =
        gate_type == BUILDING_NONE ? nullptr : building_type_registry_impl::definition_for_type(gate_type);
    if (!definition || (gate_type != BUILDING_NONE && !gate_definition)) {
        return 0;
    }

    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                items_placed++;
                Building &building_obj = city_building_runtime().create(*definition, x, y);
                building *b = const_cast<building *>(building_obj.record());
                if (building_variant_has_variants(type)) {
                    building_obj.runtime_instance()->set_graphics_variant(
                        building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(b->type)));
                } else {
                    b->subtype.orientation = static_cast<short>(rotation);
                }
                game_undo_add_building(b);
                building_obj.add_map_tiles();
                building_obj.refresh_graphic();
            } else if (!map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR_EXCEPT_ROAD)) {
                if (gate_definition) {
                    items_placed++;
                    gates_placed++;
                    Building &building_obj = city_building_runtime().create(*gate_definition, x, y);
                    building *b = const_cast<building *>(building_obj.record());
                    if (building_variant_has_variants(gate_type)) {
                        building_obj.runtime_instance()->set_graphics_variant(
                            building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(b->type)));
                    } else {
                        b->subtype.orientation = static_cast<short>(rotation);
                    }
                    game_undo_add_building(b);
                    building_obj.add_map_tiles();
                    building_obj.refresh_graphic();
                    map_terrain_add(grid_offset, TERRAIN_ROAD);
                }
            }
        }
    }

    if (building_is_connectable(type)) {
        map_property_clear_constructing_and_deleted();
        building_connectable_update_connections();
        if (gates_placed) {
            map_tiles_update_all_roads();
            map_tiles_update_all_plazas();
        }
    }

    Route::updateLandTerrain();
    return items_placed;
}

static int place_reservoir_and_aqueducts(
    int measure_only,
    int x_start,
    int y_start,
    int x_end,
    int y_end,
    building_type reservoir_type,
    building_type aqueduct_type,
    struct reservoir_info *info,
    grid_slice *aqueduct_route = nullptr)
{
    info->cost = 0;
    info->place_reservoir_at_start = PLACE_RESERVOIR_NO;
    info->place_reservoir_at_end = PLACE_RESERVOIR_NO;
    if (aqueduct_route) {
        aqueduct_route->size = 0;
    }

    game_undo_restore_map(0);
    int distance = calc_maximum_distance(x_start, y_start, x_end, y_end);
    if (reservoir_type == BUILDING_NONE || aqueduct_type == BUILDING_NONE) {
        return 0;
    }
    if (measure_only && !data.in_progress) {
        distance = 0;
    }
    const reservoir_endpoint start_reservoir = reservoir_endpoint_from_center(x_start, y_start);
    const reservoir_endpoint end_reservoir = reservoir_endpoint_from_center(x_end, y_end);
    if (distance > 0) {
        if (map_building_is_reservoir(start_reservoir.origin_x, start_reservoir.origin_y)) {
            info->place_reservoir_at_start = PLACE_RESERVOIR_EXISTS;
        } else if (map_tiles_are_clear_with_terrain_exception(start_reservoir.origin_x, start_reservoir.origin_y, 3,
            TERRAIN_NOT_CLEAR, TERRAIN_AQUEDUCT, 1)) {
            info->place_reservoir_at_start = PLACE_RESERVOIR_YES;
        } else {
            info->place_reservoir_at_start = PLACE_RESERVOIR_BLOCKED;
        }
    }
    if (map_building_is_reservoir(end_reservoir.origin_x, end_reservoir.origin_y)) {
        info->place_reservoir_at_end = PLACE_RESERVOIR_EXISTS;
    } else if (map_tiles_are_clear_with_terrain_exception(end_reservoir.origin_x, end_reservoir.origin_y, 3,
        TERRAIN_NOT_CLEAR, TERRAIN_AQUEDUCT, 1)) {
        info->place_reservoir_at_end = PLACE_RESERVOIR_YES;
    } else {
        info->place_reservoir_at_end = PLACE_RESERVOIR_BLOCKED;
    }
    if (info->place_reservoir_at_start == PLACE_RESERVOIR_BLOCKED
        || info->place_reservoir_at_end == PLACE_RESERVOIR_BLOCKED) {
        return 0;
    }
    if (info->place_reservoir_at_start == PLACE_RESERVOIR_YES
        && info->place_reservoir_at_end == PLACE_RESERVOIR_YES && distance < 3) {
        return 0;
    }
    if (!distance) {
        if (info->place_reservoir_at_end == PLACE_RESERVOIR_YES) {
            info->cost = model_get_building(reservoir_type)->cost;
        }
        return 1;
    }
    {
        PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_CONSTRUCTION);
        if (!Route::calculateConstructionDistances(RoutePolicyKind::ConstructionAqueduct, { x_start, y_start })) {
            return 0;
        }
    }
    int terrain_mask = TERRAIN_NOT_CLEAR & ~TERRAIN_AQUEDUCT & ~TERRAIN_BUILDING;
    if (info->place_reservoir_at_start != PLACE_RESERVOIR_NO) {
        Route::blockDistanceArea(start_reservoir.origin_x, start_reservoir.origin_y, 3);
        mark_construction(start_reservoir.origin_x, start_reservoir.origin_y, 3, terrain_mask, 1);
    }
    if (info->place_reservoir_at_end != PLACE_RESERVOIR_NO) {
        Route::blockDistanceArea(end_reservoir.origin_x, end_reservoir.origin_y, 3);
        mark_construction(end_reservoir.origin_x, end_reservoir.origin_y, 3, terrain_mask, 1);
    }
    const int aqueduct_offsets_x[] = { 0, 2, 0, -2 };
    const int aqueduct_offsets_y[] = { -2, 0, 2, 0 };
    int min_dist = 10000;
    int min_dir_start = 0, min_dir_end = 0;
    for (int dir_start = 0; dir_start < 4; dir_start++) {
        int dx_start = aqueduct_offsets_x[dir_start];
        int dy_start = aqueduct_offsets_y[dir_start];
        for (int dir_end = 0; dir_end < 4; dir_end++) {
            int dx_end = aqueduct_offsets_x[dir_end];
            int dy_end = aqueduct_offsets_y[dir_end];
            int dist;
            if (building_construction_place_aqueduct_for_reservoir(1, aqueduct_type,
                x_start + dx_start, y_start + dy_start, x_end + dx_end, y_end + dy_end, &dist)) {
                if (dist && dist < min_dist) {
                    min_dist = dist;
                    min_dir_start = dir_start;
                    min_dir_end = dir_end;
                }
            }
        }
    }
    if (min_dist == 10000) {
        return 0;
    }
    int x_aq_start = aqueduct_offsets_x[min_dir_start];
    int y_aq_start = aqueduct_offsets_y[min_dir_start];
    int x_aq_end = aqueduct_offsets_x[min_dir_end];
    int y_aq_end = aqueduct_offsets_y[min_dir_end];
    int aq_items;
    building_construction_place_aqueduct_for_reservoir(measure_only, aqueduct_type, x_start + x_aq_start, y_start + y_aq_start,
        x_end + x_aq_end, y_end + y_aq_end, &aq_items, aqueduct_route);
    if (info->place_reservoir_at_start == PLACE_RESERVOIR_YES) {
        info->cost += model_get_building(reservoir_type)->cost;
    }
    if (info->place_reservoir_at_end == PLACE_RESERVOIR_YES) {
        info->cost += model_get_building(reservoir_type)->cost;
    }
    if (aq_items) {
        info->cost += aq_items * model_get_building(aqueduct_type)->cost;
    }
    return 1;
}

static int place_normalized_reservoir(
    building_type reservoir_type,
    const reservoir_endpoint &endpoint)
{
    if (reservoir_endpoint_has_existing_reservoir(endpoint)) {
        return 1;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(reservoir_type);
    if (!definition ||
        !building_construction_show_placement_warning(*definition, endpoint.origin_x, endpoint.origin_y, 1, 0)) {
        return 0;
    }
    return building_construction_place_building(reservoir_type, endpoint.origin_x, endpoint.origin_y, 1);
}

void building_construction_set_cost(int cost)
{
    data.cost_preview = cost;
    data.force_place_clear_cost = 0;
}

void building_construction_set_force_place_clear_cost(int cost)
{
    data.force_place_clear_cost = cost;
}

int building_construction_can_rotate(void)
{
    return building_rotation_type_has_rotations(data.tool.type);
}

void building_construction_set_type(const building_type_registry_impl::BuildingType *type, int setup_rotation)
{
    const building_type old_type = data.tool.type;
    data.tool.select_requested_type(type ? type->type() : BUILDING_NONE, hotkey_get_modifiers());
    if (data.tool.type != old_type) {
        building_rotation_remove_rotation();
    }
    data.in_progress = 0;
    data.cost_preview = 0;
    data.force_place_clear_cost = 0;
    data.can_place = 0;
    clear_reservoir_aqueduct_preview_route();

    if (data.tool.type != BUILDING_NONE) {
        set_required_terrain(data.tool.type);
    }
    if (building_construction_can_rotate()) {
        building_rotation_setup_rotation(setup_rotation);
    }
}

void building_construction_clear_type(void)
{
    data.cost_preview = 0;
    data.force_place_clear_cost = 0;
    data.can_place = 0;
    clear_reservoir_aqueduct_preview_route();
    data.tool.clear();
    data.in_progress = 0;
    building_rotation_remove_rotation();
}

building_type building_construction_type(void)
{
    sync_construction_type(data.in_progress);
    return data.tool.type;
}

building_type building_construction_selection_type(void)
{
    if (data.tool.selected_type) {
        return data.tool.selected_type;
    }
    return data.tool.type;
}

void building_construction_set_hover_tile(int x, int y, int grid_offset)
{
    if (data.in_progress || !grid_offset) {
        return;
    }
    data.tool.set_raw_start(x, y, grid_offset);
    sync_construction_type(data.in_progress);
    data.tool.sync_drag_points(hotkey_get_modifiers());
    data.can_place = 1;
}

int building_construction_cost(void)
{
    return data.cost_preview;
}

int building_construction_force_place_clear_cost(void)
{
    return data.force_place_clear_cost;
}

int building_construction_can_place(void)
{
    if (construction_tool_for_type(data.tool.type).is_road()) {
        return data.can_place;
    }
    return building_construction_is_land_work_type(data.tool.type) || data.can_place;
}

int building_construction_size(int *x, int *y)
{
    if (!config_get(CONFIG_UI_SHOW_CONSTRUCTION_SIZE) ||
        !building_construction_is_updatable() || !data.in_progress ||
        (!building_construction_is_land_work_type(data.tool.type) && !data.cost_preview)) {
        return 0;
    }
    int size_x = data.tool.end.x - data.tool.start.x;
    int size_y = data.tool.end.y - data.tool.start.y;
    if (size_x < 0) {
        size_x = -size_x;
    }
    if (size_y < 0) {
        size_y = -size_y;
    }
    size_x++;
    size_y++;
    *x = size_x;
    *y = size_y;
    return 1;
}

int building_construction_in_progress(void)
{
    return data.in_progress;
}

void building_construction_start(int x, int y, int grid_offset)
{
    clear_reservoir_aqueduct_preview_route();
    data.tool.set_raw_start(x, y, grid_offset);
    // Mouse-down begins the construction session. Preserve an already
    // resolved single-target mode (roadblock/bridge) while fixing its start
    // tile, just as subsequent drag updates do.
    sync_construction_type(1);
    data.tool.sync_drag_points(hotkey_get_modifiers());

    if (game_undo_start_build(data.tool.type)) {
        data.in_progress = 1;
        int can_start = 1;
        const building_type_registry_impl::ConstructionToolDefinition &tool = construction_tool_for_type(data.tool.type);
        PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_CONSTRUCTION);
        if (tool.is_road()) {
            can_start = Route::calculateConstructionDistances(
                RoutePolicyKind::ConstructionRoad,
                { data.tool.start.x, data.tool.start.y });
        } else if (tool.is_aqueduct()) {
            can_start = Route::calculateConstructionDistances(
                RoutePolicyKind::ConstructionAqueduct,
                { data.tool.start.x, data.tool.start.y });
        } else if (tool.is_draggable_reservoir()) {
            can_start = Route::calculateConstructionDistances(
                RoutePolicyKind::ConstructionDraggableReservoir,
                { data.tool.start.x, data.tool.start.y });
        } else if (tool.is_wall()) {
            can_start = Route::calculateConstructionDistances(
                RoutePolicyKind::ConstructionWall,
                { data.tool.start.x, data.tool.start.y });
        } else if (tool.is_highway()) {
            can_start = Route::calculateConstructionDistances(
                RoutePolicyKind::ConstructionHighway,
                { data.tool.start.x, data.tool.start.y });
        }
        if (!can_start) {
            building_construction_cancel();
        }
    }
}

int building_construction_is_updatable(void)
{
    if (data.tool.selection_is_drag_tool()) {
        return 1;
    }

    return is_vacant_lot_type(data.tool.type) ||
        ConstructionAreaTilePlacement::is_area_tile_type(data.tool.type) ||
        construction_tool_for_type(data.tool.type).has_any();
}

void building_construction_cancel(void)
{
    building_type type = data.tool.type;
    map_property_clear_constructing_and_deleted();
    if (data.in_progress && building_construction_is_updatable()) {
        game_undo_restore_building_state();
        ConstructionAreaTilePlacement::restore_preview_map(type);
        data.in_progress = 0;
        data.cost_preview = 0;
        data.force_place_clear_cost = 0;
        clear_reservoir_aqueduct_preview_route();
    } else {
        building_construction_clear_type();
    }
    building_rotation_reset_rotation();
}

static int should_mark_for_construction(building_type type)
{
    if (building_monument_is_grand_temple(type) &&
        building_monument_count_grand_temples() >= config_get(CONFIG_GP_CH_MAX_GRAND_TEMPLES)) {
        return 0;
    } else if (construction_tool_for_type(type).is_aqueduct()) {
        return 0;
    } else if (construction_tool_for_type(type).is_roadblock()) {
        return 0;
    }
    return 1;
}

void building_construction_update(int x, int y, int grid_offset)
{
    if (grid_offset) {
        data.tool.set_raw_end(x, y, grid_offset);
    } else {
        x = data.tool.raw_end.grid_offset ? data.tool.raw_end.x : data.tool.raw_start.x;
        y = data.tool.raw_end.grid_offset ? data.tool.raw_end.y : data.tool.raw_start.y;
        grid_offset = data.tool.raw_end.grid_offset ? data.tool.raw_end.grid_offset : data.tool.raw_start.grid_offset;
    }
    sync_construction_type(data.in_progress);
    data.tool.sync_drag_points(hotkey_get_modifiers());
    x = data.tool.end.x;
    y = data.tool.end.y;
    grid_offset = data.tool.end.grid_offset;
    building_type type = data.tool.type;
    data.force_place_clear_cost = 0;
    clear_reservoir_aqueduct_preview_route();
    if (!type || (city_finance_out_of_money() && !building_type_allows_out_of_money_construction(type))) {
        data.cost_preview = 0;
        return;
    }

    map_property_clear_constructing_and_deleted();
    int current_cost = model_get_building(type)->cost;
    int repaired_buildings = 0;
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    const building_type_registry_impl::ConstructionToolDefinition &tool = construction_tool_for_type(type);
    if (tool.is_clear_land()) {
        int items_placed = last_items_cleared = building_construction_clear_land(1, data.tool.start.x, data.tool.start.y, x, y);
        if (items_placed >= 0) {
            current_cost *= items_placed;
        }
    } else if (tool.is_clear_trees()) {
        int items_placed = building_construction_clear_trees(1, data.tool.start.x, data.tool.start.y, x, y);
        if (items_placed >= 0) {
            current_cost *= items_placed;
        }
    } else if (tool.is_repair_land()) {
        int cost = building_construction_repair_land(1, data.tool.start.x, data.tool.start.y, x, y, &repaired_buildings);
        if (cost >= 0) {
            current_cost = cost;  // Use total cost directly, don't multiply
        }
    } else if (tool.is_wall()) {
        int items_placed = place_wall(data.tool.start.x, data.tool.start.y, x, y, 1, 1, type);
        if (items_placed >= 0) {
            current_cost *= items_placed;
        }
    } else if (tool.is_road()) {
        int items_placed = building_construction_place_road(
            1, data.tool.start.x, data.tool.start.y, x, y, data.tool.type);
        data.can_place = items_placed > 0;
        if (items_placed >= 0) {
            current_cost *= items_placed;
        }
    } else if (tool.is_highway()) {
        int items_placed = building_construction_place_highway(
            1, data.tool.start.x, data.tool.start.y, x, y, data.tool.type);
        if (items_placed >= 0) {
            current_cost *= items_placed;
            current_cost /= 4; // Highway special case: cost is 100dn per 2x2 tiles, so it's 1/4 the price per tile
        }
    } else if (ConstructionAreaTilePlacement::is_area_tile_type(type)) {
        int items_placed = place_area_tile(data.tool.start.x, data.tool.start.y, x, y, type, 1);
        if (items_placed >= 0) {
            data.can_place = 1;
            current_cost *= items_placed;
        } else {
            data.can_place = 0;
            current_cost = 0;
        }
    } else if (tool.is_draggable_building()) {
        int items_placed = plot_draggable_building(
            data.tool.start.x,
            data.tool.start.y,
            x,
            y,
            tool.draggable_allows_roads());
        if (items_placed >= 0) {
            current_cost *= items_placed;
        }
    } else if (definition && definition->bridge().is_bridge()) {
        int length = map_bridge_building_length();
        if (length > 1) {
            current_cost *= length;
        }
    } else if (tool.is_aqueduct()) {
        data.can_place = building_construction_place_aqueduct(1, type, data.tool.start.x, data.tool.start.y, x, y, &current_cost);
    } else if (tool.is_draggable_reservoir()) {
        struct reservoir_info info;
        const building_type reservoir_type = type;
        const building_type aqueduct_type = building_tool_mode_resolve_context(
            reservoir_type,
            building_type_registry_impl::SmartToolContext::RoutedSegment,
            KEY_MOD_NONE);
        if (reservoir_type == BUILDING_NONE) {
            data.can_place = 0;
            current_cost = 0;
        } else if (calc_maximum_distance(data.tool.start.x, data.tool.start.y, x, y) == 0) {
            const reservoir_endpoint end_reservoir = reservoir_endpoint_from_center(x, y);
            if (reservoir_endpoint_has_existing_reservoir(end_reservoir)) {
                data.can_place = 1;
                current_cost = 0;
            } else if (const building_type_registry_impl::BuildingType *reservoir_definition =
                    building_type_registry_impl::definition_for_type(reservoir_type)) {
                building_construction_assessment assessment =
                    building_construction_assess_placement(
                        *reservoir_definition, end_reservoir.origin_x, end_reservoir.origin_y, 1, 0);
                data.can_place = assessment.can_place;
                current_cost = data.can_place ? model_get_building(reservoir_type)->cost : 0;
            } else {
                data.can_place = 0;
                current_cost = 0;
            }
        } else {
            data.can_place = place_reservoir_and_aqueducts(
                1,
                data.tool.start.x,
                data.tool.start.y,
                x,
                y,
                reservoir_type,
                aqueduct_type,
                &info,
                &data.reservoir_aqueduct_preview_route);
            data.has_reservoir_aqueduct_preview_route = data.can_place && data.reservoir_aqueduct_preview_route.size > 0;
            data.reservoir_aqueduct_preview_type = data.has_reservoir_aqueduct_preview_route ? aqueduct_type : BUILDING_NONE;
            current_cost = data.can_place ? info.cost : 0;
        }
        data.draw_as_constructing = 0;
    } else if (is_vacant_lot_type(type)) {
        int items_placed = place_houses(1, data.tool.start.x, data.tool.start.y, x, y);
        if (items_placed >= 0) {
            current_cost *= items_placed;
        }
    } else {
        const int force_place_active = building_construction_force_place_active();
        building_construction_assessment assessment = building_construction_assess_placement(
            *building_type_registry_impl::definition_for_type(type), x, y, 0, force_place_active);
        data.can_place = assessment.can_place;
        if (force_place_active) {
            if (assessment.can_place) {
                data.force_place_clear_cost = assessment.clear_cost;
                current_cost = model_get_building(type)->cost;
            }
        } else if (!data.required_terrain.meadow && !data.required_terrain.rock && !data.required_terrain.tree &&
            !data.required_terrain.water && !data.required_terrain.wall && !data.required_terrain.distant_water &&
            should_mark_for_construction(type) && data.can_place) {
            mark_placement_construction(assessment.placement, TERRAIN_ALL);
        }
    }
    data.cost_preview = current_cost;
}

figure_type building_construction_nearby_enemy_type(const int *grid_offsets, int count)
{
    if (!grid_offsets || count <= 0) {
        return FIGURE_NONE;
    }

    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (config_get(CONFIG_GP_CH_WOLVES_BLOCK)) {
            if (f->state != FIGURE_STATE_ALIVE || (!f->is_enemy() && f->type != FIGURE_WOLF)) {
                continue;
            }
        } else if (f->is_dead() || !f->is_enemy()) {
            continue;
        }

        int distance = f->type == FIGURE_WOLF ? 6 : 12;

        for (int j = 0; j < count; j++) {
            int grid_offset = grid_offsets[j];
            int tile_x = map_grid_offset_to_x(grid_offset);
            int tile_y = map_grid_offset_to_y(grid_offset);

            int dx = (f->x > tile_x) ? (f->x - tile_x) : (tile_x - f->x);
            int dy = (f->y > tile_y) ? (f->y - tile_y) : (tile_y - f->y);

            if (dx <= distance && dy <= distance) {
                return static_cast<figure_type>(f->type);
            }
        }
    }

    return FIGURE_NONE;
}

void building_construction_offset_start_from_orientation(int *x, int *y, int size)
{
    switch (city_view_orientation()) {
        case DIR_2_RIGHT: *x = *x - size + 1; break;
        case DIR_4_BOTTOM: *x = *x - size + 1; *y = *y - size + 1; break;
        case DIR_6_LEFT: *y = *y - size + 1; break;
    }
}

void building_construction_place(void)
{
    building_type type = building_construction_type();
    int preview_cost = data.cost_preview;
    data.cost_preview = 0;
    data.force_place_clear_cost = 0;
    data.in_progress = 0;
    int x_start = data.tool.start.x;
    int y_start = data.tool.start.y;
    int x_end = data.tool.end.x;
    int y_end = data.tool.end.y;
    grid_slice *slice = map_grid_get_grid_slice_from_corners(x_start, y_start, x_end, y_end);
    building_construction_warning_reset();
    if (!type) {
        return;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    const building_type_registry_impl::ConstructionToolDefinition &tool = construction_tool_for_type(type);
    if (!building_construction_can_place() &&
        (!building_construction_is_updatable() || preview_cost <= 0)) {
        if (definition && !tool.has_any() && !is_vacant_lot_type(type) &&
            !ConstructionAreaTilePlacement::is_area_tile_type(type) && !definition->bridge().is_bridge()) {
            const int placement_rules_passed = building_construction_show_placement_warning(
                *definition, x_end, y_end, 0, building_construction_force_place_active());
            if (placement_rules_passed && city_finance_out_of_money() &&
                !building_type_allows_out_of_money_construction(type)) {
                city_warning_show_translated(WARNING_OUT_OF_MONEY);
            }
        }
        map_property_clear_constructing_and_deleted();
        return;
    }

    if (city_finance_out_of_money() && !building_type_allows_out_of_money_construction(type)) {
        map_property_clear_constructing_and_deleted();
        city_warning_show_translated(WARNING_OUT_OF_MONEY);
        return;
    }

    figure_type enemy_figure_type = building_construction_nearby_enemy_type(
        slice ? slice->grid_offsets : nullptr,
        slice ? slice->size : 0);

    if (!tool.is_clear_land() && !tool.is_clear_trees() && enemy_figure_type != FIGURE_NONE) {
        if (tool.is_wall() || tool.is_road() || tool.is_aqueduct() || tool.is_highway()) {
            game_undo_restore_map(0);
        } else if ((definition && definition->has_tile()) || building_is_connectable(type)) {
            ConstructionAreaTilePlacement::restore_preview_map(type);
        } else if (definition && definition->bridge().is_bridge()) {
            map_bridge_reset_building_length();
        } else {
            map_property_clear_constructing_and_deleted();
        }
        if (enemy_figure_type == FIGURE_WOLF) {
            city_warning_show(WARNING_WOLF_NEARBY, translation_for_key("TR_WARNING_NEARBY_WOLF"));
        } else {
            city_warning_show_translated(WARNING_ENEMY_NEARBY);
        }
        return;
    }

    int placement_cost = model_get_building(type)->cost;
    int repaired_buildings = 0;
    if (tool.is_clear_land()) {
        // BUG in original (keep this behaviour): if confirmation has to be asked (bridge/fort),
        // the previous cost is deducted from treasury and if user chooses 'no', they still pay for removal.
        // If we don't do it this way, the user doesn't pay for the removal at all since we don't come back
        // here when the user says yes.
        int items_placed = building_construction_clear_land(0, x_start, y_start, x_end, y_end);
        if (items_placed < 0) {
            items_placed = last_items_cleared;
        }
        placement_cost *= items_placed;
        map_property_clear_constructing_and_deleted();
    } else if (tool.is_clear_trees()) {
        int items_placed = building_construction_clear_trees(0, x_start, y_start, x_end, y_end);
        placement_cost *= items_placed;
        map_property_clear_constructing_and_deleted();
    } else if (tool.is_repair_land()) {
        building_construction_repair_land(0, data.tool.start.x, data.tool.start.y, x_end, y_end, &repaired_buildings);
        //cost processed inside the repair land function
        map_property_clear_constructing_and_deleted();
    } else if (tool.is_wall()) {
        placement_cost *= place_wall(x_start, y_start, x_end, y_end, 0, 1, type);
    } else if (tool.is_road()) {
        placement_cost *= building_construction_place_road(0, x_start, y_start, x_end, y_end, type);
    } else if (tool.is_highway()) {
        placement_cost *= building_construction_place_highway(0, x_start, y_start, x_end, y_end, type);
        placement_cost /= 4; // Highway special case: cost is 100dn per 2x2 tiles, so it's 1/4 the price per tile
    } else if (ConstructionAreaTilePlacement::is_area_tile_type(type)) {
        int items_placed = place_area_tile(x_start, y_start, x_end, y_end, type, 0);
        if (items_placed < 0) {
            map_property_clear_constructing_and_deleted();
            return;
        }
        placement_cost *= items_placed;
        if (ConstructionAreaTilePlacement::updates_land_routing(type)) {
            Route::updateLandTerrain();
        }
    } else if (definition && definition->bridge().is_bridge()) {
        int length = map_bridge_add(x_end, y_end, definition->bridge().is_ship_bridge());
        if (length <= 1) {
            city_warning_show_translated(WARNING_SHORE_NEEDED);
            return;
        }
        placement_cost *= length;
    } else if (tool.is_aqueduct()) {
        int cost;
        if (!building_construction_place_aqueduct(0, type, x_start, y_start, x_end, y_end, &cost)) {
            city_warning_show_translated(WARNING_CLEAR_LAND_NEEDED);
            return;
        }
        placement_cost = cost;
        map_tiles_update_all_aqueducts(0);
        Route::updateLandTerrain();
    } else if (tool.is_draggable_reservoir()) {
        struct reservoir_info info;
        const building_type reservoir_type = type;
        const building_type aqueduct_type = building_tool_mode_resolve_context(
            reservoir_type,
            building_type_registry_impl::SmartToolContext::RoutedSegment,
            KEY_MOD_NONE);
        const reservoir_endpoint start_reservoir = reservoir_endpoint_from_center(x_start, y_start);
        const reservoir_endpoint end_reservoir = reservoir_endpoint_from_center(x_end, y_end);
        if (calc_maximum_distance(x_start, y_start, x_end, y_end) == 0) {
            const int existing_reservoir = reservoir_endpoint_has_existing_reservoir(end_reservoir);
            if (!place_normalized_reservoir(reservoir_type, end_reservoir)) {
                map_property_clear_constructing_and_deleted();
                city_warning_show_translated(WARNING_CLEAR_LAND_NEEDED);
                return;
            }
            placement_cost = existing_reservoir ? 0 : model_get_building(reservoir_type)->cost;
            map_tiles_update_all_aqueducts(0);
            Route::updateLandTerrain();
        } else {
            if (!place_reservoir_and_aqueducts(
                    0,
                    x_start,
                    y_start,
                    x_end,
                    y_end,
                    reservoir_type,
                    aqueduct_type,
                    &info)) {
                map_property_clear_constructing_and_deleted();
                city_warning_show_translated(WARNING_CLEAR_LAND_NEEDED);
                return;
            }
            if (info.place_reservoir_at_start == PLACE_RESERVOIR_YES) {
                if (!place_normalized_reservoir(reservoir_type, start_reservoir)) {
                    map_property_clear_constructing_and_deleted();
                    city_warning_show_translated(WARNING_CLEAR_LAND_NEEDED);
                    return;
                }
            }
            if (info.place_reservoir_at_end == PLACE_RESERVOIR_YES) {
                if (!place_normalized_reservoir(reservoir_type, end_reservoir)) {
                    map_property_clear_constructing_and_deleted();
                    city_warning_show_translated(WARNING_CLEAR_LAND_NEEDED);
                    return;
                }
                if (!map_terrain_exists_tile_in_area_with_type(start_reservoir.origin_x - 1, start_reservoir.origin_y - 1, 5, TERRAIN_WATER)
                    && info.place_reservoir_at_start == PLACE_RESERVOIR_NO &&
            !water_access_runtime_reservoir_has_network_access(end_reservoir.origin_grid_offset)) {
                    building_construction_warning_check_reservoir(reservoir_type);
                }
            }
            placement_cost = info.cost;
            map_tiles_update_all_aqueducts(0);
            Route::updateLandTerrain();
        }
    } else if (tool.is_draggable_building()) {
        placement_cost *= place_draggable_building(
            x_start,
            y_start,
            x_end,
            y_end,
            type,
            draggable_tool_rotation(type, tool));
    } else if (is_vacant_lot_type(type)) {
        placement_cost *= place_houses(0, x_start, y_start, x_end, y_end);
    } else {
        int force_place_clear_cost = 0;
        int placed = building_construction_force_place_active() ?
            building_construction_force_place_building(type, x_end, y_end, 0, &force_place_clear_cost) :
            building_construction_place_building(type, x_end, y_end, 0);
        if (!placed) {
            map_property_clear_constructing_and_deleted();
            return;
        }
        placement_cost += force_place_clear_cost;
    }

    if (data.auto_cycling && building_construction_type_can_cycle(data.tool.type)) {
        for (int i = 0; i < building_construction_type_cycle_steps(data.tool.type); i++) {
            building_rotation_rotate_forward();
        }
    }
    formation_move_herds_away(x_end, y_end);
    city_finance_process_construction(placement_cost);
    game_undo_finish_build(placement_cost);
}

void building_construction_set_can_place(int can_place)
{
    data.can_place = can_place;
}

static void set_warning(warning_type *warning, translation_key *text_key, warning_type type, translation_key key)
{
    if (warning) {
        *warning = type;
    }
    if (text_key) {
        *text_key = key;
    }
}


int building_construction_can_place_on_terrain(int x, int y, warning_type *warning, translation_key *text_key)
{
    if (data.required_terrain.meadow) {
        if (!map_terrain_exists_tile_in_radius_with_type(x, y, 3, 1, TERRAIN_MEADOW)) {
            set_warning(warning, text_key, WARNING_MEADOW_NEEDED, "TR_CITY_WARNING_MEADOW_NEEDED");
            return 0;
        }
    } else if (data.required_terrain.rock) {
        if (!map_terrain_exists_rock_in_radius(x, y, 2, 1)) {
            set_warning(warning, text_key, WARNING_ROCK_NEEDED, "TR_CITY_WARNING_ROCK_NEEDED");
            return 0;
        }
    } else if (data.required_terrain.tree) {
        if (!map_terrain_exists_tile_in_radius_with_type(x, y, 2, 1, TERRAIN_SHRUB | TERRAIN_TREE)) {
            set_warning(warning, text_key, WARNING_TREE_NEEDED, "TR_CITY_WARNING_TREE_NEEDED");
            return 0;
        }
    } else if (data.required_terrain.water) {
        if (!map_terrain_exists_tile_in_radius_with_type(x, y, 2, 3, TERRAIN_WATER)) {
            set_warning(warning, text_key, WARNING_WATER_NEEDED, "TR_CITY_WARNING_WATER_NEEDED");
            return 0;
        }
    } else if (data.required_terrain.wall) {
        if (!map_terrain_all_tiles_in_radius_are(x, y, 2, 0, TERRAIN_WALL)) {
            set_warning(warning, text_key, WARNING_WALL_NEEDED, "TR_CITY_WARNING_WALL_NEEDED");
            return 0;
        }
    } else if (data.required_terrain.distant_water) {
        if (!map_terrain_exists_tile_in_radius_with_type(x, y, 3, 9, TERRAIN_WATER)) {
            set_warning(warning, text_key, WARNING_WATER_NEEDED_FOR_BUILDING, "TR_WARNING_WATER_NEEDED_FOR_BUILDING");
            return 0;
        }
    }
    return 1;
}

void building_construction_record_view_position(int view_x, int view_y, int grid_offset)
{
    if (grid_offset == data.tool.start.grid_offset) {
        data.start_offset_x_view = view_x;
        data.start_offset_y_view = view_y;
    }
}

void building_construction_get_view_position(int *view_x, int *view_y)
{
    *view_x = data.start_offset_x_view;
    *view_y = data.start_offset_y_view;
}

int building_construction_get_start_grid_offset(void)
{
    return data.tool.start.grid_offset;
}

int building_construction_get_reservoir_aqueduct_preview_route(grid_slice *route, building_type *aqueduct_type)
{
    if (!route || !aqueduct_type) {
        return 0;
    }
    route->size = 0;
    *aqueduct_type = BUILDING_NONE;
    if (!data.in_progress || !construction_tool_for_type(data.tool.type).is_draggable_reservoir() ||
        !data.has_reservoir_aqueduct_preview_route) {
        return 0;
    }
    *route = data.reservoir_aqueduct_preview_route;
    *aqueduct_type = data.reservoir_aqueduct_preview_type;
    return route->size > 0 && *aqueduct_type != BUILDING_NONE;
}

void building_construction_reset_draw_as_constructing(void)
{
    data.draw_as_constructing = 0;
}

int building_construction_draw_as_constructing(void)
{
    return data.draw_as_constructing;
}

