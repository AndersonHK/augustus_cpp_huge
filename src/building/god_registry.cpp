#

#include "building/god_registry.h"

#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "city/god.h"
#include "game/mod_manager.h"

#include "core/file.h"
extern "C" {
#include "core/dir.h"
#include "core/log.h"
#include "core/xml_parser.h"
}

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_god_path;

namespace {

struct ParseState {
    std::unique_ptr<God> definition;
    int saw_legacy = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<God>> g_gods;
std::vector<const God *> g_runtime_gods;
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

god_type parse_god_type(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (compare_text(text.c_str(), "ceres") == 0) {
        return GOD_CERES;
    }
    if (compare_text(text.c_str(), "neptune") == 0) {
        return GOD_NEPTUNE;
    }
    if (compare_text(text.c_str(), "mercury") == 0) {
        return GOD_MERCURY;
    }
    if (compare_text(text.c_str(), "mars") == 0) {
        return GOD_MARS;
    }
    if (compare_text(text.c_str(), "venus") == 0) {
        return GOD_VENUS;
    }
    return GOD_ALL;
}

GodBlessingType parse_blessing_type(const char *value, int *ok)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (compare_text(text.c_str(), "neptune_trade_bonus") == 0) {
        *ok = 1;
        return GodBlessingType::NeptuneTradeBonus;
    }
    if (compare_text(text.c_str(), "venus_employment") == 0) {
        *ok = 1;
        return GodBlessingType::VenusEmployment;
    }
    *ok = 0;
    return GodBlessingType::NeptuneTradeBonus;
}

int parse_root()
{
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<God>(std::string());
    }
    if (!xml_parser_has_attribute("legacy")) {
        log_error("God xml is missing required attribute 'legacy'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *legacy_text = xml_parser_get_attribute_string("legacy");
    god_type type = parse_god_type(legacy_text);
    if (type == GOD_ALL) {
        log_error("Unsupported God legacy value", legacy_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_legacy_type(type);
    g_parse_state.saw_legacy = 1;
    return 1;
}

int parse_blessing()
{
    if (!g_parse_state.definition) {
        log_error("Encountered God blessing before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("type")) {
        log_error("God blessing is missing required attribute 'type'", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("months")) {
        log_error("God blessing is missing required attribute 'months'", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int valid_type = 0;
    GodBlessingType blessing_type = parse_blessing_type(xml_parser_get_attribute_string("type"), &valid_type);
    if (!valid_type) {
        log_error("Unsupported God blessing type", xml_parser_get_attribute_string("type"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int months = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("months"), &months) || months < 0) {
        log_error("Unsupported God blessing months", xml_parser_get_attribute_string("months"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_blessing_months(blessing_type, months);
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "god", parse_root, nullptr, nullptr, nullptr },
    { "blessing", parse_blessing, nullptr, "god", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("god_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(filename, buffer, "God")) {
        error_context_report_error("Failed to load God definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<God>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize God xml parser", filename, 0);
        error_context_report_error("Unable to initialize God xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_legacy) {
        log_error("Unable to parse God xml", filename, 0);
        error_context_report_error("Unable to parse God xml.", filename);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_gods.find(key) != g_gods.end()) {
        log_error("Duplicate God definition path", key.c_str(), 0);
        error_context_report_error("Duplicate God definition path.", key.c_str());
        return 0;
    }

    g_gods.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

void assign_runtime_ids()
{
    std::vector<std::string> paths;
    paths.reserve(g_gods.size());
    for (const auto &entry : g_gods) {
        paths.push_back(entry.first);
    }
    std::sort(paths.begin(), paths.end());

    g_runtime_gods.clear();
    g_runtime_gods.reserve(paths.size());
    int runtime_id = 0;
    for (const std::string &path : paths) {
        auto found = g_gods.find(path);
        if (found == g_gods.end() || !found->second) {
            continue;
        }
        found->second->set_runtime_id(runtime_id++);
        g_runtime_gods.push_back(found->second.get());
    }
}

} // namespace

const God *find_god_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_gods.find(normalized);
    return found != g_gods.end() ? found->second.get() : nullptr;
}

const God *find_god_definition(god_type legacy_type)
{
    for (const auto &entry : g_gods) {
        if (entry.second && entry.second->legacy_type() == legacy_type) {
            return entry.second.get();
        }
    }
    return nullptr;
}

const God *find_god_definition_by_runtime_id(int runtime_id)
{
    if (runtime_id < 0 || runtime_id >= static_cast<int>(g_runtime_gods.size())) {
        return nullptr;
    }
    return g_runtime_gods[static_cast<size_t>(runtime_id)];
}

const God *god_definition_at_runtime_index(int index)
{
    return find_god_definition_by_runtime_id(index);
}

int god_definition_count(void)
{
    return static_cast<int>(g_runtime_gods.size());
}

} // namespace building_type_registry_impl

extern "C" const char *god_registry_get_god_path(void)
{
    building_type_registry_impl::g_god_path = mod_manager::mod_path() + "Gods/";
    return building_type_registry_impl::g_god_path.c_str();
}

extern "C" int god_registry_load(void)
{
    using namespace building_type_registry_impl;

    god_registry_get_god_path();
    g_gods.clear();
    g_runtime_gods.clear();

    const dir_listing *files = dir_find_files_with_extension(g_god_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No God xml files found in", g_god_path.c_str(), 0);
        return 0;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        std::snprintf(full_path, FILE_NAME_MAX, "%s%s", g_god_path.c_str(), files->files[i].name);
        const std::string normalized_path = xml_definition::normalize_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported God file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    assign_runtime_ids();

    return 1;
}
