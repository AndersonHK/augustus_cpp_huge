#

#include "building/culture_module_registry.h"

#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "game/mod_manager.h"

#include "core/file.h"
#include "core/dir.h"
#include "core/log.h"
#include "core/xml_parser.h"

#include <cstdio>
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

CultureModuleType parse_type(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (compare_text(text.c_str(), "theater") == 0) {
        return CultureModuleType::Theater;
    }
    if (compare_text(text.c_str(), "amphitheater") == 0) {
        return CultureModuleType::Amphitheater;
    }
    if (compare_text(text.c_str(), "arena") == 0) {
        return CultureModuleType::Arena;
    }
    if (compare_text(text.c_str(), "colosseum") == 0) {
        return CultureModuleType::Colosseum;
    }
    if (compare_text(text.c_str(), "colosseum_presence") == 0) {
        return CultureModuleType::ColosseumPresence;
    }
    if (compare_text(text.c_str(), "hippodrome") == 0) {
        return CultureModuleType::Hippodrome;
    }
    if (compare_text(text.c_str(), "tavern") == 0) {
        return CultureModuleType::Tavern;
    }
    if (compare_text(text.c_str(), "school") == 0) {
        return CultureModuleType::School;
    }
    if (compare_text(text.c_str(), "library") == 0) {
        return CultureModuleType::Library;
    }
    if (compare_text(text.c_str(), "academy") == 0) {
        return CultureModuleType::Academy;
    }
    if (compare_text(text.c_str(), "hospital") == 0) {
        return CultureModuleType::Hospital;
    }
    if (compare_text(text.c_str(), "oracle") == 0) {
        return CultureModuleType::Oracle;
    }
    if (compare_text(text.c_str(), "temple") == 0) {
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

    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(filename, buffer, "CultureModule")) {
        error_context_report_error("Failed to load CultureModule definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<CultureModule>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize CultureModule xml parser", filename, 0);
        error_context_report_error("Unable to initialize CultureModule xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
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

    const dir_listing *files = dir_find_files_with_extension(g_culture_module_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        return 1;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        std::snprintf(full_path, FILE_NAME_MAX, "%s%s", g_culture_module_path.c_str(), files->files[i].name);
        const std::string normalized_path = xml_definition::normalize_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported CultureModule file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    return 1;
}
