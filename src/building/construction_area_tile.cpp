#include "building/construction_area_tile.h"

#include "building/building_type_registry_internal.h"
#include "building/construction_plan.h"
#include "building/properties.h"
#include "game/undo.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/terrain.h"
#include "map/tiles.h"

using TileDefinition = building_type_registry_impl::TileDefinition;
using TilePlacementBehavior = building_type_registry_impl::TilePlacementBehavior;

int ConstructionAreaTilePlacement::GardenTileSet::placed_count() const
{
    return static_cast<int>(grid_offsets_.size());
}

void ConstructionAreaTilePlacement::GardenTileSet::add(int grid_offset)
{
    grid_offsets_.push_back(grid_offset);
}

void ConstructionAreaTilePlacement::GardenTileSet::refresh_preview_tiles() const
{
    if (grid_offsets_.empty()) {
        return;
    }
    map_tiles_update_garden_preview_tiles(
        grid_offsets_.data(),
        static_cast<int>(grid_offsets_.size()));
}

ConstructionAreaTilePlacement::ConstructionAreaTilePlacement(
    int x_start,
    int y_start,
    int x_end,
    int y_end,
    building_type type,
    bool preview)
    : type_(type),
      preview_(preview)
{
    map_grid_start_end_to_area(x_start, y_start, x_end, y_end, &x_min_, &y_min_, &x_max_, &y_max_);
}

int ConstructionAreaTilePlacement::place()
{
    const TileDefinition *tile = tile_definition_for_type(type_);
    if (!tile) {
        return 0;
    }

    if (is_garden_area_tile(*tile)) {
        return place_garden_area(region(), *tile, preview_);
    }

    game_undo_restore_map(1);
    refresh_area_tile_after_restore(*tile);

    int items_placed = 0;
    switch (tile->placement_behavior()) {
        case TilePlacementBehavior::Plaza:
            items_placed = place_plaza_area(region());
            break;
        default:
            break;
    }

    map_tiles_update_area_placement_tile(x_min_, y_min_, x_max_, y_max_, *tile);
    return items_placed;
}

bool ConstructionAreaTilePlacement::is_area_tile_type(building_type type)
{
    const TileDefinition *tile = tile_definition_for_type(type);
    return tile && tile->is_area_placement();
}

ConstructionAreaTilePlacement::Region ConstructionAreaTilePlacement::region() const
{
    return { x_min_, y_min_, x_max_, y_max_ };
}

const TileDefinition *ConstructionAreaTilePlacement::tile_definition_for_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_tile() ? &definition->tile() : nullptr;
}

bool ConstructionAreaTilePlacement::is_garden_area_tile(const TileDefinition &tile)
{
    return tile.placement_behavior() == TilePlacementBehavior::Garden;
}

void ConstructionAreaTilePlacement::refresh_area_tile_after_restore(const TileDefinition &tile)
{
    if (is_garden_area_tile(tile)) {
        return;
    }
    map_tiles_update_all_tile(tile);
}

void ConstructionAreaTilePlacement::refresh_garden_region(const Region &region, const TileDefinition &tile)
{
    map_tiles_update_region_tile(region.x_min, region.y_min, region.x_max, region.y_max, tile);
}

int ConstructionAreaTilePlacement::place_garden_area(
    const Region &region,
    const TileDefinition &tile,
    bool preview)
{
    game_undo_restore_map(1);

    GardenTileSet placed_tiles;
    for (int y = region.y_min; y <= region.y_max; y++) {
        for (int x = region.x_min; x <= region.x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                continue;
            }
            placed_tiles.add(grid_offset);
            map_terrain_add(grid_offset, TERRAIN_GARDEN);
            if (tile.overgrown()) {
                map_property_mark_plaza_earthquake_or_overgrown_garden(grid_offset);
            }
        }
    }

    int items_placed = placed_tiles.placed_count();
    if (preview && items_placed > 0) {
        placed_tiles.refresh_preview_tiles();
    } else if (!preview && items_placed > 0) {
        refresh_garden_region(region, tile);
    }
    return items_placed;
}

int ConstructionAreaTilePlacement::place_plaza_area(const Region &region)
{
    int items_placed = 0;
    support_cost_ = 0;
    for (int y = region.y_min; y <= region.y_max; y++) {
        for (int x = region.x_min; x <= region.x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            const building_construction::ConstructionPlacementPlan plan(type_, x, y, 1, 0);
            if (!plan.can_place() || map_terrain_is(grid_offset, TERRAIN_WATER | TERRAIN_BUILDING | TERRAIN_AQUEDUCT)) continue;
            if (!map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
                items_placed++;
                support_cost_ += plan.support_cost();
            }
            map_terrain_add(grid_offset, TERRAIN_ROAD);
            map_image_set(grid_offset, 0);
            map_property_mark_plaza_earthquake_or_overgrown_garden(grid_offset);
            map_property_set_legacy_multi_tile_size(grid_offset, 1);
            map_property_mark_draw_tile(grid_offset);
        }
    }
    return items_placed;
}

bool ConstructionAreaTilePlacement::updates_land_routing(building_type type)
{
    const TileDefinition *tile = tile_definition_for_type(type);
    return tile && (tile->updates_land_routing() || tile->placement_behavior() == TilePlacementBehavior::Plaza);
}

void ConstructionAreaTilePlacement::restore_preview_map(building_type type)
{
    game_undo_restore_map(1);

    const TileDefinition *tile = tile_definition_for_type(type);
    if (!tile) {
        return;
    }

    if (is_garden_area_tile(*tile)) {
        return;
    }
    refresh_area_tile_after_restore(*tile);
}
