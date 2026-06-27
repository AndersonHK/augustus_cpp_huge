#include "assets/xml_path_resolution.h"

#include "core/file.h"
#include "core/log.h"
#include "game/mod_manager.h"

#include <cstdio>
#include <string>

namespace {

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

int source_already_listed(const xml_asset_source *sources, int count, xml_asset_source source)
{
    for (int i = 0; i < count; i++) {
        if (sources[i] == source) {
            return 1;
        }
    }
    return 0;
}

xml_asset_source source_for_mod_index(int index, int top_index)
{
    if (index == top_index) {
        return XML_ASSET_SOURCE_MOD;
    }
    const std::string &name = mod_manager::mod_names().at(index);
    if (ascii_equals_ignore_case(name.c_str(), "Augustus")) {
        return XML_ASSET_SOURCE_AUGUSTUS;
    }
    if (ascii_equals_ignore_case(name.c_str(), "Julius")) {
        return XML_ASSET_SOURCE_JULIUS;
    }
    return XML_ASSET_SOURCE_AUTO;
}

int collect_graphics_source_priority(xml_asset_source *sources, int max_sources)
{
    int count = 0;
    const int mod_count = static_cast<int>(mod_manager::graphics_paths().size());
    const int top_index = mod_count - 1;
    for (int i = top_index; i >= 0 && count < max_sources; i--) {
        const xml_asset_source source = source_for_mod_index(i, top_index);
        if (source == XML_ASSET_SOURCE_AUTO || source_already_listed(sources, count, source)) {
            continue;
        }
        sources[count++] = source;
    }
    return count;
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

int xml_resolve_graphics_path(
    char *full_path,
    const char *relative_path,
    xml_asset_source source,
    xml_asset_source *resolved_source)
{
    if (!relative_path || !*relative_path) {
        return 0;
    }

    xml_asset_source priority[8] = {};
    const int priority_count = collect_graphics_source_priority(
        priority,
        static_cast<int>(sizeof(priority) / sizeof(priority[0])));
    int start_index = 0;

    if (source != XML_ASSET_SOURCE_AUTO) {
        start_index = -1;
        for (int i = 0; i < priority_count; i++) {
            if (priority[i] == source) {
                start_index = i;
                break;
            }
        }
        if (start_index < 0) {
            char candidate[FILE_NAME_MAX] = { 0 };
            append_graphics_path_for_source(candidate, relative_path, source);
            return try_resolved_path(full_path, candidate, source, resolved_source) ||
                append_graphics_path_for_source(full_path, relative_path, source);
        }
    }

    for (int i = start_index; i < priority_count; i++) {
        char candidate[FILE_NAME_MAX] = { 0 };
        append_graphics_path_for_source(candidate, relative_path, priority[i]);
        if (try_resolved_path(full_path, candidate, priority[i], resolved_source)) {
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

int xml_collect_assetlist_sources(const char *assetlist_key, xml_asset_source *sources, int max_sources)
{
    if (!assetlist_key || !*assetlist_key || !sources || max_sources <= 0) {
        return 0;
    }

    xml_asset_source priority[8] = {};
    const int priority_count = collect_graphics_source_priority(
        priority,
        static_cast<int>(sizeof(priority) / sizeof(priority[0])));

    int count = 0;
    for (int i = 0; i < priority_count; i++) {
        if (count >= max_sources) {
            break;
        }

        char full_path[FILE_NAME_MAX] = { 0 };
        xml_asset_source resolved_source = XML_ASSET_SOURCE_AUTO;
        if (xml_resolve_assetlist_path(full_path, assetlist_key, priority[i], &resolved_source) &&
            resolved_source == priority[i]) {
            sources[count++] = priority[i];
        }
    }
    return count;
}

int xml_resolve_image_path(char *full_path, const char *assetlist_key, const char *image_file_name, xml_asset_source source)
{
    char relative_path[FILE_NAME_MAX];
    if (!assetlist_key || !*assetlist_key || !image_file_name || !*image_file_name) {
        return 0;
    }
    if (snprintf(relative_path, FILE_NAME_MAX, "%s/%s.png", assetlist_key, image_file_name) >= FILE_NAME_MAX) {
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
