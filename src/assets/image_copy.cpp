#include "core/image.h"

#include <cstring>

namespace {

void determine_max_width_and_height(const image_copy_info *copy, int *width, int *height)
{
    int max_src_width = copy->src.width - copy->src.x;
    int max_dst_width = copy->dst.width - copy->dst.x;
    *width = copy->rect.width > max_src_width ? max_src_width : copy->rect.width;
    if (*width > max_dst_width) {
        *width = max_dst_width;
    }
    int max_src_height = copy->src.height - copy->src.y;
    int max_dst_height = copy->dst.height - copy->dst.y;
    *height = copy->rect.height > max_src_height ? max_src_height : copy->rect.height;
    if (*height > max_dst_height) {
        *height = max_dst_height;
    }
}

} // namespace

void image_copy(const image_copy_info *copy)
{
    int width, height;
    determine_max_width_and_height(copy, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }
    color_t *dst = &copy->dst.pixels[(copy->dst.y + copy->rect.y_offset) * copy->dst.width +
        copy->dst.x + copy->rect.x_offset];
    const color_t *src = &copy->src.pixels[copy->src.y * copy->src.width + copy->src.x];
    for (int y = 0; y < height; y++) {
        memcpy(&dst[y * copy->dst.width], &src[y * copy->src.width], width * sizeof(color_t));
    }
}

void image_copy_isometric_footprint(const image_copy_info *copy)
{
    int width, height;
    determine_max_width_and_height(copy, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }
    int half_height = height / 2;
    color_t *dst = &copy->dst.pixels[(copy->dst.y + copy->rect.y_offset) * copy->dst.width +
        copy->dst.x + copy->rect.x_offset];
    const color_t *src = &copy->src.pixels[copy->src.y * copy->src.width + copy->src.x];

    for (int y = 0; y < height; y++) {
        int x_read = 2 + 4 * (y < half_height ? y : height - 1 - y);
        int x_skip = (copy->rect.width - x_read) / 2;
        if (x_skip > width) {
            continue;
        } else if (x_skip + x_read > width) {
            x_read = width - x_skip;
        }
        memcpy(&dst[y * copy->dst.width] + x_skip, &src[y * copy->src.width + x_skip], x_read * sizeof(color_t));
    }
}
