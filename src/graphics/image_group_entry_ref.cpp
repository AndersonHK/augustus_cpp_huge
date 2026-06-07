#include "graphics/image.h"

#include "assets/assets.h"
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

int ImageGroupEntryRef::image_id() const
{
    if (cached_image_id_ > 0) {
        return cached_image_id_;
    }
    if (group_path_.empty()) {
        return 0;
    }

    const char *image_name = entry_id_.empty() ? nullptr : entry_id_.c_str();
    if (!image_name && image_group_payload_load(group_path_.c_str())) {
        if (const ImageGroupPayload *payload = image_group_payload_get(group_path_.c_str())) {
            image_name = payload->default_image_id();
        }
    }
    cached_image_id_ = assets_get_image_id_from_path_or_name(group_path_.c_str(), image_name);
    return cached_image_id_;
}

const Image &ImageGroupEntryRef::image() const
{
    return Image::from_id(image_id());
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
    return image().runtime_slice();
}

RuntimeDrawSlice ImageGroupEntryRef::top_runtime_slice() const
{
    if (const ImageGroupEntry *resolved_entry = entry()) {
        if (const RuntimeDrawSlice *slice = resolved_entry->top()) {
            return *slice;
        }
    }
    if (const Image *top = image().top()) {
        return top->runtime_slice();
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
    image().draw_scaled_centered(x, y, color, draw_scale_percent);
}
