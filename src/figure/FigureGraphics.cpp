#include "figure/FigureGraphics.h"
#include "figure/figure_type_registry_internal.h"

#include "assets/image_group_payload.h"
#include "core/crash_context.h"
#include "core/image.h"
#include "core/log.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/image.h"
#include "game/Animation.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace figure_type_registry_impl {

namespace {

constexpr int RESOURCE_CART_GRAPHICS_COUNT = RESOURCE_SLOT_COUNT;
constexpr int RESOURCE_CART_MARKER_BASE = 0x3f000000;

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

resource_type legacy_cart_overlay_resource(const Figure &figure)
{
    if (figure.type == FIGURE_LIGHTHOUSE_SUPPLIER &&
        figure.action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING) {
        return static_cast<resource_type>(figure.collecting_item_id);
    }
    return static_cast<resource_type>(figure.resource_id);
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

int corpse_frame_offset(const Figure &figure)
{
    return figure.action_state == FIGURE_ACTION_149_CORPSE ?
        figure_image_corpse_offset(const_cast<Figure *>(&figure)) :
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
    return animation && animation->has_frames();
}

RuntimeDrawSlice GraphicsTargetBinding::resolved_slice() const
{
    if (uses_animation()) {
        RuntimeDrawSlice slice = animation->frame_slice_at_offset(frame);
        if (slice.is_valid()) {
            return slice;
        }
    }

    const RuntimeDrawSlice *slice = entry ? entry->footprint() : nullptr;
    return slice ? *slice : RuntimeDrawSlice();
}

int FigureGraphicsLayer::is_valid() const
{
    return slice.is_valid();
}

int FigureGraphics::default_source_count() const
{
    return (image_group ? 1 : 0) +
        (image_asset != ASSET_MAX_KEY ? 1 : 0) +
        (!path_pattern.empty() || !image_pattern.empty() ? 1 : 0);
}

int FigureGraphics::action_source_count() const
{
    return (action_image_group ? 1 : 0) +
        (!action_path_pattern.empty() || !action_image_pattern.empty() ? 1 : 0);
}

int FigureGraphics::corpse_source_count() const
{
    return (corpse_image_group ? 1 : 0) +
        (corpse_image_asset != ASSET_MAX_KEY ? 1 : 0) +
        (!corpse_path_pattern.empty() || !corpse_image_pattern.empty() ? 1 : 0);
}

int FigureGraphics::has_native_payload() const
{
    return !path_pattern.empty();
}

int FigureGraphics::has_action_native_payload() const
{
    return !action_path_pattern.empty();
}

int FigureGraphics::has_corpse_native_payload() const
{
    return !corpse_path_pattern.empty();
}

int FigureGraphics::action_graphics_matches(int figure_action_state, int wait_ticks) const
{
    return action_state &&
        figure_action_state == action_state &&
        wait_ticks >= action_min_wait_ticks &&
        has_action_native_payload();
}

int FigureGraphics::has_fixed_logical_size() const
{
    return fixed_logical_size.width > 0 && fixed_logical_size.height > 0;
}

int FigureGraphics::has_legacy_default_source() const
{
    return image_group || image_asset != ASSET_MAX_KEY;
}

int FigureGraphics::has_legacy_resource_cart_graphics() const
{
    return image_asset != ASSET_MAX_KEY &&
        corpse_image_asset != ASSET_MAX_KEY &&
        cart_mode == CartGraphicsMode::ResourceLoad;
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

int FigureGraphics::resource_cart_marker_direction(unsigned int image_id)
{
    return resource_cart_marker_is(image_id) ? static_cast<int>(image_id) - RESOURCE_CART_MARKER_BASE : 0;
}

int FigureGraphics::uses_legacy_cart_overlay(figure_type type)
{
    switch (type) {
        case FIGURE_CART_PUSHER:
        case FIGURE_WAREHOUSEMAN:
        case FIGURE_LION_TAMER:
        case FIGURE_DOCKER:
        case FIGURE_NATIVE_TRADER:
        case FIGURE_IMMIGRANT:
        case FIGURE_EMIGRANT:
        case FIGURE_LIGHTHOUSE_SUPPLIER:
            return 1;
        default:
            return 0;
    }
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

RuntimeDrawSlice FigureGraphics::legacy_cart_overlay_slice(const Figure &figure)
{
    if (!resource_cart_marker_is(figure.cart_image_id)) {
        return legacy_image(figure.cart_image_id).runtime_slice();
    }

    resource_type resource = legacy_cart_overlay_resource(figure);
    int loads = figure.loads_sold_or_carrying > 0 ? figure.loads_sold_or_carrying : 1;
    if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        resource = RESOURCE_NONE;
        loads = 0;
    }
    return resource_cart_image_for_direction(
        resource,
        loads,
        resource_is_food(resource),
        resource_cart_marker_direction(figure.cart_image_id)).runtime_slice();
}

FigureGraphicsLayer FigureGraphics::legacy_cart_overlay_layer(
    const Figure &figure,
    GraphicsPoint offset,
    int draw_before_base)
{
    FigureGraphicsLayer layer;
    layer.slice = legacy_cart_overlay_slice(figure);
    layer.offset = offset;
    layer.draw_before_base = draw_before_base >= 0 ? draw_before_base : offset.y < 0;
    layer.height = layer.slice.height;
    return layer;
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
        const int image_id = ::image_group(GROUP_FIGURE_CARTPUSHER_CART) + cart_direction;
        return legacy_image(image_id).runtime_slice();
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
    if (image_asset != ASSET_MAX_KEY) {
        return assets_lookup_image_id(image_asset) + image_group_offset;
    }
    return ::image_group(image_group) + image_group_offset;
}

int FigureGraphics::legacy_corpse_image_base() const
{
    if (corpse_image_asset != ASSET_MAX_KEY) {
        return assets_lookup_image_id(corpse_image_asset) + corpse_image_group_offset;
    }
    if (corpse_image_group) {
        return ::image_group(corpse_image_group) + corpse_image_group_offset;
    }
    return legacy_image_base() + 96;
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
    const int action_base = action_image_group ?
        ::image_group(action_image_group) + action_image_group_offset :
        legacy_image_base();
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

GraphicsTargetRole FigureGraphics::target_role_for_action_state(int figure_action_state, int wait_ticks) const
{
    if (figure_action_state == FIGURE_ACTION_149_CORPSE) {
        return GraphicsTargetRole::Corpse;
    }
    if (action_graphics_matches(figure_action_state, wait_ticks)) {
        return GraphicsTargetRole::Action;
    }
    return GraphicsTargetRole::Default;
}

int FigureGraphics::target_frame_count(GraphicsTargetRole role) const
{
    if (role == GraphicsTargetRole::Corpse) {
        return corpse_frame_count > 0 ? corpse_frame_count : 8;
    }
    return max_image_offset;
}

GraphicsTargetBinding FigureGraphics::target_binding(
    GraphicsTargetRole role,
    int direction_index,
    int frame) const
{
    const std::string *path = &path_pattern;
    const std::string *image = &image_pattern;
    switch (role) {
        case GraphicsTargetRole::Action:
            path = &action_path_pattern;
            image = &action_image_pattern;
            break;
        case GraphicsTargetRole::Corpse:
            path = &corpse_path_pattern;
            image = &corpse_image_pattern;
            break;
        case GraphicsTargetRole::Default:
        default:
            break;
    }

    GraphicsTargetBinding binding;
    binding.path = graphics_expand_direction_frame_pattern(*path, direction_index, frame);
    binding.image = graphics_expand_direction_frame_pattern(*image, direction_index, frame);
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

const GraphicsTargetBinding *FigureGraphics::cached_target_binding_for_state(
    int figure_action_state,
    int wait_ticks,
    int image_offset,
    int corpse_frame_offset,
    int direction_index) const
{
    const GraphicsTargetRole role = target_role_for_action_state(figure_action_state, wait_ticks);
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
    return cached_target_binding_for_state(
        figure.action_state,
        figure.wait_ticks,
        figure.image_offset,
        corpse_frame_offset(figure),
        target_direction_index(figure));
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
        legacy_direction_index(direction));
}

void FigureGraphics::clear_cached_native_bindings()
{
    default_target_bindings.clear();
    action_target_bindings.clear();
    corpse_target_bindings.clear();
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

static int bind_figure_graphics_payload_entry(
    const FigureTypeDefinition &definition,
    GraphicsTargetRole role,
    int direction_index,
    int frame,
    GraphicsTargetBinding &target,
    std::unordered_map<std::string, CachedFigureGraphicsTarget> &validated_targets)
{
    const std::string detail = figure_graphics_validation_detail(
        definition,
        role,
        direction_index,
        frame,
        target);
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
        return 1;
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
    validated_targets.emplace(std::move(key), CachedFigureGraphicsTarget { payload, entry, target.animation });
    return 1;
}

static int cache_figure_graphics_payload_target(
    FigureGraphics &graphics_definition,
    const FigureTypeDefinition &definition,
    GraphicsTargetRole role,
    std::unordered_map<std::string, CachedFigureGraphicsTarget> &validated_targets)
{
    std::vector<GraphicsTargetBinding> &cache = graphics_target_cache(graphics_definition, role);
    const int frame_count = graphics_definition.target_frame_count(role);
    cache.reserve(static_cast<size_t>(GRAPHICS_DIRECTION8_COUNT * frame_count));
    for (int direction_index = 0; direction_index < GRAPHICS_DIRECTION8_COUNT; direction_index++) {
        for (int frame = 1; frame <= frame_count; frame++) {
            GraphicsTargetBinding target = graphics_definition.target_binding(role, direction_index, frame);
            if (!bind_figure_graphics_payload_entry(
                    definition,
                    role,
                    direction_index,
                    frame,
                    target,
                    validated_targets)) {
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
    if (has_native_payload() &&
        !cache_figure_graphics_payload_target(
            *this,
            definition,
            GraphicsTargetRole::Default,
            validated_targets)) {
        clear_cached_native_bindings();
        return 0;
    }
    if (has_action_native_payload() &&
        !cache_figure_graphics_payload_target(
            *this,
            definition,
            GraphicsTargetRole::Action,
            validated_targets)) {
        clear_cached_native_bindings();
        return 0;
    }
    if (has_corpse_native_payload() &&
        !cache_figure_graphics_payload_target(
            *this,
            definition,
            GraphicsTargetRole::Corpse,
            validated_targets)) {
        clear_cached_native_bindings();
        return 0;
    }
    return 1;
}



} // namespace figure_type_registry_impl
