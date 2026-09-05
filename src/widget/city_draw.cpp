#include "widget/city_draw.h"

#include "assets/assets.h"
#include "assets/image_group_entry.h"
#include "building/building.h"
#include "figure/FigureGraphics.h"
#include "game/performance_tracker.h"
#include "game/resource.h"
#include "graphics/image.h"
#include "graphics/runtime_texture.h"
#include "city/view.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/terrain.h"
#include "map/tile_runtime_graphics.h"
#include "map/tiles.h"

namespace {

#define ADJACENT_OFFSET(x, y) (x + GRID_SIZE * y)
const int kAdjacentDeletionOffsets[2][4][7] = {
    {
        {ADJACENT_OFFSET(-1, 0), ADJACENT_OFFSET(-1, -1), ADJACENT_OFFSET(-1, -2), ADJACENT_OFFSET(0, -2), ADJACENT_OFFSET(1, -2)},
        {ADJACENT_OFFSET(0, -1), ADJACENT_OFFSET(1, -1), ADJACENT_OFFSET(2, -1), ADJACENT_OFFSET(2, 0), ADJACENT_OFFSET(2, 1)},
        {ADJACENT_OFFSET(1, 0), ADJACENT_OFFSET(1, 1), ADJACENT_OFFSET(1, 2), ADJACENT_OFFSET(0, 2), ADJACENT_OFFSET(-1, 2)},
        {ADJACENT_OFFSET(0, 1), ADJACENT_OFFSET(-1, 1), ADJACENT_OFFSET(-2, 1), ADJACENT_OFFSET(-2, 0), ADJACENT_OFFSET(-2, -1)}
    },
    {
        {ADJACENT_OFFSET(-1, 0), ADJACENT_OFFSET(-1, -1), ADJACENT_OFFSET(-1, -2), ADJACENT_OFFSET(-1, -3), ADJACENT_OFFSET(0, -3), ADJACENT_OFFSET(1, -3), ADJACENT_OFFSET(2, -3)},
        {ADJACENT_OFFSET(0, -1), ADJACENT_OFFSET(1, -1), ADJACENT_OFFSET(2, -1), ADJACENT_OFFSET(3, -1), ADJACENT_OFFSET(3, 0), ADJACENT_OFFSET(3, 1), ADJACENT_OFFSET(3, 2)},
        {ADJACENT_OFFSET(1, 0), ADJACENT_OFFSET(1, 1), ADJACENT_OFFSET(1, 2), ADJACENT_OFFSET(1, 3), ADJACENT_OFFSET(0, 3), ADJACENT_OFFSET(-1, 3), ADJACENT_OFFSET(-2, 3)},
        {ADJACENT_OFFSET(0, 1), ADJACENT_OFFSET(-1, 1), ADJACENT_OFFSET(-2, 1), ADJACENT_OFFSET(-3, 1), ADJACENT_OFFSET(-3, 0), ADJACENT_OFFSET(-3, -1), ADJACENT_OFFSET(-3, -2)}
    }
};
#undef ADJACENT_OFFSET

int has_adjacent_deletion(int grid_offset)
{
    const int size = map_property_legacy_multi_tile_size(grid_offset);
    if (size < 2 || size > 3) {
        return 0;
    }
    int total_adjacent_offsets = size * 2 + 1;
    const int *adjacent_offset = kAdjacentDeletionOffsets[size - 2][city_view_orientation() / 2];
    for (int i = 0; i < total_adjacent_offsets; ++i) {
        const int adjacent_grid_offset = grid_offset + adjacent_offset[i];
        if (map_property_is_deleted(adjacent_grid_offset) ||
            (map_building_exists_at(adjacent_grid_offset) &&
                city_draw_building_as_deleted(map_building_at(adjacent_grid_offset)))) {
            return 1;
        }
    }
    return 0;
}

} // namespace

void city_draw_grid_overlay(int x, int y, float scale)
{
    static int grid_id = 0;
    if (!grid_id) {
        grid_id = assets_get_image_id("UI\\Grid_Full", "Grid_Full");
    }
    performance_tracker_record_render_metric(PERFORMANCE_TRACKER_RENDER_METRIC_GRID_OVERLAYS, 1);
    Image::from_id(grid_id).draw(x, y, COLOR_GRID, scale);
}

void city_draw_prepare_render_tile_rows(CityViewRenderCommandBuffer &commands)
{
    PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ROW);
    commands.build();
}

void city_draw_render_tile_rows(const CityViewRenderCommandBuffer &commands, const CityViewRenderPhase *phases, int phase_count)
{
    PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ROW);
    commands.execute(phases, phase_count);
}

void city_draw_depot_resource(const Building &building, int x, int y, float scale)
{
    static const ImageGroupEntryRef cat =
        ImageGroupEntryRef::from_group("Admin_Logistics\\Cart_Depot_Cat", "Cart_Depot_Cat");
    const ImageGroupEntryRef &image = building.worker_count() ?
        figure_type_registry_impl::FigureGraphics::resource_cart_image(building.depot_order().resource_type, 1) :
        cat;
    image.draw(x + 11, y, COLOR_MASK_NONE, scale);
}

int city_draw_building_as_deleted(const Building &building)
{
    Building *owner = building.type && building.type->bridge().is_bridge() ?
        &building.dynamic_bridge_owner() :
        (building.Composition ? building.Composition->owner() : const_cast<Building *>(&building));
    return owner && owner->id && (owner->is_deleted() || map_property_is_deleted(owner->grid_offset()));
}

int city_draw_is_multi_tile_terrain(int grid_offset)
{
    // This path intentionally describes ownerless legacy/editor terrain.
    // Published buildings use their bound Foundation rendering path.
    return !map_building_exists_at(grid_offset) && map_property_legacy_multi_tile_size(grid_offset) > 1;
}

int city_draw_should_draw_top_before_deletion(int grid_offset)
{
    return city_draw_is_multi_tile_terrain(grid_offset) && has_adjacent_deletion(grid_offset);
}

int city_draw_terrain_foundation_footprint(
    int grid_offset, int x, int y, color_t color_mask, float scale)
{
    // Bridge tiles deliberately carry both water and road terrain bits. Draw
    // the water below the bridge here, not the traversable road deck above it.
    if (map_terrain_is(grid_offset, TERRAIN_WATER) && map_is_bridge(grid_offset)) {
        const int image_id = map_image_at(grid_offset);
        if (image_id) {
            Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale, RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE);
            return 1;
        }
    }

    if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        Image::from_id(map_tiles_road_surface_image_id(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale, RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE);
        return 1;
    }
    if (city_draw_runtime_tile_footprint(grid_offset, x, y, color_mask, scale)) {
        return 1;
    }
    const int image_id = map_image_at(grid_offset);
    if (!image_id) {
        return 0;
    }
    Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale, RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE);
    return 1;
}

int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale)
{
    const RuntimeDrawSlice *slice = tile_runtime_get_graphic_footprint_slice(grid_offset);
    if (!slice) {
        return 0;
    }

    runtime_texture_draw(*slice, x, y, color_mask, scale, RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE);
    return 1;
}

int city_draw_runtime_tile_top(int grid_offset, int x, int y, color_t color_mask, float scale)
{
    if (!tile_runtime_has_graphic(grid_offset)) {
        return 0;
    }

    const RuntimeDrawSlice *slice = tile_runtime_get_graphic_top_slice(grid_offset);
    if (slice) {
        runtime_texture_draw(*slice, x, y, color_mask, scale);
    }
    return 1;
}
