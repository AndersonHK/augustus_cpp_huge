#

#include "building/religion_registry.h"

#include "building/god_registry.h"
#include "building/religion.h"
#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include "core/log.h"
#include "core/xml_parser.h"

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
    std::unique_ptr<Religion> definition;
    std::vector<std::string> god_paths;
    bool disabled = false;
    int saw_root = 0;
    int saw_god = 0;
    int saw_tier = 0;
    int saw_capacity = 0;
    int saw_presentation = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<Religion>> g_religions;
mod_definition::DefinitionOverlayTracker g_definition_sources;
std::string g_failure_reason;
ParseState g_parse_state;

int reject_tombstone_content(const char *node)
{
    if (!g_parse_state.disabled) {
        return 0;
    }
    log_error("Disabled Religion tombstone contains definition data", node, 0);
    g_parse_state.error = 1;
    return 1;
}

ReligionTier parse_tier(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (text == "shrine") {
        return ReligionTier::Shrine;
    }
    if (text == "small") {
        return ReligionTier::Small;
    }
    if (text == "large") {
        return ReligionTier::Large;
    }
    if (text == "grand") {
        return ReligionTier::Grand;
    }
    if (text == "oracle") {
        return ReligionTier::Oracle;
    }
    return ReligionTier::None;
}

int parse_presentation_module_index(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (text == "ceres") {
        return 0;
    }
    if (text == "neptune") {
        return 1;
    }
    if (text == "mercury") {
        return 2;
    }
    if (text == "mars") {
        return 3;
    }
    if (text == "venus") {
        return 4;
    }
    if (text == "pantheon") {
        return 5;
    }
    return -1;
}

int require_presentation_attribute(const char *attribute)
{
    if (xml_parser_has_attribute(attribute)) {
        return 1;
    }
    log_error("Religion presentation is missing required attribute", attribute, 0);
    g_parse_state.error = 1;
    return 0;
}

int parse_root()
{
    if (!g_parse_state.definition || g_parse_state.saw_root) {
        log_error("Religion has an invalid or duplicate root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("Religion has an invalid disabled value", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.disabled = disabled != 0;
    g_parse_state.saw_root = 1;
    return 1;
}

int parse_presentation()
{
    if (reject_tombstone_content("presentation")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered Religion presentation before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_presentation) {
        log_error("Religion xml contains duplicate presentation nodes", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *required_attributes[] = {
        "sound", "name_key", "bonus_key", "quote_key", "banner_group", "banner_image",
        "content_y_offset", "height_blocks", "module_key"
    };
    for (const char *attribute : required_attributes) {
        if (!require_presentation_attribute(attribute)) {
            return 0;
        }
    }

    ReligionPresentation &presentation = g_parse_state.definition->presentation();
    presentation.set_sound(xml_value::trim_copy(xml_parser_get_attribute_string("sound")));
    presentation.set_name_key(translation_key(xml_value::trim_copy(xml_parser_get_attribute_string("name_key"))));
    presentation.set_bonus_key(translation_key(xml_value::trim_copy(xml_parser_get_attribute_string("bonus_key"))));
    presentation.set_quote_key(translation_key(xml_value::trim_copy(xml_parser_get_attribute_string("quote_key"))));
    presentation.set_banner_group(xml_value::trim_copy(xml_parser_get_attribute_string("banner_group")));
    presentation.set_banner_image(xml_value::trim_copy(xml_parser_get_attribute_string("banner_image")));

    int content_y_offset = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("content_y_offset"), &content_y_offset)) {
        log_error("Unsupported Religion presentation content_y_offset", xml_parser_get_attribute_string("content_y_offset"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    presentation.set_content_y_offset(content_y_offset);

    int height_blocks = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("height_blocks"), &height_blocks) ||
        height_blocks <= 0) {
        log_error("Unsupported Religion presentation height_blocks", xml_parser_get_attribute_string("height_blocks"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    presentation.set_height_blocks(height_blocks);

    int module_index = parse_presentation_module_index(xml_parser_get_attribute_string("module_key"));
    if (module_index < 0) {
        log_error("Unsupported Religion presentation module_key", xml_parser_get_attribute_string("module_key"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    presentation.set_module_index(module_index);

    if (!presentation.is_complete()) {
        log_error("Religion presentation is incomplete", g_parse_state.definition->path(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_presentation = 1;
    return 1;
}

int parse_god()
{
    if (reject_tombstone_content("god")) {
        return 0;
    }
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
    if (god_path.empty()) {
        log_error("Religion god path must not be empty", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.god_paths.push_back(std::move(god_path));
    g_parse_state.saw_god = 1;
    return 1;
}

int parse_tier()
{
    if (reject_tombstone_content("tier")) {
        return 0;
    }
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
    if (reject_tombstone_content("capacity")) {
        return 0;
    }
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
    { "capacity", parse_capacity, nullptr, "religion", nullptr },
    { "presentation", parse_presentation, nullptr, "religion", nullptr }
};

struct ParsedDefinition {
    std::unique_ptr<Religion> definition;
    std::vector<std::string> god_paths;
    bool disabled = false;
};

bool parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("religion_registry.parse_definition", filename);

    const std::string stable_id = xml_definition::normalize_path(definition_path);
    g_parse_state = {};
    g_parse_state.definition = std::make_unique<Religion>(stable_id);
    const int parsed = xml_definition::parse_buffer(
        filename,
        "Religion",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    const ReligionTier tier = g_parse_state.definition ? g_parse_state.definition->tier() : ReligionTier::None;
    const int needs_presentation = tier == ReligionTier::Grand;
    const bool complete_definition = g_parse_state.saw_god && g_parse_state.saw_tier &&
        g_parse_state.saw_capacity &&
        (!needs_presentation || g_parse_state.definition->presentation().is_complete());
    if (!parsed || stable_id.empty() || g_parse_state.error || !g_parse_state.saw_root ||
        !g_parse_state.definition || (!g_parse_state.disabled && !complete_definition)) {
        log_error("Unable to parse Religion xml", filename, 0);
        error_context_report_error("Unable to parse Religion xml.", filename);
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to parse Religion xml.", filename);
        }
        return 0;
    }

    result.definition = std::move(g_parse_state.definition);
    result.god_paths = std::move(g_parse_state.god_paths);
    result.disabled = g_parse_state.disabled;
    return 1;
}

bool parse_definition_file(
    const mod_definition::DefinitionSource &source,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "Religion")) {
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to load Religion xml.", source.full_path.c_str());
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
    std::unordered_map<std::string, std::unique_ptr<Religion>> definitions;
};

bool stage_definition(
    StagedRegistry &staged,
    ParsedDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->path() : "";
    if (!staged.overlay.apply(stable_id, parsed.disabled, source)) {
        log_error("Unable to layer Religion definition", staged.overlay.failure_reason().c_str(), 0);
        error_context_report_error(
            "Unable to layer Religion definition.", staged.overlay.failure_reason().c_str());
        if (failure_reason) {
            *failure_reason = staged.overlay.failure_reason();
        }
        return false;
    }
    staged.winners[stable_id] = {std::move(parsed), source};
    return true;
}

bool resolve_winners(StagedRegistry &staged, std::string *failure_reason)
{
    for (auto &entry : staged.winners) {
        LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        for (const std::string &god_path : winner.parsed.god_paths) {
            if (god_path == "all") {
                winner.parsed.definition->set_all_gods();
                continue;
            }
            const God *god = find_god_definition(god_path.c_str());
            if (!god) {
                const std::string detail = "Religion '" + entry.first + "' from " +
                    winner.source.describe() + " references unknown God '" + god_path + "'.";
                log_error("Unable to resolve final Religion god reference", detail.c_str(), 0);
                error_context_report_error("Unable to resolve final Religion god reference.", detail.c_str());
                if (failure_reason) {
                    *failure_reason = detail;
                }
                return false;
            }
            winner.parsed.definition->add_god(god);
        }
        staged.definitions.emplace(entry.first, std::move(winner.parsed.definition));
    }
    return true;
}

bool build_layered_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedRegistry &staged,
    std::string *failure_reason)
{
    if (!mod_definition::for_each_definition_file(
            layers,
            {"Religions"},
            "Religion",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                ParsedDefinition parsed;
                return parse_definition_file(source, parsed, failure_reason) &&
                    stage_definition(staged, std::move(parsed), source, failure_reason);
            },
            nullptr,
            failure_reason)) {
        return false;
    }
    return resolve_winners(staged, failure_reason);
}

} // namespace

const Religion *find_religion_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_religions.find(normalized);
    return found != g_religions.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

int religion_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure Religion definition layers", failure_reason.c_str(), 0);
        building_type_registry_impl::g_failure_reason = failure_reason;
        return 0;
    }
    return religion_registry_load_layers(layers, &failure_reason);
}

int religion_registry_load_layers(
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
    g_religions = std::move(staged.definitions);
    g_definition_sources = std::move(staged.overlay);
    g_failure_reason.clear();
    return 1;
}

const char *religion_registry_get_failure_reason(void)
{
    return building_type_registry_impl::g_failure_reason.c_str();
}

const char *religion_definition_source_path(const char *path)
{
    const std::string stable_id = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_definition_sources.find(stable_id);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int religion_definition_is_suppressed(const char *path)
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
    religion_layer_test_result *result)
{
    if (!result) {
        return;
    }
    *result = {};
    result->active_count = static_cast<int>(staged.overlay.active_count());
    result->suppressed_count = static_cast<int>(staged.overlay.suppressed_count());
    result->queried_source_layer = -1;
    result->queried_tier = static_cast<int>(building_type_registry_impl::ReligionTier::None);

    const std::string stable_id = xml_definition::normalize_path(query_path);
    const mod_definition::DefinitionOverlayEntry *overlay = staged.overlay.find(stable_id);
    if (overlay) {
        result->queried_disabled = overlay->disabled ? 1 : 0;
        result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
    }
    const auto definition = staged.definitions.find(stable_id);
    if (definition != staged.definitions.end() && definition->second) {
        result->queried_tier = static_cast<int>(definition->second->tier());
        result->queried_capacity = definition->second->capacity();
        result->queried_god_count = static_cast<int>(definition->second->gods().size());
        result->queried_all_gods = definition->second->has_all_gods();
    }
}

} // namespace

int religion_layered_definition_buffers_are_valid_for_test(
    const religion_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    religion_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedRegistry staged;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const religion_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        mod_definition::DefinitionSource source;
        source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.category = "Religions";
        source.full_path = input.source_path ? input.source_path : "ReligionLayerTest.xml";
        source.file_name = source.full_path;
        source.normalized_definition_path = xml_definition::normalize_path(input.definition_path);
        source.registry_relative_path = "Religions\\" + source.normalized_definition_path;

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
    if (!resolve_winners(staged, &failure_reason)) {
        return 0;
    }
    fill_layer_test_result(staged, query_path, result);
    return 1;
}

int religion_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    religion_layer_test_result *result,
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
