#pragma once

#include "core/direction.h"
#include "figure/movement.h"
#include "graphics/renderer.h"

#include <stdint.h>

struct render_logical_point {
    render_logical_unit x = 0;
    render_logical_unit y = 0;
};

struct render_screen_point {
    int x = 0;
    int y = 0;
};

class OrthographicCamera {
public:
    explicit OrthographicCamera(int orientation) : orientation_(orientation) {}

    render_logical_point figure_tile_progress_offset(int direction, int normalized_progress) const
    {
        if (normalized_progress >= FIGURE_MOVEMENT_NORMALIZED_ONE) {
            return {};
        }
        const int progress = (normalized_progress * FIGURE_LEGACY_TILE_PROGRESS_MAX + FIGURE_MOVEMENT_NORMALIZED_ONE / 2) / FIGURE_MOVEMENT_NORMALIZED_ONE;
        int x = 0;
        int y = 0;
        switch (direction) {
            case DIR_0_TOP:
            case DIR_2_RIGHT: x = 2 * progress - 28; break;
            case DIR_1_TOP_RIGHT: x = 4 * progress - 56; break;
            case DIR_4_BOTTOM:
            case DIR_6_LEFT: x = 28 - 2 * progress; break;
            case DIR_5_BOTTOM_LEFT: x = 56 - 4 * progress; break;
            default: break;
        }
        switch (direction) {
            case DIR_0_TOP:
            case DIR_6_LEFT: y = 14 - progress; break;
            case DIR_2_RIGHT:
            case DIR_4_BOTTOM: y = progress - 14; break;
            case DIR_3_BOTTOM_RIGHT: y = 2 * progress - 28; break;
            case DIR_7_TOP_LEFT: y = 28 - 2 * progress; break;
            default: break;
        }
        return from_legacy_pixels(x, y);
    }

    render_logical_point figure_cross_country_offset(int normalized_x, int normalized_y) const
    {
        const int x = (normalized_x * FIGURE_LEGACY_TILE_PROGRESS_MAX + FIGURE_MOVEMENT_NORMALIZED_ONE / 2) / FIGURE_MOVEMENT_NORMALIZED_ONE;
        const int y = (normalized_y * FIGURE_LEGACY_TILE_PROGRESS_MAX + FIGURE_MOVEMENT_NORMALIZED_ONE / 2) / FIGURE_MOVEMENT_NORMALIZED_ONE;
        int screen_x = 0;
        int screen_y = 0;
        if (orientation_ == DIR_0_TOP || orientation_ == DIR_4_BOTTOM) {
            const int base_x = 2 * x - 2 * y;
            const int base_y = x + y;
            screen_x = orientation_ == DIR_0_TOP ? base_x : -base_x;
            screen_y = orientation_ == DIR_0_TOP ? base_y : -base_y;
        } else {
            const int base_x = 2 * x + 2 * y;
            const int base_y = x - y;
            screen_x = orientation_ == DIR_2_RIGHT ? base_x : -base_x;
            screen_y = orientation_ == DIR_6_LEFT ? base_y : -base_y;
        }
        return from_legacy_pixels(screen_x, screen_y);
    }

    render_screen_point to_legacy_screen(render_logical_point point) const
    {
        return { round_logical(point.x), round_logical(point.y) };
    }

    int64_t conservative_depth(int tile_x, int tile_y, int elevation) const
    {
        return (static_cast<int64_t>(tile_x + tile_y) << 32) + (static_cast<int64_t>(elevation) << 16) + tile_x;
    }

private:
    static render_logical_point from_legacy_pixels(int x, int y) { return { x * RENDER_LOGICAL_UNITS_PER_PIXEL, y * RENDER_LOGICAL_UNITS_PER_PIXEL }; }
    static int round_logical(render_logical_unit value) { return value >= 0 ? (value + RENDER_LOGICAL_UNITS_PER_PIXEL / 2) / RENDER_LOGICAL_UNITS_PER_PIXEL : -((-value + RENDER_LOGICAL_UNITS_PER_PIXEL / 2) / RENDER_LOGICAL_UNITS_PER_PIXEL); }

    int orientation_ = DIR_0_TOP;
};
