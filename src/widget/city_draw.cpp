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
#include "map/building.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/tile_runtime_graphics.h"

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
    int size = map_property_multi_tile_size(grid_offset);
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

void city_draw_main_render_tile_row(
    city_view_render_tile_callback *callback1,
    city_view_render_tile_callback *callback2,
    city_view_render_tile_callback *callback3,
    performance_tracker_bucket bucket1,
    performance_tracker_bucket bucket2,
    performance_tracker_bucket bucket3)
{
    PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ROW);
    // TODO(renderer-command-list): publish ordered RuntimeTextureDrawRequest payloads for runtime-native slices; keep legacy image calls in row order until they have an equivalent payload.
    const CityViewRenderPhase phases[] = {
        { callback1, bucket1 },
        { callback2, bucket2 },
        { callback3, bucket3 },
    };
    city_view_foreach_valid_render_tile_row(phases, 3);
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
    Building main_building = building.main();
    return main_building.id && (main_building.is_deleted() || map_property_is_deleted(main_building.grid_offset()));
}

int city_draw_is_multi_tile_terrain(int grid_offset)
{
    return !map_building_exists_at(grid_offset) && map_property_multi_tile_size(grid_offset) > 1;
}

int city_draw_should_draw_top_before_deletion(int grid_offset)
{
    return city_draw_is_multi_tile_terrain(grid_offset) && has_adjacent_deletion(grid_offset);
}

int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale)
{
    const RuntimeDrawSlice *slice = tile_runtime_get_graphic_footprint_slice(grid_offset);
    if (!slice) {
        return 0;
    }

    runtime_texture_draw(*slice, x, y, color_mask, scale);
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
