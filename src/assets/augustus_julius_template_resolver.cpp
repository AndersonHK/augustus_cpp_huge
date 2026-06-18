#include "assets/augustus_julius_template_resolver.h"

#include "assets/graphics_extractor_common.h"
#include "core/legacy_image_extractor.h"

#include "core/file.h"
extern "C" {
#include "core/dir.h"
}

#include <algorithm>
#include <cstdlib>

namespace vespasian::graphics::extraction {

namespace {

bool parse_reference_image_index(const std::string &reference_image, int &index)
{
    index = 0;
    if (reference_image.empty()) {
        return true;
    }
    if (graphics_extractor::parse_generated_image_index(reference_image, index)) {
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

bool split_group_key(const std::string &group_key, std::string &family_key, std::string &group_name)
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

} // namespace

JuliusTemplateImage &JuliusTemplateGroup::ensure_image(const std::string &image_id)
{
    return images_[image_id];
}

void JuliusTemplateGroup::observe_generated_image_id(const std::string &image_id)
{
    int image_index = 0;
    if (graphics_extractor::parse_generated_image_index(image_id, image_index)) {
        next_generated_image_index_ = std::max(next_generated_image_index_, image_index + 1);
    }
}

int JuliusTemplateCatalog::translate_group_key(
    const std::string &reference_group,
    std::string &translated_group_key) const
{
    translated_group_key.clear();
    if (reference_group.empty()) {
        return 0;
    }

    const std::string normalized_group = graphics_extractor::normalize_key(reference_group.c_str());
    char *end = nullptr;
    const long numeric_group = strtol(normalized_group.c_str(), &end, 10);
    if (end && *end == '\0') {
        translated_group_key = JuliusExtractor().resolveLegacyGroup(static_cast<int>(numeric_group));
        return translated_group_key.empty() ? -1 : 1;
    }

    translated_group_key = normalized_group;
    return translated_group_key.empty() ? -1 : 1;
}

int JuliusTemplateCatalog::next_generated_image_index(
    const std::string &julius_graphics_root,
    const std::string &group_key)
{
    JuliusTemplateGroup group;
    if (!load_group(julius_graphics_root, group_key, group)) {
        return 0;
    }
    return group.next_generated_image_index();
}

std::vector<std::string> JuliusTemplateCatalog::collect_semantic_continuation_group_keys(
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

int JuliusTemplateCatalog::translate_numeric_group_image_from_extracted_julius(
    const std::string &julius_graphics_root,
    const std::string &base_group_key,
    int requested_image_index,
    std::string &translated_group_key,
    std::string &translated_image_id)
{
    int remaining_image_index = requested_image_index;
    for (const std::string &candidate_group_key :
        collect_semantic_continuation_group_keys(julius_graphics_root, base_group_key)) {
        JuliusTemplateGroup group;
        if (!load_group(julius_graphics_root, candidate_group_key, group)) {
            continue;
        }

        const int next_index = group.next_generated_image_index();
        if (remaining_image_index < next_index) {
            const std::string candidate_image_id = graphics_extractor::make_generated_image_id(remaining_image_index);
            if (!group.has_image(candidate_image_id)) {
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

int JuliusTemplateCatalog::translate_group_image(
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
            translated_image_id = graphics_extractor::make_generated_image_id(image_index);
        } else {
            translated_image_id = reference_image.empty() ? std::string("Image_0000") : reference_image;
        }
        return translated_group_key.empty() ? -1 : 1;
    }

    int requested_image_index = 0;
    if (!parse_reference_image_index(reference_image, requested_image_index)) {
        return -1;
    }

    const GroupImageKey live_key = JuliusExtractor().resolveLegacyImage(
        static_cast<int>(numeric_group),
        requested_image_index);
    if (live_key.valid()) {
        translated_group_key = live_key.group_key();
        translated_image_id = live_key.image_id();
        return 1;
    }

    const std::string base_group_key = JuliusExtractor().resolveLegacyGroup(static_cast<int>(numeric_group));
    if (base_group_key.empty()) {
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
    translated_image_id = graphics_extractor::make_generated_image_id(requested_image_index);
    return translated_group_key.empty() ? -1 : 1;
}

bool JuliusTemplateCatalog::parse_group_xml(
    const std::string &xml_path,
    JuliusTemplateGroup &group) const
{
    std::string contents;
    if (!graphics_extractor::read_text_file(xml_path, contents)) {
        return false;
    }

    group = JuliusTemplateGroup();
    std::string current_image_id;
    const XmlReader reader;
    for (const XmlToken &token : reader.parse(contents)) {
        const XmlElement &element = token.element();
        if (token.type() == XmlToken::Type::end) {
            if (element.name() == "image") {
                current_image_id.clear();
            }
            continue;
        }

        if (element.name() == "image") {
            current_image_id = element.attribute("id");
            if (!current_image_id.empty()) {
                group.ensure_image(current_image_id);
                group.observe_generated_image_id(current_image_id);
            }
            if (token.is_self_closing()) {
                current_image_id.clear();
            }
            continue;
        }
        if (element.name() == "layer" && !current_image_id.empty()) {
            const std::string &part = element.attribute("part");
            if (!part.empty()) {
                group.ensure_image(current_image_id).add_part(part);
            }
        }
    }
    return true;
}

bool JuliusTemplateCatalog::load_group(
    const std::string &julius_graphics_root,
    const std::string &group_key,
    JuliusTemplateGroup &group)
{
    auto cached = cached_groups_.find(group_key);
    if (cached != cached_groups_.end()) {
        group = cached->second;
        return true;
    }

    const std::string xml_path = graphics_extractor::append_path_component(julius_graphics_root, group_key + ".xml");
    if (xml_path.empty() || !file_exists(xml_path.c_str(), NOT_LOCALIZED)) {
        return false;
    }

    if (!parse_group_xml(xml_path, group)) {
        return false;
    }

    cached_groups_.emplace(group_key, group);
    return true;
}

} // namespace vespasian::graphics::extraction
