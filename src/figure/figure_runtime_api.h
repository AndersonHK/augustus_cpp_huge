#pragma once

#include "figure/figure.h"
#include "figure/PathingMode.h"
#include "graphics/runtime_texture.h"

void figure_runtime_reset();
void figure_runtime_initialize_city();
void figure_runtime_on_created(Figure *f);
void figure_runtime_on_deleted(Figure *f);

// Creates a native FigureType walker using an explicit profile-owned start contract.
Figure *figure_runtime_create_profiled(
    figure_type type,
    int x,
    int y,
    direction_type dir,
    Building &building,
    const char *profile_id);

// Rebinds an existing native figure to a profile after legacy creation/load code sets owner state.
int figure_runtime_bind_profile(Figure *f, const char *profile_id);

// Applies the bound/default XML movement profile to legacy action walkers.
int figure_runtime_apply_profile_movement(Figure *f);

const figure_type_registry_impl::PathingPolicy *figure_runtime_pathing_policy(Figure *f);

// Executes a native FigureType controller; returns zero when legacy action should handle it.
int figure_runtime_execute(Figure *f);

// Updates legacy-action figures from their XML graphics policy.
int figure_runtime_update_graphics(Figure *f);

struct FigureGraphicDrawLayer {
    RuntimeDrawSlice slice = {};
    int x_offset = 0;
    int y_offset = 0;
    int draw_before_base = 0;
    int use_figure_color_mask = 1;
    color_t color = COLOR_MASK_NONE;

    bool has_slice() const
    {
        return slice.is_valid();
    }

    color_t draw_color(color_t figure_color_mask) const
    {
        return use_figure_color_mask ? figure_color_mask : color;
    }
};

struct FigureGraphicDrawRequest {
    static constexpr int MAX_LAYERS = 4;

    RuntimeDrawSlice base_slice = {};
    FigureGraphicDrawLayer layers[MAX_LAYERS] = {};
    int layer_count = 0;
    int sprite_offset_x = 0;
    int sprite_offset_y = 0;
    float logical_width = 0.0f;
    float logical_height = 0.0f;
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;

    bool has_base_slice() const
    {
        return base_slice.is_valid();
    }

    bool add_layer(const FigureGraphicDrawLayer &layer)
    {
        if (!layer.has_slice() || layer_count >= MAX_LAYERS) {
            return false;
        }
        layers[layer_count++] = layer;
        return true;
    }

    bool has_layers() const
    {
        return layer_count > 0;
    }

    bool has_explicit_logical_size() const
    {
        return logical_width > 0.0f && logical_height > 0.0f;
    }
};

// Returns nonzero when figure-owned graphics handles the draw; base_slice is the resolved base image when available.
int figure_runtime_graphic_draw_request(const Figure *f, FigureGraphicDrawRequest *request);

// Lets XML pathing policies override a vanilla roaming direction at intersections.
int figure_runtime_choose_roaming_direction(
    Figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction);

// Records pathing-only road recency for smart service walkers.
void figure_runtime_record_road_service_visit(Figure *f);
