#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct AugustusJuliusTemplateImage {
    std::vector<std::string> parts;
};

struct AugustusJuliusTemplateGroup {
    std::string group_key;
    std::unordered_map<std::string, AugustusJuliusTemplateImage> images;
    int next_generated_image_index = 0;
};

void augustus_julius_template_resolver_clear_cache(void);
int augustus_julius_template_resolver_translate_group_key(
    const std::string &reference_group,
    std::string &translated_group_key);
int augustus_julius_template_resolver_translate_group_image(
    const std::string &julius_graphics_root,
    const std::string &reference_group,
    const std::string &reference_image,
    std::string &translated_group_key,
    std::string &translated_image_id);
int augustus_julius_template_resolver_next_generated_image_index(
    const std::string &julius_graphics_root,
    const std::string &group_key);
int augustus_julius_template_resolver_load_group(
    const std::string &julius_graphics_root,
    const std::string &group_key,
    AugustusJuliusTemplateGroup &group);
