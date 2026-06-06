#include "building/distribution.h"

#include "building/storage_type_registry.h"
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
#include <vector>

namespace building_type_registry_impl {

std::string g_distribution_path;

namespace {

struct ParseState {
    std::unique_ptr<Distribution> definition;
    int saw_rule = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<Distribution>> g_distributions;
ParseState g_parse_state;

int parse_optional_nonnegative_int(const char *attribute_name, int *out_value)
{
    if (!xml_parser_has_attribute(attribute_name)) {
        return 1;
    }

    int parsed = 0;
    const char *text = xml_parser_get_attribute_string(attribute_name);
    if (!xml_value::parse_int_strict(text ? text : "", &parsed) || parsed < 0) {
        log_error("Distribution rule has invalid non-negative integer attribute", attribute_name, 0);
        g_parse_state.error = 1;
        return 0;
    }
    *out_value = parsed;
    return 1;
}

int parse_root()
{
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<Distribution>(std::string());
    }
    return 1;
}

int parse_takes()
{
    if (!g_parse_state.definition) {
        log_error("Encountered Distribution takes before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int has_resource = xml_parser_has_attribute("resource");
    const int has_storage = xml_parser_has_attribute("storage");
    if (has_resource == has_storage) {
        log_error("Distribution takes must reference exactly one resource or storage", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int priority = 0;
    int baseline_stock = 0;
    int max_stock = 0;
    if (!parse_optional_nonnegative_int("priority", &priority) ||
        !parse_optional_nonnegative_int("baseline_stock", &baseline_stock) ||
        !parse_optional_nonnegative_int("max_stock", &max_stock)) {
        return 0;
    }

    if (has_resource) {
        const char *resource_text = xml_parser_get_attribute_string("resource");
        resource_type resource = resource_type_from_xml_attr(resource_text);
        if (resource == RESOURCE_NONE) {
            log_error("Unsupported Distribution resource", resource_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->add_resource({ resource, priority, baseline_stock, max_stock });
    } else {
        std::string storage_path = xml_definition::normalize_path(xml_parser_get_attribute_string("storage"));
        const StorageType *storage_type = storage_path.empty() ? nullptr : find_storage_type_definition(storage_path.c_str());
        if (!storage_type) {
            log_error("Unable to resolve Distribution storage reference", storage_path.c_str(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->add_storage_type(storage_type, priority, baseline_stock, max_stock);
    }

    g_parse_state.saw_rule = 1;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "distribution", parse_root, nullptr, nullptr, nullptr },
    { "takes", parse_takes, nullptr, "distribution", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("distribution_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(filename, buffer, "Distribution")) {
        error_context_report_error("Failed to load Distribution definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<Distribution>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize Distribution xml parser", filename, 0);
        error_context_report_error("Unable to initialize Distribution xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_rule ||
        g_parse_state.definition->resources().empty()) {
        log_error("Unable to parse Distribution xml", filename, 0);
        error_context_report_error("Unable to parse Distribution xml.", filename);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_distributions.find(key) != g_distributions.end()) {
        log_error("Duplicate Distribution definition path", key.c_str(), 0);
        error_context_report_error("Duplicate Distribution definition path.", key.c_str());
        return 0;
    }

    g_distributions.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const Distribution *find_distribution_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_distributions.find(normalized);
    return found != g_distributions.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

extern "C" const char *distribution_registry_get_distribution_path(void)
{
    building_type_registry_impl::g_distribution_path = std::string(mod_manager_get_mod_path()) + "Distribution/";
    return building_type_registry_impl::g_distribution_path.c_str();
}

extern "C" int distribution_registry_load(void)
{
    using namespace building_type_registry_impl;

    distribution_registry_get_distribution_path();
    g_distributions.clear();

    const dir_listing *files = dir_find_files_with_extension(g_distribution_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        return 1;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        std::snprintf(full_path, FILE_NAME_MAX, "%s%s", g_distribution_path.c_str(), files->files[i].name);
        const std::string normalized_path = xml_definition::normalize_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported Distribution file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    return 1;
}
