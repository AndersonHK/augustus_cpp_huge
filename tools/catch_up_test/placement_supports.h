#pragma once
#include "building/construction_plan.h"
#include "building/construction_building.h"
#include "building/construction.h"
#include "building/construction_area_tile.h"
#include "building/properties.h"
#include "map/building.h"
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
    for (int yy = 3; yy < map_grid_height() - 5 && x < 0; ++yy) for (int xx = 3; xx < map_grid_width() - 5; ++xx) {
        bool clear = true;
        for (int dy = 0; dy < 4; ++dy) for (int dx = 0; dx < 4; ++dx) {
            const int offset = map_grid_offset(xx + dx, yy + dy);
            if (!map_grid_is_inside(xx + dx, yy + dy, 1) || map_terrain_is(offset, TERRAIN_NOT_CLEAR) || map_figure_at(offset)) clear = false;
        }
        if (clear) { x = xx; y = yy; break; }
    }
    require(x >= 0, "Support fixture requires a clear 4x4 area");
    const auto *tower = definition_for_type(type_from_attr("tower"));
    const auto *wall = definition_for_type(type_from_attr("wall"));
    require(tower && wall, "Support fixture requires tower and wall data");
    const int offset = map_grid_offset(x, y);
    const int original = map_terrain_get(offset);
    struct RestoreTerrain { int offset, terrain; ~RestoreTerrain() { map_terrain_set(offset, terrain); } } restore{offset, original};
    for (int rotation = 0; rotation < 4; ++rotation) {
        ConstructionPlacementPlan bare(*tower, x, y, 1, 0, rotation, nullptr, rotation, false, true);
        require(bare.can_place() && bare.support_cost() == 4 * model_get_construction_cost(wall->type()), "Bare tower must quote four full-price wall supports in every rotation");
    }
    map_terrain_add(offset, TERRAIN_WALL);
    ConstructionPlacementPlan partial(*tower, x, y, 1, 0);
    require(partial.can_place() && partial.support_cost() == 3 * model_get_construction_cost(wall->type()), "Pre-existing walls must not be charged twice");
    map_terrain_set(offset, original | TERRAIN_BUILDING | TERRAIN_WALL);
    require(!ConstructionPlacementPlan(*tower, x, y, 1, 0).can_place(), "Unowned building occupancy must not qualify as a supporting wall");
    map_terrain_set(offset, original);
    const auto *roadblock = definition_for_type(type_from_attr("roadblock"));
    if (roadblock) {
        ConstructionPlacementPlan road(*roadblock, x, y, 1, 0);
        require(road.can_place() && road.support_cost() == model_get_construction_cost(type_from_attr("road")), "Roadblock must quote a missing road");
    }
    FoundationDef proximity("test_proximity");
    proximity.set_dimensions(1, 1);
    FoundationCellDefinition cell;
    cell.added_terrain = TERRAIN_BUILDING;
    proximity.add_cell(cell);
    proximity.add_proximity_requirement({TERRAIN_WATER, 2, 2, 2, false, false});
    BuildingType candidate(tower->type(), "proximity_fixture");
    candidate.set_foundation_definition(&proximity);
    const int water1 = map_grid_offset(x + 2, y);
    const int water2 = map_grid_offset(x + 2, y + 1);
    RestoreTerrain restore_water1{water1, map_terrain_get(water1)}, restore_water2{water2, map_terrain_get(water2)};
    map_terrain_add(water1, TERRAIN_WATER);
    require(!ConstructionPlacementPlan(candidate, x, y, 1, 0).can_place(), "Proximity must enforce minimum matching tile count");
    map_terrain_add(water2, TERRAIN_WATER);
    require(ConstructionPlacementPlan(candidate, x, y, 1, 0).can_place(), "Proximity must accept two matching tiles at the specified distance");
    require(!ConstructionPlacementPlan(candidate, x + 1, y, 1, 0).can_place(), "Proximity must reject matching tiles inside the minimum distance");
    map_terrain_set(water1, restore_water1.terrain);
    map_terrain_set(water2, restore_water2.terrain);
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
        require(map_terrain_get(offset) == original && !map_building_exists_at(offset), "Hover support graphics must not mutate the map");
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
    require(map_terrain_is(offset, TERRAIN_WALL) && map_building_exists_at(offset), "Published tower must own its wall support");
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
    require(!map_building_exists_at(offset) && map_terrain_get(offset) == original, "Undo must restore pre-support terrain and ownership");
    const auto plaza = type_from_attr("plaza");
    if (plaza != BUILDING_NONE) {
        require(game_undo_start_build(plaza) != 0, "Could not start plaza support fixture");
        ConstructionAreaTilePlacement area(x, y, x + 1, y + 1, plaza, true);
        require(area.place() == 4 && area.support_cost() == 4 * model_get_construction_cost(type_from_attr("road")), "Plaza drag must quote all four missing roads");
        require(map_terrain_is(offset, TERRAIN_ROAD) && map_property_is_plaza_earthquake_or_overgrown_garden(offset), "Plaza preview must contain road and plaza terrain");
        ConstructionAreaTilePlacement::restore_preview_map(plaza);
        require(map_terrain_get(offset) == original, "Plaza preview cancellation must restore bare ground");
        game_undo_disable();
    }
    building_construction_clear_type();
    std::fprintf(stdout, "Placement support contracts passed: four rotations, partial walls, overlap rejection, road quote, single tower after wall refresh, publication and undo.\n");
}
