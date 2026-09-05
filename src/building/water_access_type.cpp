#include "building/water_access_type.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

constexpr int kMaxWaterAccessTypes = 8;

std::vector<std::unique_ptr<WaterAccessType>> g_water_access_types;
std::array<const WaterAccessType *, kMaxWaterAccessTypes> g_water_access_by_number = {};
uint8_t g_defined_mask = 0;
mod_definition::DefinitionOverlayTracker g_water_access_overlays;
std::string g_failure_reason;

struct ParseState {
    std::unique_ptr<WaterAccessType> definition;
    bool disabled = false;
    bool saw_root = false;
    bool error = false;
};

ParseState g_parse_state;

int parse_root()
{
    if (g_parse_state.saw_root || !xml_parser_has_attribute("text_id") ||
        !xml_parser_has_attribute("number_id")) {
        log_error("WaterAccessType xml is missing identity attributes or has duplicate roots", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const char *text_id = xml_parser_get_attribute_string("text_id");
    std::string normalized_text_id = xml_definition::normalize_path(text_id);
    int number_id = -1;
    int disabled = 0;
    if (normalized_text_id.empty() ||
        !xml_value::parse_int_strict(xml_parser_get_attribute_string("number_id"), &number_id) ||
        number_id < 0 || number_id >= kMaxWaterAccessTypes ||
        (xml_parser_has_attribute("disabled") &&
            !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled))) {
        log_error("Unsupported WaterAccessType identity or disabled value", text_id, number_id);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.definition = std::make_unique<WaterAccessType>(
        std::move(normalized_text_id),
        static_cast<uint8_t>(number_id));
    g_parse_state.disabled = disabled != 0;
    g_parse_state.saw_root = true;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "water_access_type", parse_root, nullptr, nullptr, nullptr }
};

struct ParsedDefinition {
    std::unique_ptr<WaterAccessType> definition;
    bool disabled = false;
};

bool parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("water_access_type_registry.parse_definition", filename);
    g_parse_state = {};
    const bool parsed = xml_definition::parse_buffer(
        filename,
        "WaterAccessType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || !g_parse_state.definition) {
        const std::string detail = xml_definition::format_failure_reason(
            "Unable to parse WaterAccessType xml.", filename);
        error_context_report_error("Unable to parse WaterAccessType xml.", filename);
        if (failure_reason) {
            *failure_reason = detail;
        }
        return false;
    }

    const std::string stable_id = xml_definition::normalize_path(definition_path);
    if (stable_id.empty() || stable_id != g_parse_state.definition->text_id()) {
        const std::string detail = "WaterAccessType text_id '" +
            std::string(g_parse_state.definition->text_id()) + "' does not match definition path '" +
            stable_id + "' in " + (filename ? filename : "<unknown source>") + '.';
        log_error("WaterAccessType text_id does not match its definition path", detail.c_str(), 0);
        error_context_report_error("WaterAccessType identity mismatch.", detail.c_str());
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
    if (!xml_definition::load_file_to_buffer(
            source.full_path.c_str(), buffer, "WaterAccessType")) {
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to load WaterAccessType xml.", source.full_path.c_str());
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

struct StableIdentity {
    uint8_t number_id = 0;
    mod_definition::DefinitionSource source;
};

struct LayeredDefinition {
    ParsedDefinition parsed;
    mod_definition::DefinitionSource source;
};

struct StagedRegistry {
    mod_definition::DefinitionOverlayTracker overlay;
    std::map<std::string, StableIdentity> identities;
    std::map<std::string, LayeredDefinition> winners;
    std::vector<std::unique_ptr<WaterAccessType>> definitions;
    std::array<const WaterAccessType *, kMaxWaterAccessTypes> by_number = {};
    uint8_t defined_mask = 0;
};

bool stage_definition(
    StagedRegistry &staged,
    ParsedDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->text_id() : "";
    const uint8_t number_id = parsed.definition ? parsed.definition->number_id() : 0;
    const auto identity = staged.identities.find(stable_id);
    if (identity != staged.identities.end() && identity->second.number_id != number_id) {
        const std::string detail = "WaterAccessType '" + stable_id + "' changes number_id from " +
            std::to_string(identity->second.number_id) + " in " + identity->second.source.describe() +
            " to " + std::to_string(number_id) + " in " + source.describe() + '.';
        log_error("WaterAccessType replacement changes stable numeric identity", detail.c_str(), 0);
        error_context_report_error("WaterAccessType replacement changes stable numeric identity.", detail.c_str());
        if (failure_reason) {
            *failure_reason = detail;
        }
        return false;
    }
    if (identity == staged.identities.end()) {
        staged.identities.emplace(stable_id, StableIdentity{number_id, source});
    }

    if (!staged.overlay.apply(stable_id, parsed.disabled, source)) {
        log_error("Unable to layer WaterAccessType definition", staged.overlay.failure_reason().c_str(), 0);
        error_context_report_error(
            "Unable to layer WaterAccessType definition.", staged.overlay.failure_reason().c_str());
        if (failure_reason) {
            *failure_reason = staged.overlay.failure_reason();
        }
        return false;
    }
    staged.winners[stable_id] = {std::move(parsed), source};
    return true;
}

bool materialize_winners(StagedRegistry &staged, std::string *failure_reason)
{
    std::array<const LayeredDefinition *, kMaxWaterAccessTypes> slot_owners = {};
    for (const auto &entry : staged.winners) {
        const LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        const uint8_t number_id = winner.parsed.definition->number_id();
        const LayeredDefinition *existing = slot_owners[number_id];
        if (existing) {
            const std::string detail = "WaterAccessType number_id " + std::to_string(number_id) +
                " is claimed by both " + existing->source.describe() + " and " + winner.source.describe() + '.';
            log_error("Duplicate active WaterAccessType number_id", detail.c_str(), number_id);
            error_context_report_error("Duplicate active WaterAccessType number_id.", detail.c_str());
            if (failure_reason) {
                *failure_reason = detail;
            }
            return false;
        }
        slot_owners[number_id] = &winner;
    }

    for (auto &entry : staged.winners) {
        LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        const uint8_t number_id = winner.parsed.definition->number_id();
        const uint8_t mask = winner.parsed.definition->mask();
        WaterAccessType *definition = winner.parsed.definition.get();
        staged.definitions.push_back(std::move(winner.parsed.definition));
        staged.by_number[number_id] = definition;
        staged.defined_mask |= mask;
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
            {"WaterAccessType"},
            "WaterAccessType",
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
    return materialize_winners(staged, failure_reason);
}

} // namespace

WaterAccessType::WaterAccessType(std::string text_id, uint8_t number_id)
    : text_id_(std::move(text_id))
    , number_id_(number_id)
    , mask_(static_cast<uint8_t>(1u << number_id))
{
}

const char *WaterAccessType::text_id() const
{
    return text_id_.c_str();
}

uint8_t WaterAccessType::number_id() const
{
    return number_id_;
}

uint8_t WaterAccessType::mask() const
{
    return mask_;
}

const WaterAccessType *find_water_access_type(const char *text_id)
{
    const std::string normalized = xml_definition::normalize_path(text_id);
    if (normalized.empty()) {
        return nullptr;
    }
    for (const std::unique_ptr<WaterAccessType> &definition : g_water_access_types) {
        if (definition && normalized == definition->text_id()) {
            return definition.get();
        }
    }
    return nullptr;
}

const WaterAccessType *water_access_type_from_number_id(uint8_t number_id)
{
    return number_id < kMaxWaterAccessTypes ? g_water_access_by_number[number_id] : nullptr;
}

uint8_t water_access_mask_from_text(const char *text_id)
{
    const WaterAccessType *definition = find_water_access_type(text_id);
    return definition ? definition->mask() : 0;
}

uint8_t water_access_defined_mask(void)
{
    return g_defined_mask;
}

const std::vector<std::unique_ptr<WaterAccessType>> &water_access_types(void)
{
    return g_water_access_types;
}

} // namespace building_type_registry_impl

int water_access_type_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure WaterAccessType definition layers", failure_reason.c_str(), 0);
        building_type_registry_impl::g_failure_reason = failure_reason;
        return 0;
    }
    return water_access_type_registry_load_layers(layers, &failure_reason);
}

int water_access_type_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        g_failure_reason = failure_reason ? *failure_reason : "Unable to load layered WaterAccessType definitions.";
        return 0;
    }
    g_water_access_types = std::move(staged.definitions);
    g_water_access_by_number = staged.by_number;
    g_defined_mask = staged.defined_mask;
    g_water_access_overlays = std::move(staged.overlay);
    g_failure_reason.clear();
    return 1;
}

const char *water_access_type_registry_get_failure_reason(void)
{
    return building_type_registry_impl::g_failure_reason.c_str();
}

const char *water_access_type_definition_source_path(const char *text_id)
{
    const std::string normalized = xml_definition::normalize_path(text_id);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_water_access_overlays.find(normalized);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int water_access_type_definition_is_suppressed(const char *text_id)
{
    const std::string normalized = xml_definition::normalize_path(text_id);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_water_access_overlays.find(normalized);
    return entry && entry->disabled;
}

#ifdef STARTUP_PARSER_TEST
namespace {

void fill_layer_test_result(
    const building_type_registry_impl::StagedRegistry &staged,
    const char *query_path,
    water_access_type_layer_test_result *result)
{
    if (!result) {
        return;
    }
    *result = {};
    result->active_count = static_cast<int>(staged.overlay.active_count());
    result->suppressed_count = static_cast<int>(staged.overlay.suppressed_count());
    result->queried_source_layer = -1;
    result->queried_number_id = -1;
    result->defined_mask = staged.defined_mask;

    const std::string stable_id = xml_definition::normalize_path(query_path);
    const mod_definition::DefinitionOverlayEntry *overlay = staged.overlay.find(stable_id);
    if (overlay) {
        result->queried_disabled = overlay->disabled ? 1 : 0;
        result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
    }
    const auto winner = staged.winners.find(stable_id);
    if (winner != staged.winners.end() && winner->second.parsed.definition) {
        result->queried_number_id = winner->second.parsed.definition->number_id();
    } else {
        const auto identity = staged.identities.find(stable_id);
        if (identity != staged.identities.end()) {
            result->queried_number_id = identity->second.number_id;
        }
    }
}

} // namespace

int water_access_type_layered_definition_buffers_are_valid_for_test(
    const water_access_type_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    water_access_type_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedRegistry staged;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const water_access_type_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        mod_definition::DefinitionSource source;
        source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.category = "WaterAccessType";
        source.full_path = input.source_path ? input.source_path : "WaterAccessTypeLayerTest.xml";
        source.file_name = source.full_path;
        source.normalized_definition_path = xml_definition::normalize_path(input.definition_path);
        source.registry_relative_path = "WaterAccessType\\" + source.normalized_definition_path;

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
    if (!materialize_winners(staged, &failure_reason)) {
        return 0;
    }
    fill_layer_test_result(staged, query_path, result);
    return 1;
}

int water_access_type_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    water_access_type_layer_test_result *result,
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

const char *water_access_type_text_from_number_id(uint8_t number_id)
{
    const building_type_registry_impl::WaterAccessType *definition =
        building_type_registry_impl::water_access_type_from_number_id(number_id);
    return definition ? definition->text_id() : nullptr;
}

uint8_t water_access_type_mask_from_text_id(const char *text_id)
{
    return building_type_registry_impl::water_access_mask_from_text(text_id);
}

uint8_t water_access_type_defined_mask_c(void)
{
    return building_type_registry_impl::water_access_defined_mask();
}
