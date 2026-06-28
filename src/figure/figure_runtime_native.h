#pragma once

#include "figure/figure_type_registry_internal.h"
#include "graphics/runtime_texture.h"

#include <memory>

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

class FigureMapFlagNumberOverlay {
public:
    void set_from_resource_id(int resource_id);
    void draw(int x, int y, float scale) const;

private:
    static constexpr int kXOffset = 6;
    static constexpr int kYOffset = 7;

    static int number_from_resource_id(int resource_id);

    int number_ = 0;
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
    FigureMapFlagNumberOverlay map_flag_number_overlay = {};

    bool add_layer(const FigureGraphicDrawLayer &layer);
    bool add_layer(const figure_type_registry_impl::FigureGraphicsLayer &layer);
    void draw(int x, int y, color_t color_mask, float scale) const;

private:
    void draw_layers(int x, int y, color_t color_mask, float scale, int draw_before_base) const;
    static void draw_runtime_slice(
        const RuntimeDrawSlice &slice,
        int x,
        int y,
        color_t color,
        float scale,
        render_logical_size fixed_logical_size,
        render_scaling_policy scaling_policy);
};

namespace figure_runtime_native_impl {

class NativeFigure {
public:
    NativeFigure(
        Figure *data,
        const figure_type_registry_impl::FigureTypeDefinition *definition,
        const figure_type_registry_impl::FigureTypeProfile *profile)
        : figure_(data)
        , definition_(definition)
        , profile_(profile)
    {
    }

    virtual ~NativeFigure() = default;

    void set_figure(Figure *data)
    {
        figure_ = data;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition() const
    {
        return definition_;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile() const
    {
        return profile_;
    }

    virtual int execute() = 0;

protected:
    Figure *data_figure() const
    {
        return figure_;
    }

private:
    Figure *figure_ = nullptr;
    const figure_type_registry_impl::FigureTypeDefinition *definition_ = nullptr;
    const figure_type_registry_impl::FigureTypeProfile *profile_ = nullptr;
};

// FigureType profiles own native walker behavior; this factory keeps runtime binding separate from controller actions.
std::unique_ptr<NativeFigure> make_controller(
    Figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    const figure_type_registry_impl::FigureTypeProfile *profile);

road_service_effect primary_service_effect_for_profile(
    const figure_type_registry_impl::FigureTypeProfile *profile);

bool is_road_history_tile(int grid_offset);

} // namespace figure_runtime_native_impl

bool figure_graphics_resolve_draw_request(const Figure &figure, FigureGraphicDrawRequest &request);
