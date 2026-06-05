#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace vespasian::graphics::extraction {

class JuliusTemplateImage {
public:
    void add_part(const std::string &part) { parts_.push_back(part); }
    const std::vector<std::string> &parts() const { return parts_; }

private:
    std::vector<std::string> parts_;
};

class JuliusTemplateGroup {
public:
    int next_generated_image_index() const { return next_generated_image_index_; }
    bool has_image(const std::string &image_id) const { return images_.find(image_id) != images_.end(); }
    const JuliusTemplateImage *find_image(const std::string &image_id) const
    {
        auto it = images_.find(image_id);
        return it == images_.end() ? nullptr : &it->second;
    }
    JuliusTemplateImage &ensure_image(const std::string &image_id);
    void observe_generated_image_id(const std::string &image_id);

private:
    std::unordered_map<std::string, JuliusTemplateImage> images_;
    int next_generated_image_index_ = 0;
};

class JuliusTemplateCatalog {
public:
    int translate_group_key(const std::string &reference_group, std::string &translated_group_key) const;
    int translate_group_image(
        const std::string &julius_graphics_root,
        const std::string &reference_group,
        const std::string &reference_image,
        std::string &translated_group_key,
        std::string &translated_image_id);
    int next_generated_image_index(const std::string &julius_graphics_root, const std::string &group_key);
    bool load_group(const std::string &julius_graphics_root, const std::string &group_key, JuliusTemplateGroup &group);

private:
    std::unordered_map<std::string, JuliusTemplateGroup> cached_groups_;

    bool parse_group_xml(const std::string &xml_path, JuliusTemplateGroup &group) const;
    std::vector<std::string> collect_semantic_continuation_group_keys(
        const std::string &julius_graphics_root,
        const std::string &base_group_key);
    int translate_numeric_group_image_from_extracted_julius(
        const std::string &julius_graphics_root,
        const std::string &base_group_key,
        int requested_image_index,
        std::string &translated_group_key,
        std::string &translated_image_id);
};

} // namespace vespasian::graphics::extraction
