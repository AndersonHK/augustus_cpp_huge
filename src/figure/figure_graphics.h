#pragma once

#include "graphics/runtime_texture.h"

class Figure;

struct FigureGraphicDrawLayer {
    RuntimeDrawSlice slice = {};
    int x_offset = 0;
    int y_offset = 0;
    int draw_before_base = 0;
    int use_figure_color_mask = 1;
    render_logical_size fixed_logical_size = {};
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;
};

struct FigureGraphicDrawRequest {
    static constexpr int MAX_LAYERS = 4;

    RuntimeDrawSlice base_slice = {};
    FigureGraphicDrawLayer layers[MAX_LAYERS] = {};
    int layer_count = 0;
    int sprite_offset_x = 0;
    int sprite_offset_y = 0;
    render_logical_size fixed_logical_size = {};
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;
};

class FigureGraphics {
public:
    explicit FigureGraphics(const Figure &figure)
        : figure_(figure)
    {
    }

    bool resolve(FigureGraphicDrawRequest &request) const;

private:
    const Figure &figure_;
};
