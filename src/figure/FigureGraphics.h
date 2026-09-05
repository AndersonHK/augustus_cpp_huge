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
class FigureGraphics;

enum class CartGraphicsMode {
    None,
    ResourceLoad
};

enum class FigureOverlayDirection {
    Static,
    Figure
};

enum class FigureOverlayFrame {
    Static,
    Figure
};

enum class FigureStateDirection {
    Movement,
    Attack
};

enum class FigureStateFrame {
    Static,
    ImageOffset,
    MissileLauncher
};

enum class FigureMissileLauncherCursor {
    None,
    AttackImageOffset,
    MissileWaitTicks
};

enum class FigureResourceCartSource {
    None,
    ResourceId,
    CollectingItemOnAction
};

enum class FigureResourceCartLoadMode {
    FixedOne,
    CarriedLoads,
    CarriedOrOne
};

enum class FigureResourceCartStateSource {
    Action,
    RuntimeState
};

enum class FigureResourceCartPresentation {
    Hidden,
    Empty,
    Resource
};

struct FigureResourceCartGraphics {
    int enabled = 0;
    std::string empty_path;
    const ImageGroupPayload *empty_payload = nullptr;
    FigureResourceCartSource resource_source = FigureResourceCartSource::None;
    int resource_action = 0;
    FigureResourceCartLoadMode load_mode = FigureResourceCartLoadMode::FixedOne;
    FigureResourceCartStateSource state_source = FigureResourceCartStateSource::Action;
    int suppress_body_when_hidden = 0;
    int lift_food_at_loads = 0;
    int lift_food_y_adjust = 0;
    int hide_on_corpse = 1;
    std::array<int, 8> offsets_x = {};
    std::array<int, 8> offsets_y = {};

    resource_type resource_for(const Figure &figure) const;
    int loads_for(const Figure &figure, resource_type resource) const;
    int direction_for(const Figure &figure) const;
    GraphicsPoint offset_for(const Figure &figure, resource_type resource, int loads) const;
    FigureResourceCartPresentation normalize_presentation(
        FigureResourceCartPresentation requested,
        resource_type resource) const;
};

class FigureGraphicsState {
public:
    struct SelectedLayer {
        std::string role;
        std::string entry;
        int frame = 0;
        GraphicsPoint offset = {};
        int draw_before_base = 0;
    };

    void bind(Figure &figure, const FigureGraphics *graphics);
    int is_bound_to(const Figure &figure) const;
    void begin_update();
    void select_default_entry(std::string image_id, int frame = 0);
    void hide_default_entry();
    const char *selected_default_entry() const;
    int selected_default_frame() const;
    void set_default_offset(GraphicsPoint offset);
    GraphicsPoint selected_default_offset() const;
    void add_required_layer(std::string role, std::string entry, int frame, GraphicsPoint offset, int draw_before_base);
    const std::vector<SelectedLayer> &selected_layers() const;
    int default_entry_hidden() const;
    void show_empty_cart();
    void show_resource_cart();
    void hide_cart();
    FigureResourceCartPresentation cart_presentation() const;

private:
    void publish_cart(FigureResourceCartPresentation presentation);

    Figure *owner_ = nullptr;
    unsigned short created_sequence_ = 0;
    const FigureGraphics *graphics_ = nullptr;
    std::string selected_default_entry_;
    int selected_default_frame_ = 0;
    GraphicsPoint selected_default_offset_ = {};
    std::vector<SelectedLayer> selected_layers_;
    int default_entry_hidden_ = 0;
    FigureResourceCartPresentation cart_presentation_ = FigureResourceCartPresentation::Hidden;
};

struct FigureMissileLauncherGraphics {
    FigureMissileLauncherCursor cursor = FigureMissileLauncherCursor::None;
    int frame_divisor = 0;
    int after_frame = 0;
    std::vector<int> frames;

    int enabled() const;
    int frame_for(int attack_image_offset, int missile_wait_ticks) const;
};

struct FigureDirectionalPoseGraphics {
    std::string role;
    int action_state = 0;
    int base_image_offset = 0;
};

struct FigureDirectionalGraphics {
    int enabled = 0;
    std::string path;
    const ImageGroupPayload *payload = nullptr;
    int default_base_image_offset = 0;
    int view_adjustments = 1;
    int frame_divisor = 1;
    int frame_stride = 0;
    std::vector<FigureDirectionalPoseGraphics> poses;

    const FigureDirectionalPoseGraphics *pose_for_action(int action_state) const;
    int image_offset_for(
        int action_state,
        int raw_direction,
        int view_orientation,
        int image_offset) const;
};

struct FigureGraphicsStateLayer {
    std::string role;
    int action_state = 0;
    std::string path;
    const ImageGroupPayload *payload = nullptr;
    int base_image_offset = 0;
    FigureStateDirection direction = FigureStateDirection::Movement;
    int view_adjustments = 1;
    FigureStateFrame frame = FigureStateFrame::ImageOffset;
    int frame_divisor = 1;
    int direction_frame_stride = 8;

    int legacy_image_offset(int raw_direction, int view_orientation, int image_offset) const;
};

struct FigureGraphicsOverlay {
    std::string role;
    std::string path;
    const ImageGroupPayload *payload = nullptr;
    FigureOverlayDirection direction = FigureOverlayDirection::Static;
    FigureOverlayFrame frame = FigureOverlayFrame::Static;
    int direction_frame_stride = 8;
    int resource_base_image_offset = 0;
    int resource_stride = 0;
    int hide_on_corpse = 0;
    std::array<int, 8> offsets_x = {};
    std::array<int, 8> offsets_y = {};
    std::vector<int> visible_actions;

    int is_visible(const Figure &figure) const;
    int legacy_image_offset(int normalized_direction, int figure_frame, int resource_id) const;
    GraphicsPoint legacy_draw_offset(int normalized_direction) const;
};

struct GraphicsTargetBinding {
    std::string path;
    std::string image;
    int frame = 0;
    int frame_selects_entry = 0;
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

struct FigureGraphicsLayerSet {
    std::array<FigureGraphicsLayer, 4> layers = {};
    int count = 0;
    GraphicsPoint sprite_offset = {};

    int add(FigureGraphicsLayer layer);
};

struct FigureStandardFlagGraphics {
    figure_type unit_type = FIGURE_NONE;
    std::string path;
    const ImageGroupPayload *payload = nullptr;
    int moving_base_offset = 0;
    int halted_frame_offset = 0;
};

struct FigureStandardGraphics {
    int enabled = 0;
    int moving_frame_divisor = 0;
    int moving_frame_count = 0;
    std::string icon_path;
    const ImageGroupPayload *icon_payload = nullptr;
    std::vector<FigureStandardFlagGraphics> flags;

    const FigureStandardFlagGraphics *flag_for(figure_type unit_type) const;
    int flag_image_offset(figure_type unit_type, int halted, int figure_frame) const;
};

struct FigureMapFlagMarkerGraphics {
    int resource_min = 0;
    int resource_max_exclusive = 0;
    std::string path;
    const ImageGroupPayload *payload = nullptr;
    int image_offset = 0;
    int number_base = 0;
};

struct FigureMapFlagGraphics {
    int enabled = 0;
    int resource_min = 0;
    int resource_max_exclusive = 0;
    int base_direction = 0;
    int view_adjustments = 0;
    int frame_divisor = 0;
    int frame_stride = 0;
    GraphicsPoint number_offset = {};
    std::vector<FigureMapFlagMarkerGraphics> markers;

    const FigureMapFlagMarkerGraphics *marker_for(int resource_id) const;
    int number_for(int resource_id) const;
    int legacy_base_image_offset(int view_orientation, int image_offset) const;
    int covers_authored_range() const;
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
    static const FigureGraphics *for_type(figure_type type);
    static int corpse_frame_for_wait_ticks(int wait_ticks);
    static int missile_launcher_frame_for(const Figure &figure);

    GraphicsAssetReference &asset_target(GraphicsTargetRole role);
    const GraphicsAssetReference &asset_target(GraphicsTargetRole role) const;
    int asset_target_allows_empty(GraphicsTargetRole role) const;
    const ImageGroupEntry *asset_entry(GraphicsTargetRole role, const char *image_id) const;

    int add_overlay(FigureGraphicsOverlay overlay);
    const std::vector<FigureGraphicsOverlay> &overlays() const;
    FigureGraphicsLayer legacy_overlay_layer(
        const Figure &figure,
        const FigureGraphicsOverlay &overlay) const;
    int add_state_layer(FigureGraphicsStateLayer state_layer);
    const std::vector<FigureGraphicsStateLayer> &state_layers() const;
    const FigureGraphicsStateLayer *state_layer_for_action(int requested_action_state) const;
    int legacy_state_layer_image_id(const Figure &figure) const;
    FigureGraphicsLayerSet legacy_state_layers(const Figure &figure) const;
    int configure_standard(int moving_frame_divisor, int moving_frame_count, std::string icon_path = {});
    int add_standard_flag(FigureStandardFlagGraphics flag);
    const FigureStandardGraphics &standard() const;
    FigureGraphicsLayerSet standard_layers(figure_type unit_type, int halted, int legion_flag_image_id,
        int pole_frame, int animation_frame) const;
    int configure_map_flag(
        int resource_min,
        int resource_max_exclusive,
        int base_direction,
        int view_adjustments,
        int frame_divisor,
        int frame_stride,
        GraphicsPoint number_offset);
    int add_map_flag_marker(FigureMapFlagMarkerGraphics marker);
    const FigureMapFlagGraphics &map_flag() const;
    FigureGraphicsLayerSet legacy_map_flag_layers(const Figure &figure) const;
    int configure_missile_launcher(
        FigureMissileLauncherCursor cursor,
        int frame_divisor,
        std::vector<int> frames,
        int after_frame);
    const FigureMissileLauncherGraphics &missile_launcher() const;
    int missile_launcher_frame(const Figure &figure) const;
    int configure_resource_cart(FigureResourceCartGraphics resource_cart);
    const FigureResourceCartGraphics &resource_cart() const;
    FigureResourceCartPresentation resource_cart_presentation(
        const Figure &figure,
        const FigureGraphicsState *state) const;
    FigureGraphicsLayer resource_cart_layer(
        const Figure &figure,
        const FigureGraphicsState *state) const;
    int configure_directional(FigureDirectionalGraphics directional);
    int add_directional_pose(FigureDirectionalPoseGraphics pose);
    const FigureDirectionalGraphics &directional() const;
    int directional_image_id(const Figure &figure) const;
    FigureGraphicsLayer directional_layer(const Figure &figure) const;
    GraphicsPoint directional_sprite_offset(const Figure &figure) const;

    int action_state = 0;
    int action_frame_count = 0;
    int action_min_wait_ticks = 0;
    int action_min_missile_wait_ticks = 0;
    int action_entry_offset = 0;
    int default_entry_offset = 0;
    int max_image_offset = 12;
    int payload_frame_count = 0;
    int direction_frame_stride = 8;
    int static_frame_count = 0;
    int default_allows_empty = 0;
    int corpse_entry_offset = 96;
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
    int action_graphics_matches(int figure_action_state, int wait_ticks, int missile_wait_ticks = 0) const;
    int has_resource_cart_graphics() const;
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
    void apply_legacy_prefect_service_image_state(Figure &figure, int direction) const;
    void apply_legacy_entertainment_image_state(Figure &figure, int direction) const;
    GraphicsTargetRole target_role_for_action_state(int figure_action_state, int wait_ticks, int missile_wait_ticks = 0) const;
    int target_frame_count(GraphicsTargetRole role) const;
    GraphicsTargetBinding target_binding(GraphicsTargetRole role, int direction_index, int frame) const;
    const GraphicsTargetBinding *cached_target_binding(
        GraphicsTargetRole role,
        int direction_index,
        int frame) const;
    const GraphicsTargetBinding *cached_target_binding_for_state(int figure_action_state, int wait_ticks, int image_offset, int corpse_frame_offset, int direction_index, int missile_wait_ticks = 0) const;
    const GraphicsTargetBinding *cached_target_binding_for_figure(const Figure &figure) const;
    const GraphicsTargetBinding *cached_target_binding_for_figure_direction(
        const Figure &figure,
        int direction) const;
    void clear_cached_native_bindings();
    int cache_native_payload_bindings(const FigureTypeDefinition &definition);

    std::vector<GraphicsTargetBinding> default_target_bindings;
    std::vector<GraphicsTargetBinding> action_target_bindings;
    std::vector<GraphicsTargetBinding> corpse_target_bindings;

private:
    GraphicsAssetReference default_asset_target_;
    GraphicsAssetReference action_asset_target_;
    GraphicsAssetReference corpse_asset_target_;
    FigureMissileLauncherGraphics missile_launcher_;
    FigureResourceCartGraphics resource_cart_;
    FigureDirectionalGraphics directional_;
    std::vector<FigureGraphicsOverlay> overlay_definitions;
    std::vector<FigureGraphicsStateLayer> state_layer_definitions;
    FigureStandardGraphics standard_definition;
    FigureMapFlagGraphics map_flag_definition;
};

}
