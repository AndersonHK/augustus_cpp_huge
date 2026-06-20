#include "graphics/image.h"

#include "assets/image_group_entry.h"
#include "assets/image_group_payload.h"
#include "graphics/runtime_texture.h"

#include <utility>

ImageGroupEntryRef ImageGroupEntryRef::from_group(std::string group_path, std::string entry_id)
{
    ImageGroupEntryRef ref;
    ref.group_path_ = std::move(group_path);
    ref.entry_id_ = std::move(entry_id);
    return ref;
}

int ImageGroupEntryRef::is_bound() const
{
    return !group_path_.empty();
}

const std::string &ImageGroupEntryRef::group_path() const
{
    return group_path_;
}

const std::string &ImageGroupEntryRef::entry_id() const
{
    return entry_id_;
}

const ImageGroupEntry *ImageGroupEntryRef::entry() const
{
    if (group_path_.empty() || !image_group_payload_load(group_path_.c_str())) {
        return nullptr;
    }
    const ImageGroupPayload *payload = image_group_payload_get(group_path_.c_str());
    if (!payload) {
        return nullptr;
    }
    return entry_id_.empty() ? payload->default_entry() : payload->entry_for(entry_id_.c_str());
}

RuntimeDrawSlice ImageGroupEntryRef::runtime_slice() const
{
    if (const ImageGroupEntry *resolved_entry = entry()) {
        if (const RuntimeDrawSlice *slice = resolved_entry->footprint()) {
            return *slice;
        }
    }
    return RuntimeDrawSlice();
}

RuntimeDrawSlice ImageGroupEntryRef::top_runtime_slice() const
{
    if (const ImageGroupEntry *resolved_entry = entry()) {
        if (const RuntimeDrawSlice *slice = resolved_entry->top()) {
            return *slice;
        }
    }
    return RuntimeDrawSlice();
}

int ImageGroupEntryRef::width() const
{
    return runtime_slice().width;
}

int ImageGroupEntryRef::height() const
{
    return runtime_slice().height;
}

void ImageGroupEntryRef::draw(int x, int y, color_t color, float scale) const
{
    const RuntimeDrawSlice slice = runtime_slice();
    if (slice.is_valid()) {
        runtime_texture_draw(slice, x, y, color, scale);
    }
}

void ImageGroupEntryRef::draw_top(int x, int y, color_t color, float scale) const
{
    const RuntimeDrawSlice slice = top_runtime_slice();
    if (slice.is_valid()) {
        runtime_texture_draw(slice, x, y, color, scale);
    }
}

void ImageGroupEntryRef::draw_scaled_centered(int x, int y, color_t color, int draw_scale_percent) const
{
    const RuntimeDrawSlice slice = runtime_slice();
    if (!slice.is_valid()) {
        return;
    }

    const float scale = 100.0f / draw_scale_percent;
    const float scaled_x = (x + slice.width / 2.0f - (slice.width / scale) / 2.0f) * scale;
    const float scaled_y = (y + slice.height / 2.0f - (slice.height / scale) / 2.0f) * scale;
    runtime_texture_draw(slice, static_cast<int>(scaled_x), static_cast<int>(scaled_y), color, scale);
}
