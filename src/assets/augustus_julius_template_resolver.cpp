#include "assets/augustus_julius_template_resolver.h"

#include "assets/graphics_extractor_common.h"
#include "core/legacy_image_extractor.h"

extern "C" {
#include "core/dir.h"
#include "core/file.h"
#include "core/xml_parser.h"
}

#include <cstdlib>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct LegacyTemplateParseState {
    AugustusJuliusTemplateGroup group;
    std::string current_image_id;
    int error = 0;
};

std::unordered_map<std::string, AugustusJuliusTemplateGroup> g_legacy_template_groups;
LegacyTemplateParseState g_legacy_template_parse_state;

static bool parse_generated_image_index(const std::string &image_id, int &index)
{
    index = 0;
    if (image_id.size() <= 6 || image_id.compare(0, 6, "Image_") != 0) {
        return false;
    }

    char *end = nullptr;
    const long value = strtol(image_id.c_str() + 6, &end, 10);
    if (!end || *end != '\0' || value < 0) {
        return false;
    }
    index = static_cast<int>(value);
    return true;
}

static bool parse_reference_image_index(const std::string &reference_image, int &index)
{
    index = 0;
    if (reference_image.empty()) {
        return true;
    }
    if (parse_generated_image_index(reference_image, index)) {
        return true;
    }

    char *end = nullptr;
    const long value = strtol(reference_image.c_str(), &end, 10);
    if (!end || *end != '\0' || value < 0) {
        return false;
    }
    index = static_cast<int>(value);
    return true;
}

static std::string make_generated_image_id(int image_index)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Image_%04d", image_index);
    return buffer;
}

static bool split_group_key(const std::string &group_key, std::string &family_key, std::string &group_name)
{
    const size_t separator = group_key.rfind('\\');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= group_key.size()) {
        family_key.clear();
        group_name.clear();
        return false;
    }

    family_key = group_key.substr(0, separator);
    group_name = group_key.substr(separator + 1);
    return true;
}

static int legacy_template_start_assetlist(void)
{
    return 1;
}

static int legacy_template_start_image(void)
{
    const char *id = xml_parser_get_attribute_string("id");
    g_legacy_template_parse_state.current_image_id = id ? id : "";
    if (!g_legacy_template_parse_state.current_image_id.empty()) {
        g_legacy_template_parse_state.group.images[g_legacy_template_parse_state.current_image_id];
        int image_index = 0;
        if (parse_generated_image_index(g_legacy_template_parse_state.current_image_id, image_index)) {
            g_legacy_template_parse_state.group.next_generated_image_index =
                std::max(g_legacy_template_parse_state.group.next_generated_image_index, image_index + 1);
        }
    }
    return 1;
}

static int legacy_template_start_layer(void)
{
    if (g_legacy_template_parse_state.current_image_id.empty()) {
        return 1;
    }

    const char *part = xml_parser_get_attribute_string("part");
    if (part && *part) {
        g_legacy_template_parse_state.group.images[g_legacy_template_parse_state.current_image_id].parts.push_back(part);
    }
    return 1;
}

static void legacy_template_end_image(void)
{
    g_legacy_template_parse_state.current_image_id.clear();
}

static const xml_parser_element kLegacyTemplateXmlElements[] = {
    { "assetlist", legacy_template_start_assetlist, 0 },
    { "image", legacy_template_start_image, legacy_template_end_image, "assetlist" },
    { "layer", legacy_template_start_layer, 0, "image" },
    { "animation", 0, 0, "image" },
    { "frame", 0, 0, "animation" }
};

} // namespace

void augustus_julius_template_resolver_clear_cache(void)
{
    g_legacy_template_groups.clear();
}

int augustus_julius_template_resolver_translate_group_key(
    const std::string &reference_group,
    std::string &translated_group_key)
{
    translated_group_key.clear();
    if (reference_group.empty()) {
        return 0;
    }

    const std::string normalized_group = graphics_extractor::normalize_key(reference_group.c_str());
    char *end = nullptr;
    const long numeric_group = strtol(normalized_group.c_str(), &end, 10);
    if (end && *end == '\0') {
        char group_key[FILE_NAME_MAX];
        if (!legacy_image_extractor_get_group_key(static_cast<int>(numeric_group), group_key, sizeof(group_key))) {
            return -1;
        }
        translated_group_key = group_key;
        return translated_group_key.empty() ? -1 : 1;
    }

    translated_group_key = normalized_group;
    return translated_group_key.empty() ? -1 : 1;
}

int augustus_julius_template_resolver_next_generated_image_index(
    const std::string &julius_graphics_root,
    const std::string &group_key)
{
    AugustusJuliusTemplateGroup group;
    if (!augustus_julius_template_resolver_load_group(julius_graphics_root, group_key, group)) {
        return 0;
    }
    return group.next_generated_image_index;
}

static std::vector<std::string> collect_semantic_continuation_group_keys(
    const std::string &julius_graphics_root,
    const std::string &base_group_key)
{
    std::string family_key;
    std::string base_group_name;
    if (!split_group_key(base_group_key, family_key, base_group_name)) {
        return { base_group_key };
    }

    std::vector<std::string> group_keys;
    const std::string family_path = graphics_extractor::append_path_component(julius_graphics_root, family_key);
    const dir_listing *xml_files = dir_find_files_with_extension(family_path.c_str(), "xml");
    const std::string continuation_prefix = base_group_name + "_";
    for (int i = 0; xml_files && i < xml_files->num_files; ++i) {
        std::string file_name = xml_files->files[i].name ? xml_files->files[i].name : "";
        if (file_name.size() <= 4 || file_name.substr(file_name.size() - 4) != ".xml") {
            continue;
        }
        file_name.resize(file_name.size() - 4);
        if (file_name == base_group_name ||
            (file_name.size() > continuation_prefix.size() &&
                file_name.compare(0, continuation_prefix.size(), continuation_prefix) == 0)) {
            group_keys.push_back(family_key + "\\" + file_name);
        }
    }

    std::sort(group_keys.begin(), group_keys.end(), [&](const std::string &left, const std::string &right) {
        if (left == right) {
            return false;
        }
        if (left == base_group_key) {
            return true;
        }
        if (right == base_group_key) {
            return false;
        }
        return left < right;
    });
    group_keys.erase(std::unique(group_keys.begin(), group_keys.end()), group_keys.end());
    if (group_keys.empty()) {
        group_keys.push_back(base_group_key);
    }
    return group_keys;
}

static int translate_numeric_group_image_from_extracted_julius(
    const std::string &julius_graphics_root,
    const std::string &base_group_key,
    int requested_image_index,
    std::string &translated_group_key,
    std::string &translated_image_id)
{
    int remaining_image_index = requested_image_index;
    for (const std::string &candidate_group_key : collect_semantic_continuation_group_keys(julius_graphics_root, base_group_key)) {
        AugustusJuliusTemplateGroup group;
        if (!augustus_julius_template_resolver_load_group(julius_graphics_root, candidate_group_key, group)) {
            continue;
        }

        const int next_index = group.next_generated_image_index;
        if (remaining_image_index < next_index) {
            const std::string candidate_image_id = make_generated_image_id(remaining_image_index);
            if (group.images.find(candidate_image_id) == group.images.end()) {
                return 0;
            }
            translated_group_key = candidate_group_key;
            translated_image_id = candidate_image_id;
            return 1;
        }
        remaining_image_index -= next_index;
    }

    return 0;
}

int augustus_julius_template_resolver_translate_group_image(
    const std::string &julius_graphics_root,
    const std::string &reference_group,
    const std::string &reference_image,
    std::string &translated_group_key,
    std::string &translated_image_id)
{
    translated_group_key.clear();
    translated_image_id.clear();
    if (reference_group.empty()) {
        return 0;
    }

    const std::string normalized_group = graphics_extractor::normalize_key(reference_group.c_str());
    char *end = nullptr;
    const long numeric_group = strtol(normalized_group.c_str(), &end, 10);
    if (!end || *end != '\0') {
        translated_group_key = normalized_group;
        int image_index = 0;
        if (parse_reference_image_index(reference_image, image_index)) {
            translated_image_id = make_generated_image_id(image_index);
        } else {
            translated_image_id = reference_image.empty() ? std::string("Image_0000") : reference_image;
        }
        return translated_group_key.empty() ? -1 : 1;
    }

    int requested_image_index = 0;
    if (!parse_reference_image_index(reference_image, requested_image_index)) {
        return -1;
    }

    char live_group_key[FILE_NAME_MAX] = { 0 };
    char live_image_id[32] = { 0 };
    if (legacy_image_extractor_get_group_image_key(
            static_cast<int>(numeric_group),
            requested_image_index,
            live_group_key,
            sizeof(live_group_key),
            live_image_id,
            sizeof(live_image_id))) {
        translated_group_key = live_group_key;
        translated_image_id = live_image_id;
        return 1;
    }

    char base_group_key[FILE_NAME_MAX] = { 0 };
    if (!legacy_image_extractor_get_group_key(static_cast<int>(numeric_group), base_group_key, sizeof(base_group_key))) {
        return -1;
    }

    if (translate_numeric_group_image_from_extracted_julius(
            julius_graphics_root,
            base_group_key,
            requested_image_index,
            translated_group_key,
            translated_image_id)) {
        return 1;
    }

    translated_group_key = base_group_key;
    translated_image_id = make_generated_image_id(requested_image_index);
    return translated_group_key.empty() ? -1 : 1;
}

int augustus_julius_template_resolver_load_group(
    const std::string &julius_graphics_root,
    const std::string &group_key,
    AugustusJuliusTemplateGroup &group)
{
    auto cached = g_legacy_template_groups.find(group_key);
    if (cached != g_legacy_template_groups.end()) {
        group = cached->second;
        return 1;
    }

    const std::string xml_path = graphics_extractor::append_path_component(julius_graphics_root, group_key + ".xml");
    if (xml_path.empty() || !file_exists(xml_path.c_str(), NOT_LOCALIZED)) {
        return 0;
    }

    std::vector<char> buffer;
    if (!graphics_extractor::load_file_to_buffer(xml_path, buffer)) {
        return 0;
    }

    g_legacy_template_parse_state = {};
    g_legacy_template_parse_state.group.group_key = group_key;
    if (!xml_parser_init(
            kLegacyTemplateXmlElements,
            static_cast<int>(sizeof(kLegacyTemplateXmlElements) / sizeof(kLegacyTemplateXmlElements[0])),
            1)) {
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_legacy_template_parse_state.error) {
        return 0;
    }

    g_legacy_template_groups.emplace(group_key, g_legacy_template_parse_state.group);
    group = g_legacy_template_parse_state.group;
    return 1;
}
