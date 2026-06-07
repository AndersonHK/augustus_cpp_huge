#include "map/tile_runtime_internal.h"

#include "assets/image_group_payload.h"
#include "building/building_type_registry_internal.h"
#include "map/tile_runtime_graphics.h"
#include "core/crash_context.h"

extern "C" {
#include "assets/assets.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/log.h"
#include "map/grid.h"
#include "map/tile_runtime_api.h"
}

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

namespace {

std::unordered_set<std::string> g_logged_tile_graphics_fallbacks;

void make_tile_context(char *buffer, size_t buffer_size, int grid_offset)
{
    snprintf(buffer, buffer_size, "grid_offset=%d", grid_offset);
}

void log_tile_scope_state(void *userdata)
{
    const tile_runtime *runtime = static_cast<const tile_runtime *>(userdata);
    if (!runtime || !runtime->definition()) {
        return;
    }

    char details[256];
    snprintf(
        details,
        sizeof(details),
        "grid_offset=%d graphics_path=%s plaza_image=%s",
        runtime->grid_offset(),
        runtime->graphics_path(),
        runtime->plaza_image_id());
    log_info("Graphics tile state", details, 0);
}

void log_tile_graphics_fallback_once(const char *message, const tile_runtime *runtime, const char *detail)
{
    char key_buffer[512];
    snprintf(
        key_buffer,
        sizeof(key_buffer),
        "%s|grid_offset=%d|path=%s|detail=%s",
        message ? message : "",
        runtime ? runtime->grid_offset() : -1,
        runtime ? runtime->graphics_path() : "",
        detail ? detail : "");

    if (!g_logged_tile_graphics_fallbacks.insert(key_buffer).second) {
        return;
    }

    error_context_report_error(message, detail);
}

}

namespace tile_runtime_impl {

std::unordered_map<int, std::unique_ptr<tile_runtime>> g_runtime_tiles;

static const building_type_registry_impl::BuildingType *find_tile_definition(building_type_registry_impl::TileKind kind)
{
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (definition && definition->tile().kind() == kind) {
            return definition.get();
        }
    }
    return nullptr;
}

static const building_type_registry_impl::GraphicsVariant *plaza_large_graphics_variant(
    const building_type_registry_impl::BuildingType &definition)
{
    for (const building_type_registry_impl::GraphicsVariant &variant : definition.graphics().variants()) {
        if (variant.role == "tile_large") {
            return &variant;
        }
    }
    return nullptr;
}

static const char *target_image_id_at(const building_type_registry_impl::GraphicsTarget &target, int index)
{
    if (index < 0) {
        return nullptr;
    }
    if (target.has_options()) {
        const building_type_registry_impl::GraphicsTarget *option = target.option(index);
        return option && option->has_image() ? option->image() : nullptr;
    }
    return index == 0 && target.has_image() ? target.image() : nullptr;
}

static int target_asset_image_id_at(const building_type_registry_impl::GraphicsTarget &target, int index)
{
    if (index < 0) {
        return 0;
    }
    if (!target.has_options() && index != 0) {
        return 0;
    }

    building_type_registry_impl::GraphicsTarget resolved =
        target.has_options() ? target.resolved_option(static_cast<unsigned char>(index)) : target;
    if (!resolved.has_path()) {
        return 0;
    }
    int image_id = assets_get_image_id_from_path_or_name(
        resolved.path(),
        resolved.has_image() ? resolved.image() : nullptr);
    if (image_id) {
        return image_id;
    }

    if (resolved.has_image() && image_group_payload_load(resolved.path())) {
        const ImageGroupPayload *payload = image_group_payload_get(resolved.path());
        if (payload && payload->entry_for(resolved.image())) {
            return image_group(GROUP_TERRAIN_FLAT_TILE);
        }
    }
    return 0;
}

static int target_option_count(const building_type_registry_impl::GraphicsTarget &target)
{
    if (target.has_options()) {
        return target.option_count();
    }
    return target.has_path() ? 1 : 0;
}

static std::string target_path_for_image_id(const building_type_registry_impl::GraphicsTarget &target, const char *image_id)
{
    if (!image_id || !*image_id) {
        return {};
    }

    const int option_count = target.has_options() ? target.option_count() : 1;
    for (int i = 0; i < option_count; i++) {
        building_type_registry_impl::GraphicsTarget resolved = target.resolved_option(static_cast<unsigned char>(i));
        if (resolved.has_image() && strcmp(resolved.image(), image_id) == 0 && resolved.has_path()) {
            return resolved.path();
        }
    }
    return {};
}

static std::string plaza_path_for_image_id(
    const building_type_registry_impl::BuildingType &definition,
    const char *image_id)
{
    std::string path = target_path_for_image_id(definition.graphics().default_target(), image_id);
    if (!path.empty()) {
        return path;
    }

    if (const building_type_registry_impl::GraphicsVariant *variant = plaza_large_graphics_variant(definition)) {
        path = target_path_for_image_id(variant->target, image_id);
    }
    return path;
}

tile_runtime *get_instance(int grid_offset)
{
    auto it = g_runtime_tiles.find(grid_offset);
    return it == g_runtime_tiles.end() ? nullptr : it->second.get();
}

tile_runtime *get_or_create_instance(int grid_offset, building_type_registry_impl::TileKind kind, const char *image_id)
{
    if (!map_grid_is_valid_offset(grid_offset) || kind == building_type_registry_impl::TileKind::None) {
        return nullptr;
    }

    const building_type_registry_impl::BuildingType *definition = find_tile_definition(kind);
    if (!definition || !definition->has_graphic()) {
        return nullptr;
    }

    std::string graphics_path = plaza_path_for_image_id(*definition, image_id);
    if (graphics_path.empty()) {
        return nullptr;
    }

    std::unique_ptr<tile_runtime> &slot = g_runtime_tiles[grid_offset];
    if (!slot || slot->grid_offset() != grid_offset || slot->definition() != definition ||
        strcmp(slot->graphics_path(), graphics_path.c_str()) != 0) {
        slot = std::make_unique<tile_runtime>(grid_offset, definition, std::move(graphics_path));
    }
    return slot.get();
}

}

// Input: one runtime tile wrapper that already knows its authored plaza image id.
// Output: the native footprint slice for that tile, or null when the authored runtime tile graphic cannot be resolved.
const RuntimeDrawSlice *tile_runtime::resolve_graphic_slice() const
{
    if (!definition_ || !definition_->has_graphic() || graphics_path_.empty() || !plaza_image_id_[0]) {
        return nullptr;
    }

    char context[128];
    make_tile_context(context, sizeof(context), grid_offset_);
    CrashContextScope crash_scope(
        "tile_runtime.resolve_graphic_image",
        context,
        log_tile_scope_state,
        const_cast<tile_runtime *>(this));
    if (!image_group_payload_load(graphics_path())) {
        char detail[256];
        snprintf(
            detail,
            sizeof(detail),
            "%s image=%s",
            graphics_path(),
            plaza_image_id());
        log_tile_graphics_fallback_once(
            "Tile graphics image could not be resolved. Falling back to legacy rendering.",
            this,
            detail);
        return nullptr;
    }

    const ImageGroupPayload *payload = image_group_payload_get(graphics_path());
    if (!payload) {
        return nullptr;
    }

    const ImageGroupEntry *entry = payload->entry_for(plaza_image_id());
    if (!entry || !entry->footprint()) {
        char detail[256];
        snprintf(
            detail,
            sizeof(detail),
            "%s image=%s",
            graphics_path(),
            plaza_image_id());
        log_tile_graphics_fallback_once(
            "Tile graphics image could not be resolved. Falling back to legacy rendering.",
            this,
            detail);
        return nullptr;
    }
    return entry->footprint();
}

extern "C" void tile_runtime_reset(void)
{
    tile_runtime_impl::g_runtime_tiles.clear();
    g_logged_tile_graphics_fallbacks.clear();
}

extern "C" void tile_runtime_clear(int grid_offset)
{
    tile_runtime_impl::g_runtime_tiles.erase(grid_offset);
}

extern "C" void tile_runtime_set_plaza_image_id(int grid_offset, const char *image_id)
{
    if (!image_id || !*image_id) {
        tile_runtime_clear(grid_offset);
        return;
    }

    if (tile_runtime *instance = tile_runtime_impl::get_or_create_instance(
            grid_offset,
            building_type_registry_impl::TileKind::Plaza,
            image_id)) {
        instance->set_plaza_image_id(image_id);
    }
}

extern "C" const char *tile_runtime_plaza_single_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    return definition ? tile_runtime_impl::target_image_id_at(definition->graphics().default_target(), index) : nullptr;
}

extern "C" const char *tile_runtime_plaza_large_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::plaza_large_graphics_variant(*definition) : nullptr;
    return variant ? tile_runtime_impl::target_image_id_at(variant->target, index) : nullptr;
}

extern "C" int tile_runtime_plaza_single_asset_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    return definition ? tile_runtime_impl::target_asset_image_id_at(definition->graphics().default_target(), index) : 0;
}

extern "C" int tile_runtime_plaza_single_option_count(void)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    return definition ? tile_runtime_impl::target_option_count(definition->graphics().default_target()) : 0;
}

extern "C" int tile_runtime_plaza_large_option_count(void)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::plaza_large_graphics_variant(*definition) : nullptr;
    return variant ? tile_runtime_impl::target_option_count(variant->target) : 0;
}

extern "C" int tile_runtime_plaza_large_asset_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::plaza_large_graphics_variant(*definition) : nullptr;
    return variant ? tile_runtime_impl::target_asset_image_id_at(variant->target, index) : 0;
}

const RuntimeDrawSlice *tile_runtime_get_graphic_footprint_slice(int grid_offset)
{
    if (tile_runtime *instance = tile_runtime_impl::get_instance(grid_offset)) {
        return instance->resolve_graphic_slice();
    }
    return nullptr;
}
