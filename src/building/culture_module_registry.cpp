#include "building/culture_module_registry.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

struct ParseState {
    std::unique_ptr<CultureModule> definition;
    bool disabled = false;
    bool saw_root = false;
    bool saw_type = false;
    bool error = false;
};

std::unordered_map<std::string, std::unique_ptr<CultureModule>> g_culture_modules;
mod_definition::DefinitionOverlayTracker g_definition_sources;
std::string g_failure_reason;
ParseState g_parse_state;

CultureModuleType parse_type(const char *value)
{
    const std::string text = xml_value::trim_copy(value ? value : "");
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
    if (!g_parse_state.definition || g_parse_state.saw_root) {
        log_error("CultureModule has an invalid or duplicate root", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("CultureModule has an invalid disabled value", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.disabled = disabled != 0;
    g_parse_state.saw_root = true;
    return 1;
}

int parse_type_node()
{
    if (g_parse_state.disabled) {
        log_error("Disabled CultureModule tombstone contains definition data", "type", 0);
        g_parse_state.error = true;
        return 0;
    }
    if (!g_parse_state.definition || g_parse_state.saw_type ||
        !xml_parser_has_attribute("value")) {
        log_error("CultureModule has an invalid or duplicate type node", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const CultureModuleType type = parse_type(xml_parser_get_attribute_string("value"));
    if (type == CultureModuleType::None) {
        log_error("Unsupported CultureModule type", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->set_type(type);
    g_parse_state.saw_type = true;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "culture_module", parse_root, nullptr, nullptr, nullptr },
    { "type", parse_type_node, nullptr, "culture_module", nullptr }
};

struct ParsedDefinition {
    std::unique_ptr<CultureModule> definition;
    bool disabled = false;
};

bool parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("culture_module_registry.parse_definition", filename);
    const std::string stable_id = xml_definition::normalize_path(definition_path);
    g_parse_state = {};
    g_parse_state.definition = std::make_unique<CultureModule>(stable_id);
    const bool parsed = xml_definition::parse_buffer(
        filename,
        "CultureModule",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || stable_id.empty() || g_parse_state.error || !g_parse_state.saw_root ||
        !g_parse_state.definition || (!g_parse_state.disabled && !g_parse_state.saw_type)) {
        const std::string detail = xml_definition::format_failure_reason(
            "Unable to parse CultureModule xml.", filename);
        log_error("Unable to parse CultureModule xml", filename, 0);
        error_context_report_error("Unable to parse CultureModule xml.", filename);
        if (failure_reason) {
            *failure_reason = detail;
        }
        return false;
    }

    result.definition = std::move(g_parse_state.definition);
    result.disabled = g_parse_state.disabled;
    return true;
}

bool parse_definition_file(
    const mod_definition::DefinitionSource &source,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "CultureModule")) {
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to load CultureModule xml.", source.full_path.c_str());
        }
        return false;
    }
    return parse_definition_buffer(
        source.full_path.c_str(),
        source.normalized_definition_path.c_str(),
        buffer,
        result,
        failure_reason);
}

struct LayeredDefinition {
    ParsedDefinition parsed;
    mod_definition::DefinitionSource source;
};

struct StagedRegistry {
    mod_definition::DefinitionOverlayTracker overlay;
    std::map<std::string, LayeredDefinition> winners;
    std::unordered_map<std::string, std::unique_ptr<CultureModule>> definitions;
};

bool stage_definition(
    StagedRegistry &staged,
    ParsedDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->path() : "";
    if (!staged.overlay.apply(stable_id, parsed.disabled, source)) {
        log_error("Unable to layer CultureModule definition", staged.overlay.failure_reason().c_str(), 0);
        error_context_report_error(
            "Unable to layer CultureModule definition.", staged.overlay.failure_reason().c_str());
        if (failure_reason) {
            *failure_reason = staged.overlay.failure_reason();
        }
        return false;
    }
    staged.winners[stable_id] = {std::move(parsed), source};
    return true;
}

void materialize_winners(StagedRegistry &staged)
{
    for (auto &entry : staged.winners) {
        if (!entry.second.parsed.disabled) {
            staged.definitions.emplace(entry.first, std::move(entry.second.parsed.definition));
        }
    }
}

bool build_layered_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedRegistry &staged,
    std::string *failure_reason)
{
    if (!mod_definition::for_each_definition_file(
            layers,
            {"CultureModule"},
            "CultureModule",
            false,
            [&](const mod_definition::DefinitionSource &source) {
                ParsedDefinition parsed;
                return parse_definition_file(source, parsed, failure_reason) &&
                    stage_definition(staged, std::move(parsed), source, failure_reason);
            },
            nullptr,
            failure_reason)) {
        return false;
    }
    materialize_winners(staged);
    return true;
}

} // namespace

const CultureModule *find_culture_module_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_culture_modules.find(normalized);
    return found != g_culture_modules.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

int culture_module_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure CultureModule definition layers", failure_reason.c_str(), 0);
        building_type_registry_impl::g_failure_reason = failure_reason;
        return 0;
    }
    return culture_module_registry_load_layers(layers, &failure_reason);
}

int culture_module_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    std::string local_failure;
    std::string *out_failure = failure_reason ? failure_reason : &local_failure;
    if (!build_layered_registry(layers, staged, out_failure)) {
        g_failure_reason = *out_failure;
        return 0;
    }
    g_culture_modules = std::move(staged.definitions);
    g_definition_sources = std::move(staged.overlay);
    g_failure_reason.clear();
    return 1;
}

const char *culture_module_registry_get_failure_reason(void)
{
    return building_type_registry_impl::g_failure_reason.c_str();
}

const char *culture_module_definition_source_path(const char *path)
{
    const std::string stable_id = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_definition_sources.find(stable_id);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int culture_module_definition_is_suppressed(const char *path)
{
    const std::string stable_id = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_definition_sources.find(stable_id);
    return entry && entry->disabled;
}

#ifdef STARTUP_PARSER_TEST
namespace {

void fill_layer_test_result(
    const building_type_registry_impl::StagedRegistry &staged,
    const char *query_path,
    culture_module_layer_test_result *result)
{
    if (!result) {
        return;
    }
    *result = {};
    result->active_count = static_cast<int>(staged.overlay.active_count());
    result->suppressed_count = static_cast<int>(staged.overlay.suppressed_count());
    result->queried_source_layer = -1;
    result->queried_type = static_cast<int>(building_type_registry_impl::CultureModuleType::None);

    const std::string stable_id = xml_definition::normalize_path(query_path);
    const mod_definition::DefinitionOverlayEntry *overlay = staged.overlay.find(stable_id);
    if (overlay) {
        result->queried_disabled = overlay->disabled ? 1 : 0;
        result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
    }
    const auto definition = staged.definitions.find(stable_id);
    if (definition != staged.definitions.end() && definition->second) {
        result->queried_type = static_cast<int>(definition->second->type());
    }
}

} // namespace

int culture_module_layered_definition_buffers_are_valid_for_test(
    const culture_module_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    culture_module_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedRegistry staged;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const culture_module_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        mod_definition::DefinitionSource source;
        source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.category = "CultureModule";
        source.full_path = input.source_path ? input.source_path : "CultureModuleLayerTest.xml";
        source.file_name = source.full_path;
        source.normalized_definition_path = xml_definition::normalize_path(input.definition_path);
        source.registry_relative_path = "CultureModule\\" + source.normalized_definition_path;

        ParsedDefinition parsed;
        if (!parse_definition_buffer(
                source.full_path.c_str(),
                source.normalized_definition_path.c_str(),
                buffer,
                parsed,
                &failure_reason) ||
            !stage_definition(staged, std::move(parsed), source, &failure_reason)) {
            return 0;
        }
    }
    materialize_winners(staged);
    fill_layer_test_result(staged, query_path, result);
    return 1;
}

int culture_module_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    culture_module_layer_test_result *result,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        return 0;
    }
    fill_layer_test_result(staged, query_path, result);
    return 1;
}
#endif
