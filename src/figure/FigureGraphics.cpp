#include "figure/FigureGraphics.h"
#include "figure/figure_type_registry_internal.h"

#include "assets/image_group_payload.h"
#include "city/view.h"
#include "core/crash_context.h"
#include "core/direction.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/log.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/image.h"
#include "game/Animation.h"
#include "scenario/property.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace figure_type_registry_impl {

namespace {

constexpr int RESOURCE_CART_GRAPHICS_COUNT = RESOURCE_SLOT_COUNT;
constexpr int RESOURCE_CART_MARKER_BASE = 0x3f000000;
constexpr int EMPTY_CART_MARKER = 0x3e000000;

struct ResourceCartGraphics {
    ImageGroupEntryRef single_load;
    ImageGroupEntryRef multiple_loads;
    ImageGroupEntryRef eight_loads;
};

std::array<ResourceCartGraphics, RESOURCE_CART_GRAPHICS_COUNT> g_resource_cart_graphics;

const char *CART_DIRECTION_SUFFIXES[8] = {
    "_NE",
    "_E",
    "_SE",
    "_S",
    "_SW",
    "_W",
    "_NW",
    "_N"
};

const char *FIGURE_SEQUENCE_DIRECTIONS[8] = {
    "ne", "e", "se", "s", "sw", "w", "nw", "n"
};

int normalize_resource_cart_direction(int direction)
{
    direction %= 8;
    if (direction < 0) {
        direction += 8;
    }
    return direction;
}

const ImageGroupEntryRef &empty_resource_cart_image()
{
    static const ImageGroupEntryRef image = ImageGroupEntryRef::from_group("Walkers\\Group_097", "Image_0000");
    return image;
}

const char *resource_name(resource_type resource)
{
    const resource_data *data = resource_get_data(resource);
    return data && data->xml_attr_name ? data->xml_attr_name : "unknown";
}

void report_invalid_resource_cart_graphic(const char *message, resource_type resource, int value)
{
    char detail[128];
    snprintf(detail, sizeof(detail), "%s resource=%d value=%d", resource_name(resource), resource, value);
    error_context_report_error(message, detail);
}

int valid_resource_cart_graphics_index(resource_type resource)
{
    return resource >= RESOURCE_NONE && resource < RESOURCE_CART_GRAPHICS_COUNT;
}

int legacy_direction_index(int direction)
{
    int dir = figure_image_normalize_direction(direction);
    if (dir < 0 || dir >= GRAPHICS_DIRECTION8_COUNT) {
        return 0;
    }
    return dir;
}

int target_direction_index(const Figure &figure)
{
    return legacy_direction_index(
        figure.direction < GRAPHICS_DIRECTION8_COUNT ?
            figure.direction :
            figure.previous_tile_direction);
}

int direction_after_view_adjustments(int direction, int view_orientation, int adjustments)
{
    direction %= GRAPHICS_DIRECTION8_COUNT;
    if (direction < 0) {
        direction += GRAPHICS_DIRECTION8_COUNT;
    }
    view_orientation %= GRAPHICS_DIRECTION8_COUNT;
    if (view_orientation < 0) {
        view_orientation += GRAPHICS_DIRECTION8_COUNT;
    }
    for (int pass = 0; pass < adjustments; ++pass) {
        direction -= view_orientation;
        if (direction < 0) {
            direction += GRAPHICS_DIRECTION8_COUNT;
        }
    }
    return direction;
}

int legacy_unbounded_direction_after_view_adjustments(
    int direction,
    int view_orientation,
    int adjustments)
{
    for (int pass = 0; pass < adjustments; ++pass) {
        direction -= view_orientation;
        if (direction < 0) {
            direction += GRAPHICS_DIRECTION8_COUNT;
        }
    }
    return direction;
}

int corpse_frame_offset(const Figure &figure)
{
    return figure.action_state == FIGURE_ACTION_149_CORPSE ?
        FigureGraphics::corpse_frame_for_wait_ticks(figure.wait_ticks) :
        0;
}

int string_ends_with(const std::string &value, const char *suffix)
{
    const size_t suffix_length = std::strlen(suffix);
    return value.size() >= suffix_length &&
        value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

int replace_direction_suffix(std::string &value, int direction)
{
    for (const char *suffix : CART_DIRECTION_SUFFIXES) {
        if (!string_ends_with(value, suffix)) {
            continue;
        }
        value.replace(value.size() - std::strlen(suffix), std::string::npos, CART_DIRECTION_SUFFIXES[direction]);
        return 1;
    }
    return 0;
}

int numeric_image_entry_with_offset(const std::string &entry_id, int direction, std::string &out_entry_id)
{
    constexpr const char *prefix = "Image_";
    constexpr size_t prefix_length = 6;
    if (entry_id.size() <= prefix_length || entry_id.compare(0, prefix_length, prefix) != 0) {
        return 0;
    }

    int image_number = 0;
    for (size_t i = prefix_length; i < entry_id.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(entry_id[i]))) {
            return 0;
        }
        image_number = image_number * 10 + entry_id[i] - '0';
    }

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Image_%0*d",
        static_cast<int>(entry_id.size() - prefix_length),
        image_number + direction);
    out_entry_id = buffer;
    return 1;
}

const ImageGroupEntry *payload_entry_at(const ImageGroupPayload *payload, int image_offset)
{
    if (!payload || image_offset < 0) return nullptr;
    char entry_id[32];
    snprintf(entry_id, sizeof(entry_id), "Image_%04d", image_offset);
    return payload->entry_for(entry_id);
}

FigureGraphicsLayer payload_image_layer(const ImageGroupPayload *payload, int image_offset, GraphicsPoint offset = {}, int use_figure_color_mask = 1)
{
    const ImageGroupEntry *entry = payload_entry_at(payload, image_offset);
    const RuntimeDrawSlice *slice = entry ? entry->footprint() : nullptr;
    if (!slice) return {};
    FigureGraphicsLayer layer;
    layer.slice = *slice;
    layer.offset = offset;
    layer.use_figure_color_mask = use_figure_color_mask;
    layer.height = slice->height;
    return layer;
}

} // namespace


static constexpr int LEGACY_ATTACK_ROW_IMAGE_OFFSET = 104;
static constexpr int LEGACY_DIRECTION_FRAME_STRIDE = 8;

int GraphicsTargetBinding::is_complete() const
{
    return !path.empty();
}

int GraphicsTargetBinding::is_resolved() const
{
    return payload && entry;
}

int GraphicsTargetBinding::uses_animation() const
{
    return !frame_selects_entry && animation && animation->has_frames();
}

RuntimeDrawSlice GraphicsTargetBinding::resolved_slice() const
{
    if (uses_animation()) {
        return animation->frame_slice_at_offset(frame, 0);
    }

    const RuntimeDrawSlice *slice = entry ? entry->footprint() : nullptr;
    return slice ? *slice : RuntimeDrawSlice();
}

int FigureGraphicsLayer::is_valid() const
{
    return slice.is_valid();
}

int FigureGraphicsLayerSet::add(FigureGraphicsLayer layer)
{
    if (!layer.is_valid() || count >= static_cast<int>(layers.size())) {
        return 0;
    }
    layers[count++] = std::move(layer);
    return 1;
}

int FigureMissileLauncherGraphics::enabled() const
{
    return cursor != FigureMissileLauncherCursor::None &&
        frame_divisor > 0 &&
        !frames.empty();
}

int FigureMissileLauncherGraphics::frame_for(
    int attack_image_offset,
    int missile_wait_ticks) const
{
    if (!enabled()) {
        return 0;
    }
    const int cursor_value = cursor == FigureMissileLauncherCursor::AttackImageOffset ?
        attack_image_offset :
        missile_wait_ticks;
    if (cursor_value < 0) {
        return frames.front();
    }
    const std::size_t index = static_cast<std::size_t>(cursor_value / frame_divisor);
    return index < frames.size() ? frames[index] : after_frame;
}

const FigureDirectionalPoseGraphics *FigureDirectionalGraphics::pose_for_action(int action_state) const
{
    for (const FigureDirectionalPoseGraphics &pose : poses) {
        if (pose.action_state == action_state) {
            return &pose;
        }
    }
    return nullptr;
}

int FigureDirectionalGraphics::image_offset_for(
    int action_state,
    int raw_direction,
    int view_orientation,
    int image_offset) const
{
    const FigureDirectionalPoseGraphics *pose = pose_for_action(action_state);
    const int base_image_offset = pose ? pose->base_image_offset : default_base_image_offset;
    return base_image_offset +
        direction_after_view_adjustments(raw_direction, view_orientation, view_adjustments) +
        frame_stride * (image_offset / frame_divisor);
}

const FigureGraphics *FigureGraphics::for_type(figure_type type)
{
    return graphics_for(type);
}

int FigureGraphics::corpse_frame_for_wait_ticks(int wait_ticks)
{
    if (wait_ticks <= 0) {
        return 0;
    }
    if (wait_ticks < 8) {
        return wait_ticks / 2;
    }
    if (wait_ticks < 32) {
        return 4;
    }
    if (wait_ticks < 72) {
        return 5;
    }
    if (wait_ticks < 152) {
        return 6;
    }
    return 7;
}

int FigureGraphics::missile_launcher_frame_for(const Figure &figure)
{
    const FigureGraphics *graphics = for_type(static_cast<figure_type>(figure.type));
    return graphics ? graphics->missile_launcher_frame(figure) : 0;
}

const FigureStandardFlagGraphics *FigureStandardGraphics::flag_for(figure_type unit_type) const
{
    for (const FigureStandardFlagGraphics &flag : flags) {
        if (flag.unit_type == unit_type) {
            return &flag;
        }
    }
    return nullptr;
}

int FigureStandardGraphics::flag_image_offset(
    figure_type unit_type,
    int halted,
    int figure_frame) const
{
    const FigureStandardFlagGraphics *flag = flag_for(unit_type);
    if (!flag || moving_frame_divisor <= 0) {
        return -1;
    }
    return halted ? flag->halted_frame_offset : flag->moving_base_offset + figure_frame / moving_frame_divisor;
}

int FigureGraphicsOverlay::is_visible(const Figure &figure) const
{
    if (hide_on_corpse && figure.action_state == FIGURE_ACTION_149_CORPSE) {
        return 0;
    }
    if (visible_actions.empty()) {
        return 1;
    }
    return std::find(visible_actions.begin(), visible_actions.end(), figure.action_state) !=
        visible_actions.end();
}

int FigureGraphicsOverlay::legacy_image_offset(
    int normalized_direction,
    int figure_frame,
    int resource_id) const
{
    const int direction_offset = direction == FigureOverlayDirection::Figure ? normalized_direction : 0;
    const int frame_offset = frame == FigureOverlayFrame::Figure ? figure_frame : 0;
    return resource_base_image_offset + resource_stride * resource_id +
        direction_offset + direction_frame_stride * frame_offset;
}

int FigureGraphicsStateLayer::legacy_image_offset(
    int raw_direction,
    int view_orientation,
    int image_offset) const
{
    const int direction_offset = direction_after_view_adjustments(
        raw_direction, view_orientation, view_adjustments);
    const int frame_offset = frame == FigureStateFrame::Static ? 0 : image_offset / frame_divisor;
    return base_image_offset + direction_offset + direction_frame_stride * frame_offset;
}

const FigureMapFlagMarkerGraphics *FigureMapFlagGraphics::marker_for(int resource_id) const
{
    for (const FigureMapFlagMarkerGraphics &marker : markers) {
        if (resource_id >= marker.resource_min && resource_id < marker.resource_max_exclusive) {
            return &marker;
        }
    }
    return nullptr;
}

int FigureMapFlagGraphics::number_for(int resource_id) const
{
    const FigureMapFlagMarkerGraphics *marker = marker_for(resource_id);
    return marker && marker->number_base > 0 ?
        marker->number_base + resource_id - marker->resource_min :
        0;
}

int FigureMapFlagGraphics::legacy_base_image_offset(int view_orientation, int image_offset) const
{
    if (!enabled || frame_divisor <= 0) {
        return -1;
    }
    return direction_after_view_adjustments(base_direction, view_orientation, view_adjustments) +
        frame_stride * (image_offset / frame_divisor);
}

int FigureMapFlagGraphics::covers_authored_range() const
{
    if (!enabled || resource_max_exclusive <= resource_min) {
        return 0;
    }
    for (int resource_id = resource_min; resource_id < resource_max_exclusive; ++resource_id) {
        if (!marker_for(resource_id)) {
            return 0;
        }
    }
    return 1;
}

GraphicsPoint FigureGraphicsOverlay::legacy_draw_offset(int normalized_direction) const
{
    return { offsets_x[normalized_direction], offsets_y[normalized_direction] };
}

GraphicsAssetReference &FigureGraphics::asset_target(GraphicsTargetRole role)
{
    if (role == GraphicsTargetRole::Action) return action_asset_target_;
    if (role == GraphicsTargetRole::Corpse) return corpse_asset_target_;
    return default_asset_target_;
}

const GraphicsAssetReference &FigureGraphics::asset_target(GraphicsTargetRole role) const
{
    if (role == GraphicsTargetRole::Action) return action_asset_target_;
    if (role == GraphicsTargetRole::Corpse) return corpse_asset_target_;
    return default_asset_target_;
}

int FigureGraphics::asset_target_allows_empty(GraphicsTargetRole role) const
{
    return role == GraphicsTargetRole::Default && default_allows_empty;
}

const ImageGroupEntry *FigureGraphics::asset_entry(GraphicsTargetRole role, const char *image_id) const
{
    const ImageGroupPayload *payload = asset_target(role).cached_payload();
    return payload && image_id && *image_id ? payload->entry_for(image_id) : nullptr;
}

int FigureGraphics::default_source_count() const
{
    return default_asset_target_.has_path() || default_asset_target_.has_image();
}

int FigureGraphics::action_source_count() const
{
    return action_asset_target_.has_path() || action_asset_target_.has_image();
}

int FigureGraphics::corpse_source_count() const
{
    return corpse_asset_target_.has_path() || corpse_asset_target_.has_image();
}

int FigureGraphics::has_native_payload() const
{
    return default_asset_target_.has_path();
}

int FigureGraphics::has_action_native_payload() const
{
    return action_asset_target_.has_path();
}

int FigureGraphics::has_corpse_native_payload() const
{
    return corpse_asset_target_.has_path();
}

int FigureGraphics::action_graphics_matches(int figure_action_state, int wait_ticks, int missile_wait_ticks) const
{
    return (!action_state || figure_action_state == action_state) &&
        (!action_min_wait_ticks || wait_ticks >= action_min_wait_ticks) &&
        (!action_min_missile_wait_ticks || missile_wait_ticks >= action_min_missile_wait_ticks) &&
        has_action_native_payload();
}

int FigureGraphics::has_resource_cart_graphics() const
{
    return cart_mode == CartGraphicsMode::ResourceLoad && has_native_payload() && has_corpse_native_payload();
}

resource_type FigureResourceCartGraphics::resource_for(const Figure &figure) const
{
    int resource = RESOURCE_NONE;
    switch (resource_source) {
        case FigureResourceCartSource::ResourceId:
            resource = figure.resource_id;
            break;
        case FigureResourceCartSource::CollectingItemOnAction:
            if (figure.action_state == resource_action) {
                resource = figure.collecting_item_id;
            }
            break;
        case FigureResourceCartSource::None:
        default:
            break;
    }
    return resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ?
        static_cast<resource_type>(resource) :
        RESOURCE_NONE;
}

int FigureResourceCartGraphics::loads_for(const Figure &figure, resource_type resource) const
{
    if (resource == RESOURCE_NONE) {
        return 0;
    }
    switch (load_mode) {
        case FigureResourceCartLoadMode::CarriedLoads:
            return figure.loads_sold_or_carrying;
        case FigureResourceCartLoadMode::CarriedOrOne:
            return figure.loads_sold_or_carrying ? figure.loads_sold_or_carrying : 1;
        case FigureResourceCartLoadMode::FixedOne:
        default:
            return 1;
    }
}

int FigureResourceCartGraphics::direction_for(const Figure &figure) const
{
    return target_direction_index(figure);
}

GraphicsPoint FigureResourceCartGraphics::offset_for(
    const Figure &figure,
    resource_type resource,
    int loads) const
{
    const int direction = direction_for(figure);
    GraphicsPoint offset = { offsets_x[direction], offsets_y[direction] };
    if (lift_food_at_loads > 0 && loads >= lift_food_at_loads && resource_is_food(resource)) {
        offset.y += lift_food_y_adjust;
    }
    return offset;
}

FigureResourceCartPresentation FigureResourceCartGraphics::normalize_presentation(
    FigureResourceCartPresentation requested,
    resource_type resource) const
{
    if (requested == FigureResourceCartPresentation::Resource && resource == RESOURCE_NONE) {
        return FigureResourceCartPresentation::Empty;
    }
    return requested;
}

void FigureGraphicsState::bind(Figure &figure, const FigureGraphics *graphics)
{
    if (is_bound_to(figure) && graphics_ == graphics) {
        return;
    }

    owner_ = &figure;
    created_sequence_ = figure.created_sequence;
    graphics_ = graphics;
    selected_default_entry_.clear();
    selected_default_frame_ = 0;
    selected_default_offset_ = {};
    selected_layers_.clear();
    default_entry_hidden_ = 0;
    cart_presentation_ = FigureResourceCartPresentation::Hidden;
    if (!graphics_) {
        return;
    }

    const FigureResourceCartGraphics &policy = graphics_->resource_cart();
    if (!policy.enabled || policy.state_source != FigureResourceCartStateSource::RuntimeState) {
        return;
    }

    if (FigureGraphics::resource_cart_marker_is(figure.cart_image_id)) {
        cart_presentation_ = FigureResourceCartPresentation::Resource;
    } else if (figure.cart_image_id) {
        cart_presentation_ = FigureResourceCartPresentation::Empty;
    }
    cart_presentation_ = policy.normalize_presentation(
        cart_presentation_, policy.resource_for(figure));
}

int FigureGraphicsState::is_bound_to(const Figure &figure) const
{
    return owner_ == &figure && created_sequence_ == figure.created_sequence;
}

void FigureGraphicsState::begin_update()
{
    selected_default_entry_.clear();
    selected_default_frame_ = 0;
    selected_default_offset_ = {};
    selected_layers_.clear();
    default_entry_hidden_ = 0;
    publish_cart(FigureResourceCartPresentation::Hidden);
}

void FigureGraphicsState::select_default_entry(std::string image_id, int frame)
{
    selected_default_entry_ = std::move(image_id);
    selected_default_frame_ = frame;
    default_entry_hidden_ = 0;
    if (owner_) owner_->image_id = 0;
}

void FigureGraphicsState::hide_default_entry()
{
    selected_default_entry_.clear();
    selected_default_frame_ = 0;
    default_entry_hidden_ = 1;
    if (owner_) owner_->image_id = 0;
}

const char *FigureGraphicsState::selected_default_entry() const
{
    return selected_default_entry_.empty() ? nullptr : selected_default_entry_.c_str();
}

int FigureGraphicsState::selected_default_frame() const
{
    return selected_default_frame_;
}

void FigureGraphicsState::set_default_offset(GraphicsPoint offset)
{
    selected_default_offset_ = offset;
}

GraphicsPoint FigureGraphicsState::selected_default_offset() const
{
    return selected_default_offset_;
}

void FigureGraphicsState::add_required_layer(std::string role, std::string entry, int frame, GraphicsPoint offset, int draw_before_base)
{
    selected_layers_.push_back({ std::move(role), std::move(entry), frame, offset, draw_before_base });
}

const std::vector<FigureGraphicsState::SelectedLayer> &FigureGraphicsState::selected_layers() const
{
    return selected_layers_;
}

int FigureGraphicsState::default_entry_hidden() const
{
    return default_entry_hidden_;
}

void FigureGraphicsState::show_empty_cart()
{
    publish_cart(FigureResourceCartPresentation::Empty);
}

void FigureGraphicsState::show_resource_cart()
{
    publish_cart(FigureResourceCartPresentation::Resource);
}

void FigureGraphicsState::hide_cart()
{
    publish_cart(FigureResourceCartPresentation::Hidden);
}

FigureResourceCartPresentation FigureGraphicsState::cart_presentation() const
{
    return cart_presentation_;
}

void FigureGraphicsState::publish_cart(FigureResourceCartPresentation presentation)
{
    if (!owner_ || !is_bound_to(*owner_) || !graphics_) {
        return;
    }

    const FigureResourceCartGraphics &policy = graphics_->resource_cart();
    if (!policy.enabled || policy.state_source != FigureResourceCartStateSource::RuntimeState) {
        return;
    }

    cart_presentation_ = policy.normalize_presentation(
        presentation, policy.resource_for(*owner_));
    switch (cart_presentation_) {
        case FigureResourceCartPresentation::Resource:
            owner_->cart_image_id = FigureGraphics::resource_cart_marker_for_direction(0);
            break;
        case FigureResourceCartPresentation::Empty:
            owner_->cart_image_id = EMPTY_CART_MARKER;
            break;
        case FigureResourceCartPresentation::Hidden:
        default:
            owner_->cart_image_id = 0;
            break;
    }
}

void FigureGraphics::set_resource_cart_images(
    resource_type resource,
    ImageGroupEntryRef single_load,
    ImageGroupEntryRef multiple_loads,
    ImageGroupEntryRef eight_loads)
{
    if (!valid_resource_cart_graphics_index(resource)) {
        report_invalid_resource_cart_graphic("Invalid resource cart graphics assignment", resource, 0);
        return;
    }
    ResourceCartGraphics &graphics = g_resource_cart_graphics[static_cast<size_t>(resource)];
    graphics.single_load = std::move(single_load);
    graphics.multiple_loads = std::move(multiple_loads);
    graphics.eight_loads = std::move(eight_loads);
}

const ImageGroupEntryRef &FigureGraphics::resource_cart_image(
    resource_type resource,
    int carried_loads,
    int use_food_eight_load_variant)
{
    if (carried_loads <= 0) {
        return empty_resource_cart_image();
    }
    if (!valid_resource_cart_graphics_index(resource)) {
        report_invalid_resource_cart_graphic("Invalid resource cart graphics lookup", resource, carried_loads);
        return empty_resource_cart_image();
    }

    const ResourceCartGraphics &graphics = g_resource_cart_graphics[static_cast<size_t>(resource)];
    if (carried_loads == 1) {
        return graphics.single_load;
    }
    if (use_food_eight_load_variant && carried_loads >= 8) {
        return graphics.eight_loads;
    }
    return graphics.multiple_loads;
}

ImageGroupEntryRef FigureGraphics::resource_cart_image_for_direction(
    resource_type resource,
    int carried_loads,
    int use_food_eight_load_variant,
    int direction)
{
    direction = normalize_resource_cart_direction(direction);
    const ImageGroupEntryRef &base = resource_cart_image(resource, carried_loads, use_food_eight_load_variant);
    if (!base.is_bound()) {
        return base;
    }

    std::string group_path = base.group_path();
    std::string entry_id = base.entry_id();
    if (replace_direction_suffix(group_path, direction)) {
        replace_direction_suffix(entry_id, direction);
        return ImageGroupEntryRef::from_group(std::move(group_path), std::move(entry_id));
    }
    if (replace_direction_suffix(entry_id, direction)) {
        return ImageGroupEntryRef::from_group(std::move(group_path), std::move(entry_id));
    }

    std::string directional_entry_id;
    if (numeric_image_entry_with_offset(entry_id, direction, directional_entry_id)) {
        return ImageGroupEntryRef::from_group(std::move(group_path), std::move(directional_entry_id));
    }

    return base;
}

void FigureGraphics::reset_resource_cart_images()
{
    g_resource_cart_graphics = {};
}

int FigureGraphics::resource_cart_marker_for_direction(int direction)
{
    return RESOURCE_CART_MARKER_BASE + normalize_resource_cart_direction(direction);
}

int FigureGraphics::resource_cart_marker_is(unsigned int image_id)
{
    return image_id >= RESOURCE_CART_MARKER_BASE && image_id < RESOURCE_CART_MARKER_BASE + 8;
}

const Image &FigureGraphics::legacy_image(int image_id)
{
    if (image_id >= IMAGE_MAIN_ENTRIES) {
        assets_load_unpacked_asset(image_id);
    }
    const Image &image = Image::from_id(image_id);
    if (image.is_external()) {
        image.load_external_data();
    }
    return image;
}

FigureGraphicsLayer FigureGraphics::image_layer(
    const Image &image,
    GraphicsPoint offset,
    int use_figure_color_mask)
{
    FigureGraphicsLayer layer;
    layer.slice = image.runtime_slice();
    layer.offset = offset;
    layer.use_figure_color_mask = use_figure_color_mask;
    layer.height = image.height();
    return layer;
}

FigureGraphicsLayer FigureGraphics::legacy_image_layer(
    int image_id,
    GraphicsPoint offset,
    int use_figure_color_mask)
{
    return image_layer(legacy_image(image_id), offset, use_figure_color_mask);
}

FigureGraphicsLayer FigureGraphics::legacy_image_layer_above(int image_id, int stacked_height)
{
    const Image &image = legacy_image(image_id);
    return image_layer(image, { 0, -image.height() - stacked_height }, 0);
}

FigureGraphicsLayer FigureGraphics::native_entry_layer_above(const ImageGroupEntry &entry, int stacked_height)
{
    const RuntimeDrawSlice *slice = entry.footprint();
    if (!slice) {
        return {};
    }

    FigureGraphicsLayer layer;
    layer.slice = *slice;
    layer.offset.y = -slice->height - stacked_height;
    layer.use_figure_color_mask = 0;
    layer.height = slice->height;
    return layer;
}

int FigureGraphics::configure_resource_cart(FigureResourceCartGraphics resource_cart)
{
    if (resource_cart_.enabled || resource_cart.empty_path.empty() ||
        resource_cart.resource_source == FigureResourceCartSource::None ||
        (resource_cart.resource_source == FigureResourceCartSource::CollectingItemOnAction &&
            resource_cart.resource_action <= 0) ||
        (resource_cart.state_source == FigureResourceCartStateSource::RuntimeState &&
            resource_cart.resource_source != FigureResourceCartSource::ResourceId) ||
        (resource_cart.suppress_body_when_hidden &&
            resource_cart.state_source != FigureResourceCartStateSource::RuntimeState) ||
        resource_cart.lift_food_at_loads < 0 ||
        ((resource_cart.lift_food_at_loads > 0) != (resource_cart.lift_food_y_adjust != 0))) {
        return 0;
    }
    for (std::size_t index = 0; index < resource_cart.offsets_x.size(); ++index) {
        if (resource_cart.offsets_x[index] < std::numeric_limits<signed char>::min() ||
            resource_cart.offsets_x[index] > std::numeric_limits<signed char>::max() ||
            resource_cart.offsets_y[index] < std::numeric_limits<signed char>::min() ||
            resource_cart.offsets_y[index] > std::numeric_limits<signed char>::max()) {
            return 0;
        }
    }
    resource_cart.enabled = 1;
    resource_cart_ = std::move(resource_cart);
    return 1;
}

const FigureResourceCartGraphics &FigureGraphics::resource_cart() const
{
    return resource_cart_;
}

FigureResourceCartPresentation FigureGraphics::resource_cart_presentation(
    const Figure &figure,
    const FigureGraphicsState *state) const
{
    if (!resource_cart_.enabled ||
        (resource_cart_.hide_on_corpse && figure.action_state == FIGURE_ACTION_149_CORPSE)) {
        return FigureResourceCartPresentation::Hidden;
    }

    const resource_type resource = resource_cart_.resource_for(figure);
    if (resource_cart_.state_source == FigureResourceCartStateSource::RuntimeState) {
        const FigureResourceCartPresentation requested =
            state && state->is_bound_to(figure) ? state->cart_presentation() :
            FigureResourceCartPresentation::Hidden;
        return resource_cart_.normalize_presentation(requested, resource);
    }
    return resource == RESOURCE_NONE ?
        FigureResourceCartPresentation::Empty :
        FigureResourceCartPresentation::Resource;
}

FigureGraphicsLayer FigureGraphics::resource_cart_layer(
    const Figure &figure,
    const FigureGraphicsState *state) const
{
    const FigureResourceCartPresentation presentation =
        resource_cart_presentation(figure, state);
    if (presentation == FigureResourceCartPresentation::Hidden) {
        return {};
    }

    const resource_type resource = presentation == FigureResourceCartPresentation::Resource ?
        resource_cart_.resource_for(figure) : RESOURCE_NONE;
    const int loads = resource_cart_.loads_for(figure, resource);
    const int direction = resource_cart_.direction_for(figure);
    const GraphicsPoint offset = resource_cart_.offset_for(figure, resource, loads);
    FigureGraphicsLayer layer;
    if (presentation == FigureResourceCartPresentation::Empty || loads <= 0) {
        layer = payload_image_layer(resource_cart_.empty_payload, direction, offset);
    } else {
        const ImageGroupEntryRef image = resource_cart_image_for_direction(
            resource,
            loads,
            resource_cart_.lift_food_at_loads > 0,
            direction);
        layer.slice = image.runtime_slice();
        if (!layer.slice.is_valid()) {
            return {};
        }
        layer.offset = offset;
        layer.height = image.height();
    }
    layer.draw_before_base = offset.y < 0;
    return layer;
}

int FigureGraphics::configure_directional(FigureDirectionalGraphics directional)
{
    if (directional_.enabled || directional.path.empty() ||
        directional.default_base_image_offset < 0 ||
        directional.view_adjustments < 0 || directional.view_adjustments > 4 ||
        directional.frame_divisor <= 0 || directional.frame_stride < 0) {
        return 0;
    }
    directional.enabled = 1;
    directional_ = std::move(directional);
    return 1;
}

int FigureGraphics::add_directional_pose(FigureDirectionalPoseGraphics pose)
{
    if (!directional_.enabled || pose.role.empty() || !pose.action_state ||
        pose.base_image_offset < 0) {
        return 0;
    }
    for (const FigureDirectionalPoseGraphics &existing : directional_.poses) {
        if (existing.role == pose.role || existing.action_state == pose.action_state) {
            return 0;
        }
    }
    directional_.poses.push_back(std::move(pose));
    return 1;
}

const FigureDirectionalGraphics &FigureGraphics::directional() const
{
    return directional_;
}

int FigureGraphics::directional_image_id(const Figure &figure) const
{
    if (!directional_.enabled) {
        return 0;
    }
    const int raw_direction = figure.direction < GRAPHICS_DIRECTION8_COUNT ?
        figure.direction : figure.previous_tile_direction;
    return directional_.image_offset_for(
        figure.action_state,
        raw_direction,
        city_view_orientation(),
        figure.image_offset);
}

const ImageGroupPayload *FigureDirectionalGraphics::active_payload() const
{
    const int climate = scenario_property_climate();
    return climate >= 0 && climate < static_cast<int>(climate_payloads.size()) && climate_payloads[climate] ? climate_payloads[climate] : payload;
}

FigureGraphicsLayer FigureGraphics::directional_layer(const Figure &figure) const
{
    if (!directional_.enabled) return {};
    const int raw_direction = figure.direction < GRAPHICS_DIRECTION8_COUNT ? figure.direction : figure.previous_tile_direction;
    const int image_offset = directional_.image_offset_for(figure.action_state, raw_direction, city_view_orientation(), figure.image_offset);
    return payload_image_layer(directional_.active_payload(), image_offset);
}

GraphicsPoint FigureGraphics::directional_sprite_offset(const Figure &figure) const
{
    if (!directional_.enabled) return {};
    const int raw_direction = figure.direction < GRAPHICS_DIRECTION8_COUNT ? figure.direction : figure.previous_tile_direction;
    const int image_offset = directional_.image_offset_for(figure.action_state, raw_direction, city_view_orientation(), figure.image_offset);
    const ImageGroupEntry *entry = payload_entry_at(directional_.active_payload(), image_offset);
    return entry && entry->has_sprite_offset() ? GraphicsPoint { entry->sprite_offset_x(), entry->sprite_offset_y() } : GraphicsPoint {};
}

int FigureGraphics::add_overlay(FigureGraphicsOverlay overlay)
{
    if (overlay.role.empty() || overlay.path.empty() || overlay.direction_frame_stride <= 0 ||
        overlay.resource_base_image_offset < 0 || overlay.resource_stride < 0) {
        return 0;
    }
    for (std::size_t index = 0; index < overlay.offsets_x.size(); ++index) {
        if (overlay.offsets_x[index] < std::numeric_limits<signed char>::min() ||
            overlay.offsets_x[index] > std::numeric_limits<signed char>::max() ||
            overlay.offsets_y[index] < std::numeric_limits<signed char>::min() ||
            overlay.offsets_y[index] > std::numeric_limits<signed char>::max()) {
            return 0;
        }
    }
    for (std::size_t index = 0; index < overlay.visible_actions.size(); ++index) {
        if (overlay.visible_actions[index] <= 0 ||
            std::find(
                overlay.visible_actions.begin(),
                overlay.visible_actions.begin() + index,
                overlay.visible_actions[index]) != overlay.visible_actions.begin() + index) {
            return 0;
        }
    }
    for (const FigureGraphicsOverlay &existing : overlay_definitions) {
        if (existing.role == overlay.role) {
            return 0;
        }
    }
    overlay_definitions.push_back(std::move(overlay));
    return 1;
}

const std::vector<FigureGraphicsOverlay> &FigureGraphics::overlays() const
{
    return overlay_definitions;
}

FigureGraphicsLayer FigureGraphics::legacy_overlay_layer(
    const Figure &figure,
    const FigureGraphicsOverlay &overlay) const
{
    if (!overlay.is_visible(figure)) {
        return {};
    }

    const int direction = overlay.direction == FigureOverlayDirection::Figure ?
        target_direction_index(figure) :
        0;
    const GraphicsPoint offset = overlay.legacy_draw_offset(direction);

    const int image_offset = overlay.legacy_image_offset(direction, figure.image_offset, figure.resource_id);
    FigureGraphicsLayer layer = payload_image_layer(overlay.payload, image_offset, offset);
    layer.draw_before_base = offset.y < 0;
    return layer;
}

int FigureGraphics::add_state_layer(FigureGraphicsStateLayer state_layer)
{
    if (state_layer.role.empty() || !state_layer.action_state || state_layer.path.empty() ||
        state_layer.base_image_offset < 0 || state_layer.view_adjustments < 0 ||
        state_layer.view_adjustments > 4 || state_layer.frame_divisor <= 0 ||
        state_layer.direction_frame_stride <= 0) {
        return 0;
    }
    for (const FigureGraphicsStateLayer &existing : state_layer_definitions) {
        if (existing.role == state_layer.role || existing.action_state == state_layer.action_state) {
            return 0;
        }
    }
    state_layer_definitions.push_back(std::move(state_layer));
    return 1;
}

const std::vector<FigureGraphicsStateLayer> &FigureGraphics::state_layers() const
{
    return state_layer_definitions;
}

const FigureGraphicsStateLayer *FigureGraphics::state_layer_for_action(int requested_action_state) const
{
    for (const FigureGraphicsStateLayer &state_layer : state_layer_definitions) {
        if (state_layer.action_state == requested_action_state) {
            return &state_layer;
        }
    }
    return nullptr;
}

int FigureGraphics::legacy_state_layer_image_id(const Figure &figure) const
{
    const FigureGraphicsStateLayer *state_layer = state_layer_for_action(figure.action_state);
    if (!state_layer) {
        return 0;
    }
    const int raw_direction = state_layer->direction == FigureStateDirection::Attack ?
        figure.attack_direction :
        (figure.direction < GRAPHICS_DIRECTION8_COUNT ? figure.direction : figure.previous_tile_direction);
    const int frame_offset = state_layer->frame == FigureStateFrame::MissileLauncher ?
        missile_launcher_frame(figure) :
        figure.image_offset;
    return state_layer->legacy_image_offset(
        raw_direction,
        city_view_orientation(),
        frame_offset);
}

FigureGraphicsLayerSet FigureGraphics::legacy_state_layers(const Figure &figure) const
{
    FigureGraphicsLayerSet result;
    const FigureGraphicsStateLayer *state_layer = state_layer_for_action(figure.action_state);
    if (state_layer && state_layer->payload) {
        const int raw_direction = state_layer->direction == FigureStateDirection::Attack ? figure.attack_direction : (figure.direction < GRAPHICS_DIRECTION8_COUNT ? figure.direction : figure.previous_tile_direction);
        const int frame_offset = state_layer->frame == FigureStateFrame::MissileLauncher ? missile_launcher_frame(figure) : figure.image_offset;
        const ImageGroupEntry *entry = payload_entry_at(state_layer->payload, state_layer->legacy_image_offset(raw_direction, city_view_orientation(), frame_offset));
        if (!entry) return result;
        if (entry->has_sprite_offset()) result.sprite_offset = { entry->sprite_offset_x(), entry->sprite_offset_y() };
        result.add(payload_image_layer(state_layer->payload, state_layer->legacy_image_offset(raw_direction, city_view_orientation(), frame_offset)));
        return result;
    }
    const int image_id = legacy_state_layer_image_id(figure);
    if (!image_id) {
        return result;
    }
    const Image &image = legacy_image(image_id);
    if (const image_animation *animation = image.animation()) {
        result.sprite_offset.x = animation->sprite_offset_x;
        result.sprite_offset.y = animation->sprite_offset_y;
    }
    result.add(image_layer(image));
    return result;
}

int FigureGraphics::configure_standard(int moving_frame_divisor, int moving_frame_count, std::string icon_path)
{
    if (standard_definition.enabled || moving_frame_divisor <= 0 || moving_frame_count <= 0) {
        return 0;
    }
    standard_definition.enabled = 1;
    standard_definition.moving_frame_divisor = moving_frame_divisor;
    standard_definition.moving_frame_count = moving_frame_count;
    standard_definition.icon_path = std::move(icon_path);
    return 1;
}

int FigureGraphics::add_standard_flag(FigureStandardFlagGraphics flag)
{
    if (!standard_definition.enabled || flag.unit_type == FIGURE_NONE || flag.path.empty() ||
        flag.moving_base_offset < 0 || flag.halted_frame_offset < 0 ||
        standard_definition.flag_for(flag.unit_type)) {
        return 0;
    }
    standard_definition.flags.push_back(std::move(flag));
    return 1;
}

const FigureStandardGraphics &FigureGraphics::standard() const
{
    return standard_definition;
}

FigureGraphicsLayerSet FigureGraphics::standard_layers(figure_type unit_type, int halted, int legion_flag_image_id,
    int pole_frame, int animation_frame) const
{
    FigureGraphicsLayerSet result;
    const FigureStandardFlagGraphics *flag = standard_definition.flag_for(unit_type);
    if (!flag) {
        return result;
    }

    const GraphicsTargetBinding *pole_binding = cached_target_binding(GraphicsTargetRole::Default, 0, pole_frame + 1);
    if (pole_binding && pole_binding->entry) {
        if (pole_binding->entry->has_sprite_offset()) result.sprite_offset = { pole_binding->entry->sprite_offset_x(), pole_binding->entry->sprite_offset_y() };
        FigureGraphicsLayer pole_layer;
        pole_layer.slice = pole_binding->resolved_slice();
        pole_layer.use_figure_color_mask = 0;
        pole_layer.height = pole_layer.slice.height;
        result.add(pole_layer);
    }

    const int flag_offset = standard_definition.flag_image_offset(unit_type, halted, animation_frame);
    FigureGraphicsLayer flag_layer = payload_image_layer(flag->payload, flag_offset, {}, 0);
    flag_layer.offset.y = -flag_layer.height;
    result.add(flag_layer);
    if (standard_definition.icon_payload) {
        const int icon_offset = legion_flag_image_id - ::image_group(GROUP_FIGURE_FORT_STANDARD_ICONS);
        const ImageGroupEntry *icon_entry = payload_entry_at(standard_definition.icon_payload, icon_offset);
        if (icon_entry) result.add(native_entry_layer_above(*icon_entry, flag_layer.height));
    } else {
        result.add(legacy_image_layer_above(legion_flag_image_id, flag_layer.height));
    }
    return result;
}

int FigureGraphics::configure_map_flag(
    int resource_min,
    int resource_max_exclusive,
    int base_direction,
    int view_adjustments,
    int frame_divisor,
    int frame_stride,
    GraphicsPoint number_offset)
{
    if (map_flag_definition.enabled || resource_min < 0 ||
        resource_max_exclusive <= resource_min || base_direction < 0 ||
        base_direction >= GRAPHICS_DIRECTION8_COUNT || view_adjustments < 0 ||
        view_adjustments > 4 || frame_divisor <= 0 || frame_stride <= 0) {
        return 0;
    }
    map_flag_definition.enabled = 1;
    map_flag_definition.resource_min = resource_min;
    map_flag_definition.resource_max_exclusive = resource_max_exclusive;
    map_flag_definition.base_direction = base_direction;
    map_flag_definition.view_adjustments = view_adjustments;
    map_flag_definition.frame_divisor = frame_divisor;
    map_flag_definition.frame_stride = frame_stride;
    map_flag_definition.number_offset = number_offset;
    return 1;
}

int FigureGraphics::add_map_flag_marker(FigureMapFlagMarkerGraphics marker)
{
    if (!map_flag_definition.enabled || marker.resource_min < 0 ||
        marker.resource_max_exclusive <= marker.resource_min || marker.path.empty() ||
        marker.image_offset < 0 || marker.number_base < 0 ||
        marker.resource_min < map_flag_definition.resource_min ||
        marker.resource_max_exclusive > map_flag_definition.resource_max_exclusive) {
        return 0;
    }
    for (const FigureMapFlagMarkerGraphics &existing : map_flag_definition.markers) {
        if (marker.resource_min < existing.resource_max_exclusive &&
            existing.resource_min < marker.resource_max_exclusive) {
            return 0;
        }
    }
    map_flag_definition.markers.push_back(std::move(marker));
    return 1;
}

const FigureMapFlagGraphics &FigureGraphics::map_flag() const
{
    return map_flag_definition;
}

FigureGraphicsLayerSet FigureGraphics::legacy_map_flag_layers(const Figure &figure) const
{
    FigureGraphicsLayerSet result;
    const FigureMapFlagMarkerGraphics *marker = map_flag_definition.marker_for(figure.resource_id);
    const int base_offset = map_flag_definition.legacy_base_image_offset(
        city_view_orientation(), figure.image_offset);
    if (!marker || base_offset < 0) {
        return result;
    }

    const GraphicsTargetBinding *base_binding = cached_target_binding(GraphicsTargetRole::Default, base_offset, 1);
    if (base_binding && base_binding->entry) {
        if (base_binding->entry->has_sprite_offset()) result.sprite_offset = { base_binding->entry->sprite_offset_x(), base_binding->entry->sprite_offset_y() };
        FigureGraphicsLayer base_layer;
        base_layer.slice = base_binding->resolved_slice();
        base_layer.use_figure_color_mask = 0;
        base_layer.height = base_layer.slice.height;
        result.add(base_layer);
    } else {
        const Image &base_image = legacy_image(legacy_image_base() + base_offset);
        if (const image_animation *animation = base_image.animation()) result.sprite_offset = { animation->sprite_offset_x, animation->sprite_offset_y };
        result.add(image_layer(base_image, {}, 0));
    }
    FigureGraphicsLayer marker_layer = payload_image_layer(marker->payload, marker->image_offset, {}, 0);
    marker_layer.offset.y = -marker_layer.height;
    result.add(marker_layer);
    return result;
}

int FigureGraphics::configure_missile_launcher(
    FigureMissileLauncherCursor cursor,
    int frame_divisor,
    std::vector<int> frames,
    int after_frame)
{
    if (missile_launcher_.enabled() ||
        cursor == FigureMissileLauncherCursor::None ||
        frame_divisor <= 0 ||
        frames.empty()) {
        return 0;
    }
    missile_launcher_.cursor = cursor;
    missile_launcher_.frame_divisor = frame_divisor;
    missile_launcher_.frames = std::move(frames);
    missile_launcher_.after_frame = after_frame;
    return 1;
}

const FigureMissileLauncherGraphics &FigureGraphics::missile_launcher() const
{
    return missile_launcher_;
}

int FigureGraphics::missile_launcher_frame(const Figure &figure) const
{
    return missile_launcher_.frame_for(
        figure.attack_image_offset,
        figure.wait_ticks_missile);
}

GraphicsPoint FigureGraphics::legacy_resource_cart_layer_offset(int cart_direction, int carried_loads) const
{
    GraphicsPoint offset;
    offset.x = cart_offsets_x[cart_direction];
    offset.y = cart_offsets_y[cart_direction];
    if (cart_high_load_threshold > 0 && carried_loads >= cart_high_load_threshold) {
        offset.y += cart_high_load_y_adjust;
    } else if (cart_direction == 3) {
        offset.y += cart_direction_3_y_adjust;
    }
    return offset;
}

RuntimeDrawSlice FigureGraphics::legacy_resource_cart_slice(
    resource_type resource,
    int carried_loads,
    int cart_direction) const
{
    const int carried = resource == RESOURCE_NONE ? 0 : carried_loads;
    if (carried <= 0) {
        return resource_cart_image_for_direction(RESOURCE_NONE, 0, 0, cart_direction).runtime_slice();
    }

    if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return resource_cart_image_for_direction(RESOURCE_NONE, 0, 0, cart_direction).runtime_slice();
    }
    return resource_cart_image_for_direction(
        resource,
        carried,
        resource_is_food(resource),
        cart_direction).runtime_slice();
}

FigureGraphicsLayer FigureGraphics::legacy_resource_cart_layer(
    resource_type resource,
    int carried_loads,
    int cart_direction) const
{
    FigureGraphicsLayer layer;
    layer.slice = legacy_resource_cart_slice(resource, carried_loads, cart_direction);
    layer.offset = legacy_resource_cart_layer_offset(cart_direction, carried_loads);
    layer.draw_before_base = layer.offset.y < 0;
    layer.height = layer.slice.height;
    return layer;
}

int FigureGraphics::legacy_image_base() const
{
    return default_entry_offset;
}

int FigureGraphics::legacy_corpse_image_base() const
{
    return corpse_entry_offset;
}

int FigureGraphics::legacy_corpse_image_id(int corpse_frame_offset) const
{
    return legacy_corpse_image_base() + corpse_frame_offset;
}

int FigureGraphics::legacy_static_frame_image_id(unsigned int figure_id) const
{
    const int base_image_id = legacy_image_base();
    if (static_frame_count <= 0) {
        return base_image_id;
    }
    return base_image_id + static_cast<int>(figure_id % static_cast<unsigned int>(static_frame_count));
}

int FigureGraphics::legacy_directional_image_id(int normalized_direction) const
{
    return legacy_image_base() + normalized_direction;
}

int FigureGraphics::legacy_directional_frame_image_id(int normalized_direction, int frame_offset) const
{
    return legacy_image_base() + normalized_direction + direction_frame_stride * frame_offset;
}

int FigureGraphics::legacy_direction_major_frame_image_id(int normalized_direction, int frame_offset) const
{
    return legacy_image_base() + normalized_direction * direction_frame_stride + frame_offset;
}

int FigureGraphics::legacy_attack_directional_frame_image_id(int normalized_direction, int frame_offset) const
{
    return legacy_image_base() + LEGACY_ATTACK_ROW_IMAGE_OFFSET + normalized_direction +
        LEGACY_DIRECTION_FRAME_STRIDE * frame_offset;
}

int FigureGraphics::legacy_action_directional_frame_image_id(int normalized_direction, int frame_offset) const
{
    const int action_base = action_asset_target_.has_path() ? action_entry_offset : legacy_image_base();
    return action_base + normalized_direction + direction_frame_stride * frame_offset;
}

int FigureGraphics::legacy_image_id_for_state(
    int figure_action_state,
    unsigned int figure_id,
    int normalized_direction,
    int image_offset,
    int corpse_frame_offset) const
{
    if (figure_action_state == FIGURE_ACTION_149_CORPSE) {
        return legacy_corpse_image_id(corpse_frame_offset);
    }
    if (static_frame_count > 0) {
        return legacy_static_frame_image_id(figure_id);
    }
    return legacy_directional_frame_image_id(normalized_direction, image_offset);
}

int FigureGraphics::legacy_image_id_for_figure_direction(const Figure &figure, int direction) const
{
    return legacy_image_id_for_state(
        figure.action_state,
        figure.id(),
        figure_image_normalize_direction(direction),
        figure.image_offset,
        corpse_frame_offset(figure));
}

GraphicsTargetRole FigureGraphics::target_role_for_action_state(int figure_action_state, int wait_ticks, int missile_wait_ticks) const
{
    if (figure_action_state == FIGURE_ACTION_149_CORPSE) {
        return GraphicsTargetRole::Corpse;
    }
    if (action_graphics_matches(figure_action_state, wait_ticks, missile_wait_ticks)) {
        return GraphicsTargetRole::Action;
    }
    return GraphicsTargetRole::Default;
}

int FigureGraphics::target_frame_count(GraphicsTargetRole role) const
{
    if (role == GraphicsTargetRole::Corpse) {
        return corpse_frame_count > 0 ? corpse_frame_count : 8;
    }
    if (role == GraphicsTargetRole::Action && action_frame_count > 0) return action_frame_count;
    if (role == GraphicsTargetRole::Default && static_frame_count > 0) {
        return static_frame_count;
    }
    if (role == GraphicsTargetRole::Default && payload_frame_count > 0) return payload_frame_count;
    return max_image_offset;
}

static std::string extracted_payload_entry_name(int index)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Image_%04d", index);
    return buffer;
}

static int extracted_group_entry_index(const FigureGraphics &graphics, GraphicsTargetRole role, int direction_index, int frame)
{
    if (role == GraphicsTargetRole::Corpse) return graphics.corpse_entry_offset + frame - 1;
    if (role == GraphicsTargetRole::Action) return graphics.action_entry_offset + direction_index + graphics.direction_frame_stride * (frame - 1);
    if (graphics.static_frame_count > 0) return graphics.default_entry_offset + frame - 1;
    return graphics.default_entry_offset + direction_index + graphics.direction_frame_stride * (frame - 1);
}

GraphicsTargetBinding FigureGraphics::target_binding(
    GraphicsTargetRole role,
    int direction_index,
    int frame) const
{
    const GraphicsAssetReference &target = asset_target(role);
    const std::string path = target.path();
    const std::string image = target.image();

    GraphicsTargetBinding binding;
    binding.frame_selects_entry = path.find("{frame}") != std::string::npos || image.find("{frame}") != std::string::npos;
    binding.path = graphics_expand_direction_frame_pattern(path, direction_index, frame);
    binding.image = graphics_expand_direction_frame_pattern(image, direction_index, frame);
    if (binding.image.empty() && target.cached_payload()) {
        const std::string extracted_entry = extracted_payload_entry_name(
            extracted_group_entry_index(*this, role, direction_index, frame));
        if (target.cached_payload()->entry_for(extracted_entry.c_str())) {
            binding.image = extracted_entry;
            binding.frame_selects_entry = 1;
        }
    }
    binding.frame = frame;
    return binding;
}

static const std::vector<GraphicsTargetBinding> &graphics_target_cache(
    const FigureGraphics &graphics_definition,
    GraphicsTargetRole role)
{
    switch (role) {
        case GraphicsTargetRole::Action:
            return graphics_definition.action_target_bindings;
        case GraphicsTargetRole::Corpse:
            return graphics_definition.corpse_target_bindings;
        case GraphicsTargetRole::Default:
        default:
            return graphics_definition.default_target_bindings;
    }
}

static std::vector<GraphicsTargetBinding> &graphics_target_cache(
    FigureGraphics &graphics_definition,
    GraphicsTargetRole role)
{
    switch (role) {
        case GraphicsTargetRole::Action:
            return graphics_definition.action_target_bindings;
        case GraphicsTargetRole::Corpse:
            return graphics_definition.corpse_target_bindings;
        case GraphicsTargetRole::Default:
        default:
            return graphics_definition.default_target_bindings;
    }
}

const GraphicsTargetBinding *FigureGraphics::cached_target_binding(
    GraphicsTargetRole role,
    int direction_index,
    int frame) const
{
    const int frame_count = target_frame_count(role);
    if (direction_index < 0 ||
        direction_index >= GRAPHICS_DIRECTION8_COUNT ||
        frame <= 0 ||
        frame > frame_count) {
        return nullptr;
    }

    const std::vector<GraphicsTargetBinding> &cache = graphics_target_cache(*this, role);
    const size_t index = static_cast<size_t>(direction_index * frame_count + frame - 1);
    return index < cache.size() ? &cache[index] : nullptr;
}

const GraphicsTargetBinding *FigureGraphics::cached_target_binding_for_state(int figure_action_state, int wait_ticks, int image_offset, int corpse_frame_offset, int direction_index, int missile_wait_ticks) const
{
    const GraphicsTargetRole role = target_role_for_action_state(figure_action_state, wait_ticks, missile_wait_ticks);
    int frame = role == GraphicsTargetRole::Corpse ? corpse_frame_offset : image_offset;
    if (role == GraphicsTargetRole::Corpse &&
        corpse_frame_count > 0 &&
        frame >= corpse_frame_count) {
        frame = corpse_frame_count - 1;
    }
    return cached_target_binding(role, direction_index, frame + 1);
}

const GraphicsTargetBinding *FigureGraphics::cached_target_binding_for_figure(const Figure &figure) const
{
    if (figure.action_state != FIGURE_ACTION_149_CORPSE && static_frame_count > 0) {
        return cached_target_binding(GraphicsTargetRole::Default, 0, static_cast<int>(figure.id() % static_cast<unsigned int>(static_frame_count)) + 1);
    }
    return cached_target_binding_for_state(
        figure.action_state,
        figure.wait_ticks,
        figure.image_offset,
        corpse_frame_offset(figure),
        target_direction_index(figure),
        figure.wait_ticks_missile);
}

const GraphicsTargetBinding *FigureGraphics::cached_target_binding_for_figure_direction(
    const Figure &figure,
    int direction) const
{
    return cached_target_binding_for_state(
        figure.action_state,
        figure.wait_ticks,
        figure.image_offset,
        corpse_frame_offset(figure),
        legacy_direction_index(direction),
        figure.wait_ticks_missile);
}

void FigureGraphics::clear_cached_native_bindings()
{
    default_asset_target_.clear_cached_asset_binding();
    action_asset_target_.clear_cached_asset_binding();
    corpse_asset_target_.clear_cached_asset_binding();
    default_target_bindings.clear();
    action_target_bindings.clear();
    corpse_target_bindings.clear();
    resource_cart_.empty_payload = nullptr;
    directional_.payload = nullptr;
    directional_.climate_payloads.fill(nullptr);
    standard_definition.icon_payload = nullptr;
    for (FigureGraphicsOverlay &overlay : overlay_definitions) overlay.payload = nullptr;
    for (FigureGraphicsStateLayer &state_layer : state_layer_definitions) state_layer.payload = nullptr;
    for (FigureStandardFlagGraphics &flag : standard_definition.flags) flag.payload = nullptr;
    for (FigureMapFlagMarkerGraphics &marker : map_flag_definition.markers) marker.payload = nullptr;
}


static std::string figure_graphics_validation_detail(
    const FigureTypeDefinition &definition,
    GraphicsTargetRole role,
    int direction_index,
    int frame,
    const GraphicsTargetBinding &target)
{
    std::string detail = "figure=";
    detail += definition.attr();
    detail += " profile=* context=";
    switch (role) {
        case GraphicsTargetRole::Action:
            detail += "action";
            detail += " action=";
            detail += std::to_string(definition.graphics().action_state);
            break;
        case GraphicsTargetRole::Corpse:
            detail += "corpse";
            break;
        case GraphicsTargetRole::Default:
        default:
            detail += "default";
            break;
    }
    detail += " direction=";
    detail += graphics_direction8_suffix(direction_index);
    if (frame > 0) {
        detail += " frame=";
        detail += std::to_string(frame);
    }
    if (!target.path.empty()) {
        detail += " path=";
        detail += target.path;
    }
    if (!target.image.empty()) {
        detail += " image=";
        detail += target.image;
    }
    return detail;
}

static int fail_figure_graphics_validation(const char *message, const std::string &detail)
{
    set_failure_reason(message, detail.c_str());
    log_error(message, detail.c_str(), 0);
    return 0;
}


struct CachedFigureGraphicsTarget {
    const ImageGroupPayload *payload = nullptr;
    const ImageGroupEntry *entry = nullptr;
    const Animation *animation = nullptr;
};

static int bind_figure_graphics_payload_entry(const FigureTypeDefinition &definition, GraphicsTargetRole role, int direction_index, int frame, GraphicsTargetBinding &target, std::unordered_map<std::string, CachedFigureGraphicsTarget> &validated_targets)
{
    const std::string detail = figure_graphics_validation_detail(
        definition,
        role,
        direction_index,
        frame,
        target);
    const auto validate_frame = [&]() {
        if (target.uses_animation() && (target.frame <= 0 || target.frame > target.animation->frame_count())) {
            return fail_figure_graphics_validation("FigureType animation does not contain its requested frame.", detail);
        }
        return 1;
    };
    if (!target.is_complete()) {
        return fail_figure_graphics_validation(
            "FigureType graphics target is missing path.",
            detail);
    }

    std::string key = target.path;
    key += '\n';
    key += target.image;
    auto cached = validated_targets.find(key);
    if (cached != validated_targets.end()) {
        target.payload = cached->second.payload;
        target.entry = cached->second.entry;
        target.animation = cached->second.animation;
        return validate_frame();
    }

    if (!image_group_payload_load(target.path.c_str())) {
        return fail_figure_graphics_validation(
            "FigureType graphics path target could not be loaded.",
            detail);
    }
    const ImageGroupPayload *payload = image_group_payload_get(target.path.c_str());
    if (!payload) {
        return fail_figure_graphics_validation(
            "FigureType graphics payload could not be found after load.",
            detail);
    }
    const ImageGroupEntry *entry = target.image.empty() ?
        payload->default_entry() :
        payload->entry_for(target.image.c_str());
    if (!entry) {
        return fail_figure_graphics_validation(
            target.image.empty() ?
                "FigureType graphics path target has no default image entry." :
                "FigureType graphics image target could not be resolved.",
            detail);
    }
    target.payload = payload;
    target.entry = entry;
    target.animation = entry->has_animation() ? &entry->animation() : nullptr;
    if (!validate_frame()) return 0;
    validated_targets.emplace(std::move(key), CachedFigureGraphicsTarget { payload, entry, target.animation });
    return 1;
}

static int cache_figure_graphics_payload_target(FigureGraphics &graphics_definition, const FigureTypeDefinition &definition, GraphicsTargetRole role, std::unordered_map<std::string, CachedFigureGraphicsTarget> &validated_targets)
{
    std::vector<GraphicsTargetBinding> &cache = graphics_target_cache(graphics_definition, role);
    const int frame_count = graphics_definition.target_frame_count(role);
    cache.reserve(static_cast<size_t>(GRAPHICS_DIRECTION8_COUNT * frame_count));
    for (int direction_index = 0; direction_index < GRAPHICS_DIRECTION8_COUNT; direction_index++) {
        for (int frame = 1; frame <= frame_count; frame++) {
            const bool linear_default = role == GraphicsTargetRole::Default && definition.type() == FIGURE_FORT_STANDARD;
            const int target_direction = linear_default ? 0 : direction_index;
            GraphicsTargetBinding target = graphics_definition.target_binding(role, target_direction, frame);
            if (linear_default) target.image = extracted_payload_entry_name(graphics_definition.default_entry_offset + frame - 1);
            if (!bind_figure_graphics_payload_entry(definition, role, direction_index, frame, target, validated_targets)) {
                return 0;
            }
            cache.push_back(std::move(target));
        }
    }
    return 1;
}

int FigureGraphics::cache_native_payload_bindings(const FigureTypeDefinition &definition)
{
    clear_cached_native_bindings();
    std::unordered_map<std::string, CachedFigureGraphicsTarget> validated_targets;
    const auto cache_asset = [&](GraphicsTargetRole role) {
        GraphicsAssetReference &target = asset_target(role);
        if (!target.has_path() || target.cache_asset_binding()) return 1;
        const std::string detail = "figure=" + std::string(definition.attr()) + " context=asset path=" + target.path();
        return fail_figure_graphics_validation("FigureType graphics asset reference could not be resolved.", detail);
    };
    if (!cache_asset(GraphicsTargetRole::Default) || !cache_asset(GraphicsTargetRole::Action) || !cache_asset(GraphicsTargetRole::Corpse)) return 0;
    if (has_native_payload() && !cache_figure_graphics_payload_target(*this, definition, GraphicsTargetRole::Default, validated_targets)) {
        clear_cached_native_bindings();
        return 0;
    }
    if (has_action_native_payload() && !cache_figure_graphics_payload_target(*this, definition, GraphicsTargetRole::Action, validated_targets)) {
        clear_cached_native_bindings();
        return 0;
    }
    if (has_corpse_native_payload() && !cache_figure_graphics_payload_target(*this, definition, GraphicsTargetRole::Corpse, validated_targets)) {
        clear_cached_native_bindings();
        return 0;
    }
    const auto bind_auxiliary = [&](const std::string &path, const ImageGroupPayload *&payload, const char *role) {
        if (path.empty()) return 1;
        std::string detail = "figure=" + std::string(definition.attr()) + " context=" + role + " path=" + path;
        if (!image_group_payload_load(path.c_str())) return fail_figure_graphics_validation("FigureType auxiliary graphics path could not be loaded.", detail);
        payload = image_group_payload_get(path.c_str());
        return payload ? 1 : fail_figure_graphics_validation("FigureType auxiliary graphics payload could not be found after load.", detail);
    };
    const auto require_auxiliary_offset = [&](const ImageGroupPayload *payload, const std::string &path, const char *role, int offset) {
        if (!payload || payload_entry_at(payload, offset)) return 1;
        std::string detail = "figure=" + std::string(definition.attr()) + " context=" + role + " path=" + path + " image=" + extracted_payload_entry_name(offset);
        return fail_figure_graphics_validation("FigureType auxiliary graphics payload is missing a required frame.", detail);
    };
    if (!bind_auxiliary(resource_cart_.empty_path, resource_cart_.empty_payload, "resource_cart")) return 0;
    if (!bind_auxiliary(directional_.path, directional_.payload, "directional")) return 0;
    for (size_t climate = 0; climate < directional_.climate_paths.size(); ++climate) {
        if (!bind_auxiliary(directional_.climate_paths[climate], directional_.climate_payloads[climate], "directional climate")) return 0;
    }
    if (!bind_auxiliary(standard_definition.icon_path, standard_definition.icon_payload, "standard_icon")) return 0;
    if (standard_definition.icon_payload) {
        for (int icon_offset = 0; icon_offset < 10; ++icon_offset) {
            if (!payload_entry_at(standard_definition.icon_payload, icon_offset)) {
            std::string detail = "figure=" + std::string(definition.attr()) + " context=standard_icon path=" + standard_definition.icon_path + " image=" + extracted_payload_entry_name(icon_offset);
                return fail_figure_graphics_validation("FigureType standard icon payload is incomplete.", detail);
            }
        }
    }
    for (FigureGraphicsOverlay &overlay : overlay_definitions) if (!bind_auxiliary(overlay.path, overlay.payload, overlay.role.c_str())) return 0;
    for (FigureGraphicsStateLayer &state_layer : state_layer_definitions) if (!bind_auxiliary(state_layer.path, state_layer.payload, state_layer.role.c_str())) return 0;
    for (FigureStandardFlagGraphics &flag : standard_definition.flags) if (!bind_auxiliary(flag.path, flag.payload, "standard_flag")) return 0;
    for (FigureMapFlagMarkerGraphics &marker : map_flag_definition.markers) if (!bind_auxiliary(marker.path, marker.payload, "map_flag_marker")) return 0;
    if (resource_cart_.empty_payload) {
        for (int direction = 0; direction < GRAPHICS_DIRECTION8_COUNT; ++direction) if (!require_auxiliary_offset(resource_cart_.empty_payload, resource_cart_.empty_path, "resource_cart", direction)) return 0;
    }
    if (directional_.payload) {
        std::vector<int> base_offsets = { directional_.default_base_image_offset };
        for (const FigureDirectionalPoseGraphics &pose : directional_.poses) base_offsets.push_back(pose.base_image_offset);
        for (int base_offset : base_offsets) {
            for (int direction = 0; direction < GRAPHICS_DIRECTION8_COUNT; ++direction) {
                for (int image_offset = 0; image_offset < max_image_offset; ++image_offset) {
                    const int offset = base_offset + direction + directional_.frame_stride * (image_offset / directional_.frame_divisor);
                    if (!require_auxiliary_offset(directional_.payload, directional_.path, "directional", offset)) return 0;
                    for (size_t climate = 0; climate < directional_.climate_paths.size(); ++climate) {
                        if (!require_auxiliary_offset(directional_.climate_payloads[climate], directional_.climate_paths[climate], "directional climate", offset)) return 0;
                    }
                }
            }
        }
    }
    for (const FigureGraphicsOverlay &overlay : overlay_definitions) {
        if (!overlay.payload) continue;
        const int direction_count = overlay.direction == FigureOverlayDirection::Figure ? GRAPHICS_DIRECTION8_COUNT : 1;
        const int frame_count = overlay.frame == FigureOverlayFrame::Figure ? max_image_offset : 1;
        for (int direction = 0; direction < direction_count; ++direction) {
            for (int frame = 0; frame < frame_count; ++frame) {
                const int offset = overlay.legacy_image_offset(direction, frame, 0);
                if (!require_auxiliary_offset(overlay.payload, overlay.path, overlay.role.c_str(), offset)) return 0;
            }
        }
    }
    for (const FigureGraphicsStateLayer &state_layer : state_layer_definitions) {
        if (!state_layer.payload) continue;
        std::vector<int> frame_offsets = { 0 };
        if (state_layer.frame == FigureStateFrame::ImageOffset) {
            frame_offsets.clear();
            for (int frame = 0; frame < max_image_offset; ++frame) frame_offsets.push_back(frame);
        } else if (state_layer.frame == FigureStateFrame::MissileLauncher) {
            frame_offsets = missile_launcher_.frames;
            frame_offsets.push_back(missile_launcher_.after_frame);
        }
        for (int direction = 0; direction < GRAPHICS_DIRECTION8_COUNT; ++direction) {
            for (int frame : frame_offsets) {
                const int offset = state_layer.legacy_image_offset(direction, 0, frame);
                if (!require_auxiliary_offset(state_layer.payload, state_layer.path, state_layer.role.c_str(), offset)) return 0;
            }
        }
    }
    for (const FigureStandardFlagGraphics &flag : standard_definition.flags) {
        if (!flag.payload) continue;
        if (!require_auxiliary_offset(flag.payload, flag.path, "standard_flag", flag.halted_frame_offset)) return 0;
        for (int frame = 0; frame < standard_definition.moving_frame_count; ++frame) {
            const int offset = flag.moving_base_offset + frame;
            if (!require_auxiliary_offset(flag.payload, flag.path, "standard_flag", offset)) return 0;
        }
    }
    for (const FigureMapFlagMarkerGraphics &marker : map_flag_definition.markers) {
        if (!require_auxiliary_offset(marker.payload, marker.path, "map_flag_marker", marker.image_offset)) return 0;
    }
    return 1;
}



} // namespace figure_type_registry_impl
