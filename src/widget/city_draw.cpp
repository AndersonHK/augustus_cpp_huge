#include "widget/city_draw.h"

#include "assets/image_group_entry.h"
#include "assets/image_group_payload.h"
#include "building/building.h"
#include "building/building_record.h"
#include "game/animation.h"
#include "graphics/runtime_texture.h"
#include "map/building.h"
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

} // namespace

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
