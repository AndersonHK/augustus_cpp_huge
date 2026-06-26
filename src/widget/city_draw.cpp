#include "widget/city_draw.h"

#include "assets/assets.h"
#include "assets/image_group_entry.h"
#include "assets/image_group_payload.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/granary.h"
#include "game/animation.h"
#include "game/performance_tracker.h"
#include "game/resource.h"
#include "game/resource_graphics.h"
#include "graphics/image.h"
#include "graphics/runtime_texture.h"
#include "city/view.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/tile_runtime_graphics.h"

namespace {

struct StorageFlagAsset {
    const char *group_path;
    const char *image_id;
};

const StorageFlagAsset kStorageFlagAssets[] = {
    { nullptr, nullptr },
    { "UI\\Warehouse_Flag_Market_1", "Warehouse_Flag_Market" },
    { "UI\\Warehouse_Flag_Land_1", "Warehouse_Flag_Land" },
    { "UI\\Warehouse_Flag_Market_Land_1", "Warehouse_Flag_Market_Land" },
    { "UI\\Warehouse_Flag_Sea_1", "Warehouse_Flag_Sea" },
    { "UI\\Warehouse_Flag_Market_Sea_1", "Warehouse_Flag_Market_Sea" },
    { "UI\\Warehouse_Flag_Land_Sea_1", "Warehouse_Flag_Land_Sea" },
    { "UI\\Warehouse_Flag_All_1", "Warehouse_Flag_All" },
};

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

const RuntimeDrawSlice *storage_flag_slice(const ImageGroupEntry &entry, int frame)
{
    if (frame <= 0 || !entry.has_animation()) {
        return entry.footprint();
    }

    const RuntimeAnimationTrack &track = entry.animation();
    if (frame <= track.num_frames && frame <= static_cast<int>(track.frames.size())) {
        return &track.frames[frame - 1];
    }
    return entry.footprint();
}

void advance_storage_flag(Building &building, const RuntimeAnimationTrack &track)
{
    ::building *record = building_get(building.id());
    if (!record || track.num_frames <= 0 || !track.speed_id) {
        return;
    }
    if (game_animation_should_advance(track.speed_id)) {
        record->data.warehouse.flag_frame++;
    }
    if (record->data.warehouse.flag_frame > track.num_frames) {
        record->data.warehouse.flag_frame = 0;
    }
}

int has_adjacent_deletion(int grid_offset)
{
    int size = map_property_multi_tile_size(grid_offset);
    int total_adjacent_offsets = size * 2 + 1;
    const int *adjacent_offset = kAdjacentDeletionOffsets[size - 2][city_view_orientation() / 2];
    for (int i = 0; i < total_adjacent_offsets; ++i) {
        if (map_property_is_deleted(grid_offset + adjacent_offset[i]) ||
            city_draw_building_as_deleted(Building(building_get(map_building_at(grid_offset + adjacent_offset[i]))))) {
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
        resource_graphics(building.depot_order().resource_type).cart_image(1) :
        cat;
    image.draw(x + 11, y, COLOR_MASK_NONE, scale);
}

void city_draw_warehouse_ornaments(int x, int y, color_t color_mask, float scale)
{
    Image::from_id(Image::group(GROUP_BUILDING_WAREHOUSE) + 17).draw(x - 4, y - 42, color_mask, scale);
}

void city_draw_granary_stores(const image &image, Building &building, int x, int y, color_t color_mask, float scale)
{
    if (image.animation) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 1).draw(
            x + image.animation->sprite_offset_x,
            y + 60 + image.animation->sprite_offset_y - image.height,
            color_mask,
            scale);
    }

    if (building.resource_amount(RESOURCE_NONE) < FULL_GRANARY) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 2).draw(x + 33, y - 60, color_mask, scale);
        if (building.resource_amount(RESOURCE_NONE) < THREEQUARTERS_GRANARY) {
            Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 3).draw(x + 56, y - 50, color_mask, scale);
        }
        if (building.resource_amount(RESOURCE_NONE) < HALF_GRANARY) {
            Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 4).draw(x + 91, y - 50, color_mask, scale);
        }
        if (building.resource_amount(RESOURCE_NONE) < QUARTER_GRANARY) {
            Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 5).draw(x + 117, y - 62, color_mask, scale);
        }
    }
    city_draw_storage_permission_flag(building, x + 81, y - 101, color_mask, scale);
}

int city_draw_building_as_deleted(const Building &building)
{
    Building main_building = building.main();
    return main_building.id() && (main_building.is_deleted() || map_property_is_deleted(main_building.grid_offset()));
}

int city_draw_is_multi_tile_terrain(int grid_offset)
{
    return !map_building_at(grid_offset) && map_property_multi_tile_size(grid_offset) > 1;
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

int city_draw_storage_permission_flag(Building &building, int x, int y, color_t color_mask, float scale)
{
    if (building.has_plague()) {
        return 0;
    }

    const int permissions = building.blocked_storage_permission_mask();
    if (permissions <= 0 || permissions >= static_cast<int>(sizeof(kStorageFlagAssets) / sizeof(kStorageFlagAssets[0]))) {
        return 0;
    }

    const StorageFlagAsset &asset = kStorageFlagAssets[permissions];
    if (!image_group_payload_load(asset.group_path)) {
        return 0;
    }

    const ImageGroupPayload *payload = image_group_payload_get(asset.group_path);
    const ImageGroupEntry *entry = payload ? payload->entry_for(asset.image_id) : nullptr;
    if (!entry) {
        return 0;
    }

    const RuntimeDrawSlice *slice = storage_flag_slice(*entry, building.warehouse_flag_frame());
    if (slice && slice->is_valid()) {
        runtime_texture_draw(*slice, x, y, color_mask, scale);
    }
    if (entry->has_animation()) {
        advance_storage_flag(building, entry->animation());
    }
    return 1;
}
