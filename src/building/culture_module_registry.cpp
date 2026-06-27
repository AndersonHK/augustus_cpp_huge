#

#include "building/culture_module_registry.h"

#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "game/mod_manager.h"

#include "core/log.h"
#include "core/xml_parser.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_culture_module_path;

namespace {

struct ParseState {
    std::unique_ptr<CultureModule> definition;
    int saw_type = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<CultureModule>> g_culture_modules;
ParseState g_parse_state;

CultureModuleType parse_type(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (text == "theater") {
        return CultureModuleType::Theater;
    }
    if (text == "amphitheater") {
        return CultureModuleType::Amphitheater;
    }
    if (text == "arena") {
        return CultureModuleType::Arena;
    }
    if (text == "colosseum") {
        return CultureModuleType::Colosseum;
    }
    if (text == "colosseum_presence") {
        return CultureModuleType::ColosseumPresence;
    }
    if (text == "hippodrome") {
        return CultureModuleType::Hippodrome;
    }
    if (text == "tavern") {
        return CultureModuleType::Tavern;
    }
    if (text == "school") {
        return CultureModuleType::School;
    }
    if (text == "library") {
        return CultureModuleType::Library;
    }
    if (text == "academy") {
        return CultureModuleType::Academy;
    }
    if (text == "hospital") {
        return CultureModuleType::Hospital;
    }
    if (text == "oracle") {
        return CultureModuleType::Oracle;
    }
    if (text == "temple") {
        return CultureModuleType::Temple;
    }
    return CultureModuleType::None;
}

int parse_root()
{
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<CultureModule>(std::string());
    }
    return 1;
}

int parse_type_node()
{
    if (!g_parse_state.definition) {
        log_error("Encountered CultureModule type before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_type) {
        log_error("CultureModule xml contains duplicate type nodes", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("CultureModule type is missing required attribute 'value'", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    CultureModuleType type = parse_type(xml_parser_get_attribute_string("value"));
    if (type == CultureModuleType::None) {
        log_error("Unsupported CultureModule type", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_type(type);

    g_parse_state.saw_type = 1;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "culture_module", parse_root, nullptr, nullptr, nullptr },
    { "type", parse_type_node, nullptr, "culture_module", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("culture_module_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<CultureModule>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_file(
        filename,
        "CultureModule",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_type) {
        log_error("Unable to parse CultureModule xml", filename, 0);
        error_context_report_error("Unable to parse CultureModule xml.", filename);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_culture_modules.find(key) != g_culture_modules.end()) {
        log_error("Duplicate CultureModule definition path", key.c_str(), 0);
        error_context_report_error("Duplicate CultureModule definition path.", key.c_str());
        return 0;
    }

    g_culture_modules.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const CultureModule *find_culture_module_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_culture_modules.find(normalized);
    return found != g_culture_modules.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

const char *culture_module_registry_get_culture_module_path(void)
{
    building_type_registry_impl::g_culture_module_path = mod_manager::mod_path() + "CultureModule/";
    return building_type_registry_impl::g_culture_module_path.c_str();
}

int culture_module_registry_load(void)
{
    using namespace building_type_registry_impl;

    culture_module_registry_get_culture_module_path();
    g_culture_modules.clear();

    return xml_definition::for_each_definition_file(
        g_culture_module_path,
        "CultureModule",
        false,
        [](const xml_definition::DefinitionFile &file, const std::string &normalized_path) {
            return parse_definition_file(file.full_path.c_str(), normalized_path.c_str());
        });
}
