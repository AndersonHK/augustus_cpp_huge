#

#include "building/god_registry.h"

#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "city/god.h"
#include "game/mod_manager.h"

#include "core/log.h"
#include "core/xml_parser.h"

#include <algorithm>
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

god_type parse_god_type(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (text == "ceres") {
        return GOD_CERES;
    }
    if (text == "neptune") {
        return GOD_NEPTUNE;
    }
    if (text == "mercury") {
        return GOD_MERCURY;
    }
    if (text == "mars") {
        return GOD_MARS;
    }
    if (text == "venus") {
        return GOD_VENUS;
    }
    return GOD_ALL;
}

GodBlessingType parse_blessing_type(const char *value, int *ok)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (text == "neptune_trade_bonus") {
        *ok = 1;
        return GodBlessingType::NeptuneTradeBonus;
    }
    if (text == "venus_employment") {
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

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<God>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_file(
        filename,
        "God",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
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

const char *god_registry_get_god_path(void)
{
    building_type_registry_impl::g_god_path = mod_manager::mod_path() + "Gods/";
    return building_type_registry_impl::g_god_path.c_str();
}

int god_registry_load(void)
{
    using namespace building_type_registry_impl;

    god_registry_get_god_path();
    g_gods.clear();
    g_runtime_gods.clear();

    if (!xml_definition::for_each_definition_file(
        g_god_path,
        "God",
        true,
        [](const xml_definition::DefinitionFile &file, const std::string &normalized_path) {
            return parse_definition_file(file.full_path.c_str(), normalized_path.c_str());
        })) {
        return 0;
    }

    assign_runtime_ids();

    return 1;
}
