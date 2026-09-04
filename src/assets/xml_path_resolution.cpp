#include "assets/xml_path_resolution.h"

#include "core/file.h"
#include "core/log.h"
#include "game/mod_manager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace {

bool regular_file_exists(const char *path)
{
    std::error_code error;
    return path && *path && std::filesystem::is_regular_file(std::filesystem::path(path), error);
}

int append_mod_graphics_path(char *full_path, const char *relative_path)
{
    return snprintf(full_path, FILE_NAME_MAX, "%s%s", mod_manager::graphics_path().c_str(), relative_path) < FILE_NAME_MAX;
}

int append_augustus_graphics_path(char *full_path, const char *relative_path)
{
    return snprintf(full_path, FILE_NAME_MAX, "%s%s", mod_manager::augustus_graphics_path().c_str(), relative_path) < FILE_NAME_MAX;
}

int append_julius_graphics_path(char *full_path, const char *relative_path)
{
    return snprintf(full_path, FILE_NAME_MAX, "%s%s", mod_manager::julius_graphics_path().c_str(), relative_path) < FILE_NAME_MAX;
}

int append_graphics_path_for_source(char *full_path, const char *relative_path, xml_asset_source source)
{
    switch (source) {
        case XML_ASSET_SOURCE_MOD:
            return append_mod_graphics_path(full_path, relative_path);
        case XML_ASSET_SOURCE_AUGUSTUS:
            return append_augustus_graphics_path(full_path, relative_path);
        case XML_ASSET_SOURCE_JULIUS:
            return append_julius_graphics_path(full_path, relative_path);
        case XML_ASSET_SOURCE_AUTO:
        default:
            return append_mod_graphics_path(full_path, relative_path);
    }
}

int ascii_equals_ignore_case(const char *left, const char *right)
{
    if (!left || !right) {
        return 0;
    }
    while (*left && *right) {
        char l = *left++;
        char r = *right++;
        if (l >= 'A' && l <= 'Z') {
            l = static_cast<char>(l - 'A' + 'a');
        }
        if (r >= 'A' && r <= 'Z') {
            r = static_cast<char>(r - 'A' + 'a');
        }
        if (l != r) {
            return 0;
        }
    }
    return *left == *right;
}

int append_layer_graphics_path(char *full_path, const GraphicsLayerSource &source, const char *relative_path)
{
    const char separator = source.graphics_root.empty() ? '\0' : source.graphics_root.back();
    const char *join = separator == '/' || separator == '\\' ? "" : "/";
    return snprintf(full_path, FILE_NAME_MAX, "%s%s%s", source.graphics_root.c_str(), join, relative_path) < FILE_NAME_MAX;
}

xml_asset_source legacy_source_for_layer(const GraphicsLayerSource &source, std::size_t top_index)
{
    if (source.layer_index == top_index) {
        return XML_ASSET_SOURCE_MOD;
    }
    if (ascii_equals_ignore_case(source.mod_name.c_str(), "Augustus")) {
        return XML_ASSET_SOURCE_AUGUSTUS;
    }
    if (ascii_equals_ignore_case(source.mod_name.c_str(), "Julius")) {
        return XML_ASSET_SOURCE_JULIUS;
    }
    return XML_ASSET_SOURCE_AUTO;
}

std::size_t layer_position(
    const std::vector<GraphicsLayerSource> &layers,
    const GraphicsLayerSource &source)
{
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (layers[i] == source) {
            return i;
        }
    }
    return layers.size();
}

int resolve_layered_path(
    const std::vector<GraphicsLayerSource> &layers,
    char *full_path,
    const char *relative_path,
    const GraphicsLayerSource &source,
    GraphicsLayerSource *resolved_source)
{
    const std::size_t start = layer_position(layers, source);
    if (start == layers.size()) {
        char candidate[FILE_NAME_MAX] = { 0 };
        if (!append_layer_graphics_path(candidate, source, relative_path)) {
            return 0;
        }
        if (regular_file_exists(candidate)) {
            snprintf(full_path, FILE_NAME_MAX, "%s", candidate);
            if (resolved_source) *resolved_source = source;
            return 1;
        }
        return 0;
    }

    for (std::size_t i = start + 1; i > 0; --i) {
        const GraphicsLayerSource &candidate_source = layers[i - 1];
        char candidate[FILE_NAME_MAX] = { 0 };
        if (!append_layer_graphics_path(candidate, candidate_source, relative_path)) {
            return 0;
        }
        if (regular_file_exists(candidate)) {
            snprintf(full_path, FILE_NAME_MAX, "%s", candidate);
            if (resolved_source) *resolved_source = candidate_source;
            return 1;
        }
    }
    return 0;
}

std::string assetlist_relative_path(const char *assetlist_key)
{
    if (!assetlist_key || !*assetlist_key) {
        return {};
    }
    char relative_path[FILE_NAME_MAX] = { 0 };
    if (snprintf(relative_path, FILE_NAME_MAX, "%s.xml", assetlist_key) >= FILE_NAME_MAX) {
        log_error("Assetlist path too long", assetlist_key, 0);
        return {};
    }
    return relative_path;
}

int try_resolved_path(char *full_path, const char *candidate, xml_asset_source candidate_source, xml_asset_source *resolved_source)
{
    if (candidate && *candidate && file_exists(candidate, NOT_LOCALIZED)) {
        snprintf(full_path, FILE_NAME_MAX, "%s", candidate);
        if (resolved_source) {
            *resolved_source = candidate_source;
        }
        return 1;
    }
    return 0;
}

} // namespace

std::vector<GraphicsLayerSource> xml_configured_graphics_sources()
{
    const std::vector<std::string> &names = mod_manager::mod_names();
    const std::vector<std::string> &paths = mod_manager::graphics_paths();
    std::vector<GraphicsLayerSource> sources;
    if (names.size() != paths.size()) {
        log_error("Configured mod names and graphics paths have different lengths", 0, 0);
        return sources;
    }
    const std::size_t count = names.size();
    sources.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        sources.push_back({i, names[i], paths[i]});
    }
    return sources;
}

std::vector<GraphicsLayerSource> xml_collect_assetlist_sources(
    const std::vector<GraphicsLayerSource> &layers,
    const char *assetlist_key)
{
    std::vector<GraphicsLayerSource> sources;
    const std::string relative_path = assetlist_relative_path(assetlist_key);
    if (relative_path.empty()) {
        return sources;
    }
    sources.reserve(layers.size());
    for (std::size_t i = layers.size(); i > 0; --i) {
        char full_path[FILE_NAME_MAX] = { 0 };
        if (append_layer_graphics_path(full_path, layers[i - 1], relative_path.c_str()) &&
            regular_file_exists(full_path)) {
            sources.push_back(layers[i - 1]);
        }
    }
    return sources;
}

int xml_resolve_assetlist_path(char *full_path, const char *assetlist_key, const GraphicsLayerSource &source)
{
    const std::string relative_path = assetlist_relative_path(assetlist_key);
    if (relative_path.empty() || !append_layer_graphics_path(full_path, source, relative_path.c_str())) {
        return 0;
    }
    return regular_file_exists(full_path);
}

int xml_resolve_image_path(
    const std::vector<GraphicsLayerSource> &layers,
    char *full_path,
    const char *assetlist_key,
    const char *image_file_name,
    const GraphicsLayerSource &source)
{
    char relative_path[FILE_NAME_MAX];
    if (!assetlist_key || !*assetlist_key || !image_file_name || !*image_file_name) {
        return 0;
    }
    std::string normalized_image_path = image_file_name;
    std::replace(normalized_image_path.begin(), normalized_image_path.end(), '\\', '/');
    const bool graphics_root_relative = normalized_image_path.find('/') != std::string::npos;
    if (normalized_image_path.front() == '/' || normalized_image_path.find(':') != std::string::npos || normalized_image_path == ".." || normalized_image_path.rfind("../", 0) == 0 || normalized_image_path.find("/../") != std::string::npos || (normalized_image_path.size() >= 3 && normalized_image_path.compare(normalized_image_path.size() - 3, 3, "/..") == 0)) {
        log_error("Image path must remain below the graphics root", image_file_name, 0);
        return 0;
    }
    const int path_length = graphics_root_relative ? snprintf(relative_path, FILE_NAME_MAX, "%s.png", normalized_image_path.c_str()) : snprintf(relative_path, FILE_NAME_MAX, "%s/%s.png", assetlist_key, normalized_image_path.c_str());
    if (path_length >= FILE_NAME_MAX) {
        log_error("Image path too long", image_file_name, 0);
        return 0;
    }
    return resolve_layered_path(layers, full_path, relative_path, source, nullptr);
}

int xml_resolve_image_path(
    char *full_path,
    const char *assetlist_key,
    const char *image_file_name,
    const GraphicsLayerSource &source)
{
    return xml_resolve_image_path(
        xml_configured_graphics_sources(), full_path, assetlist_key, image_file_name, source);
}

int xml_resolve_graphics_path(
    char *full_path,
    const char *relative_path,
    xml_asset_source source,
    xml_asset_source *resolved_source)
{
    if (!relative_path || !*relative_path) {
        return 0;
    }

    const std::vector<GraphicsLayerSource> layers = xml_configured_graphics_sources();
    if (layers.empty()) {
        return append_graphics_path_for_source(
            full_path, relative_path, source == XML_ASSET_SOURCE_AUTO ? XML_ASSET_SOURCE_MOD : source);
    }
    std::size_t start_index = layers.size() - 1;
    bool found_start = source == XML_ASSET_SOURCE_AUTO || source == XML_ASSET_SOURCE_MOD;
    if (!found_start) {
        for (std::size_t i = layers.size(); i > 0; --i) {
            const xml_asset_source layer_source = legacy_source_for_layer(layers[i - 1], layers.size() - 1);
            if (layer_source == source) {
                start_index = i - 1;
                found_start = true;
                break;
            }
        }
        if (!found_start) {
            char candidate[FILE_NAME_MAX] = { 0 };
            append_graphics_path_for_source(candidate, relative_path, source);
            return try_resolved_path(full_path, candidate, source, resolved_source) ||
                append_graphics_path_for_source(full_path, relative_path, source);
        }
    }

    for (std::size_t i = start_index + 1; i > 0; --i) {
        char candidate[FILE_NAME_MAX] = { 0 };
        const GraphicsLayerSource &candidate_source = layers[i - 1];
        if (!append_layer_graphics_path(candidate, candidate_source, relative_path)) {
            return 0;
        }
        const xml_asset_source legacy_source = legacy_source_for_layer(candidate_source, layers.size() - 1);
        if (try_resolved_path(full_path, candidate, legacy_source, resolved_source)) {
            return 1;
        }
    }

    return append_graphics_path_for_source(
        full_path,
        relative_path,
        source == XML_ASSET_SOURCE_AUTO ? XML_ASSET_SOURCE_MOD : source);
}

int xml_resolve_assetlist_path(char *full_path, const char *assetlist_key, xml_asset_source source, xml_asset_source *resolved_source)
{
    char relative_path[FILE_NAME_MAX];
    if (!assetlist_key || !*assetlist_key) {
        return 0;
    }
    if (snprintf(relative_path, FILE_NAME_MAX, "%s.xml", assetlist_key) >= FILE_NAME_MAX) {
        log_error("Assetlist path too long", assetlist_key, 0);
        return 0;
    }
    return xml_resolve_graphics_path(full_path, relative_path, source, resolved_source);
}

int xml_resolve_image_path(char *full_path, const char *assetlist_key, const char *image_file_name, xml_asset_source source)
{
    char relative_path[FILE_NAME_MAX];
    if (!assetlist_key || !*assetlist_key || !image_file_name || !*image_file_name) {
        return 0;
    }
    std::string normalized_image_path = image_file_name;
    std::replace(normalized_image_path.begin(), normalized_image_path.end(), '\\', '/');
    const bool graphics_root_relative = normalized_image_path.find('/') != std::string::npos;
    if (normalized_image_path.front() == '/' || normalized_image_path.find(':') != std::string::npos || normalized_image_path == ".." || normalized_image_path.rfind("../", 0) == 0 || normalized_image_path.find("/../") != std::string::npos || (normalized_image_path.size() >= 3 && normalized_image_path.compare(normalized_image_path.size() - 3, 3, "/..") == 0)) {
        log_error("Image path must remain below the graphics root", image_file_name, 0);
        return 0;
    }
    const int path_length = graphics_root_relative ? snprintf(relative_path, FILE_NAME_MAX, "%s.png", normalized_image_path.c_str()) : snprintf(relative_path, FILE_NAME_MAX, "%s/%s.png", assetlist_key, normalized_image_path.c_str());
    if (path_length >= FILE_NAME_MAX) {
        log_error("Image path too long", image_file_name, 0);
        return 0;
    }
    return xml_resolve_graphics_path(full_path, relative_path, source, 0);
}

int xml_resolve_group_image_path(char *full_path, const char *group_name, xml_asset_source source)
{
    char relative_path[FILE_NAME_MAX];
    if (!group_name || !*group_name) {
        return 0;
    }
    if (snprintf(relative_path, FILE_NAME_MAX, "%s.png", group_name) >= FILE_NAME_MAX) {
        log_error("Group image path too long", group_name, 0);
        return 0;
    }
    return xml_resolve_graphics_path(full_path, relative_path, source, 0);
}
