#include "construction_routed.h"

#include "core/calc.h"
#include "core/config.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/construction_plan.h"
#include "building/image.h"
#include "building/properties.h"
#include "building/roadblock.h"
#include "figure/PathingMode.h"
#include "game/performance_tracker.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/road_aqueduct.h"
#include "map/routing.h"
#include "figure/route.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "graphics/window.h"

#include <stdlib.h>
#include <set>
#include <vector>

namespace {

static const int ROUTE_DIRECTION_INDICES[8][4] = {
    {0, 2, 6, 4}, {0, 2, 6, 4}, {2, 4, 0, 6}, {2, 4, 0, 6},
    {4, 6, 2, 0}, {4, 6, 2, 0}, {6, 0, 4, 2}, {6, 0, 4, 2}
};

int trace_routed_offsets(int x_start, int y_start, int x_end, int y_end, std::vector<int> *offsets)
{
    if (!offsets) {
        return 0;
    }
    offsets->clear();
    int grid_offset = map_grid_offset(x_end, y_end);
    for (int guard = 0; guard < 400; ++guard) {
        const int distance = Route::constructionDistanceTo(grid_offset);
        if (distance <= 0) {
            return 0;
        }
        offsets->push_back(grid_offset);
        const int direction = calc_general_direction(x_end, y_end, x_start, y_start);
        if (direction == DIR_8_NONE) {
            return 1;
        }
        int routed = 0;
        for (int i = 0; i < 4; ++i) {
            const int next = grid_offset + map_grid_direction_delta(ROUTE_DIRECTION_INDICES[direction][i]);
            const int next_distance = Route::constructionDistanceTo(next);
            if (next_distance > 0 && next_distance < distance) {
                grid_offset = next;
                x_end = map_grid_offset_to_x(next);
                y_end = map_grid_offset_to_y(next);
                routed = 1;
                break;
            }
        }
        if (!routed) {
            return 0;
        }
    }
    return 0;
}

const building_type_registry_impl::BuildingType *routed_surface_definition(
    building_type_registry_impl::ConstructionToolKind kind)
{
    return building_type_registry_impl::definition_for_type(
        building_type_registry_impl::first_type_where([kind](
            const building_type_registry_impl::BuildingType &definition) {
            return definition.tool().kind() == kind;
        }));
}

int build_routed_surface_plans(
    const building_type_registry_impl::BuildingType &definition,
    const std::vector<int> &offsets,
    int skip_transformable_gates,
    std::vector<building_construction::ConstructionPlacementPlan> *placements,
    std::vector<int> *gate_offsets,
    int *items)
{
    if (!placements || !gate_offsets || !items) {
        return 0;
    }
    placements->clear();
    gate_offsets->clear();
    *items = 0;
    std::set<int> charged_cells;
    for (int grid_offset : offsets) {
        if (skip_transformable_gates && figure_type_registry_impl::PathingMode::gateIsTransformable(grid_offset)) {
            gate_offsets->push_back(grid_offset);
            continue;
        }
        building_construction::ConstructionPlacementPlan placement(
            definition,
            map_grid_offset_to_x(grid_offset),
            map_grid_offset_to_y(grid_offset),
            1,
            0,
            0,
            nullptr,
            0,
            false,
            true);
        if (!placement.can_place()) {
            return 0;
        }
        int changes_terrain = 0;
        for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
            for (const building_construction::ConstructionPlacementTile &tile : part.tiles) {
                const unsigned int terrain = static_cast<unsigned int>(map_terrain_get(tile.grid_offset));
                changes_terrain = changes_terrain ||
                    (tile.added_terrain & ~terrain) || (tile.removed_terrain & terrain);
                const unsigned int transport = tile.added_terrain &
                    (TERRAIN_ROAD | TERRAIN_HIGHWAY);
                const int is_new_transport = (transport & TERRAIN_HIGHWAY)
                    ? !(terrain & TERRAIN_HIGHWAY)
                    : ((transport & TERRAIN_ROAD) && !(terrain & TERRAIN_ROAD));
                if (is_new_transport && charged_cells.insert(tile.grid_offset).second) {
                    ++*items;
                }
            }
        }
        if (changes_terrain) {
            placements->push_back(std::move(placement));
        }
    }
    return 1;
}

void transform_routed_gates(const std::vector<int> &gate_offsets)
{
    for (int grid_offset : gate_offsets) {
        if (!map_building_exists_at(grid_offset)) {
            continue;
        }
        Building &gate = map_building_at(grid_offset);
        building *gate_record = const_cast<::building *>(gate.record());
        const building_type gate_type = static_cast<building_type>(
            building_connectable_gate_type(gate.type ? gate.type->type() : BUILDING_NONE));
        if (!gate_record || gate_type == BUILDING_NONE) {
            continue;
        }
        game_undo_record_building_type(gate_record);
        gate.change_type(gate_type);
        Roadblock roadblock(gate);
        if (config_get(CONFIG_GP_CH_GATES_DEFAULT_TO_PASS_ALL_WALKERS)) {
            roadblock.accept_all();
        } else {
            roadblock.accept_none();
        }
        map_tiles_set_road(map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset));
    }
}

} // namespace

static int place_aqueduct_tile(building_type aqueduct_type, int x, int y)
{
    const int grid_offset = map_grid_offset(x, y);
    const int already_aqueduct = map_terrain_is(grid_offset, TERRAIN_AQUEDUCT);

    if (map_building_exists_at(grid_offset) && !map_building_at(grid_offset).matches("aqueduct")) {
        // Reservoir connection cells are still semantic nodes outside the
        // reservoir Foundation. Keep this connector publication until those
        // nodes become explicit foundation/composition members; ordinary
        // aqueduct cells already use ConstructionPlacementPlan.
        map_terrain_add(grid_offset, TERRAIN_AQUEDUCT);
        map_property_clear_constructing(grid_offset);
        return already_aqueduct ? 0 : 1;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(aqueduct_type);
    if (!definition) {
        return 0;
    }
    return building_construction_place_building(aqueduct_type, x, y, 1)
        ? (already_aqueduct ? 0 : 1)
        : 0;
}

static int place_routed_building(int x_start, int y_start, int x_end, int y_end,
    routed_building_type type, building_type building_type_to_place, int *items, grid_slice *route = nullptr)
{
    *items = 0;
    int grid_offset = map_grid_offset(x_end, y_end);
    int distance = 0;
    int guard = 0;
    // reverse routing
    while (1) {
        if (++guard >= 400) {
            return 0;
        }
        distance = Route::constructionDistanceTo(grid_offset);
        if (distance <= 0) {
            return 0;
        }
        if (route && route->size < MAX_SLICE_SIZE) {
            route->grid_offsets[route->size++] = grid_offset;
        }
        switch (type) {
            default:
            case ROUTED_BUILDING_ROAD:
                *items += map_tiles_set_road(x_end, y_end);
                break;
            case ROUTED_BUILDING_AQUEDUCT:
                *items += place_aqueduct_tile(building_type_to_place, x_end, y_end);
                break;
            case ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC:
                *items += 1;
                break;
            case ROUTED_BUILDING_HIGHWAY:
                *items += map_tiles_set_highway(x_end, y_end);
                break;
        }
        int direction = calc_general_direction(x_end, y_end, x_start, y_start);
        if (direction == DIR_8_NONE) {
            return 1; // destination reached
        }
        int routed = 0;
        for (int i = 0; i < 4; i++) {
            int index = ROUTE_DIRECTION_INDICES[direction][i];
            int new_grid_offset = grid_offset + map_grid_direction_delta(index);
            int new_dist = Route::constructionDistanceTo(new_grid_offset);
            if (new_dist > 0 && new_dist < distance) {
                grid_offset = new_grid_offset;
                x_end = map_grid_offset_to_x(grid_offset);
                y_end = map_grid_offset_to_y(grid_offset);
                routed = 1;
                break;
            }
        }
        if (!routed) {
            return 0;
        }
    }
}

int building_construction_place_road(
    int measure_only, int x_start, int y_start, int x_end, int y_end,
    building_type road_type)
{
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_CONSTRUCTION);
    game_undo_restore_map(0);

    int start_offset = map_grid_offset(x_start, y_start);
    int end_offset = map_grid_offset(x_end, y_end);
    int forbidden_terrain_mask =
        TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER |
        TERRAIN_SHRUB | TERRAIN_GARDEN | TERRAIN_ELEVATION |
        TERRAIN_RUBBLE | TERRAIN_BUILDING | TERRAIN_WALL;
    if (map_terrain_is(start_offset, forbidden_terrain_mask)) {
        if (!(figure_type_registry_impl::PathingMode::gateIsTransformable(start_offset)) && !map_terrain_is(start_offset, TERRAIN_AQUEDUCT)) {
            return 0;
        }
    }
    if (map_terrain_is(end_offset, forbidden_terrain_mask)) {
        if (!(figure_type_registry_impl::PathingMode::gateIsTransformable(end_offset)) && !map_terrain_is(end_offset, TERRAIN_AQUEDUCT)) {
            return 0;
        }
    }

    int items_placed = 0;
    if (Route::calculateConstructionDistances(RoutePolicyKind::ConstructionRoad, { x_start, y_start })) {
        int placed = 0;
        if (measure_only) {
            placed = place_routed_building(x_start, y_start, x_end, y_end,
                ROUTED_BUILDING_ROAD, BUILDING_NONE, &items_placed);
        } else {
            std::vector<int> offsets;
            std::vector<int> gate_offsets;
            std::vector<building_construction::ConstructionPlacementPlan> placements;
            const building_type_registry_impl::BuildingType *definition = road_type == BUILDING_NONE
                ? routed_surface_definition(building_type_registry_impl::ConstructionToolKind::Road)
                : building_type_registry_impl::definition_for_type(road_type);
            placed = definition && trace_routed_offsets(x_start, y_start, x_end, y_end, &offsets) &&
                build_routed_surface_plans(
                    *definition, offsets, 1, &placements, &gate_offsets, &items_placed) &&
                building_construction_publish_placement_batch(placements);
            if (placed) {
                transform_routed_gates(gate_offsets);
                map_tiles_update_all_roads();
                map_tiles_update_all_highways();
            }
        }
        if (placed) {
            if (!measure_only) {
                Route::updateLandTerrain();
                building_connectable_update_connections();
                window_invalidate();
            } else {
                building_connectable_update_connections();
            }
        }
    }
    return items_placed;
}

int building_construction_place_highway(
    int measure_only, int x_start, int y_start, int x_end, int y_end,
    building_type highway_type)
{
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_CONSTRUCTION);
    game_undo_restore_map(0);
    int start_offset = map_grid_offset(x_start, y_start);
    int end_offset = map_grid_offset(x_end, y_end);
    int forbidden_terrain_mask =
        TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER | TERRAIN_BUILDING |
        TERRAIN_SHRUB | TERRAIN_GARDEN | TERRAIN_ELEVATION |
        TERRAIN_RUBBLE | TERRAIN_ACCESS_RAMP;
    if (map_terrain_is(start_offset, forbidden_terrain_mask) && !map_terrain_is(start_offset, TERRAIN_AQUEDUCT)) {
        return 0;
    }
    if (map_terrain_is(end_offset, forbidden_terrain_mask) && !map_terrain_is(end_offset, TERRAIN_AQUEDUCT)) {
        return 0;
    }

    int items_placed = 0;
    if (Route::calculateConstructionDistances(RoutePolicyKind::ConstructionHighway, { x_start, y_start })) {
        int placed = 0;
        if (measure_only) {
            placed = place_routed_building(x_start, y_start, x_end, y_end,
                ROUTED_BUILDING_HIGHWAY, BUILDING_NONE, &items_placed);
        } else {
            std::vector<int> offsets;
            std::vector<int> gate_offsets;
            std::vector<building_construction::ConstructionPlacementPlan> placements;
            const building_type_registry_impl::BuildingType *definition = highway_type == BUILDING_NONE
                ? routed_surface_definition(building_type_registry_impl::ConstructionToolKind::Highway)
                : building_type_registry_impl::definition_for_type(highway_type);
            placed = definition && trace_routed_offsets(x_start, y_start, x_end, y_end, &offsets) &&
                build_routed_surface_plans(
                    *definition, offsets, 0, &placements, &gate_offsets, &items_placed) &&
                building_construction_publish_placement_batch(placements);
            if (placed) {
                map_tiles_update_all_highways();
                map_tiles_update_all_roads();
            }
        }
        if (placed) {
            map_tiles_update_all_plazas();
            if (!measure_only) {
                Route::updateLandTerrain();
                window_invalidate();
            }
        }
    }
    return items_placed;
}

int building_construction_can_place_aqueduct_endpoint(building_type aqueduct_type, int grid_offset)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(aqueduct_type);
    if (definition) {
        building_construction::ConstructionPlacementPlan placement(
            *definition,
            map_grid_offset_to_x(grid_offset),
            map_grid_offset_to_y(grid_offset),
            1,
            0);
        if (placement.can_place()) {
            return 1;
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        return map_is_straight_road_for_aqueduct(grid_offset) &&
            !map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset) &&
            !map_terrain_count_directly_adjacent_with_types(grid_offset, TERRAIN_ROAD | TERRAIN_AQUEDUCT);
    }
    if (!map_can_place_aqueduct_on_highway(grid_offset, 0)) {
        return 0;
    }
    return !map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR) || map_terrain_is(grid_offset, TERRAIN_HIGHWAY);
}

int building_construction_place_aqueduct(
    int measure_only, building_type aqueduct_type, int x_start, int y_start, int x_end, int y_end, int *cost)
{
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_CONSTRUCTION);
    game_undo_restore_map(0);

    const model_building *model = model_get_building(aqueduct_type);
    if (!model) {
        return 0;
    }
    int item_cost = model->cost;
    *cost = 0;
    if (!building_construction_can_place_aqueduct_endpoint(aqueduct_type, map_grid_offset(x_start, y_start)) ||
        !building_construction_can_place_aqueduct_endpoint(aqueduct_type, map_grid_offset(x_end, y_end))) {
        return 0;
    }
    if (!Route::calculateConstructionDistances(RoutePolicyKind::ConstructionAqueduct, { x_start, y_start })) {
        return 0;
    }
    int num_items;
    const routed_building_type route_type =
        measure_only ? ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC : ROUTED_BUILDING_AQUEDUCT;
    place_routed_building(x_start, y_start, x_end, y_end,
        route_type, aqueduct_type, &num_items);
    *cost = item_cost * num_items;
    return 1;
}

int building_construction_preview_aqueduct_route(
    building_type aqueduct_type, int x_start, int y_start, int x_end, int y_end, grid_slice *route, int *cost)
{
    if (route) {
        route->size = 0;
    }
    if (cost) {
        *cost = 0;
    }

    const model_building *model = model_get_building(aqueduct_type);
    if (!model || !route || !cost) {
        return 0;
    }
    if (!building_construction_can_place_aqueduct_endpoint(aqueduct_type, map_grid_offset(x_start, y_start)) ||
        !building_construction_can_place_aqueduct_endpoint(aqueduct_type, map_grid_offset(x_end, y_end))) {
        return 0;
    }
    if (!Route::calculateConstructionDistances(RoutePolicyKind::ConstructionAqueduct, { x_start, y_start })) {
        return 0;
    }

    int num_items = 0;
    if (!place_routed_building(
            x_start,
            y_start,
            x_end,
            y_end,
            ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC,
            aqueduct_type,
            &num_items,
            route)) {
        route->size = 0;
        return 0;
    }
    *cost = model->cost * num_items;
    return 1;
}

int building_construction_place_aqueduct_for_reservoir(
    int measure_only,
    building_type aqueduct_type,
    int x_start,
    int y_start,
    int x_end,
    int y_end,
    int *items,
    grid_slice *route)
{
    if (route) {
        route->size = 0;
    }
    routed_building_type type = measure_only ? ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC : ROUTED_BUILDING_AQUEDUCT;
    const int placed = place_routed_building(x_start, y_start, x_end, y_end, type, aqueduct_type, items, route);
    if (!placed && route) {
        route->size = 0;
    }
    return placed;
}
