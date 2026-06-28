#include "building/construction_area_tile.h"

#include "building/building_type_registry_internal.h"
#include "game/undo.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/terrain.h"
#include "map/tile_runtime_api.h"
#include "map/tiles.h"

#include <algorithm>

using TileDefinition = building_type_registry_impl::TileDefinition;
using TilePlacementBehavior = building_type_registry_impl::TilePlacementBehavior;

void ConstructionAreaTilePlacement::Region::include(const Region &other)
{
    if (!other.active) {
        return;
    }
    if (!active) {
        *this = other;
        return;
    }
    x_min = std::min(x_min, other.x_min);
    y_min = std::min(y_min, other.y_min);
    x_max = std::max(x_max, other.x_max);
    y_max = std::max(y_max, other.y_max);
}

ConstructionAreaTilePlacement::Region ConstructionAreaTilePlacement::GardenPreviewState::consume()
{
    Region previous = region_;
    if (!region_.active) {
        return previous;
    }

    for (int y = region_.y_min; y <= region_.y_max; y++) {
        for (int x = region_.x_min; x <= region_.x_max; x++) {
            tile_runtime_clear(map_grid_offset(x, y));
        }
    }

    region_ = {};
    return previous;
}

void ConstructionAreaTilePlacement::GardenPreviewState::remember(const Region &region)
{
    region_ = region;
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

    Region previous_preview = garden_preview().consume();
    game_undo_restore_map(1);
    refresh_garden_preview_region(previous_preview);
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
    return { true, x_min_, y_min_, x_max_, y_max_ };
}

ConstructionAreaTilePlacement::GardenPreviewState &ConstructionAreaTilePlacement::garden_preview()
{
    static GardenPreviewState state;
    return state;
}

const TileDefinition *ConstructionAreaTilePlacement::tile_definition_for_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_tile() ? &definition->tile() : nullptr;
}

const TileDefinition *ConstructionAreaTilePlacement::tile_definition_for_kind(
    building_type_registry_impl::TileKind kind)
{
    for (const auto &definition : building_type_registry_impl::g_building_types) {
        if (definition && definition->has_tile() && definition->tile().kind() == kind) {
            return &definition->tile();
        }
    }
    return nullptr;
}

const TileDefinition *ConstructionAreaTilePlacement::garden_tile_definition()
{
    return tile_definition_for_kind(building_type_registry_impl::TileKind::Garden);
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
    if (!region.active) {
        return;
    }
    map_tiles_update_region_tile(region.x_min, region.y_min, region.x_max, region.y_max, tile);
}

void ConstructionAreaTilePlacement::refresh_garden_preview_region(const Region &region)
{
    const TileDefinition *garden_tile = garden_tile_definition();
    if (garden_tile) {
        refresh_garden_region(region, *garden_tile);
    }
}

int ConstructionAreaTilePlacement::place_garden_area(
    const Region &region,
    const TileDefinition &tile,
    bool preview)
{
    Region refresh_region = garden_preview().consume();
    game_undo_restore_map(1);
    refresh_garden_preview_region(refresh_region);
    refresh_region.include(region);

    int items_placed = 0;
    for (int y = region.y_min; y <= region.y_max; y++) {
        for (int x = region.x_min; x <= region.x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
                continue;
            }
            items_placed++;
            map_terrain_add(grid_offset, TERRAIN_GARDEN);
            if (tile.overgrown()) {
                map_property_mark_plaza_earthquake_or_overgrown_garden(grid_offset);
            }
        }
    }

    refresh_garden_region(refresh_region, tile);
    if (preview && items_placed > 0) {
        garden_preview().remember(region);
    }
    return items_placed;
}

int ConstructionAreaTilePlacement::place_plaza_area(const Region &region)
{
    int items_placed = 0;
    for (int y = region.y_min; y <= region.y_max; y++) {
        for (int x = region.x_min; x <= region.x_max; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, TERRAIN_ROAD) ||
                map_terrain_is(grid_offset, TERRAIN_WATER | TERRAIN_BUILDING | TERRAIN_AQUEDUCT)) {
                continue;
            }
            if (!map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
                items_placed++;
            }
            map_image_set(grid_offset, 0);
            map_property_mark_plaza_earthquake_or_overgrown_garden(grid_offset);
            map_property_set_multi_tile_size(grid_offset, 1);
            map_property_mark_draw_tile(grid_offset);
        }
    }
    return items_placed;
}

bool ConstructionAreaTilePlacement::updates_land_routing(building_type type)
{
    const TileDefinition *tile = tile_definition_for_type(type);
    return tile && tile->updates_land_routing();
}

void ConstructionAreaTilePlacement::restore_preview_map(building_type type)
{
    Region previous_preview = garden_preview().consume();
    game_undo_restore_map(1);
    refresh_garden_preview_region(previous_preview);

    const TileDefinition *tile = tile_definition_for_type(type);
    if (!tile) {
        return;
    }

    if (is_garden_area_tile(*tile)) {
        return;
    }
    refresh_area_tile_after_restore(*tile);
}
