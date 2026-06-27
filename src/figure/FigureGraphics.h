#pragma once

#include "assets/assets.h"
#include "figure/type.h"
#include "game/resource.h"
#include "graphics/GraphicsDefinition.h"
#include "graphics/image.h"
#include "graphics/renderer.h"

#include <array>
#include <string>
#include <vector>

class ImageGroupEntry;
class ImageGroupPayload;
class Animation;
class Figure;

namespace figure_type_registry_impl {

class FigureTypeDefinition;

enum class CartGraphicsMode {
    None,
    ResourceLoad
};

struct GraphicsTargetBinding {
    std::string path;
    std::string image;
    int frame = 0;
    const ImageGroupPayload *payload = nullptr;
    const ImageGroupEntry *entry = nullptr;
    const Animation *animation = nullptr;

    int is_complete() const;
    int is_resolved() const;
    int uses_animation() const;
    RuntimeDrawSlice resolved_slice() const;
};

struct FigureGraphicsLayer {
    RuntimeDrawSlice slice = {};
    GraphicsPoint offset = {};
    int draw_before_base = 0;
    int use_figure_color_mask = 1;
    int height = 0;

    int is_valid() const;
};

class FigureGraphics : public GraphicsDefinition {
public:
    FigureGraphics()
        : ::GraphicsDefinition(::GraphicsDefinitionKind::Figure)
    {
    }

    static void set_resource_cart_images(
        resource_type resource,
        ImageGroupEntryRef single_load,
        ImageGroupEntryRef multiple_loads,
        ImageGroupEntryRef eight_loads);
    static const ImageGroupEntryRef &resource_cart_image(
        resource_type resource,
        int carried_loads,
        int use_food_eight_load_variant = 0);
    static ImageGroupEntryRef resource_cart_image_for_direction(
        resource_type resource,
        int carried_loads,
        int use_food_eight_load_variant,
        int direction);
    static void reset_resource_cart_images();
    static int resource_cart_marker_for_direction(int direction);
    static int resource_cart_marker_is(unsigned int image_id);
    static int resource_cart_marker_direction(unsigned int image_id);
    static int uses_legacy_cart_overlay(figure_type type);
    static const Image &legacy_image(int image_id);
    static FigureGraphicsLayer image_layer(
        const Image &image,
        GraphicsPoint offset = {},
        int use_figure_color_mask = 1);
    static FigureGraphicsLayer legacy_image_layer(
        int image_id,
        GraphicsPoint offset = {},
        int use_figure_color_mask = 1);
    static FigureGraphicsLayer legacy_image_layer_above(int image_id, int stacked_height);
    static FigureGraphicsLayer native_entry_layer_above(const ImageGroupEntry &entry, int stacked_height);
    static RuntimeDrawSlice legacy_cart_overlay_slice(const Figure &figure);
    static FigureGraphicsLayer legacy_cart_overlay_layer(
        const Figure &figure,
        GraphicsPoint offset = {},
        int draw_before_base = -1);

    int image_group = 0;
    asset_id image_asset = ASSET_MAX_KEY;
    std::string path_pattern;
    std::string image_pattern;
    int has_sprite_offset = 0;
    int sprite_offset_x = 0;
    int sprite_offset_y = 0;
    render_logical_size fixed_logical_size = {};
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;
    int action_state = 0;
    int action_min_wait_ticks = 0;
    int action_image_group = 0;
    int action_image_group_offset = 0;
    std::string action_path_pattern;
    std::string action_image_pattern;
    int image_group_offset = 0;
    int max_image_offset = 12;
    int direction_frame_stride = 8;
    int static_frame_count = 0;
    int corpse_image_group = 0;
    asset_id corpse_image_asset = ASSET_MAX_KEY;
    std::string corpse_path_pattern;
    std::string corpse_image_pattern;
    int corpse_image_group_offset = 96;
    int corpse_frame_count = 0;
    CartGraphicsMode cart_mode = CartGraphicsMode::None;
    std::array<int, 8> cart_offsets_x = {};
    std::array<int, 8> cart_offsets_y = {};
    int cart_high_load_threshold = 0;
    int cart_high_load_y_adjust = 0;
    int cart_direction_3_y_adjust = 0;

    int default_source_count() const;
    int action_source_count() const;
    int corpse_source_count() const;
    int has_native_payload() const;
    int has_action_native_payload() const;
    int has_corpse_native_payload() const;
    int action_graphics_matches(int figure_action_state, int wait_ticks) const;
    int has_fixed_logical_size() const;
    int has_legacy_default_source() const;
    int has_legacy_resource_cart_graphics() const;
    GraphicsPoint legacy_resource_cart_layer_offset(int cart_direction, int carried_loads) const;
    RuntimeDrawSlice legacy_resource_cart_slice(resource_type resource, int carried_loads, int cart_direction) const;
    FigureGraphicsLayer legacy_resource_cart_layer(resource_type resource, int carried_loads, int cart_direction) const;
    int legacy_image_base() const;
    int legacy_corpse_image_base() const;
    int legacy_corpse_image_id(int corpse_frame_offset) const;
    int legacy_static_frame_image_id(unsigned int figure_id) const;
    int legacy_directional_image_id(int normalized_direction) const;
    int legacy_directional_frame_image_id(int normalized_direction, int frame_offset) const;
    int legacy_direction_major_frame_image_id(int normalized_direction, int frame_offset) const;
    int legacy_attack_directional_frame_image_id(int normalized_direction, int frame_offset) const;
    int legacy_action_directional_frame_image_id(int normalized_direction, int frame_offset) const;
    int legacy_image_id_for_state(
        int figure_action_state,
        unsigned int figure_id,
        int normalized_direction,
        int image_offset,
        int corpse_frame_offset) const;
    int legacy_image_id_for_figure_direction(const Figure &figure, int direction) const;
    int apply_legacy_image_state_for_direction(Figure &figure, int direction) const;
    int apply_legacy_image_state(Figure &figure) const;
    void apply_legacy_prefect_service_image_state(Figure &figure, int direction) const;
    void apply_legacy_entertainment_image_state(Figure &figure, int direction) const;
    GraphicsTargetRole target_role_for_action_state(int figure_action_state, int wait_ticks) const;
    int target_frame_count(GraphicsTargetRole role) const;
    GraphicsTargetBinding target_binding(GraphicsTargetRole role, int direction_index, int frame) const;
    const GraphicsTargetBinding *cached_target_binding(
        GraphicsTargetRole role,
        int direction_index,
        int frame) const;
    const GraphicsTargetBinding *cached_target_binding_for_state(
        int figure_action_state,
        int wait_ticks,
        int image_offset,
        int corpse_frame_offset,
        int direction_index) const;
    const GraphicsTargetBinding *cached_target_binding_for_figure(const Figure &figure) const;
    const GraphicsTargetBinding *cached_target_binding_for_figure_direction(
        const Figure &figure,
        int direction) const;
    void clear_cached_native_bindings();
    int cache_native_payload_bindings(const FigureTypeDefinition &definition);

    std::vector<GraphicsTargetBinding> default_target_bindings;
    std::vector<GraphicsTargetBinding> action_target_bindings;
    std::vector<GraphicsTargetBinding> corpse_target_bindings;
};

}
