#include "building/distribution.h"

#include "building/storage_type_registry.h"
#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {
namespace {

struct PendingDistributionRule {
    std::string reference;
    int priority = 0;
    int baseline_stock = 0;
    int max_stock = 0;
    bool storage = false;
};

struct ParseState {
    std::unique_ptr<Distribution> definition;
    std::vector<PendingDistributionRule> rules;
    bool disabled = false;
    bool saw_root = false;
    bool saw_rule = false;
    bool error = false;
};

struct StagedDistribution {
    std::unique_ptr<Distribution> definition;
    std::vector<PendingDistributionRule> rules;
    mod_definition::DefinitionSource source;
    bool disabled = false;
};

struct StagedDistributions {
    std::unordered_map<std::string, StagedDistribution> winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

std::unordered_map<std::string, std::unique_ptr<Distribution>> g_distributions;
mod_definition::DefinitionOverlayTracker g_distribution_overlays;
std::string g_failure_reason;
ParseState g_parse_state;

int parse_root()
{
    if (!g_parse_state.definition || g_parse_state.saw_root) {
        log_error("Distribution has an invalid or duplicate root", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("Distribution has invalid Boolean attribute 'disabled'", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.disabled = disabled != 0;
    g_parse_state.saw_root = true;
    return 1;
}

int parse_takes()
{
    if (!g_parse_state.definition || !g_parse_state.saw_root) {
        log_error("Encountered Distribution takes before root", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.disabled) {
        log_error("Disabled Distribution tombstone contains definition data", "takes", 0);
        g_parse_state.error = true;
        return 0;
    }

    const bool has_resource = xml_parser_has_attribute("resource") != 0;
    const bool has_storage = xml_parser_has_attribute("storage") != 0;
    if (has_resource == has_storage) {
        log_error("Distribution takes must reference exactly one resource or storage", g_parse_state.definition->path(), 0);
        g_parse_state.error = true;
        return 0;
    }

    PendingDistributionRule rule;
    if (!xml_definition::parse_optional_nonnegative_int_attribute("priority", &rule.priority) ||
        !xml_definition::parse_optional_nonnegative_int_attribute("baseline_stock", &rule.baseline_stock) ||
        !xml_definition::parse_optional_nonnegative_int_attribute("max_stock", &rule.max_stock)) {
        log_error("Distribution rule has invalid non-negative integer attribute", g_parse_state.definition->path(), 0);
        g_parse_state.error = true;
        return 0;
    }

    rule.storage = has_storage;
    const char *reference = xml_parser_get_attribute_string(has_storage ? "storage" : "resource");
    rule.reference = xml_value::trim_copy(reference ? reference : "");
    if (rule.storage) {
        rule.reference = xml_definition::normalize_path(rule.reference.c_str());
    }
    if (rule.reference.empty()) {
        log_error("Distribution takes reference must not be empty", g_parse_state.definition->path(), 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.rules.push_back(std::move(rule));
    g_parse_state.saw_rule = true;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "distribution", parse_root, nullptr, nullptr, nullptr },
    { "takes", parse_takes, nullptr, "distribution", nullptr }
};

int parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    StagedDistribution &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("distribution_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<Distribution>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_buffer(
        filename,
        "Distribution",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_root ||
        (!g_parse_state.disabled && !g_parse_state.saw_rule)) {
        const std::string detail = std::string("file=") + (filename ? filename : "") +
            " path=" + (definition_path ? definition_path : "");
        log_error("Unable to parse Distribution xml", detail.c_str(), 0);
        error_context_report_error("Unable to parse Distribution xml.", detail.c_str());
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason("Unable to parse Distribution xml.", detail.c_str());
        }
        return 0;
    }

    result.definition = std::move(g_parse_state.definition);
    result.rules = std::move(g_parse_state.rules);
    result.disabled = g_parse_state.disabled;
    return 1;
}

int parse_definition_file(
    const mod_definition::DefinitionSource &source,
    StagedDistribution &result,
    std::string *failure_reason)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "Distribution")) {
        if (failure_reason) {
            *failure_reason = "Unable to load Distribution xml: " + source.full_path;
        }
        return 0;
    }
    return parse_definition_buffer(
        source.full_path.c_str(), source.normalized_definition_path.c_str(), buffer, result, failure_reason);
}

int stage_definition(StagedDistributions &staged, StagedDistribution definition)
{
    const std::string key = definition.definition ? definition.definition->path() : "";
    if (key.empty()) {
        staged.failure_reason = "Distribution definition path must not be empty.";
        return 0;
    }
    if (!staged.overlays.apply(key, definition.disabled, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        log_error("Unable to layer Distribution definition", staged.failure_reason.c_str(), 0);
        return 0;
    }
    staged.winners.insert_or_assign(key, std::move(definition));
    return 1;
}

int fail_reference(
    StagedDistributions &staged,
    const std::string &path,
    const StagedDistribution &winner,
    const PendingDistributionRule &rule)
{
    staged.failure_reason = "Distribution '" + path + "' from " + winner.source.describe() +
        " has an unknown " + (rule.storage ? "storage" : "resource") + " reference '" + rule.reference + "'.";
    log_error("Unable to resolve Distribution reference", staged.failure_reason.c_str(), 0);
    return 0;
}

int resolve_winners(StagedDistributions &staged)
{
    for (auto &entry : staged.winners) {
        StagedDistribution &winner = entry.second;
        if (winner.disabled) {
            continue;
        }
        for (const PendingDistributionRule &rule : winner.rules) {
            if (rule.storage) {
                const StorageType *storage = find_storage_type_definition(rule.reference.c_str());
                if (!storage) {
                    return fail_reference(staged, entry.first, winner, rule);
                }
                winner.definition->add_storage_type(storage, rule.priority, rule.baseline_stock, rule.max_stock);
            } else {
                const resource_type resource = resource_type_from_xml_attr(rule.reference.c_str());
                if (resource == RESOURCE_NONE) {
                    return fail_reference(staged, entry.first, winner, rule);
                }
                winner.definition->add_resource({resource, rule.priority, rule.baseline_stock, rule.max_stock});
            }
        }
        if (winner.definition->resources().empty()) {
            staged.failure_reason = "Distribution '" + entry.first + "' from " + winner.source.describe() +
                " does not resolve to any resources.";
            log_error("Distribution has no effective resources", staged.failure_reason.c_str(), 0);
            return 0;
        }
    }
    return 1;
}

int build_layered_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedDistributions &staged,
    std::string *failure_reason)
{
    if (!mod_definition::for_each_definition_file(
            layers,
            {"Distribution"},
            "Distribution",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedDistribution definition;
                definition.source = source;
                if (!parse_definition_file(source, definition, failure_reason)) {
                    return false;
                }
                if (!stage_definition(staged, std::move(definition))) {
                    if (failure_reason) {
                        *failure_reason = staged.failure_reason;
                    }
                    return false;
                }
                return true;
            },
            nullptr,
            failure_reason)) {
        if (failure_reason && failure_reason->empty()) {
            *failure_reason = staged.failure_reason;
        }
        return 0;
    }
    if (!resolve_winners(staged)) {
        if (failure_reason) {
            *failure_reason = staged.failure_reason;
        }
        return 0;
    }
    return 1;
}

#ifdef STARTUP_PARSER_TEST
void fill_test_result(
    const StagedDistributions &staged,
    const char *query_path,
    distribution_layer_test_result *result)
{
    if (!result) {
        return;
    }
    *result = {};
    result->active_count = static_cast<int>(staged.overlays.active_count());
    result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
    result->queried_source_layer = -1;

    const std::string query = xml_definition::normalize_path(query_path);
    const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(query);
    if (overlay) {
        result->queried_disabled = overlay->disabled ? 1 : 0;
        result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
    }
    const auto winner = staged.winners.find(query);
    if (winner != staged.winners.end() && !winner->second.disabled && winner->second.definition) {
        result->queried_resource_count = static_cast<int>(winner->second.definition->resources().size());
        result->queried_wheat_priority = winner->second.definition->priority_for(resource_wheat());
    }
}
#endif

} // namespace

const Distribution *find_distribution_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_distributions.find(normalized);
    return found != g_distributions.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

const char *distribution_registry_get_failure_reason(void)
{
    return building_type_registry_impl::g_failure_reason.c_str();
}

const char *distribution_definition_source_path(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_distribution_overlays.find(normalized);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int distribution_definition_is_suppressed(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_distribution_overlays.find(normalized);
    return entry && entry->disabled;
}

int distribution_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedDistributions staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        g_failure_reason = failure_reason ? *failure_reason : staged.failure_reason;
        return 0;
    }

    std::unordered_map<std::string, std::unique_ptr<Distribution>> published;
    for (auto &entry : staged.winners) {
        if (!entry.second.disabled) {
            published.emplace(entry.first, std::move(entry.second.definition));
        }
    }
    g_distributions = std::move(published);
    g_distribution_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

int distribution_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason) ||
        !distribution_registry_load_layers(layers, &failure_reason)) {
        building_type_registry_impl::g_failure_reason = failure_reason;
        error_context_report_error("Unable to load layered Distribution definitions.", failure_reason.c_str());
        return 0;
    }
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int distribution_layered_definition_buffers_are_valid_for_test(
    const distribution_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    distribution_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedDistributions staged;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const distribution_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedDistribution definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.category = "Distribution";
        definition.source.full_path = input.source_path ? input.source_path : "DistributionLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.normalized_definition_path = xml_definition::normalize_path(input.definition_path);
        definition.source.registry_relative_path = "Distribution\\" + definition.source.normalized_definition_path;
        if (!parse_definition_buffer(
                definition.source.full_path.c_str(),
                definition.source.normalized_definition_path.c_str(),
                buffer,
                definition,
                &failure_reason) ||
            !stage_definition(staged, std::move(definition))) {
            return 0;
        }
    }
    if (!resolve_winners(staged)) {
        return 0;
    }
    fill_test_result(staged, query_path, result);
    return 1;
}

int distribution_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    distribution_layer_test_result *result,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedDistributions staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        return 0;
    }
    fill_test_result(staged, query_path, result);
    return 1;
}
#endif
