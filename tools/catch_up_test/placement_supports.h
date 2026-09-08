#pragma once
#include "building/construction_plan.h"
#include "building/construction_building.h"
#include "building/construction.h"
#include "building/construction_area_tile.h"
#include "building/properties.h"
#include "map/building.h"
#include "map/bridge.h"
#include "game/defines.h"
#include "figuretype/enemy.h"
#include "figure/formation.h"
#include "figure/movement.h"
#include "building/construction_clear.h"
#include "city/data_private.h"
#include "city/warning.h"
#include "map/figure.h"
#include "map/property.h"
#include "map/image.h"
#include "map/tiles.h"
#include "game/undo.h"
#include "city/view.h"
#include "widget/city_without_overlay.h"
#include "graphics/graphics.h"
#include <stdexcept>

inline void validate_placement_supports()
{
    using namespace building_type_registry_impl;
    using namespace building_construction;
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    int x = -1, y = -1;
    for (int yy = 3; yy < map_grid_height() - 8 && x < 0; ++yy) for (int xx = 3; xx < map_grid_width() - 8; ++xx) {
        bool clear = true;
        for (int dy = 0; dy < 7; ++dy) for (int dx = 0; dx < 7; ++dx) {
            const int offset = map_grid_offset(xx + dx, yy + dy);
            if (!map_grid_is_inside(xx + dx, yy + dy, 1) || terrain_map().contains(offset, terrain_types().not_clear) || map_figure_at(offset)) clear = false;
        }
        if (clear) { x = xx + 2; y = yy + 2; break; }
    }
    require(x >= 0, "Support fixture requires a clear 7x7 area for rotated compositions");
    const auto *tower = definition_for_type(type_from_attr("tower"));
    const auto *wall = definition_for_type(type_from_attr("wall"));
    require(tower && wall, "Support fixture requires tower and wall data");
    const int offset = map_grid_offset(x, y);
    const TerrainSet original = terrain_map().at(offset);
    struct RestoreTerrain { int offset; TerrainSet terrain; ~RestoreTerrain() { terrain_map().set(offset, terrain); } } restore{offset, original};
    for (int rotation = 0; rotation < 4; ++rotation) {
        ConstructionPlacementPlan bare(*tower, x, y, 1, 0, rotation, nullptr, rotation, false, true);
        require(bare.can_place() && bare.support_cost() == 4 * model_get_construction_cost(wall->type()), "Bare tower must quote four full-price wall supports in every rotation");
    }
    terrain_map().add(offset, terrain_types().wall);
    ConstructionPlacementPlan partial(*tower, x, y, 1, 0);
    require(partial.can_place() && partial.support_cost() == 3 * model_get_construction_cost(wall->type()), "Pre-existing walls must not be charged twice");
    terrain_map().set(offset, original | terrain_types().building | terrain_types().wall);
    require(!ConstructionPlacementPlan(*tower, x, y, 1, 0).can_place(), "Unowned building occupancy must not qualify as a supporting wall");
    terrain_map().set(offset, original);
    const auto *roadblock = definition_for_type(type_from_attr("roadblock"));
    if (roadblock) {
        ConstructionPlacementPlan road(*roadblock, x, y, 1, 0);
        require(road.can_place() && road.support_cost() == model_get_construction_cost(type_from_attr("road")), "Roadblock must quote a missing road");
    }
    FoundationDef proximity("test_proximity");
    proximity.set_dimensions(1, 1);
    FoundationCellDefinition cell;
    cell.added_terrain = terrain_types().building;
    proximity.add_cell(cell);
    proximity.add_proximity_requirement({terrain_types().water, 2, 2, 2, false, false});
    BuildingType candidate(tower->type(), "proximity_fixture");
    candidate.set_foundation_definition(&proximity);
    const int water1 = map_grid_offset(x + 2, y);
    const int water2 = map_grid_offset(x + 2, y + 1);
    RestoreTerrain restore_water1{water1, terrain_map().at(water1)}, restore_water2{water2, terrain_map().at(water2)};
    terrain_map().add(water1, terrain_types().water);
    require(!ConstructionPlacementPlan(candidate, x, y, 1, 0).can_place(), "Proximity must enforce minimum matching tile count");
    terrain_map().add(water2, terrain_types().water);
    require(ConstructionPlacementPlan(candidate, x, y, 1, 0).can_place(), "Proximity must accept two matching tiles at the specified distance");
    require(!ConstructionPlacementPlan(candidate, x + 1, y, 1, 0).can_place(), "Proximity must reject matching tiles inside the minimum distance");
    terrain_map().set(water1, restore_water1.terrain);
    terrain_map().set(water2, restore_water2.terrain);
    // The authored lighthouse requirement is part of the same planner as ordinary terrain rules.
    if (const auto *lighthouse = definition_for_type(type_from_attr("lighthouse"))) {
        require(!lighthouse->foundation_def()->proximity_requirements().empty(), "Lighthouse must declare its proximity rule in data");
        const auto &rule = lighthouse->foundation_def()->proximity_requirements().front();
        require(rule.sea && rule.max_distance == 9 && rule.min_count == 1, "Lighthouse must require nearby entrance-connected navigable water");
    }
    building_construction_set_type(tower, 0);
    struct Stock { Building *space; resource_type resource; std::array<short, RESOURCE_SLOT_COUNT> amounts; };
    std::vector<Stock> stocks;
    std::vector<Building *> warehouses;
    const auto resource_cache = city_data.resource;
    auto restore_stocks = std::shared_ptr<void>(nullptr, [&](void *) {
        for (const auto &stock : stocks) {
            stock.space->set_warehouse_resource_id(stock.resource);
            for (int slot = 0; slot < RESOURCE_SLOT_COUNT; ++slot) stock.space->set_resource_amount(static_cast<resource_type>(slot), stock.amounts[slot]);
        }
        for (auto *warehouse : warehouses) building_warehouse_recount_resources(*warehouse);
        city_data.resource = resource_cache;
    });
    Building::for_each(BuildingRuntimeList::Warehouses, [&](Building *warehouse) {
        if (!warehouse->is_in_use() || !warehouse->Composition) return;
        warehouses.push_back(warehouse);
        for (auto *part : warehouse->Composition->children()) {
            Building *space = part->building();
            if (!space || !space->type->attr_is("warehouse_space")) continue;
            Stock stock{space, space->warehouse_resource_id()};
            for (int slot = 0; slot < RESOURCE_SLOT_COUNT; ++slot) stock.amounts[slot] = static_cast<short>(space->resource_amount(static_cast<resource_type>(slot)));
            stocks.push_back(stock);
        }
    });
    const int required_stone = ConstructionPlacementPlan(*tower, x, y, 1, 0).support_resource_amount(resource_stone());
    if (required_stone) {
        require(!stocks.empty(), "Support fixture needs a warehouse bay");
        for (int slot = 0; slot < RESOURCE_SLOT_COUNT; ++slot) stocks.front().space->set_resource_amount(static_cast<resource_type>(slot), 0);
        stocks.front().space->set_warehouse_resource_id(resource_stone());
        stocks.front().space->set_resource_amount(resource_stone(), required_stone);
        building_warehouse_recount_resources(*stocks.front().space->Composition->owner());
    }
    const int stone_before = city_resource_count_warehouses_amount(resource_stone());
    int camera_x, camera_y;
    city_view_get_camera_absolute(&camera_x, &camera_y);
    auto restore_camera = std::shared_ptr<void>(nullptr, [&](void *) { city_view_set_camera_absolute(camera_x, camera_y); });
    window_city_show();
    city_view_go_to_grid_offset(offset);
    view_tile selected_view;
    city_view_grid_offset_to_xy_view(offset, &selected_view.x, &selected_view.y);
    city_view_set_selected_view_tile(&selected_view);
    const map_tile hovered = {x, y, offset};
    for (const auto *definition : {tower, roadblock}) {
        if (!definition) continue;
        building_construction_set_type(definition, 0);
        window_draw(1);
        screen_set_pixel_render_scale();
        city_without_overlay_draw(0, nullptr, &hovered, 0);
        const ConstructionPlacementPlan preview(*definition, x, y, 0, 0);
        require(building_construction_cost() == model_get_construction_cost(definition->type()) + preview.support_cost(), "Hover price must include the supporting foundation");
        require(terrain_map().at(offset) == original && !map_building_exists_at(offset), "Hover support graphics must not mutate the map");
        const int width = screen_pixel_width(), height = screen_pixel_height();
        std::vector<color_t> pixels(static_cast<size_t>(width) * height);
        require(graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width) != 0, "Could not capture support preview");
        std::filesystem::create_directories("out/catch-up-ui");
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        require(surface != nullptr, "Could not create support preview surface");
        const int result = SDL_SaveBMP(surface, (std::string("out/catch-up-ui/support-") + definition->attr() + ".bmp").c_str());
        SDL_FreeSurface(surface);
        require(result == 0, "Could not save support preview");
        screen_set_ui_render_scale();
        building_construction_clear_type();
    }
    building_construction_set_type(tower, 0);
    require(game_undo_start_build(tower->type()) != 0, "Could not start support publication undo fixture");
    require(building_construction_place_building(tower->type(), x, y, 1) != 0, "Could not publish tower and supports atomically");
    require(city_resource_count_warehouses_amount(resource_stone()) == stone_before - required_stone, "Supports must consume their complete material cost");
    require(terrain_map().contains(offset, terrain_types().wall) && map_building_exists_at(offset), "Published tower must own its wall support");
    auto require_single_tower = [&]() {
        Building &placed_tower = map_building_at(offset);
        int draw_tiles = 0;
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
            const int tile = map_grid_offset(x + dx, y + dy);
            require(map_building_exists_at(tile) && map_building_at(tile).record() == placed_tower.record(), "Tower footprint must retain one building owner");
            require(map_property_legacy_multi_tile_size(tile) == 2, "Wall refresh must preserve tower footprint dimensions");
            require(map_property_is_multi_tile_xy(tile, dx, dy), "Wall refresh must preserve tower footprint coordinates");
            const bool draw = map_property_is_draw_tile(tile) != 0;
            require(draw == (placed_tower.Foundation->draw_grid_offset(city_view_orientation()) == tile), "Wall refresh must preserve the tower draw anchor");
            draw_tiles += draw;
        }
        require(draw_tiles == 1, "Tower must render once after supporting walls refresh");
    };
    require_single_tower();
    std::array<unsigned int, 4> tower_images;
    for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) tower_images[dy * 2 + dx] = map_image_at(map_grid_offset(x + dx, y + dy));
    for (int refresh = 0; refresh < 4; ++refresh) {
        map_tiles_update_area_walls(x, y, 4);
        map_tiles_update_all_walls();
        require_single_tower();
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
            require(map_image_at(map_grid_offset(x + dx, y + dy)) == tower_images[dy * 2 + dx], "Wall refresh must not replace tower images");
        }
        window_draw(1);
    }
    game_undo_finish_build(0);
    game_undo_perform();
    require(city_resource_count_warehouses_amount(resource_stone()) == stone_before, "Undo must return support material costs");
    require(!map_building_exists_at(offset) && terrain_map().at(offset) == original, "Undo must restore pre-support terrain and ownership");
    // Previously placed walls have records that are retired on a later state update.
    building_construction_set_type(wall, 0);
    for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
        require(building_construction_place_building(wall->type(), x + dx, y + dy, 1) != 0, "Could not place existing wall fixture");
    }
    const int stone_after_walls = city_resource_count_warehouses_amount(resource_stone());
    building_construction_set_type(tower, 0);
    require(ConstructionPlacementPlan(*tower, x, y, 1, 0).support_cost() == 0, "Existing walls must not be charged as missing supports");
    require(building_construction_place_building(tower->type(), x, y, 1) != 0, "Could not place a tower on four existing walls");
    require(city_resource_count_warehouses_amount(resource_stone()) == stone_after_walls, "Tower must not consume wall materials twice");
    require_single_tower();
    for (int tick = 0; tick < 4; ++tick) {
        building_update_state();
        map_tiles_update_all_walls();
        require_single_tower();
        for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
            require(terrain_map().contains(map_grid_offset(x + dx, y + dy), terrain_types().wall), "Retired walls must not remove the tower's wall terrain");
        }
        window_draw(1);
    }
    map_building_at(offset).destroy_without_rubble();
    building_update_state();
    for (int dy = 0; dy < 2; ++dy) for (int dx = 0; dx < 2; ++dx) {
        const int tile = map_grid_offset(x + dx, y + dy);
        require(!map_building_exists_at(tile) && !terrain_map().contains(tile, terrain_types().building | terrain_types().wall), "Tower cleanup must release its complete footprint");
    }
    if (roadblock) {
        for (bool existing_road : {false, true}) {
            if (existing_road) terrain_map().add(offset, terrain_types().road);
            building_construction_set_type(roadblock, 0);
            require(game_undo_start_build(roadblock->type()) != 0, "Could not start blocker undo fixture");
            require(building_construction_place_building(roadblock->type(), x, y, 1) != 0, "Could not place blocker fixture");
            const auto *delta = map_building_at(offset).Foundation->terrain_delta_at(offset);
            require(delta && !(delta->added_terrain & terrain_types().road), "Blocker must not own the underlying road");
            game_undo_finish_build(0);
            game_undo_perform();
            require(!map_building_exists_at(offset) && bool(terrain_map().contains(offset, terrain_types().road)) == existing_road, "Undo must restore the terrain before road/blocker placement");
            require(building_construction_place_building(roadblock->type(), x, y, 1) != 0, "Could not replace blocker after undo");
            building *record = const_cast<building *>(map_building_at(offset).record());
            record->state = BUILDING_STATE_DELETED_BY_PLAYER;
            record->is_deleted = 1;
            building_update_state();
            require(!map_building_exists_at(offset) && terrain_map().contains(offset, terrain_types().road), "Deleting a blocker must leave its supporting road, however that road was placed");
            terrain_map().remove(offset, terrain_types().road);
        }
    }
    if (const auto *gate = definition_for_type(type_from_attr("palisade_gate"))) {
        const auto finance = city_data.finance;
        auto restore_finance = std::shared_ptr<void>(nullptr, [&](void *) { city_data.finance = finance; });
        building_construction_set_type(gate, 0);
        require(building_construction_place_building(gate->type(), x, y, 1) != 0, "Could not place palisade gate repair fixture");
        building_update_state();
        map_building_at(offset).destroy_by_collapse();
        require(map_building_exists_at(offset) && map_building_at(offset).Rubble, "Collapsed gate must retain recoverable rubble");
        // The collapse animation temporarily occupies the tile; repair after it clears.
        for (unsigned int id = 1; id < Figure::count(); ++id) {
            Figure *effect = Figure::get(id);
            if (effect && effect->type == FIGURE_EXPLOSION && effect->grid_offset == offset) effect->remove();
        }
        if (map_building_at(offset).repair() <= 0) {
            const auto &rubble = map_building_at(offset);
            const auto assessment = building_construction_assess_repair(*rubble.Rubble->original_type(), *rubble.Rubble->state());
            std::fprintf(stderr, "Gate repair rejected: treasury=%d quoted_cost=%d type=%s model_cost=%d allowed=%d global=%d plan=%d reason=%d owner_charges=%d rubble=%d/%d\n", city_data.finance.treasury, rubble.repair_cost(), rubble.Rubble->original_type()->attr(), model_get_construction_cost(gate->type()), assessment.can_place, assessment.global_blocked, assessment.placement.can_place(), static_cast<int>(assessment.placement.failure_reason()), assessment.placement.owner_charge_count(), assessment.placement.replaceable_rubble_tiles(), assessment.placement.required_rubble_tiles());
            throw std::runtime_error("Palisade gate rubble must be repairable");
        }
        require(map_building_exists_at(offset) && map_building_at(offset).type == gate && terrain_map().contains(offset, terrain_types().road),
            "Repair must republish the gate's road through its authored foundation");
        building *record = const_cast<building *>(map_building_at(offset).record());
        record->state = BUILDING_STATE_DELETED_BY_PLAYER; record->is_deleted = 1;
        building_update_state();
        terrain_map().set(offset, original);
        for (unsigned int id = 1; id < Figure::count(); ++id) {
            Figure *effect = Figure::get(id);
            if (effect && effect->type == FIGURE_EXPLOSION && std::abs(effect->x - x) <= 2 && std::abs(effect->y - y) <= 2) effect->remove();
        }
        std::fprintf(stdout, "Palisade gate collapse/repair passed: native foundation restores the road.\n");
    }
    const auto plaza = type_from_attr("plaza");
    if (plaza != BUILDING_NONE) {
        require(!ConstructionPlacementPlan(plaza, x, y, 1, 0).can_place(), "Plazas must reject clear ground");
        require(!ConstructionPlacementPlan(plaza, x, y, 1, 1).can_place(), "Force place must not supply a road for plazas");
        require(game_undo_start_build(plaza) != 0, "Could not start plaza fixture");
        ConstructionAreaTilePlacement area(x, y, x + 1, y + 1, plaza, true);
        require(area.place() == 0 && area.support_cost() == 0, "Plaza drag must skip clear ground without quoting roads");
        ConstructionAreaTilePlacement::restore_preview_map(plaza);
        require(terrain_map().at(offset) == original, "Plaza preview cancellation must restore bare ground");
        const int second_road = map_grid_offset(x + 1, y + 1);
        terrain_map().add(offset, terrain_types().road);
        terrain_map().add(second_road, terrain_types().road);
        require(game_undo_start_build(plaza) != 0, "Could not start mixed plaza fixture");
        require(area.place() == 2 && area.support_cost() == 0, "Plaza drag must cover only the two existing roads");
        require(map_property_is_plaza_earthquake_or_overgrown_garden(offset) && map_property_is_plaza_earthquake_or_overgrown_garden(second_road), "Plaza preview must decorate existing roads");
        require(!terrain_map().contains(map_grid_offset(x + 1, y), terrain_types().road), "Plaza preview must leave intervening clear ground alone");
        ConstructionAreaTilePlacement::restore_preview_map(plaza);
        require(terrain_map().contains(offset, terrain_types().road) && !map_property_is_plaza_earthquake_or_overgrown_garden(offset), "Cancelling a plaza preview must preserve its original road");
        terrain_map().remove(offset, terrain_types().road);
        terrain_map().remove(second_road, terrain_types().road);
        game_undo_disable();
    }
    for (const char *storage_name : {"granary", "warehouse"}) {
        const auto *storage = definition_for_type(type_from_attr(storage_name));
        require(storage, "Storage foundation fixture requires native definitions");
        for (int rotation = 0; rotation < 4; ++rotation) {
            ConstructionPlacementPlan clear(*storage, x, y, 1, 0, rotation, nullptr, rotation, false, true);
            require(clear.can_place(), "Storage must place without pre-built roads in every rotation");
            const int blocked = clear.parts().front().tiles.front().grid_offset;
            terrain_map().add(blocked, terrain_types().road);
            ConstructionPlacementPlan shifted(*storage, x, y, 1, 1, rotation, nullptr, rotation, false, true);
            require(shifted.can_place(), "Shift must allow storage to clear roads through the shared placement system");
            terrain_map().remove(blocked, terrain_types().road);
        }
        building_construction_set_type(storage, 0);
        require(game_undo_start_build(storage->type()) != 0, "Could not start storage foundation fixture");
        require(building_construction_place_building(storage->type(), x, y, 1), "Could not construct storage on clear ground");
        Building &built = map_building_at(offset);
        const auto &foundation_state = built.Foundation->state();
        int internal_roads = 0;
        for (const auto &storage_cell : built.Foundation->cells(foundation_state.rotation())) {
            if (!(storage_cell.definition->added_terrain & terrain_types().road)) continue;
            ++internal_roads;
            require(terrain_map().contains(map_grid_offset(foundation_state.origin_x() + storage_cell.x, foundation_state.origin_y() + storage_cell.y), terrain_types().road), "Foundation must publish all storage internal roads");
        }
        require(internal_roads > 0, "Storage foundation must declare its own internal roads");
        game_undo_finish_build(0);
        game_undo_perform();
    }

    // Exercise the real clear tool as well as the policy: any last invader blocks deletion,
    // and a peaceful figure on an earlier span must not hide a hostile on a later one.
    const auto original_figures = city_data.figure;
    const int original_allow_occupied = config_get(CONFIG_GP_CH_ALWAYS_DESTROY_BRIDGES);
    auto restore_policy = std::shared_ptr<void>(nullptr, [&](void *) { city_data.figure = original_figures; config_set(CONFIG_GP_CH_ALWAYS_DESTROY_BRIDGES, original_allow_occupied); });
    for (int dx = 0; dx < 3; ++dx) terrain_map().add(map_grid_offset(x + dx, y), terrain_types().water);
    require(map_bridge_create_native_chain(offset, 3, DIR_2_RIGHT, 0, 0), "Could not create bridge policy fixture");
    city_data.figure.enemies = city_data.figure.imperial_soldiers = 0;
    require(!map_bridge_demolition_warning(offset).name, "Empty bridge must be deletable outside invasions");
    for (bool imperial : {false, true}) {
        city_data.figure.enemies = imperial ? 0 : 1;
        city_data.figure.imperial_soldiers = imperial ? 1 : 0;
        for (int allow : {0, 1}) {
            config_set(CONFIG_GP_CH_ALWAYS_DESTROY_BRIDGES, allow);
            require(map_bridge_demolition_warning(offset).name == WARNING_ENEMIES_PREVENT_BRIDGE_DESTRUCTION.name, "Even one remaining invader must block demolition regardless of occupied-bridge setting");
            const auto clear_type = type_from_attr("clear_land");
            game_undo_start_build(clear_type);
            require(building_construction_clear_land(1, x, y, x + 2, y) == 0, "Blocked bridge preview must quote no demolition");
            require(building_construction_clear_land(0, x, y, x + 2, y) == 0, "Clear tool must reject invasion bridge demolition before confirmation");
            require(map_is_bridge(offset), "Rejected demolition must preserve the bridge");
            game_undo_disable();
        }
    }
    city_data.figure.enemies = city_data.figure.imperial_soldiers = 0;
    Figure *citizen = Figure::create(FIGURE_ENGINEER, x, y, DIR_2_RIGHT);
    Figure *hostile = Figure::create(FIGURE_ENEMY43_SPEAR, x + 2, y, DIR_6_LEFT);
    require(citizen && citizen->id() && hostile && hostile->id(), "Could not create bridge occupants");
    require(map_bridge_has_figures(offset) == 2 && map_bridge_demolition_warning(offset).name == WARNING_PEOPLE_ON_BRIDGE.name, "Hostile on later bridge span must override earlier peaceful occupancy");
    hostile->remove();
    config_set(CONFIG_GP_CH_ALWAYS_DESTROY_BRIDGES, 0);
    require(map_bridge_has_figures(offset) == 1 && map_bridge_demolition_warning(offset).name, "Peaceful occupancy must respect the safety setting");
    config_set(CONFIG_GP_CH_ALWAYS_DESTROY_BRIDGES, 1);
    require(!map_bridge_demolition_warning(offset).name, "Peaceful occupied bridge may be demolished only when enabled outside invasions");
    citizen->remove();
    map_bridge_remove(offset, 0);
    building_update_state();
    for (int dx = 0; dx < 3; ++dx) terrain_map().remove(map_grid_offset(x + dx, y), terrain_types().water);
    Figure *retreating = Figure::create(FIGURE_ENEMY43_SPEAR, x, y, DIR_2_RIGHT);
    require(retreating && retreating->id(), "Could not create retreat fixture");
    const int retreat_formation = formation_create_enemy(FIGURE_ENEMY43_SPEAR, x, y, 0, DIR_2_RIGHT, 0, 0, 0, 0);
    require(retreat_formation > 0, "Could not create native enemy formation for retreat fixture");
    retreating->formation_id = retreat_formation;
    retreating->action_state = FIGURE_ACTION_148_FLEEING;
    retreating->source_x = static_cast<unsigned char>(x + 3);
    retreating->source_y = static_cast<unsigned char>(y);
    retreating->progress_on_tile = 0;
    int expected_progress = 0;
    for (int step = 0; step < game_defines_enemy_retreat_speed_multiplier(); ++step) expected_progress = figure_movement_advance_tile_progress(expected_progress);
    figure_enemy43_spear_action(retreating);
    require(retreating->progress_on_tile == expected_progress, "Fleeing must consume exactly the mod-defined movement multiplier");
    retreating->remove();
    formation_get(retreat_formation)->remove();
    city_warning_clear_all();
    std::fprintf(stdout, "Retreat movement multiplier contract passed: multiplier=%d.\n", game_defines_enemy_retreat_speed_multiplier());
    std::fprintf(stdout, "Storage foundation and D12 bridge demolition contracts passed.\n");
    building_construction_clear_type();
    std::fprintf(stdout, "Placement support contracts passed: four rotations, wall supersession cleanup, blocker road retention and undo, and plazas restricted to existing roads.\n");
}
