#include "building/granary.h"
#include "building/HousingProfileDef.h"
#include "building/industry.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/road_aqueduct_rules.h"
#include "map/tiles.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/connectable.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/production_runtime.h"

#include "assets/image_group_payload.h"
#include "building/BuildingGraphicsDef.h"
#include "building/BuildingGeometry.h"
#include "building/building_runtime_graphics.h"
#include "building/variant.h"
#include "core/calc.h"
#include "core/crash_context.h"
#include "core/direction.h"

#include "core/image_group.h"
#include "city/view.h"
#include "game/resource.h"
#include "map/terrain.h"
#include "map/random.h"
#include "core/log.h"

#include <cstdio>
#include <cstdint>
#include <exception>
#include <string>

namespace {

void make_building_context(char *buffer, size_t buffer_size, const building_runtime *runtime)
{
    if (!runtime) {
        snprintf(buffer, buffer_size, "building=null");
        return;
    }
    const Building &b = runtime->building;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (b.type) {
        snprintf(
            buffer,
            buffer_size,
            "type=%s grid_offset=%d",
            definition ? definition->attr() : "unknown",
            b.grid_offset());
    } else {
        snprintf(buffer, buffer_size, "building=null");
    }
}

void log_building_scope_state(void *userdata)
{
    const building_runtime *runtime = static_cast<const building_runtime *>(userdata);
    if (!runtime) {
        return;
    }
    const Building &b = runtime->building;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (!b.type) {
        return;
    }

    const building_type_registry_impl::GraphicsTarget *target = definition && runtime ? definition->graphics().resolve_target(runtime->building) : nullptr;

    char details[256];
    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(b);
    const building_type_registry_impl::BuildingGeometryBounds bounds = geometry.bounds();
    snprintf(
        details,
        sizeof(details),
        "state=%d footprint=%dx%d cells=%d desirability=%d water=%d upgrade=%d target_path=%s target_image=%s",
        b.state_id(),
        geometry.valid() ? bounds.width() : 0,
        geometry.valid() ? bounds.height() : 0,
        static_cast<int>(geometry.cells().size()),
        b.desirability(),
        b.has_water_access(),
        definition ? definition->upgrade_level_for(b) : 0,
        target && target->has_path() ? target->path() : "",
        target && target->has_image() ? target->image() : "");
    log_info("Graphics building state", details, 0);
}

const char *building_type_attr_or_unknown(const building_runtime *runtime)
{
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    return definition ? definition->attr() : "unknown";
}

const char *graphics_target_image_or_entry_id(
    const building_type_registry_impl::GraphicsTarget *target,
    const ImageGroupEntry *entry)
{
    if (target && target->has_image()) {
        return target->image();
    }
    if (entry) {
        return entry->id().c_str();
    }
    return "";
}

void format_rebuild_failure_detail(
    char *buffer,
    size_t buffer_size,
    const building_runtime *runtime,
    const char *reason,
    const building_type_registry_impl::GraphicsTarget *target = nullptr,
    const ImageGroupEntry *entry = nullptr)
{
    snprintf(
        buffer,
        buffer_size,
        "building_attr=%s path=%s image=%s reason=%s",
        building_type_attr_or_unknown(runtime),
        target && target->has_path() ? target->path() : "",
        graphics_target_image_or_entry_id(target, entry),
        reason ? reason : "");
}

[[noreturn]] void report_rebuild_failure(
    const building_runtime *runtime,
    const char *reason,
    const building_type_registry_impl::GraphicsTarget *target = nullptr,
    const ImageGroupEntry *entry = nullptr)
{
    char detail[512];
    format_rebuild_failure_detail(detail, sizeof(detail), runtime, reason, target, entry);
    error_context_report_fatal_error_dialog(
        "Building graphics invariant violated",
        "Native building graphics cache rebuild failed.",
        detail);
    std::terminate();
}

[[noreturn]] void report_layer_rebuild_failure(
    const building_runtime *runtime,
    const char *reason,
    const building_type_registry_impl::GraphicsLayer &layer,
    const ImageGroupEntry *entry = nullptr)
{
    char detail[512];
    snprintf(
        detail,
        sizeof(detail),
        "building_attr=%s path=%s image=%s reason=%s",
        building_type_attr_or_unknown(runtime),
        layer.has_path() ? layer.path() : "",
        layer.has_image() ? layer.image() : (entry ? entry->id().c_str() : ""),
        reason ? reason : "");
    error_context_report_fatal_error_dialog(
        "Building graphics invariant violated",
        "Native building graphics layer cache rebuild failed.",
        detail);
    std::terminate();
}

int selected_option_for_selection(
    const Building &building,
    building_type_registry_impl::GraphicsOptionSelection selection,
    int option_count,
    unsigned char graphics_variant)
{
    if (option_count <= 0) {
        return 0;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::Connectable) {
        int option = building_connectable_graphics_option(building);
        return option < 0 ? 0 : option;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::StorageLoad) {
        const int empty_loads = building.resource_amount(RESOURCE_NONE);
        if (empty_loads < QUARTER_GRANARY) {
            return 4;
        }
        if (empty_loads < HALF_GRANARY) {
            return 3;
        }
        if (empty_loads < THREEQUARTERS_GRANARY) {
            return 2;
        }
        if (empty_loads < FULL_GRANARY) {
            return 1;
        }
        return 0;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::BuildRotation) {
        int option = option_count == 4 ?
            (building.orientation() + city_view_orientation() / 2) % option_count :
            graphics_variant % option_count;
        return option < 0 ? option + option_count : option;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::Orientation) {
        return building_type_registry_impl::graphics_orientation_option_index(
            building.orientation(), city_view_orientation(), option_count);
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::ProductionProgress) {
        const ::building *record = building.record();
        if (!record) {
            return 0;
        }
        Production *production = production_runtime_impl::get_or_create_primary(building);
        const Building *owner = building.Composition ? building.Composition->owner() : &building;
        const int max_value = production ? production->max_progress() :
            (building.type && !building.type->production_methods().empty() ?
                building.type->production_methods().front()->max_progress_for(owner ? *owner : building) :
                0);
        if (max_value <= 0) {
            return calc_bound(record->data.industry.progress, 0, option_count - 1);
        }
        int percentage = calc_percentage(record->data.industry.progress, max_value);
        percentage = calc_bound(percentage, 0, 100);
        int option = percentage * option_count / 101;
        return calc_bound(option, 0, option_count - 1);
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::StoragePermission) {
        if (building.has_plague()) {
            return -1;
        }
        const int permission_mask = building.blocked_storage_permission_mask();
        return permission_mask > 0 ? calc_bound(permission_mask - 1, 0, option_count - 1) : -1;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::GatehouseOrientation) {
        const int gate_orientation = building.orientation();
        const bool vertical_view = city_view_orientation() == DIR_0_TOP || city_view_orientation() == DIR_4_BOTTOM;
        const int rotated = (gate_orientation == 1) != vertical_view;
        return rotated ? 1 % option_count : 0;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::RoadCrossing) {
        const int grid_offset = building.grid_offset();
        const int option = road_aqueduct_crossing_option(
            map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_ROAD),
            map_tiles_is_paved_road(grid_offset));
        return option % option_count;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::Rubble) {
        const building_type_registry_impl::BuildingType *original_type =
            building.Rubble ? building.Rubble->original_type() : nullptr;
        const building_type_registry_impl::HousingProfileDef *profile =
            original_type && original_type->has_housing() ? original_type->housing_def().profile : nullptr;
        const int original_house_level = profile ? profile->compatibility_level : -1;
        const int was_tent = original_house_level >= HOUSE_SMALL_TENT && original_house_level <= HOUSE_LARGE_TENT;
        if (building.Rubble && building.Rubble->is_burning()) {
            const int burning_variant = map_random_get(building.grid_offset()) & 3;
            return was_tent || option_count <= 1 ? 0 : 1 + burning_variant % (option_count - 1);
        }
        return map_random_get(building.grid_offset()) % option_count;
    }
    int option = building_variant_get_graphics_option(building, 0, graphics_variant);
    return option < 0 ? 0 : option;
}

int selected_option_for_layer(
    const Building &building,
    const building_type_registry_impl::GraphicsLayer &layer,
    unsigned char graphics_variant)
{
    return selected_option_for_selection(building, layer.option_selection(), layer.option_count(), graphics_variant);
}

void draw_shifted_runtime_slice(
    const RuntimeDrawSlice *slice,
    int x,
    int y,
    int x_offset,
    int y_offset,
    color_t color,
    float scale)
{
    if (slice) {
        runtime_texture_draw(*slice, x + x_offset, y + y_offset, color, scale);
    }
}

}

int building_runtime_graphics_selected_option(
    const Building &building,
    const building_type_registry_impl::GraphicsTarget &target,
    unsigned char graphics_variant)
{
    return selected_option_for_selection(building, target.option_selection(), target.option_count(), graphics_variant);
}

void building_runtime::clear_cached_graphics_bindings()
{
    graphics_cache_ = CachedGraphicsBindings();
}

int building_runtime::has_native_graphics_definition() const
{
    const Building b = building;
    return b.type &&
        b.type->has_graphic();
}

int building_runtime::building_state_supports_native_graphics() const
{
    const Building b = building;
    return b.state_id() == BUILDING_STATE_CREATED ||
        b.state_id() == BUILDING_STATE_IN_USE ||
        b.state_id() == BUILDING_STATE_MOTHBALLED ||
        b.state_id() == BUILDING_STATE_RUBBLE;
}

void building_runtime::invalidate_graphics_cache()
{
    graphics_state_.invalidate();
}

std::uint64_t building_runtime::graphics_owner_generation() const
{
    const Building b = building;
    if (!b.type || !b.record()) {
        return 0;
    }
    const Building *owner = b.type && b.type->bridge().is_bridge() ?
        &b.dynamic_bridge_owner() :
        (b.Composition ? b.Composition->owner() : &b);
    building_runtime *owner_runtime = owner ? owner->runtime_instance() : nullptr;
    return owner_runtime && owner_runtime != this ? owner_runtime->graphics_state_.generation() : 0;
}

const building_type_registry_impl::GraphicsTarget *building_runtime::resolve_graphic_target() const
{
    if (!has_native_graphics_definition()) {
        return nullptr;
    }
    return type().graphics().resolve_target(building);
}

void building_runtime::assign_graphic_variant(int force_reseed)
{
    Building b = building;
    if (!b.type) {
        return;
    }

    refresh_runtime_state();
    int graphics_option = building_variant_get_graphics_option(b, force_reseed, graphics_variant());
    if (graphics_option < 0) {
        return;
    }

    unsigned char variant = static_cast<unsigned char>(graphics_option);
    if (graphics_variant() == variant) {
        return;
    }
    set_graphics_variant(variant);
}

int building_runtime::resolve_graphic_binding(
    const building_type_registry_impl::GraphicsTarget &target,
    const ImageGroupPayload *&payload,
    const ImageGroupEntry *&entry) const
{
    payload = nullptr;
    entry = nullptr;

    if (!has_native_graphics_definition() || !target.has_path()) {
        return 0;
    }

    if (!image_group_payload_load(target.path())) {
        report_rebuild_failure(this, "payload_group_load_failed", &target);
        return 0;
    }

    payload = image_group_payload_get(target.path());
    if (!payload) {
        report_rebuild_failure(this, "payload_group_handle_null", &target);
        return 0;
    }

    entry = target.has_image() ? payload->entry_for(target.image()) : payload->default_entry();
    if (!entry) {
        report_rebuild_failure(
            this,
            target.has_image() ? "payload_entry_lookup_failed" : "payload_default_entry_lookup_failed",
            &target);
        return 0;
    }

    return 1;
}

static int resolve_graphic_layer_binding(
    const building_runtime *runtime,
    const building_type_registry_impl::GraphicsLayer &layer,
    const ImageGroupPayload *&payload,
    const ImageGroupEntry *&entry)
{
    payload = nullptr;
    entry = nullptr;

    if (!layer.has_path()) {
        report_layer_rebuild_failure(runtime, "layer_path_missing", layer);
        return 0;
    }
    if (!image_group_payload_load(layer.path())) {
        report_layer_rebuild_failure(runtime, "payload_group_load_failed", layer);
        return 0;
    }

    payload = image_group_payload_get(layer.path());
    if (!payload) {
        report_layer_rebuild_failure(runtime, "payload_group_handle_null", layer);
        return 0;
    }

    entry = layer.has_image() ? payload->entry_for(layer.image()) : payload->default_entry();
    if (!entry) {
        report_layer_rebuild_failure(
            runtime,
            layer.has_image() ? "payload_entry_lookup_failed" : "payload_default_entry_lookup_failed",
            layer);
        return 0;
    }
    return 1;
}

static int animation_owner_is_working_for_runtime_preview(Building building)
{
    Building *animation_owner = building.type && building.type->bridge().is_bridge() ?
        &building.dynamic_bridge_owner() :
        (building.Composition ? building.Composition->owner() : &building);
    if (animation_owner && animation_owner->id) {
        return animation_owner->is_working();
    }
    return building.is_working();
}

// Input: one live building instance in the city animation draw stage.
// Output: advances the native XML animation cursor at the same layer where legacy image groups tick.
void building_runtime::advance_graphic_animation(int animation_cursor)
{
    if (!has_native_graphics_definition()) {
        return;
    }

    ensure_cached_graphics_bindings();
    const Animation *animation = cached_animation();
    if (!animation || !graphics_cache_.owns_graphic_animation) {
        return;
    }

    building.animate().frame_offset(*animation, 1, animation_cursor);
    // The frame cursor has just moved; the draw slice is rebuilt lazily by
    // graphic_animation() so ghosts and normal city draws share one read path.
    graphics_cache_.animation_slice = RuntimeDrawSlice();
}

// Input: one live building instance whose cached bindings may include an animation image-group entry.
// Output: the animation exposed by that cached animation entry, or null when this building instance has no native animation.
const Animation *building_runtime::cached_animation() const
{
    return graphics_cache_.animation_entry && graphics_cache_.animation_entry->has_animation() ?
        &graphics_cache_.animation_entry->animation() :
        nullptr;
}

int building_runtime::resolve_graphics_cache()
{
    if (!has_native_graphics_definition()) {
        clear_cached_graphics_bindings();
        return 0;
    }

    ensure_cached_graphics_bindings();
    return graphics_cache_.owns_graphics;
}

const RuntimeDrawSlice *building_runtime::cached_graphic_footprint() const
{
    return graphics_cache_.base_entry ? graphics_cache_.base_entry->footprint() : nullptr;
}

const RuntimeDrawSlice *building_runtime::cached_graphic_top() const
{
    return graphics_cache_.base_entry ? graphics_cache_.base_entry->top() : nullptr;
}

void building_runtime::advance_cached_graphic_animation(int animation_cursor)
{
    const Animation *animation = cached_animation();
    if (!animation || !graphics_cache_.owns_graphic_animation) {
        return;
    }

    building.animate().frame_offset(*animation, 1, animation_cursor);
    graphics_cache_.animation_slice = RuntimeDrawSlice();
}

const RuntimeDrawSlice *building_runtime::cached_graphic_animation(int animation_cursor)
{
    rebuild_cached_animation_slice(animation_cursor);
    return graphics_cache_.animation_slice.is_valid() ? &graphics_cache_.animation_slice : nullptr;
}

int building_runtime::cached_graphics_no_draw() const
{
    return graphics_cache_.no_draw;
}

int building_runtime::cached_graphics_uses_terrain_foundation() const
{
    return graphics_cache_.terrain_foundation;
}

void building_runtime::draw_cached_graphic_layers(
    building_type_registry_impl::GraphicsLayerStage stage,
    int animation_cursor,
    int x,
    int y,
    color_t color,
    float scale)
{
    for (CachedGraphicsBindings::Layer &layer : graphics_cache_.layers) {
        if (layer.stage == building_type_registry_impl::GraphicsLayerStage::Auto) {
            if (stage == building_type_registry_impl::GraphicsLayerStage::Footprint) {
                draw_shifted_runtime_slice(layer.entry ? layer.entry->footprint() : nullptr,
                    x, y, layer.x_offset, layer.y_offset, color, scale);
            } else if (stage == building_type_registry_impl::GraphicsLayerStage::Top) {
                draw_shifted_runtime_slice(layer.entry ? layer.entry->top() : nullptr,
                    x, y, layer.x_offset, layer.y_offset, color, scale);
            }
            continue;
        }
        if (layer.stage != stage) {
            continue;
        }
        if (layer.owns_animation && layer.entry && layer.entry->has_animation()) {
            const Animation &animation = layer.entry->animation();
            const int animation_offset = building.animate().frame_offset(animation, 1, animation_cursor);
            if (animation_offset > 0) {
                RuntimeDrawSlice frame_slice = animation.frame_slice_at_offset(animation_offset);
                if (frame_slice.is_valid()) {
                    layer.animation_slice = frame_slice;
                    draw_shifted_runtime_slice(&layer.animation_slice, x, y, layer.x_offset, layer.y_offset, color, scale);
                    continue;
                }
            }
            layer.animation_slice = RuntimeDrawSlice();
        }
        draw_shifted_runtime_slice(layer.entry ? layer.entry->footprint() : nullptr,
            x, y, layer.x_offset, layer.y_offset, color, scale);
        draw_shifted_runtime_slice(layer.entry ? layer.entry->top() : nullptr,
            x, y, layer.x_offset, layer.y_offset, color, scale);
    }
}

void building_runtime::draw_cached_graphic_layer_animations(
    int animation_cursor,
    int x,
    int y,
    color_t color,
    float scale)
{
    for (CachedGraphicsBindings::Layer &layer : graphics_cache_.layers) {
        if (layer.stage != building_type_registry_impl::GraphicsLayerStage::Animation ||
            !layer.owns_animation || !layer.entry || !layer.entry->has_animation()) {
            continue;
        }
        const Animation &animation = layer.entry->animation();
        const int animation_offset = building.animate().frame_offset(animation, 1, animation_cursor);
        if (animation_offset <= 0) {
            layer.animation_slice = RuntimeDrawSlice();
            continue;
        }
        RuntimeDrawSlice frame_slice = animation.frame_slice_at_offset(animation_offset);
        if (!frame_slice.is_valid()) {
            layer.animation_slice = RuntimeDrawSlice();
            continue;
        }

        layer.animation_slice = frame_slice;
        draw_shifted_runtime_slice(&layer.animation_slice, x, y, layer.x_offset, layer.y_offset, color, scale);
    }
}

int building_runtime::cached_owns_graphic_animation() const
{
    return graphics_cache_.owns_graphic_animation;
}

// Input: one live building instance whose cached animation binding already points at an image-group entry.
// Output: the current animation frame slice for that building instance, or an invalid slice when the animation should not draw now.
void building_runtime::rebuild_cached_animation_slice(int animation_cursor)
{
    graphics_cache_.animation_slice = RuntimeDrawSlice();
    const Animation *animation_ptr = cached_animation();
    if (!animation_ptr || !graphics_cache_.owns_graphic_animation) {
        return;
    }

    const Animation &animation = *animation_ptr;
    const int animation_offset = building.animate().frame_offset(animation, 0, animation_cursor);
    if (animation_offset <= 0) {
        return;
    }

    RuntimeDrawSlice frame_slice = animation.frame_slice_at_offset(animation_offset);
    if (!frame_slice.is_valid()) {
        return;
    }
    if (graphics_cache_.base_entry && graphics_cache_.base_entry->top()) {
        frame_slice.draw_offset_y += graphics_cache_.base_entry->top()->draw_offset_y;
    }
    graphics_cache_.animation_slice = frame_slice;
}

// Input: one live building instance whose graphics state or composition owner may have changed.
// Output: makes sure the cached image-group bindings match the latest explicit invalidation generation.
void building_runtime::ensure_cached_graphics_bindings()
{
    if (!has_native_graphics_definition()) {
        clear_cached_graphics_bindings();
        return;
    }

    refresh_runtime_state();
    if (graphics_cache_.resolved &&
        graphics_cache_.generation == graphics_state_.generation() &&
        graphics_cache_.owner_generation == graphics_owner_generation() &&
        graphics_cache_.view_orientation_generation == city_view_orientation_generation()) {
        return;
    }

    rebuild_cached_graphics_bindings();
}

// Input: one live building instance plus its shared BuildingType definition.
// Output: the authoritative cached base/animation image-group bindings for that building instance, or a disabled native cache on soft failure.
void building_runtime::rebuild_cached_graphics_bindings()
{
    clear_cached_graphics_bindings();
    if (!has_native_graphics_definition()) {
        return;
    }

    refresh_runtime_state();
    graphics_cache_.resolved = 1;
    graphics_cache_.generation = graphics_state_.generation();
    graphics_cache_.owner_generation = graphics_owner_generation();
    graphics_cache_.view_orientation_generation = city_view_orientation_generation();
    graphics_cache_.owns_graphic_animation = 0;
    if (!building_state_supports_native_graphics()) {
        return;
    }

    const building_type_registry_impl::GraphicsTarget *target = resolve_graphic_target();
    if (!target) {
        report_rebuild_failure(this, "target_selection_failed");
        return;
    }
    // Conditional graphics pick a target first; the saved variant byte only picks
    // among equivalent options inside that already-selected target.
    building_type_registry_impl::GraphicsTarget resolved_target =
        target->resolved_option(
            static_cast<unsigned char>(building_runtime_graphics_selected_option(building, *target, graphics_variant())));
    graphics_cache_.terrain_foundation = resolved_target.uses_terrain_foundation();
    if (resolved_target.no_draw()) {
        graphics_cache_.owns_graphics = 1;
        graphics_cache_.no_draw = 1;
        return;
    }

    const ImageGroupPayload *payload = nullptr;
    const ImageGroupEntry *entry = nullptr;
    if (!resolve_graphic_binding(resolved_target, payload, entry)) {
        return;
    }

    if (!entry->footprint()) {
        report_rebuild_failure(this, "public_footprint_invalid_after_materialization", &resolved_target, entry);
        return;
    }

    const int animation_owner_is_working = animation_owner_is_working_for_runtime_preview(building);

    graphics_cache_.base_payload = payload;
    graphics_cache_.base_entry = entry;
    if (resolved_target.animation_enabled() && animation_owner_is_working && entry->has_animation()) {
        graphics_cache_.animation_payload = payload;
        graphics_cache_.animation_entry = entry;
        graphics_cache_.owns_graphic_animation = 1;
    }
    for (const building_type_registry_impl::GraphicsLayer &layer : resolved_target.layers()) {
        if (!layer.matches(building)) {
            continue;
        }

        const int selected_layer_option = selected_option_for_layer(building, layer, graphics_variant());
        if (selected_layer_option < 0) {
            continue;
        }
        building_type_registry_impl::GraphicsLayer resolved_layer =
            layer.resolved_option(static_cast<unsigned char>(selected_layer_option));
        const ImageGroupPayload *layer_payload = nullptr;
        const ImageGroupEntry *layer_entry = nullptr;
        if (!resolve_graphic_layer_binding(this, resolved_layer, layer_payload, layer_entry)) {
            return;
        }

        CachedGraphicsBindings::Layer cached_layer;
        cached_layer.payload = layer_payload;
        cached_layer.entry = layer_entry;
        cached_layer.stage = resolved_layer.stage();
        cached_layer.x_offset = resolved_layer.x_offset();
        cached_layer.y_offset = resolved_layer.y_offset();
        cached_layer.role = resolved_layer.has_role() ? resolved_layer.role() : "";
        cached_layer.owns_animation =
            resolved_layer.animation_enabled() && animation_owner_is_working && layer_entry->has_animation();
        graphics_cache_.layers.push_back(cached_layer);
    }
    graphics_cache_.owns_graphics = 1;
}

// Input: the runtime building wrapper for one live building instance.
// Output: the current cached base-entry footprint slice, or null when this building instance stays on the legacy path.
const RuntimeDrawSlice *building_runtime::graphic_footprint()
{
    if (!has_native_graphics_definition()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), this);
    CrashContextScope crash_scope(
        "building_runtime.resolve_base_image",
        context,
        log_building_scope_state,
        this);
    ensure_cached_graphics_bindings();
    return graphics_cache_.base_entry ? graphics_cache_.base_entry->footprint() : nullptr;
}

// Input: the runtime building wrapper for one live building instance.
// Output: the current cached base-entry top slice, or null when no separate top exists.
const RuntimeDrawSlice *building_runtime::graphic_top()
{
    if (!has_native_graphics_definition()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), this);
    CrashContextScope crash_scope(
        "building_runtime.resolve_top_image",
        context,
        log_building_scope_state,
        this);
    ensure_cached_graphics_bindings();
    return graphics_cache_.base_entry ? graphics_cache_.base_entry->top() : nullptr;
}

// Input: the runtime wrapper for one live building instance.
// Output: the current animation frame derived from the cached animation entry plus the caller-provided runtime cursor.
const RuntimeDrawSlice *building_runtime::graphic_animation(int animation_cursor)
{
    if (!has_native_graphics_definition()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), this);
    CrashContextScope crash_scope(
        "building_runtime.resolve_animation_image",
        context,
        log_building_scope_state,
        this);
    ensure_cached_graphics_bindings();
    rebuild_cached_animation_slice(animation_cursor);
    return graphics_cache_.animation_slice.is_valid() ? &graphics_cache_.animation_slice : nullptr;
}

// Input: one live building instance whose runtime state changed or just became visible to the renderer.
// Output: eagerly rebuilds its cached image-group bindings so later draw code only reads cached refs.
void building_runtime::set_building_graphic()
{
    if (!has_native_graphics_definition()) {
        return;
    }

    if (!building_state_supports_native_graphics()) {
        clear_cached_graphics_bindings();
        return;
    }

    rebuild_cached_graphics_bindings();
}

int building_runtime::owns_graphics()
{
    if (!has_native_graphics_definition()) {
        return 0;
    }
    ensure_cached_graphics_bindings();
    return graphics_cache_.owns_graphics;
}

int building_runtime::owns_graphic_animation()
{
    if (!has_native_graphics_definition()) {
        return 0;
    }
    ensure_cached_graphics_bindings();
    return graphics_cache_.owns_graphic_animation;
}
