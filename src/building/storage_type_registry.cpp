#include "building/storage_type_registry.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"
#include "game/resource.h"

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

struct ParseState {
    std::unique_ptr<StorageType> definition;
    std::vector<std::string> resource_references;
    int disabled = 0;
    int saw_root = 0;
    int saw_capacity = 0;
    int saw_role = 0;
    int error = 0;
};

struct StagedStorageType {
    std::unique_ptr<StorageType> definition;
    std::vector<std::string> resource_references;
    mod_definition::DefinitionSource source;
    int disabled = 0;
};

struct StagedStorageTypes {
    std::unordered_map<std::string, StagedStorageType> winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

std::unordered_map<std::string, std::unique_ptr<StorageType>> g_storage_types;
mod_definition::DefinitionOverlayTracker g_storage_type_overlays;
std::string g_failure_reason;
ParseState g_parse_state;

int parse_enabled_content(const char *element)
{
    if (!g_parse_state.saw_root || g_parse_state.disabled) {
        log_error("Disabled StorageType definition must contain only its root identity", element, 0);
        g_parse_state.error = 1;
        return 0;
    }
    return 1;
}

int parse_root()
{
    if (g_parse_state.saw_root) {
        log_error("StorageType contains duplicate root nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_root = 1;

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("StorageType has invalid Boolean attribute 'disabled'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.disabled = disabled;
    if (disabled) {
        if (xml_parser_has_attribute("role")) {
            log_error("Disabled StorageType definition must not declare role", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        return 1;
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
    if (!parse_enabled_content("accepts")) {
        return 0;
    }
    if (!xml_parser_has_attribute("resource")) {
        log_error("StorageType resource is missing required attribute 'resource'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const std::string resource = xml_value::trim_copy(xml_parser_get_attribute_string("resource"));
    if (resource.empty()) {
        log_error("StorageType resource has empty attribute 'resource'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.resource_references.push_back(resource);
    return 1;
}

int parse_capacity()
{
    if (!parse_enabled_content("capacity")) {
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

    int amount = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("amount"), &amount) || amount < 0) {
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

int parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    StagedStorageType *out_definition)
{
    ErrorContextScope error_scope("storage_type_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<StorageType>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_buffer(
        filename,
        "StorageType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_root ||
        (!g_parse_state.disabled && (g_parse_state.resource_references.empty() || !g_parse_state.saw_role))) {
        error_context_report_error("Unable to parse StorageType xml.", filename);
        return 0;
    }

    if (out_definition) {
        out_definition->definition = std::move(g_parse_state.definition);
        out_definition->resource_references = std::move(g_parse_state.resource_references);
        out_definition->disabled = g_parse_state.disabled;
    }
    return 1;
}

int parse_definition_file(const mod_definition::DefinitionSource &source, StagedStorageType *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "StorageType")) {
        return 0;
    }
    return parse_definition_buffer(
        source.full_path.c_str(), source.normalized_definition_path.c_str(), buffer, out_definition);
}

int stage_definition(StagedStorageTypes &staged, StagedStorageType definition)
{
    const std::string key = definition.definition ? definition.definition->path() : "";
    if (key.empty()) {
        staged.failure_reason = "StorageType definition path must not be empty.";
        return 0;
    }
    if (!staged.overlays.apply(key, definition.disabled != 0, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }
    staged.winners.insert_or_assign(key, std::move(definition));
    return 1;
}

int resolve_winner_references(StagedStorageTypes &staged)
{
    for (auto &entry : staged.winners) {
        StagedStorageType &winner = entry.second;
        if (winner.disabled) {
            continue;
        }
        for (const std::string &reference : winner.resource_references) {
            const resource_type resource = resource_type_from_xml_attr(reference.c_str());
            if (resource == RESOURCE_NONE) {
                staged.failure_reason = "StorageType '" + entry.first + "' from " + winner.source.full_path +
                    " references unsupported resource '" + reference + "'.";
                return 0;
            }
            winner.definition->add_resource(resource);
        }
    }
    return 1;
}

int load_staged_definitions(StagedStorageTypes &staged)
{
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"StorageType"},
            "StorageType",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedStorageType definition;
                definition.source = source;
                if (!parse_definition_file(source, &definition)) {
                    staged.failure_reason = "Unable to parse StorageType xml: " + source.full_path;
                    return false;
                }
                return stage_definition(staged, std::move(definition)) != 0;
            },
            nullptr,
            &enumeration_failure)) {
        g_failure_reason = staged.failure_reason.empty() ? enumeration_failure : staged.failure_reason;
        return 0;
    }
    if (!resolve_winner_references(staged)) {
        g_failure_reason = staged.failure_reason;
        return 0;
    }
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

const char *storage_type_registry_get_failure_reason(void)
{
    return building_type_registry_impl::g_failure_reason.c_str();
}

const char *storage_type_definition_source_path(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_storage_type_overlays.find(normalized);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int storage_type_definition_is_suppressed(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_storage_type_overlays.find(normalized);
    return entry && entry->disabled;
}

int storage_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    StagedStorageTypes staged;
    if (!load_staged_definitions(staged)) {
        error_context_report_error("Unable to load layered StorageType definitions.", g_failure_reason.c_str());
        return 0;
    }

    std::unordered_map<std::string, std::unique_ptr<StorageType>> published;
    for (auto &entry : staged.winners) {
        if (!entry.second.disabled) {
            published.emplace(entry.first, std::move(entry.second.definition));
        }
    }
    g_storage_types = std::move(published);
    g_storage_type_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int storage_type_layered_definition_buffers_are_valid_for_test(
    const storage_type_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    storage_type_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedStorageTypes staged;
    for (int index = 0; index < input_count; ++index) {
        const storage_type_layer_test_input &input = inputs[index];
        const std::string definition_path = xml_definition::normalize_path(input.definition_path);
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedStorageType definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.full_path = input.source_path ? input.source_path : "StorageTypeLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.category = "StorageType";
        definition.source.normalized_definition_path = definition_path;
        definition.source.registry_relative_path = "StorageType\\" + definition_path;
        if (!parse_definition_buffer(
                definition.source.full_path.c_str(), definition_path.c_str(), buffer, &definition) ||
            !stage_definition(staged, std::move(definition))) {
            return 0;
        }
    }
    if (!resolve_winner_references(staged)) {
        return 0;
    }

    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        result->queried_source_layer = -1;
        const std::string query = xml_definition::normalize_path(query_path);
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(query);
        const auto winner = staged.winners.find(query);
        if (overlay && winner != staged.winners.end()) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
            if (!winner->second.disabled && winner->second.definition) {
                result->queried_capacity = winner->second.definition->capacity();
                result->queried_resource_count = static_cast<int>(winner->second.definition->resources().size());
            }
        }
    }
    return 1;
}
#endif
