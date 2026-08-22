#include "graphics/runtime_texture.h"

#include <math.h>

namespace {

render_logical_size scaled_logical_size(render_logical_size size, float scale)
{
    if (size.width <= 0 || size.height <= 0) {
        return {};
    }
    if (scale <= 0.0f) {
        return size;
    }
    return { static_cast<render_logical_unit>(roundf(size.width / scale)), static_cast<render_logical_unit>(roundf(size.height / scale)) };
}

managed_image_request make_managed_request(const RuntimeTextureDrawRequest &draw_request)
{
    const RuntimeDrawSlice &slice = draw_request.slice;
    managed_image_request request = {};
    request.handle = slice.handle;
    request.width = slice.width;
    request.height = slice.height;
    request.x_offset = slice.draw_offset_x;
    request.y_offset = slice.draw_offset_y;
    request.is_isometric = slice.is_isometric;
    request.x = draw_request.x;
    request.y = draw_request.y;
    request.logical_width = draw_request.logical_width > 0.0f ? draw_request.logical_width : static_cast<float>(slice.width);
    request.logical_height = draw_request.logical_height > 0.0f ? draw_request.logical_height : static_cast<float>(slice.height);
    request.fixed_logical_size = draw_request.fixed_logical_size;
    request.color = draw_request.color;
    request.domain = draw_request.domain;
    request.scaling_policy = draw_request.scaling_policy;
    request.destination_geometry_policy = draw_request.destination_geometry_policy;
    return request;
}

} // namespace

// Input: one runtime-native slice and integer screen-space placement.
// Output: no return value; the managed texture is drawn through the native renderer path only.
void runtime_texture_draw(
    const RuntimeDrawSlice &slice,
    int x,
    int y,
    color_t color,
    float scale,
    render_destination_geometry_policy destination_geometry_policy)
{
    if (!slice.is_valid()) {
        return;
    }

    render_domain domain = graphics_renderer()->get_render_domain();
    int is_pixel_domain = domain == RENDER_DOMAIN_PIXEL
        || domain == RENDER_DOMAIN_TOOLTIP_PIXEL
        || domain == RENDER_DOMAIN_SNAPSHOT_PIXEL;
    RuntimeTextureDrawRequest request = {};
    request.slice = slice;
    request.x = scale ? x / scale : static_cast<float>(x);
    request.y = scale ? y / scale : static_cast<float>(y);
    request.logical_width = scale ? slice.width / scale : static_cast<float>(slice.width);
    request.logical_height = scale ? slice.height / scale : static_cast<float>(slice.height);
    request.fixed_logical_size = scaled_logical_size(slice.fixed_logical_size, scale);
    request.color = color;
    request.domain = domain;
    request.scaling_policy = is_pixel_domain ? RENDER_SCALING_POLICY_PIXEL_ART : RENDER_SCALING_POLICY_AUTO;
    request.destination_geometry_policy = destination_geometry_policy;
    runtime_texture_draw_request(request);
}

// Input: one runtime-native slice plus fully-specified logical dimensions and render state.
// Output: no return value; the slice is submitted directly to the renderer without legacy image helpers.
void runtime_texture_draw_request(
    const RuntimeDrawSlice &slice,
    float x,
    float y,
    float logical_width,
    float logical_height,
    color_t color,
    render_domain domain,
    render_scaling_policy scaling_policy,
    render_destination_geometry_policy destination_geometry_policy)
{
    RuntimeTextureDrawRequest request = {};
    request.slice = slice;
    request.x = x;
    request.y = y;
    request.logical_width = logical_width;
    request.logical_height = logical_height;
    request.color = color;
    request.domain = domain;
    request.scaling_policy = scaling_policy;
    request.destination_geometry_policy = destination_geometry_policy;
    runtime_texture_draw_request(request);
}

void runtime_texture_draw_request(const RuntimeTextureDrawRequest &draw_request)
{
    if (!draw_request) {
        return;
    }

    managed_image_request request = make_managed_request(draw_request);
    graphics_renderer()->draw_managed_image_request(&request);
}
