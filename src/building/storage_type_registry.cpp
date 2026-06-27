#

#include "building/storage_type_registry.h"

#include "core/crash_context.h"
#include "game/resource.h"
#include "game/mod_manager.h"

#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_storage_type_path;

namespace {

struct ParseState {
    std::unique_ptr<StorageType> definition;
    int saw_resource = 0;
    int saw_capacity = 0;
    int saw_role = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<StorageType>> g_storage_types;
ParseState g_parse_state;

int parse_root()
{
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<StorageType>(std::string());
    }
    if (!xml_parser_has_attribute("role")) {
        log_error("StorageType is missing required attribute 'role'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *role_text = xml_parser_get_attribute_string("role");
    if (xml_parser_compare_multiple(role_text, "input")) {
        g_parse_state.definition->set_role(StorageRole::Input);
    } else if (xml_parser_compare_multiple(role_text, "output")) {
        g_parse_state.definition->set_role(StorageRole::Output);
    } else {
        log_error("Unsupported StorageType role", role_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_role = 1;
    return 1;
}

int parse_resource()
{
    if (!g_parse_state.definition) {
        log_error("Encountered StorageType resource before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("resource")) {
        log_error("StorageType resource is missing required attribute 'resource'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *resource_text = xml_parser_get_attribute_string("resource");
    const resource_type resource = resource_type_from_xml_attr(resource_text);
    if (resource == RESOURCE_NONE) {
        log_error("Unsupported StorageType resource", resource_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_resource(resource);
    g_parse_state.saw_resource = 1;
    return 1;
}

int parse_capacity()
{
    if (!g_parse_state.definition) {
        log_error("Encountered StorageType capacity before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_capacity) {
        log_error("StorageType xml contains duplicate capacity nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("amount")) {
        log_error("StorageType capacity is missing required attribute 'amount'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int amount = xml_parser_get_attribute_int("amount");
    if (amount < 0) {
        log_error("Unsupported StorageType capacity amount", xml_parser_get_attribute_string("amount"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_capacity(amount);
    g_parse_state.saw_capacity = 1;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "storage_type", parse_root, nullptr, nullptr, nullptr },
    { "accepts", parse_resource, nullptr, "storage_type", nullptr },
    { "capacity", parse_capacity, nullptr, "storage_type", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("storage_type_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<StorageType>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_file(
        filename,
        "StorageType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    if (!parsed || g_parse_state.error || !g_parse_state.definition ||
        !g_parse_state.saw_resource || !g_parse_state.saw_role) {
        if (!g_parse_state.saw_resource) {
            log_error("StorageType xml is missing required accepts nodes", filename, 0);
        }
        if (!g_parse_state.saw_role) {
            log_error("StorageType xml is missing required role", filename, 0);
        }

        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, definition_path ? definition_path : "");
        error_context_report_error("Unable to parse StorageType xml.", detail);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_storage_types.find(key) != g_storage_types.end()) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, key.c_str());
        log_error("Duplicate StorageType definition path", key.c_str(), 0);
        error_context_report_error("Duplicate StorageType definition path.", detail);
        return 0;
    }

    g_storage_types.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const StorageType *find_storage_type_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_storage_types.find(normalized);
    return found != g_storage_types.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

const char *storage_type_registry_get_storage_type_path(void)
{
    building_type_registry_impl::g_storage_type_path = mod_manager::mod_path() + "StorageType/";
    return building_type_registry_impl::g_storage_type_path.c_str();
}

int storage_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    storage_type_registry_get_storage_type_path();
    g_storage_types.clear();

    return xml_definition::for_each_definition_file(
        g_storage_type_path,
        "StorageType",
        false,
        [](const xml_definition::DefinitionFile &file, const std::string &normalized_path) {
            return parse_definition_file(file.full_path.c_str(), normalized_path.c_str());
        });
}
