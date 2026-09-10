#pragma once
#include "map/TerrainMap.h"
#include "map/TerrainSaveBridge.h"
#include "map/water_navigation.h"
#include "map/image_context.h"
#include "map/tile_runtime_graphics.h"
#include "map/tiles.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/routing_data.h"
#include "figure/route.h"
#include "figure/movement.h"
#include "scenario/event/action_handler.h"
#include "scenario/event/condition_handler.h"
#include "scenario/data.h"
#include "scenario/map.h"
#include "building/building_type_registry_internal.h"
#include <array>
#include <cstdlib>
#include <stdexcept>

inline void validate_terrain_runtime()
{
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    int x = -1, y = -1;
    for (int yy = 3; yy < map_grid_height() - 9 && x < 0; ++yy) for (int xx = 3; xx < map_grid_width() - 9; ++xx) {
        bool clear = true;
        for (int dy = 0; dy < 7; ++dy) for (int dx = 0; dx < 7; ++dx) {
            const int offset = map_grid_offset(xx + dx, yy + dy);
            if (!map_grid_is_inside(xx + dx, yy + dy, 1) || terrain_map().contains(offset, terrain_types().not_clear) || map_figure_at(offset)) clear = false;
        }
        if (clear) { x = xx; y = yy; break; }
    }
    require(x >= 0, "Terrain fixture requires a clear 7x7 area");
    struct Restore {
        std::vector<std::pair<int, TerrainSet>> tiles;
        ~Restore() { for (const auto &[offset, terrain] : tiles) terrain_map().set(offset, terrain); map_tiles_update_all(); Route::updateLandTerrain(); }
    } restore;
    for (int dy = 0; dy < 7; ++dy) for (int dx = 0; dx < 7; ++dx) {
        const int offset = map_grid_offset(x + dx, y + dy);
        restore.tiles.emplace_back(offset, terrain_map().at(offset));
        terrain_map().set(offset, {});
    }
    const int center = map_grid_offset(x + 3, y + 3);
    terrain_map().set(center, terrain_types().tree);
    Route::updateLandTerrain();
    require(!figure_type_registry_impl::PathingMode::noncitizenIsPassable(center), "Woodland must block noncitizen routing");
    require(terrain_land_citizen.items[center] < CITIZEN_0_ROAD, "Woodland must block soldier station routing");
    const int west_x = figure_movement_tile_to_cross_country(x + 2), center_y = figure_movement_tile_to_cross_country(y + 3);
    require(Route::groundPositionIsPassable(west_x + 63, center_y), "Position before the tile-center boundary must stay on clear ground");
    require(!Route::groundPositionIsPassable(west_x + 64, center_y), "Position at the visual woodland boundary must not use the old routing anchor");
    require(!Route::groundPositionIsPassable(west_x + 127, center_y), "Soldier drawn inside adjacent woodland must not acquire a station there");
    require(!Route::groundPositionIsPassable(west_x + 65, center_y - 63), "Diagonal local approach must not clip a woodland corner");
    require(!Route::herdCanEnter(map_grid_offset(x + 2, y + 3), center, 0), "Herd movement must respect woodland");
    terrain_map().set(center, terrain_types().building | terrain_types().road);
    const int near = map_grid_offset(x + 4, y + 3), edge = map_grid_offset(x + 5, y + 3), far = map_grid_offset(x + 6, y + 3);
    require(!Route::herdCanEnter(near, center, 0), "Roads under buildings must not grant herd access");
    require(terrain_map().distance_to_nearest(edge, terrain_types().building, 2) == 2, "Building clearance must measure occupied cells");
    require(!Route::herdCanEnter(far, edge, 2) && !Route::herdCanEnter(edge, near, 2), "Herds must avoid approaching buildings");
    require(Route::herdCanEnter(near, edge, 2) && Route::herdCanEnter(edge, far, 2), "Herds displaced by new buildings must be able to escape");
    require(map_routing_herd_can_travel(x + 4, y + 3, x + 6, y + 3, 8, 5000, 2), "Herd route planner must find an escape from a new building buffer");
    require(!map_routing_herd_can_travel(x + 6, y + 3, x + 4, y + 3, 8, 5000, 2), "Herd route planner must not re-enter a building buffer");
    for (int dy = 1; dy < 6; ++dy) for (int dx = 1; dx < 6; ++dx) terrain_map().set(map_grid_offset(x + dx, y + dy), terrain_types().water);
    terrain_map().set(center, terrain_types().water);
    require(water_navigation::is_passable(center, WaterNavigationProfile::Boat), "Deep water must remain sea-pathable");
    if (!terrain_types().shallow_water.empty()) {
        terrain_map().add(center, terrain_types().shallow_water);
        require(terrain_map().contains_all(center, terrain_types().water | terrain_types().shallow_water), "Shallow terrain must bind its included water definition");
        require(!water_navigation::is_passable(center, WaterNavigationProfile::Boat) && !water_navigation::is_passable(center, WaterNavigationProfile::Flotsam), "Authored sea barriers must invalidate navigation");
        map_tiles_update_all_water();
        const Terrain &shallow = *terrain_types().shallow_water.entries().front();
        const auto *graphic = shallow.water_image(WaterShoreShape::Open, 0);
        require(graphic && tile_runtime_get_graphic_footprint_slice(center), "Shallow water must publish its bound native terrain graphic");
        terrain_map().remove(center, terrain_types().water);
        require(!terrain_map().contains(center, terrain_types().shallow_water), "Removing a prerequisite terrain must remove dependent overlays");
        terrain_map().set(center, terrain_types().water);
        require(water_navigation::is_passable(center, WaterNavigationProfile::Boat), "Removing shallow overlay must restore navigation");
    }
    {
        struct RestoreAnchors {
            map_point entry = scenario.river_entry_point, exit = scenario.river_exit_point;
            std::array<map_point, MAX_FISH_POINTS> fishing;
            RestoreAnchors() { std::copy(std::begin(scenario.fishing_points), std::end(scenario.fishing_points), fishing.begin()); }
            ~RestoreAnchors() {
                scenario.river_entry_point = entry; scenario.river_exit_point = exit;
                std::copy(fishing.begin(), fishing.end(), std::begin(scenario.fishing_points));
                water_navigation::invalidate_river_anchors();
            }
        } anchors;
        for (int dy = 0; dy < 7; ++dy) for (int dx = 0; dx < 7; ++dx) terrain_map().set(map_grid_offset(x + dx, y + dy), dx == 3 ? TerrainSet{} : TerrainSet(terrain_types().water));
        std::fill(std::begin(scenario.fishing_points), std::end(scenario.fishing_points), map_point{-1, -1});
        scenario.fishing_points[0] = scenario.river_exit_point = {x + 5, y + 2};
        scenario.fishing_points[1] = scenario.river_entry_point = {x + 1, y + 4};
        water_navigation::invalidate_river_anchors();
        map_point destination{};
        require(scenario_map_closest_fishing_point(x + 1, y + 2, &destination) && destination.x == x + 1 && destination.y == y + 4,
            "Fishing boats must skip geometrically close spots behind sea barriers");
        require(scenario_map_closest_reachable_river_exit(x + 1, y + 2, &destination) && destination.x == x + 1,
            "Trade ships must choose the reachable river anchor");
        scenario.fishing_points[1] = scenario.river_entry_point = {-1, -1};
        water_navigation::invalidate_river_anchors();
        require(!scenario_map_closest_fishing_point(x + 1, y + 2, &destination) && !scenario_map_closest_reachable_river_exit(x + 1, y + 2, &destination),
            "Disconnected waters must not advertise an unreachable fishing spot or exit");
        for (int dy = 0; dy < 7; ++dy) terrain_map().set(map_grid_offset(x + 3, y + dy), terrain_types().water);
        require(scenario_map_closest_fishing_point(x + 1, y + 2, &destination) && destination.x == x + 5,
            "Fishing targets must become reachable after removing the sea barrier");
        std::fprintf(stdout, "Water destinations passed: blocked fishing spots and river exits, disconnected water and topology refresh.\n");
    }
    using namespace building_type_registry_impl;
    const auto *reservoir = definition_for_type(type_from_attr("reservoir"));
    require(reservoir && !reservoir->water_access().requirement_rules().empty(), "Reservoir fixture needs authored water access");
    const FoundationProximityRequirement *source = nullptr;
    for (const auto &rule : reservoir->foundation_def()->proximity_requirements()) if (rule.name == "water_source") source = &rule;
    require(source && !source->placement, "Water source must be a bound operational foundation condition");
    require(reservoir->foundation_def()->meets_proximity(*source, x, y, 0), "Foundation water-source condition must see nearby terrain");
    for (const auto &[offset, terrain] : restore.tiles) terrain_map().set(offset, {});
    require(!reservoir->foundation_def()->meets_proximity(*source, x, y, 0), "Dry foundation must not produce a water source");
    scenario_action_t action{};
    action.type = ACTION_TYPE_CHANGE_TERRAIN;
    action.terrain = terrain_types().rock | terrain_types().meadow;
    action.parameter1 = center;
    action.parameter2 = center;
    action.parameter4 = 1;
    std::array<uint8_t, 128> bytes{};
    buffer state{}, ledger{};
    buffer_init(&state, bytes.data(), bytes.size());
    terrain_save::prepare();
    scenario_action_type_save_state(&state, &action, 0, 0);
    terrain_save::write_ledger(&ledger);
    const bool ledger_loaded = terrain_save::load_ledger(&ledger, true);
    free(ledger.data);
    require(ledger_loaded, "Scenario terrain action ledger must load");
    buffer_reset(&state);
    scenario_action_t restored{};
    int link_type = 0; int32_t link_id = 0;
    scenario_action_type_load_state(&state, &restored, &link_type, &link_id, 1);
    require(restored.terrain == action.terrain && !restored.parameter3, "Scenario actions must restore bound terrain references without a numeric runtime parameter");
    require(scenario_action_type_execute(&restored) && terrain_map().contains_all(center, action.terrain), "Restored scenario terrain action must execute through its references");
    std::fprintf(stdout, "Terrain runtime contracts passed: included references, sea barriers, native graphics, operational foundations and scenario action roundtrip.\n");
}
