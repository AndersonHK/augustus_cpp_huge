#include "building/image.h"
#include "building/granary.h"
#include "building/industry.h"
#include "map/building_tiles.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/connectable.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/production_runtime.h"

#include "assets/image_group_payload.h"
#include "building/animations.h"
#include "building/building_runtime_graphics.h"
#include "building/variant.h"
#include "core/calc.h"
#include "core/crash_context.h"

#include "core/image_group.h"
#include "city/view.h"
#include "game/resource.h"
#include "map/terrain.h"
#include "core/log.h"

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

void make_building_context(char *buffer, size_t buffer_size, const building_runtime *runtime)
{
    const Building b = runtime ? runtime->building() : Building(nullptr);
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
    const Building b = runtime ? runtime->building() : Building(nullptr);
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (!b.type) {
        return;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        definition && runtime ?
            building_type_registry_impl::BuildingType::resolve_graphics_target_for_image(
                definition,
                runtime->building()) :
            nullptr;

    char details[256];
    snprintf(
        details,
        sizeof(details),
        "state=%d size=%d desirability=%d water=%d upgrade=%d target_path=%s target_image=%s",
        b.state_id(),
        b.size(),
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

void report_rebuild_failure(
    const building_runtime *runtime,
    const char *reason,
    const building_type_registry_impl::GraphicsTarget *target = nullptr,
    const ImageGroupEntry *entry = nullptr)
{
    char detail[512];
    format_rebuild_failure_detail(detail, sizeof(detail), runtime, reason, target, entry);
    error_context_report_error("Native building graphics cache rebuild failed.", detail);
}

void log_image_id_failure(
    const Building &building,
    const char *reason,
    const building_type_registry_impl::GraphicsTarget *target = nullptr,
    const ImageGroupEntry *entry = nullptr)
{
    char detail[512];
    snprintf(
        detail,
        sizeof(detail),
        "building_attr=%s grid_offset=%d path=%s image=%s reason=%s",
        building.type ? building.type->attr() : "unknown",
        building.grid_offset(),
        target && target->has_path() ? target->path() : "",
        graphics_target_image_or_entry_id(target, entry),
        reason ? reason : "");
    log_info("Native building graphics image lookup failed: ", detail, 0);
}

void report_layer_rebuild_failure(
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
    error_context_report_error("Native building graphics layer cache rebuild failed.", detail);
}

int runtime_tile_sentinel_image_id()
{
    return image_group(GROUP_TERRAIN_FLAT_TILE);
}

int selected_option_for_selection(
    const Building &building,
    building_type_registry_impl::GraphicsOptionSelection selection,
    int option_count)
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
            building.variant() % option_count;
        return option < 0 ? option + option_count : option;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::Orientation) {
        return (4 + building.orientation() - city_view_orientation() / 2) % 4;
    }
    if (selection == building_type_registry_impl::GraphicsOptionSelection::ProductionProgress) {
        const ::building *record = building.id() ? building_get(building.id()) : building.record();
        Production *production = building.id() ? production_runtime_impl::get_or_create_primary(building) : nullptr;
        const int max_value = production ? production->max_progress() :
            (building.type && !building.type->production_methods().empty() ?
                building.type->production_methods().front()->max_progress_for(building.main()) :
                0);
        if (!record || max_value <= 0) {
            return 0;
        }
        int percentage = calc_percentage(record->data.industry.progress, max_value);
        percentage = calc_bound(percentage, 0, 100);
        int option = percentage * option_count / 101;
        return calc_bound(option, 0, option_count - 1);
    }
    int option = building_variant_get_graphics_option(building, 0);
    return option < 0 ? 0 : option;
}

int selected_option_for_layer(
    const Building &building,
    const building_type_registry_impl::GraphicsLayer &layer)
{
    return selected_option_for_selection(building, layer.option_selection(), layer.option_count());
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
    const building_type_registry_impl::GraphicsTarget &target)
{
    return selected_option_for_selection(building, target.option_selection(), target.option_count());
}

int building_runtime_graphics_image_id(const Building &building_object)
{
    const building_type_registry_impl::BuildingType *definition = building_object.type;
    if (!definition || !definition->has_graphic() || definition->has_data_only_graphics()) {
        return 0;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        building_type_registry_impl::BuildingType::resolve_graphics_target_for_image(definition, building_object);
    if (!target) {
        log_image_id_failure(building_object, "target_selection_failed");
        return 0;
    }

    building_type_registry_impl::GraphicsTarget resolved_target =
        target->resolved_option(static_cast<unsigned char>(building_runtime_graphics_selected_option(building_object, *target)));
    if (resolved_target.is_resource_storage()) {
        return runtime_tile_sentinel_image_id();
    }
    if (!resolved_target.has_path()) {
        log_image_id_failure(building_object, "target_path_missing", &resolved_target);
        return 0;
    }

    if (!image_group_payload_load(resolved_target.path())) {
        log_image_id_failure(building_object, "payload_group_load_failed", &resolved_target);
        return 0;
    }
    const ImageGroupPayload *payload = image_group_payload_get(resolved_target.path());
    if (!payload) {
        log_image_id_failure(building_object, "payload_group_handle_null", &resolved_target);
        return 0;
    }
    const ImageGroupEntry *entry =
        resolved_target.has_image() ? payload->entry_for(resolved_target.image()) : payload->default_entry();
    if (!entry) {
        log_image_id_failure(building_object,
            resolved_target.has_image() ? "payload_entry_lookup_failed" : "payload_default_entry_lookup_failed",
            &resolved_target);
        return 0;
    }
    if (!entry->footprint()) {
        log_image_id_failure(building_object, "payload_entry_footprint_missing", &resolved_target, entry);
        return 0;
    }
    for (const building_type_registry_impl::GraphicsLayer &layer : resolved_target.layers()) {
        if (!layer.matches(building_object)) {
            continue;
        }
        building_type_registry_impl::GraphicsLayer resolved_layer =
            layer.resolved_option(static_cast<unsigned char>(selected_option_for_layer(building_object, layer)));
        if (!resolved_layer.has_path()) {
            log_image_id_failure(building_object, "layer_path_missing", &resolved_target);
            return 0;
        }
        if (!image_group_payload_load(resolved_layer.path())) {
            log_image_id_failure(building_object, "layer_payload_group_load_failed", &resolved_target);
            return 0;
        }
        const ImageGroupPayload *layer_payload = image_group_payload_get(resolved_layer.path());
        const ImageGroupEntry *layer_entry =
            layer_payload && resolved_layer.has_image() ? layer_payload->entry_for(resolved_layer.image()) : nullptr;
        if (!layer_entry) {
            log_image_id_failure(building_object, "layer_payload_entry_lookup_failed", &resolved_target);
            return 0;
        }
    }
    return runtime_tile_sentinel_image_id();
}

void building_runtime::clear_cached_graphics_bindings()
{
    graphics_cache_ = CachedGraphicsBindings();
}

int building_runtime::uses_new_graphics() const
{
    const Building b = building();
    return b.type &&
        b.type->has_graphic() &&
        !b.type->has_data_only_graphics();
}

int building_runtime::building_state_supports_native_graphics() const
{
    const Building b = building();
    return b.state_id() == BUILDING_STATE_CREATED ||
        b.state_id() == BUILDING_STATE_IN_USE ||
        b.state_id() == BUILDING_STATE_MOTHBALLED;
}

void building_runtime::invalidate_graphics_cache()
{
    graphics_cache_.dirty = 1;
}

std::uint64_t building_runtime::graphics_state_signature() const
{
    const Building b = building();
    if (!b.type) {
        return 0;
    }

    int selected_option = -1;
    if (const building_type_registry_impl::GraphicsTarget *target = resolve_graphic_target()) {
        if (target->has_options()) {
            selected_option = building_runtime_graphics_selected_option(b, *target);
        }
    }
    std::uint64_t signature = b.graphics_state_signature(selected_option);
    const Building owner = b.composition_owner();
    if (owner.id() && owner.id() != b.id()) {
        signature ^= owner.graphics_state_signature(-1);
        signature *= 1099511628211ull;
    }
    return signature;
}

const building_type_registry_impl::GraphicsTarget *building_runtime::resolve_graphic_target() const
{
    if (!uses_new_graphics()) {
        return nullptr;
    }
    return building_type_registry_impl::BuildingType::resolve_graphics_target_for_image(&type(), building());
}

void building_runtime::assign_graphic_variant(int force_reseed)
{
    Building b = building();
    if (!b.type) {
        return;
    }

    refresh_runtime_state();
    int graphics_option = building_variant_get_graphics_option(b, force_reseed);
    if (graphics_option < 0) {
        return;
    }

    unsigned char variant = static_cast<unsigned char>(graphics_option);
    if (b.variant() == variant) {
        return;
    }
    b.set_variant(variant);
    invalidate_graphics_cache();
}

int building_runtime::resolve_graphic_binding(
    const building_type_registry_impl::GraphicsTarget &target,
    const ImageGroupPayload *&payload,
    const ImageGroupEntry *&entry) const
{
    payload = nullptr;
    entry = nullptr;

    if (!uses_new_graphics() || !target.has_path()) {
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

// Input: one live building instance in the city animation draw stage.
// Output: advances the native XML animation cursor at the same layer where legacy image groups tick.
void building_runtime::advance_graphic_animation(int animation_cursor)
{
    if (!uses_new_graphics()) {
        return;
    }

    ensure_cached_graphics_bindings();
    const RuntimeAnimationTrack *track_ptr = cached_animation_track();
    if (!track_ptr || !graphics_cache_.owns_graphic_animation) {
        return;
    }

    building().animate().runtime_track_offset(*track_ptr, 1, animation_cursor);
    // The frame cursor has just moved; the draw slice is rebuilt lazily by
    // graphic_animation() so ghosts and normal city draws share one read path.
    graphics_cache_.animation_slice = RuntimeDrawSlice();
}

// Input: one live building instance whose cached bindings may include an animation image-group entry.
// Output: the runtime animation track exposed by that cached animation entry, or null when this building instance has no native animation.
const RuntimeAnimationTrack *building_runtime::cached_animation_track() const
{
    return graphics_cache_.animation_entry && graphics_cache_.animation_entry->has_animation() ?
        &graphics_cache_.animation_entry->animation() :
        nullptr;
}

int building_runtime::resolve_graphics_cache()
{
    if (!uses_new_graphics()) {
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
    const RuntimeAnimationTrack *track_ptr = cached_animation_track();
    if (!track_ptr || !graphics_cache_.owns_graphic_animation) {
        return;
    }

    building().animate().runtime_track_offset(*track_ptr, 1, animation_cursor);
    graphics_cache_.animation_slice = RuntimeDrawSlice();
}

const RuntimeDrawSlice *building_runtime::cached_graphic_animation(int animation_cursor)
{
    rebuild_cached_animation_slice(animation_cursor);
    return graphics_cache_.animation_slice.is_valid() ? &graphics_cache_.animation_slice : nullptr;
}

void building_runtime::draw_cached_graphic_layers(
    building_type_registry_impl::GraphicsLayerStage stage,
    int x,
    int y,
    color_t color,
    float scale)
{
    for (const CachedGraphicsBindings::Layer &layer : graphics_cache_.layers) {
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
        if (!layer.owns_animation || !layer.entry || !layer.entry->has_animation()) {
            continue;
        }
        const RuntimeAnimationTrack &track = layer.entry->animation();
        const int animation_offset = building().animate().runtime_track_offset(track, 1, animation_cursor);
        if (animation_offset <= 0) {
            layer.animation_slice = RuntimeDrawSlice();
            continue;
        }
        const size_t frame_index = static_cast<size_t>(animation_offset - 1);
        if (frame_index >= track.frames.size()) {
            layer.animation_slice = RuntimeDrawSlice();
            continue;
        }

        RuntimeDrawSlice frame_slice = track.frames[frame_index];
        frame_slice.draw_offset_x += track.sprite_offset_x;
        frame_slice.draw_offset_y += track.sprite_offset_y;
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
    const RuntimeAnimationTrack *track_ptr = cached_animation_track();
    if (!track_ptr || !graphics_cache_.owns_graphic_animation) {
        return;
    }

    const RuntimeAnimationTrack &track = *track_ptr;
    const int animation_offset = building().animate().runtime_track_offset(track, 0, animation_cursor);
    if (animation_offset <= 0) {
        return;
    }

    const size_t frame_index = static_cast<size_t>(animation_offset - 1);
    if (frame_index >= track.frames.size()) {
        return;
    }

    RuntimeDrawSlice frame_slice = track.frames[frame_index];
    frame_slice.draw_offset_x += track.sprite_offset_x;
    frame_slice.draw_offset_y += track.sprite_offset_y;
    if (graphics_cache_.base_entry && graphics_cache_.base_entry->top()) {
        frame_slice.draw_offset_y += graphics_cache_.base_entry->top()->draw_offset_y;
    }
    graphics_cache_.animation_slice = frame_slice;
}

// Input: one live building instance that may have missed an explicit graphics invalidation.
// Output: makes sure the cached image-group bindings are current before any renderer-facing accessor returns them.
void building_runtime::ensure_cached_graphics_bindings()
{
    if (!uses_new_graphics()) {
        clear_cached_graphics_bindings();
        return;
    }

    refresh_runtime_state();
    const std::uint64_t signature = graphics_state_signature();
    if (graphics_cache_.resolved && !graphics_cache_.dirty && graphics_cache_.signature == signature) {
        return;
    }

    rebuild_cached_graphics_bindings();
}

// Input: one live building instance plus its shared BuildingType definition.
// Output: the authoritative cached base/animation image-group bindings for that building instance, or a disabled native cache on soft failure.
void building_runtime::rebuild_cached_graphics_bindings()
{
    clear_cached_graphics_bindings();
    if (!uses_new_graphics()) {
        return;
    }

    refresh_runtime_state();
    graphics_cache_.resolved = 1;
    graphics_cache_.dirty = 0;
    graphics_cache_.signature = graphics_state_signature();
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
        target->resolved_option(static_cast<unsigned char>(building_runtime_graphics_selected_option(building(), *target)));

    const ImageGroupPayload *payload = nullptr;
    const ImageGroupEntry *entry = nullptr;
    if (!resolve_graphic_binding(resolved_target, payload, entry)) {
        return;
    }

    if (!entry->footprint()) {
        report_rebuild_failure(this, "public_footprint_invalid_after_materialization", &resolved_target, entry);
        return;
    }

    const Building animation_owner = building().composition_owner();
    const int animation_owner_is_working = animation_owner.id() && animation_owner.is_working();

    graphics_cache_.base_payload = payload;
    graphics_cache_.base_entry = entry;
    if (resolved_target.animation_enabled() && animation_owner_is_working && entry->has_animation()) {
        graphics_cache_.animation_payload = payload;
        graphics_cache_.animation_entry = entry;
        graphics_cache_.owns_graphic_animation = 1;
    }
    for (const building_type_registry_impl::GraphicsLayer &layer : resolved_target.layers()) {
        if (!layer.matches(building())) {
            continue;
        }

        building_type_registry_impl::GraphicsLayer resolved_layer =
            layer.resolved_option(static_cast<unsigned char>(selected_option_for_layer(building(), layer)));
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
    if (!uses_new_graphics()) {
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
    if (!uses_new_graphics()) {
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
    if (!uses_new_graphics()) {
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
    if (!uses_new_graphics()) {
        return;
    }

    if (!building_state_supports_native_graphics()) {
        clear_cached_graphics_bindings();
        return;
    }

    rebuild_cached_graphics_bindings();
    // XML payload graphics render from cached RuntimeDrawSlice entries; map_image only keeps legacy tile bookkeeping alive.
    Building b = building();
    int image_id = graphics_cache_.owns_graphics ? runtime_tile_sentinel_image_id() : b.image_id();
    if (!image_id) {
        image_id = runtime_tile_sentinel_image_id();
    }
    b.add_map_tiles(image_id);
}

int building_runtime::owns_graphics()
{
    if (!uses_new_graphics()) {
        return 0;
    }
    ensure_cached_graphics_bindings();
    return graphics_cache_.owns_graphics;
}

int building_runtime::owns_graphic_animation()
{
    if (!uses_new_graphics()) {
        return 0;
    }
    ensure_cached_graphics_bindings();
    return graphics_cache_.owns_graphic_animation;
}
