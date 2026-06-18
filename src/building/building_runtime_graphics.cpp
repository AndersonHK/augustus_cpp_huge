#include "building/image.h"
#include "building/industry.h"
#include "map/building_tiles.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/connectable.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"

#include "assets/image_group_payload.h"
#include "building/animations.h"
#include "building/building_runtime_graphics.h"
#include "building/variant.h"
#include "core/crash_context.h"
#include "graphics/image_group_reference.h"

extern "C" {
#include "core/image_group.h"
#include "game/resource.h"
#include "map/terrain.h"
#include "core/log.h"
}

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

void make_building_context(char *buffer, size_t buffer_size, const building_runtime *runtime)
{
    const building *b = runtime ? runtime->legacy_record() : nullptr;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (b) {
        snprintf(
            buffer,
            buffer_size,
            "building_id=%u type=%s grid_offset=%d",
            b->id,
            definition ? definition->attr() : "unknown",
            b->grid_offset);
    } else {
        snprintf(buffer, buffer_size, "building=null");
    }
}

void log_building_scope_state(void *userdata)
{
    const building_runtime *runtime = static_cast<const building_runtime *>(userdata);
    const building *b = runtime ? runtime->legacy_record() : nullptr;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (!b) {
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
        b->state,
        b->size,
        b->desirability,
        b->has_water_access,
        b->upgrade_level,
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
    error_context_report_error("Native building graphics cache rebuild failed. Falling back to legacy rendering.", detail);
}

int runtime_tile_sentinel_image_id()
{
    return image_group(GROUP_TERRAIN_FLAT_TILE);
}

int selected_graphics_option(const Building &building, const building_type_registry_impl::GraphicsTarget &target)
{
    if (!target.has_options()) {
        return 0;
    }
    if (target.option_selection() == building_type_registry_impl::GraphicsOptionSelection::Connectable) {
        int option = building_connectable_graphics_option(building);
        return option < 0 ? 0 : option;
    }
    int option = building_variant_get_graphics_option(building, 0);
    return option < 0 ? 0 : option;
}

}

int building_runtime_graphics_image_id(const Building &building_object)
{
    const ::building *record = building_object.legacy_record();
    const building_type_registry_impl::BuildingType *definition = building_object.type_definition();
    if (!record || !definition || !definition->has_graphic()) {
        return 0;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        building_type_registry_impl::BuildingType::resolve_graphics_target_for_image(definition, building_object);
    if (!target) {
        return 0;
    }

    building_type_registry_impl::GraphicsTarget resolved_target =
        target->resolved_option(static_cast<unsigned char>(selected_graphics_option(building_object, *target)));
    if (!resolved_target.has_path()) {
        return 0;
    }

    return graphics_image_id_for_group_reference(
        resolved_target.path(),
        resolved_target.has_image() ? resolved_target.image() : nullptr);
}

void building_runtime::clear_cached_graphics_bindings()
{
    graphics_cache_ = CachedGraphicsBindings();
}

int building_runtime::uses_new_graphics() const
{
    return legacy_record() &&
        definition() &&
        type().has_graphic() &&
        !type().has_data_only_graphics();
}

int building_runtime::building_state_supports_native_graphics() const
{
    return legacy_record() &&
        (record().state == BUILDING_STATE_CREATED ||
            record().state == BUILDING_STATE_IN_USE ||
            record().state == BUILDING_STATE_MOTHBALLED);
}

void building_runtime::invalidate_graphics_cache()
{
    graphics_cache_.dirty = 1;
}

std::uint64_t building_runtime::graphics_state_signature() const
{
    if (!legacy_record()) {
        return 0;
    }

    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };

    mix(static_cast<std::uint64_t>(record().state));
    mix(static_cast<std::uint64_t>(record().num_workers));
    mix(static_cast<std::uint64_t>(record().has_water_access));
    mix(static_cast<std::uint64_t>(record().desirability));
    mix(static_cast<std::uint64_t>(record().strike_duration_days));
    mix(static_cast<std::uint64_t>(record().data.industry.progress));
    mix(static_cast<std::uint64_t>(record().data.industry.has_raw_materials));
    mix(static_cast<std::uint64_t>(record().output_resource_id));
    mix(static_cast<std::uint64_t>(record().figure_id));
    mix(static_cast<std::uint64_t>(record().figure_id2));
    mix(static_cast<std::uint64_t>(record().figure_id4));
    mix(static_cast<std::uint64_t>(record().monument.phase));
    mix(static_cast<std::uint64_t>(record().monument.upgrades));
    mix(static_cast<std::uint64_t>(record().variant));
    if (const building_type_registry_impl::GraphicsTarget *target = resolve_graphic_target()) {
        if (target->has_options()) {
            mix(static_cast<std::uint64_t>(selected_graphics_option(building(), *target)));
        }
    }

    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        mix(static_cast<std::uint64_t>(record().resources[i]));
    }

    return hash;
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
    if (!legacy_record()) {
        return;
    }

    refresh_runtime_state();
    int graphics_option = building_variant_get_graphics_option(building(), force_reseed);
    if (graphics_option < 0) {
        return;
    }

    unsigned char variant = static_cast<unsigned char>(graphics_option);
    if (record().variant == variant) {
        return;
    }
    record().variant = variant;
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
        target->resolved_option(static_cast<unsigned char>(selected_graphics_option(building(), *target)));

    const ImageGroupPayload *payload = nullptr;
    const ImageGroupEntry *entry = nullptr;
    if (!resolve_graphic_binding(resolved_target, payload, entry)) {
        return;
    }

    if (!entry->footprint()) {
        report_rebuild_failure(this, "public_footprint_invalid_after_materialization", &resolved_target, entry);
        return;
    }

    graphics_cache_.base_payload = payload;
    graphics_cache_.base_entry = entry;
    if (resolved_target.animation_enabled() && building().is_working() && entry->has_animation()) {
        graphics_cache_.animation_payload = payload;
        graphics_cache_.animation_entry = entry;
        graphics_cache_.owns_graphic_animation = 1;
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
    int image_id = graphics_cache_.owns_graphics ? runtime_tile_sentinel_image_id() : building_image_get(legacy_record());
    if (!image_id) {
        image_id = runtime_tile_sentinel_image_id();
    }
    map_building_tiles_add(record().id, record().x, record().y, record().size, image_id, TERRAIN_BUILDING);
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
