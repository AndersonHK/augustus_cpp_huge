#pragma once

#include "graphics/color.h"
#include "graphics/renderer.h"


struct RuntimeDrawSlice {
    image_handle handle = 0;
    int width = 0;
    int height = 0;
    int draw_offset_x = 0;
    int draw_offset_y = 0;
    int is_isometric = 0;

    int is_valid() const
    {
        return handle > 0 && width > 0 && height > 0;
    }
};

struct RuntimeTextureDrawRequest {
    RuntimeDrawSlice slice;
    float x = 0.0f;
    float y = 0.0f;
    float logical_width = 0.0f;
    float logical_height = 0.0f;
    color_t color = COLOR_MASK_NONE;
    render_domain domain = RENDER_DOMAIN_PIXEL;
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;

    bool has_explicit_logical_size() const
    {
        return logical_width > 0.0f && logical_height > 0.0f;
    }

    explicit operator bool() const
    {
        return slice.is_valid();
    }
};

void runtime_texture_draw(
    const RuntimeDrawSlice &slice,
    int x,
    int y,
    color_t color = COLOR_MASK_NONE,
    float scale = 1.0f);
void runtime_texture_draw_request(
    const RuntimeDrawSlice &slice,
    float x,
    float y,
    float logical_width,
    float logical_height,
    color_t color,
    render_domain domain,
    render_scaling_policy scaling_policy);
void runtime_texture_draw_request(const RuntimeTextureDrawRequest &request);
