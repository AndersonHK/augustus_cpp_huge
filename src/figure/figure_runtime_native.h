#pragma once

#include "figure/figure_type_registry_internal.h"
#include "graphics/runtime_texture.h"

#include <memory>
#include <string>

class Figure;
struct building;

struct FigureGraphicDrawLayer {
    RuntimeDrawSlice slice = {};
    int x_offset = 0;
    int y_offset = 0;
    int draw_before_base = 0;
    int use_figure_color_mask = 1;
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;
};

class FigureMapFlagNumberOverlay {
public:
    void set(int number, GraphicsPoint offset);
    void draw(int x, int y, float scale, render_logical_unit logical_units_per_source_pixel) const;

private:
    int number_ = 0;
    GraphicsPoint offset_ = {};
};

struct FigureGraphicDrawRequest {
    static constexpr int MAX_LAYERS = 4;

    FigureGraphicDrawLayer layers[MAX_LAYERS] = {};
    int layer_count = 0;
    int sprite_offset_x = 0;
    int sprite_offset_y = 0;
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;
    FigureMapFlagNumberOverlay map_flag_number_overlay = {};
    int allows_empty = 0;
    int required_layer_count = 0;
    std::string missing_layer_role;

    void set_base_slice(RuntimeDrawSlice slice) { base_slice_ = slice; }
    bool has_base_slice() const { return base_slice_.is_valid(); }
    bool add_layer(const FigureGraphicDrawLayer &layer);
    bool add_layer(const figure_type_registry_impl::FigureGraphicsLayer &layer);
    bool has_drawable_content() const;
    bool is_semantically_complete() const;
    int scaled_sprite_offset_x() const;
    int scaled_sprite_offset_y() const;
    void draw(int x, int y, color_t color_mask, float scale) const;
    void draw_with_baseline_at_tile_center(int tile_x, int tile_y, color_t color_mask, float scale) const;

private:
    render_logical_unit logical_units_per_source_pixel() const;
    int scaled_offset(int offset) const;
    void draw_layers(int x, int y, color_t color_mask, float scale, int draw_before_base) const;
    static void draw_runtime_slice(const RuntimeDrawSlice &slice, int x, int y, color_t color, float scale, render_scaling_policy scaling_policy);

    RuntimeDrawSlice base_slice_ = {};
};

struct FigureGraphicsDebugCounters {
    unsigned long long facade_draw_requests = 0;
    unsigned long long unresolved_draw_requests = 0;
    unsigned long long incomplete_composite_requests = 0;
};

namespace figure_runtime_native_impl {

bool owner_binding_matches(const Figure *figure, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding);

class NativeFigure {
public:
    NativeFigure(Figure *data, const figure_type_registry_impl::FigureTypeDefinition *definition, const figure_type_registry_impl::FigureTypeProfile *profile)
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
std::unique_ptr<NativeFigure> make_controller(Figure *f, const figure_type_registry_impl::FigureTypeDefinition *definition, const figure_type_registry_impl::FigureTypeProfile *profile);

road_service_effect primary_service_effect_for_profile(const figure_type_registry_impl::FigureTypeProfile *profile);

bool is_road_history_tile(int grid_offset);

} // namespace figure_runtime_native_impl

bool figure_graphics_resolve_draw_request(const Figure &figure, FigureGraphicDrawRequest &request);
bool figure_graphics_validate_loaded_figures();
FigureGraphicsDebugCounters figure_graphics_debug_counters();
void figure_graphics_reset_debug_counters();
