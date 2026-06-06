#include "building/religion_registry.h"

#include "building/god_registry.h"
#include "building/religion.h"
#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"

extern "C" {
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
}

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_religion_path;

namespace {

struct ParseState {
    std::unique_ptr<Religion> definition;
    int saw_god = 0;
    int saw_tier = 0;
    int saw_capacity = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<Religion>> g_religions;
ParseState g_parse_state;

int compare_text(const char *left, const char *right)
{
    if (!left || !right) {
        return left == right ? 0 : (left ? 1 : -1);
    }
    while (*left && *right && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

ReligionTier parse_tier(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (compare_text(text.c_str(), "shrine") == 0) {
        return ReligionTier::Shrine;
    }
    if (compare_text(text.c_str(), "small") == 0) {
        return ReligionTier::Small;
    }
    if (compare_text(text.c_str(), "large") == 0) {
        return ReligionTier::Large;
    }
    if (compare_text(text.c_str(), "grand") == 0) {
        return ReligionTier::Grand;
    }
    if (compare_text(text.c_str(), "oracle") == 0) {
        return ReligionTier::Oracle;
    }
    if (compare_text(text.c_str(), "pantheon") == 0) {
        return ReligionTier::Pantheon;
    }
    return ReligionTier::None;
}

int parse_root()
{
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<Religion>(std::string());
    }
    return 1;
}

int parse_god()
{
    if (!g_parse_state.definition) {
        log_error("Encountered Religion god before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("path")) {
        log_error("Religion god is missing required attribute 'path'", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string god_path = xml_definition::normalize_path(xml_parser_get_attribute_string("path"));
    if (compare_text(god_path.c_str(), "all") == 0) {
        g_parse_state.definition->set_all_gods();
        g_parse_state.saw_god = 1;
        return 1;
    }
    const God *god = god_path.empty() ? nullptr : find_god_definition(god_path.c_str());
    if (!god) {
        log_error("Unable to resolve Religion god reference", god_path.c_str(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_god(god);
    g_parse_state.saw_god = 1;
    return 1;
}

int parse_tier()
{
    if (!g_parse_state.definition) {
        log_error("Encountered Religion tier before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_tier) {
        log_error("Religion xml contains duplicate tier nodes", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("Religion tier is missing required attribute 'value'", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    ReligionTier tier = parse_tier(xml_parser_get_attribute_string("value"));
    if (tier == ReligionTier::None) {
        log_error("Unsupported Religion tier", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_tier(tier);
    g_parse_state.saw_tier = 1;
    return 1;
}

int parse_capacity()
{
    if (!g_parse_state.definition) {
        log_error("Encountered Religion capacity before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_capacity) {
        log_error("Religion xml contains duplicate capacity nodes", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("amount")) {
        log_error("Religion capacity is missing required attribute 'amount'", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int amount = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("amount"), &amount) || amount < 0) {
        log_error("Unsupported Religion capacity amount", xml_parser_get_attribute_string("amount"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_capacity(amount);
    g_parse_state.saw_capacity = 1;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "religion", parse_root, nullptr, nullptr, nullptr },
    { "god", parse_god, nullptr, "religion", nullptr },
    { "tier", parse_tier, nullptr, "religion", nullptr },
    { "capacity", parse_capacity, nullptr, "religion", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("religion_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(filename, buffer, "Religion")) {
        error_context_report_error("Failed to load Religion definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<Religion>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize Religion xml parser", filename, 0);
        error_context_report_error("Unable to initialize Religion xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition ||
        !g_parse_state.saw_god || !g_parse_state.saw_tier || !g_parse_state.saw_capacity) {
        log_error("Unable to parse Religion xml", filename, 0);
        error_context_report_error("Unable to parse Religion xml.", filename);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_religions.find(key) != g_religions.end()) {
        log_error("Duplicate Religion definition path", key.c_str(), 0);
        error_context_report_error("Duplicate Religion definition path.", key.c_str());
        return 0;
    }

    g_religions.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const Religion *find_religion_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_religions.find(normalized);
    return found != g_religions.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

extern "C" const char *religion_registry_get_religion_path(void)
{
    building_type_registry_impl::g_religion_path = std::string(mod_manager_get_mod_path()) + "Religions/";
    return building_type_registry_impl::g_religion_path.c_str();
}

extern "C" int religion_registry_load(void)
{
    using namespace building_type_registry_impl;

    religion_registry_get_religion_path();
    g_religions.clear();

    const dir_listing *files = dir_find_files_with_extension(g_religion_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No Religion xml files found in", g_religion_path.c_str(), 0);
        return 0;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        std::snprintf(full_path, FILE_NAME_MAX, "%s%s", g_religion_path.c_str(), files->files[i].name);
        const std::string normalized_path = xml_definition::normalize_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported Religion file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    return 1;
}
