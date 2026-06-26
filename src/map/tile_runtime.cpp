#include "map/tile_runtime_internal.h"

#include "assets/image_group_payload.h"
#include "building/building_type_registry_internal.h"
#include "map/tile_runtime_graphics.h"
#include "core/crash_context.h"

#include "core/image_group.h"
#include "core/log.h"
#include "map/grid.h"
#include "map/tile_runtime_api.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

namespace {

std::unordered_set<std::string> g_logged_tile_graphics_issues;

void make_tile_context(char *buffer, size_t buffer_size, int grid_offset)
{
    snprintf(buffer, buffer_size, "grid_offset=%d", grid_offset);
}

void log_tile_scope_state(void *userdata)
{
    const tile_runtime *runtime = static_cast<const tile_runtime *>(userdata);
    if (!runtime) {
        return;
    }

    char details[256];
    snprintf(
        details,
        sizeof(details),
        "grid_offset=%d graphics_path=%s image=%s",
        runtime->grid_offset(),
        runtime->graphics_path(),
        runtime->image_id());
    log_info("Graphics tile state", details, 0);
}

void log_tile_graphics_issue_once(const char *message, int grid_offset, const char *path, const char *detail)
{
    char key_buffer[512];
    snprintf(
        key_buffer,
        sizeof(key_buffer),
        "%s|grid_offset=%d|path=%s|detail=%s",
        message ? message : "",
        grid_offset,
        path ? path : "",
        detail ? detail : "");

    if (!g_logged_tile_graphics_issues.insert(key_buffer).second) {
        return;
    }

    error_context_report_error(message, detail);
}

void log_tile_graphics_issue_once(const char *message, const tile_runtime *runtime, const char *detail)
{
    log_tile_graphics_issue_once(
        message,
        runtime ? runtime->grid_offset() : -1,
        runtime ? runtime->graphics_path() : "",
        detail);
}

}

namespace tile_runtime_impl {

std::unordered_map<int, std::unique_ptr<tile_runtime>> g_runtime_tiles;

tile_runtime *get_or_create_direct_instance(
    int grid_offset,
    const building_type_registry_impl::BuildingType *definition,
    const char *path);

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

static const building_type_registry_impl::BuildingType *find_tile_definition(const char *kind_key)
{
    if (!kind_key || !*kind_key) {
        return nullptr;
    }

    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (definition && strcmp(definition->tile().kind_key(), kind_key) == 0) {
            return definition.get();
        }
    }
    return nullptr;
}

static const building_type_registry_impl::GraphicsVariant *graphics_variant_for_role(
    const building_type_registry_impl::BuildingType &definition,
    const char *role)
{
    for (const building_type_registry_impl::GraphicsVariant &variant : definition.graphics().variants()) {
        if (variant.role == role) {
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

static int target_map_image_id_at(const building_type_registry_impl::GraphicsTarget &target, int index)
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

    if (const building_type_registry_impl::GraphicsVariant *variant = graphics_variant_for_role(definition, "tile_large")) {
        path = target_path_for_image_id(variant->target, image_id);
    }
    return path;
}

static const building_type_registry_impl::GraphicsTarget *garden_target(
    const building_type_registry_impl::BuildingType &definition,
    int is_large,
    int is_overgrown)
{
    const char *role = nullptr;
    if (is_large && is_overgrown) {
        role = "overgrown_large";
    } else if (is_large) {
        role = "tile_large";
    } else if (is_overgrown) {
        role = "overgrown";
    }

    if (!role) {
        return &definition.graphics().default_target();
    }

    const building_type_registry_impl::GraphicsVariant *variant = graphics_variant_for_role(definition, role);
    return variant ? &variant->target : nullptr;
}

static building_type_registry_impl::GraphicsTarget resolved_target_at(
    const building_type_registry_impl::GraphicsTarget &target,
    int option_index)
{
    return target.has_options() ? target.resolved_option(static_cast<unsigned char>(option_index)) : target;
}

static const ImageGroupEntry *target_entry_at(
    const building_type_registry_impl::GraphicsTarget &target,
    int option_index)
{
    if (option_index < 0 ||
        (target.has_options() && option_index >= target.option_count()) ||
        (!target.has_options() && option_index != 0)) {
        return nullptr;
    }

    building_type_registry_impl::GraphicsTarget resolved = resolved_target_at(target, option_index);
    if (!resolved.has_path() || !resolved.has_image() || !image_group_payload_load(resolved.path())) {
        return nullptr;
    }

    const ImageGroupPayload *payload = image_group_payload_get(resolved.path());
    return payload ? payload->entry_for(resolved.image()) : nullptr;
}

static int bind_target_option(
    int grid_offset,
    const building_type_registry_impl::BuildingType &definition,
    const building_type_registry_impl::GraphicsTarget &target,
    int option_index)
{
    if (option_index < 0 ||
        (target.has_options() && option_index >= target.option_count()) ||
        (!target.has_options() && option_index != 0)) {
        log_tile_graphics_issue_once("Tile graphics option is invalid.", grid_offset, "", "option_index");
        return 0;
    }

    building_type_registry_impl::GraphicsTarget resolved = resolved_target_at(target, option_index);
    if (!resolved.has_path() || !resolved.has_image()) {
        log_tile_graphics_issue_once("Tile graphics target is incomplete.", grid_offset, "", "path/image");
        return 0;
    }

    if (!image_group_payload_load(resolved.path())) {
        char detail[256];
        snprintf(detail, sizeof(detail), "%s image=%s", resolved.path(), resolved.image());
        log_tile_graphics_issue_once("Tile graphics image could not be resolved.", grid_offset, resolved.path(), detail);
        return 0;
    }

    const ImageGroupPayload *payload = image_group_payload_get(resolved.path());
    const ImageGroupEntry *entry = payload ? payload->entry_for(resolved.image()) : nullptr;
    if (!entry || !entry->footprint()) {
        char detail[256];
        snprintf(detail, sizeof(detail), "%s image=%s", resolved.path(), resolved.image());
        log_tile_graphics_issue_once("Tile graphics image could not be resolved.", grid_offset, resolved.path(), detail);
        return 0;
    }

    if (tile_runtime *instance = get_or_create_direct_instance(grid_offset, &definition, resolved.path())) {
        instance->set_image_id(resolved.image());
        return image_group(GROUP_TERRAIN_FLAT_TILE);
    }
    return 0;
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

tile_runtime *get_or_create_direct_instance(
    int grid_offset,
    const building_type_registry_impl::BuildingType *definition,
    const char *path)
{
    if (!map_grid_is_valid_offset(grid_offset) || !path || !*path) {
        return nullptr;
    }

    std::unique_ptr<tile_runtime> &slot = g_runtime_tiles[grid_offset];
    if (!slot || slot->grid_offset() != grid_offset || slot->definition() != definition ||
        strcmp(slot->graphics_path(), path) != 0) {
        slot = std::make_unique<tile_runtime>(grid_offset, definition, path);
    }
    return slot.get();
}

}

// Input: one runtime tile wrapper that already knows its authored payload image id.
// Output: the native payload entry for that tile, or null when the authored runtime tile graphic cannot be resolved.
const ImageGroupEntry *tile_runtime::resolve_graphic_entry() const
{
    if (graphics_path_.empty() || !image_id_[0]) {
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
            image_id());
        log_tile_graphics_issue_once(
            "Tile graphics image could not be resolved.",
            this,
            detail);
        return nullptr;
    }

    const ImageGroupPayload *payload = image_group_payload_get(graphics_path());
    if (!payload) {
        return nullptr;
    }

    const ImageGroupEntry *entry = payload->entry_for(image_id());
    if (!entry || !entry->footprint()) {
        char detail[256];
        snprintf(
            detail,
            sizeof(detail),
            "%s image=%s",
            graphics_path(),
            image_id());
        log_tile_graphics_issue_once(
            "Tile graphics image could not be resolved.",
            this,
            detail);
        return nullptr;
    }
    return entry;
}

// Input: one runtime tile wrapper that already knows its authored payload image id.
// Output: the native footprint slice for that tile, or null when the authored runtime tile graphic cannot be resolved.
const RuntimeDrawSlice *tile_runtime::resolve_graphic_slice() const
{
    const ImageGroupEntry *entry = resolve_graphic_entry();
    return entry ? entry->footprint() : nullptr;
}

const RuntimeDrawSlice *tile_runtime::resolve_graphic_top_slice() const
{
    const ImageGroupEntry *entry = resolve_graphic_entry();
    return entry ? entry->top() : nullptr;
}

void tile_runtime_reset(void)
{
    tile_runtime_impl::g_runtime_tiles.clear();
    g_logged_tile_graphics_issues.clear();
}

void tile_runtime_clear(int grid_offset)
{
    tile_runtime_impl::g_runtime_tiles.erase(grid_offset);
}

static void tile_runtime_clear_kind(building_type_registry_impl::TileKind kind)
{
    auto it = tile_runtime_impl::g_runtime_tiles.begin();
    while (it != tile_runtime_impl::g_runtime_tiles.end()) {
        const tile_runtime *runtime = it->second.get();
        const building_type_registry_impl::BuildingType *definition =
            runtime ? runtime->definition() : nullptr;
        if (definition && definition->tile().kind() == kind) {
            it = tile_runtime_impl::g_runtime_tiles.erase(it);
        } else {
            ++it;
        }
    }
}

void tile_runtime_clear_gardens(void)
{
    tile_runtime_clear_kind(building_type_registry_impl::TileKind::Garden);
}

void tile_runtime_clear_plazas(void)
{
    tile_runtime_clear_kind(building_type_registry_impl::TileKind::Plaza);
}

int tile_runtime_garden_option_count(int is_large, int is_overgrown)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Garden);
    if (!definition || !definition->has_graphic()) {
        return 0;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        tile_runtime_impl::garden_target(*definition, is_large, is_overgrown);
    return target ? tile_runtime_impl::target_option_count(*target) : 0;
}

int tile_runtime_set_garden_image_id(int grid_offset, int is_large, int is_overgrown, int option_index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Garden);
    if (!definition || !definition->has_graphic()) {
        log_tile_graphics_issue_once("Garden tile graphics definition is missing.", grid_offset, "", "gardens");
        tile_runtime_clear(grid_offset);
        return 0;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        tile_runtime_impl::garden_target(*definition, is_large, is_overgrown);
    if (!target) {
        log_tile_graphics_issue_once(
            "Garden tile graphics target is missing.",
            grid_offset,
            "",
            is_large ? (is_overgrown ? "overgrown_large" : "tile_large") : "overgrown");
        tile_runtime_clear(grid_offset);
        return 0;
    }

    int image_id = tile_runtime_impl::bind_target_option(grid_offset, *definition, *target, option_index);
    if (!image_id) {
        tile_runtime_clear(grid_offset);
    }
    return image_id;
}

void tile_runtime_set_plaza_image_id(int grid_offset, const char *image_id)
{
    if (!image_id || !*image_id) {
        tile_runtime_clear(grid_offset);
        return;
    }

    if (tile_runtime *instance = tile_runtime_impl::get_or_create_instance(
            grid_offset,
            building_type_registry_impl::TileKind::Plaza,
            image_id)) {
        instance->set_image_id(image_id);
    }
}

const char *tile_runtime_plaza_single_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    return definition ? tile_runtime_impl::target_image_id_at(definition->graphics().default_target(), index) : nullptr;
}

const char *tile_runtime_plaza_large_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::graphics_variant_for_role(*definition, "tile_large") : nullptr;
    return variant ? tile_runtime_impl::target_image_id_at(variant->target, index) : nullptr;
}

int tile_runtime_plaza_single_map_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    return definition ? tile_runtime_impl::target_map_image_id_at(definition->graphics().default_target(), index) : 0;
}

int tile_runtime_plaza_single_option_count(void)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    return definition ? tile_runtime_impl::target_option_count(definition->graphics().default_target()) : 0;
}

int tile_runtime_plaza_large_option_count(void)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::graphics_variant_for_role(*definition, "tile_large") : nullptr;
    return variant ? tile_runtime_impl::target_option_count(variant->target) : 0;
}

int tile_runtime_plaza_large_map_image_id(int index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(building_type_registry_impl::TileKind::Plaza);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::graphics_variant_for_role(*definition, "tile_large") : nullptr;
    return variant ? tile_runtime_impl::target_map_image_id_at(variant->target, index) : 0;
}

int tile_runtime_role_option_count(const char *tile_kind, const char *role)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(tile_kind);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::graphics_variant_for_role(*definition, role) : nullptr;
    return variant ? tile_runtime_impl::target_option_count(variant->target) : 0;
}

int tile_runtime_set_role_image_id(int grid_offset, const char *tile_kind, const char *role, int option_index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(tile_kind);
    if (!definition || !definition->has_graphic()) {
        log_tile_graphics_issue_once("Tile graphics definition is missing.", grid_offset, "", tile_kind);
        tile_runtime_clear(grid_offset);
        return 0;
    }

    const building_type_registry_impl::GraphicsVariant *variant =
        tile_runtime_impl::graphics_variant_for_role(*definition, role);
    if (!variant) {
        log_tile_graphics_issue_once("Tile graphics role is missing.", grid_offset, definition->attr(), role);
        tile_runtime_clear(grid_offset);
        return 0;
    }

    int image_id = tile_runtime_impl::bind_target_option(grid_offset, *definition, variant->target, option_index);
    if (!image_id) {
        tile_runtime_clear(grid_offset);
    }
    return image_id;
}

const RuntimeDrawSlice *tile_runtime_get_graphic_footprint_slice(int grid_offset)
{
    if (tile_runtime *instance = tile_runtime_impl::get_instance(grid_offset)) {
        return instance->resolve_graphic_slice();
    }
    return nullptr;
}

const RuntimeDrawSlice *tile_runtime_get_graphic_top_slice(int grid_offset)
{
    if (tile_runtime *instance = tile_runtime_impl::get_instance(grid_offset)) {
        return instance->resolve_graphic_top_slice();
    }
    return nullptr;
}

const RuntimeDrawSlice *tile_runtime_get_role_footprint_slice(const char *tile_kind, const char *role, int option_index)
{
    const building_type_registry_impl::BuildingType *definition =
        tile_runtime_impl::find_tile_definition(tile_kind);
    const building_type_registry_impl::GraphicsVariant *variant =
        definition ? tile_runtime_impl::graphics_variant_for_role(*definition, role) : nullptr;
    const ImageGroupEntry *entry = variant ? tile_runtime_impl::target_entry_at(variant->target, option_index) : nullptr;
    return entry ? entry->footprint() : nullptr;
}

int tile_runtime_has_graphic(int grid_offset)
{
    if (tile_runtime *instance = tile_runtime_impl::get_instance(grid_offset)) {
        return instance->resolve_graphic_entry() != nullptr;
    }
    return 0;
}
