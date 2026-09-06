#

#include "building/production_method_registry.h"

#include "core/crash_context.h"
#include "game/mod_definition_loader.h"

#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"

#include <cstdio>
#include <cstring>
#include <climits>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

mod_definition::DefinitionOverlayTracker g_production_method_overlays;

struct PendingProductionInput {
    std::string resource;
    int amount = 0;
};

struct ParseState {
    std::unique_ptr<ProductionMethod> definition;
    std::string output_resource;
    std::vector<PendingProductionInput> inputs;
    bool disabled = false;
    int saw_root = 0;
    int saw_kind = 0;
    int saw_output = 0;
    int saw_batch_size = 0;
    int saw_cart_loads = 0;
    int saw_cart_capacity = 0;
    int saw_treasury_cost = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<ProductionMethod>> g_production_methods;
ParseState g_parse_state;

int reject_tombstone_content(const char *node)
{
    if (!g_parse_state.disabled) {
        return 0;
    }
    log_error("Disabled ProductionMethod tombstone contains definition data", node, 0);
    g_parse_state.error = 1;
    return 1;
}

ProductionOutputEffect parse_output_effect_name(const char *name)
{
    if (name && strcmp(name, "spawn_fishing_boat") == 0) {
        return ProductionOutputEffect::SpawnFishingBoat;
    }
    return ProductionOutputEffect::None;
}

int parse_output_destination_name(const char *name, ProductionOutputDestination *out_destination)
{
    if (!name || !out_destination) {
        return 0;
    }
    if (strcmp(name, "building_storage") == 0) {
        *out_destination = ProductionOutputDestination::BuildingStorage;
        return 1;
    }
    if (strcmp(name, "treasury") == 0) {
        *out_destination = ProductionOutputDestination::Treasury;
        return 1;
    }
    return 0;
}

int parse_scenario_climate_name(const char *name, scenario_climate *out_climate)
{
    if (!name || !*name || !out_climate) {
        return 0;
    }

    const std::string normalized_name = xml_value::trim_copy(name);
    if (normalized_name == "central") {
        *out_climate = CLIMATE_CENTRAL;
        return 1;
    }
    if (normalized_name == "northern") {
        *out_climate = CLIMATE_NORTHERN;
        return 1;
    }
    if (normalized_name == "desert") {
        *out_climate = CLIMATE_DESERT;
        return 1;
    }
    return 0;
}

int adjust_production_with_percent(int base_production, int percent_delta)
{
    return base_production + (base_production * percent_delta) / 100;
}

int validate_definition(const ProductionMethod &definition, const char *filename, const char *definition_path)
{
    if (definition.batch_size() <= 0) {
        log_error("ProductionMethod batch_size must be positive", definition.path(), 0);
        error_context_report_error("ProductionMethod batch_size must be positive.", filename);
        return 0;
    }
    if (definition.batch_size() > UCHAR_MAX) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s batch_size=%d", filename, definition_path ? definition_path : "",
            definition.batch_size());
        log_error("ProductionMethod batch_size exceeds current cart load field", definition.path(), 0);
        error_context_report_error("ProductionMethod batch_size exceeds current cart load field.", detail);
        return 0;
    }
    if (definition.cart_load_numerator() <= 0 || definition.cart_load_denominator() <= 0) {
        log_error("ProductionMethod cart_loads must be positive", definition.path(), 0);
        error_context_report_error("ProductionMethod cart_loads must be positive.", filename);
        return 0;
    }
    const int integer_cart_loads = definition.cart_loads_per_cycle();
    if (integer_cart_loads > UCHAR_MAX) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s cart_loads=%d/%d", filename, definition_path ? definition_path : "",
            definition.cart_load_numerator(), definition.cart_load_denominator());
        log_error("ProductionMethod cart_loads exceeds current cart load field", definition.path(), 0);
        error_context_report_error("ProductionMethod cart_loads exceeds current cart load field.", detail);
        return 0;
    }
    if (definition.cart_capacity() <= 0 || definition.cart_capacity() > UCHAR_MAX) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s cart_capacity=%d", filename,
            definition_path ? definition_path : "", definition.cart_capacity());
        log_error("ProductionMethod cart_capacity exceeds current cart load field", definition.path(), 0);
        error_context_report_error("ProductionMethod cart_capacity exceeds current cart load field.", detail);
        return 0;
    }
    if (definition.treasury_cost_per_cycle() < 0) {
        log_error("ProductionMethod treasury_cost must be non-negative", definition.path(), 0);
        error_context_report_error("ProductionMethod treasury_cost must be non-negative.", filename);
        return 0;
    }

    const int base_output_production = definition.base_monthly_production();
    if (base_output_production <= 0) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s output_resource=%d production_per_month=%d", filename,
            definition_path ? definition_path : "", definition.output_resource(), base_output_production);
        log_error("ProductionMethod output has invalid production_per_month", definition.path(), 0);
        error_context_report_error("ProductionMethod output has invalid production_per_month.", detail);
        return 0;
    }

    for (const ClimateProductionBonus &bonus : definition.climate_bonuses()) {
        const int adjusted_production = adjust_production_with_percent(base_output_production, bonus.percent_delta);
        if (adjusted_production <= 0) {
            char detail[512];
            snprintf(detail, sizeof(detail), "file=%s path=%s climate=%d percent=%d", filename,
                definition_path ? definition_path : "", bonus.climate, bonus.percent_delta);
            log_error("ProductionMethod climate bonus produces non-positive throughput", definition.path(), 0);
            error_context_report_error("ProductionMethod climate bonus produces non-positive throughput.", detail);
            return 0;
        }
    }

    return 1;
}

int parse_root()
{
    if (!g_parse_state.definition || g_parse_state.saw_root) {
        log_error("ProductionMethod has an invalid or duplicate root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("ProductionMethod has an invalid disabled value", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.disabled = disabled != 0;
    if (xml_parser_has_attribute("input_source")) {
        const char *source = xml_parser_get_attribute_string("input_source");
        if (strcmp(source, "global_stockpile") == 0) g_parse_state.definition->set_input_source(ResourceConsumptionSource::GlobalStockpile);
        else if (strcmp(source, "building") != 0) { log_error("Unknown ProductionMethod input source", source, 0); g_parse_state.error = 1; return 0; }
    }
    g_parse_state.saw_root = 1;
    return 1;
}

int parse_kind()
{
    if (reject_tombstone_content("kind")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod kind before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_kind) {
        log_error("ProductionMethod xml contains duplicate kind nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("ProductionMethod kind is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *kind_text = xml_parser_get_attribute_string("value");
    if (kind_text && strcmp(kind_text, "farm") == 0) {
        g_parse_state.definition->set_kind(ProductionMethodKind::Farm);
    } else if (kind_text && strcmp(kind_text, "workshop") == 0) {
        g_parse_state.definition->set_kind(ProductionMethodKind::Workshop);
    } else {
        log_error("Unsupported ProductionMethod kind", kind_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_kind = 1;
    return 1;
}

int parse_output()
{
    if (reject_tombstone_content("output")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod output before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_output) {
        log_error("ProductionMethod xml contains duplicate output nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    const int has_resource = xml_parser_has_attribute("resource");
    const int has_effect = xml_parser_has_attribute("effect");
    if (has_resource == has_effect) {
        log_error("ProductionMethod output requires exactly one of 'resource' or 'effect'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("production_per_month")) {
        log_error("ProductionMethod output is missing required attribute 'production_per_month'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (has_resource) {
        const char *resource_text = xml_parser_get_attribute_string("resource");
        g_parse_state.output_resource = xml_value::trim_copy(resource_text ? resource_text : "");
        if (g_parse_state.output_resource.empty()) {
            log_error("ProductionMethod output resource must not be empty", resource_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        if (xml_parser_has_attribute("destination")) {
            ProductionOutputDestination destination = ProductionOutputDestination::BuildingStorage;
            const char *destination_text = xml_parser_get_attribute_string("destination");
            if (!parse_output_destination_name(destination_text, &destination)) {
                log_error("Unsupported ProductionMethod output destination", destination_text, 0);
                g_parse_state.error = 1;
                return 0;
            }
            g_parse_state.definition->set_output_destination(destination);
        }
        if (xml_parser_has_attribute("source")) {
            const char *source_text = xml_parser_get_attribute_string("source");
            if (strcmp(source_text, "worker_progress") != 0 && strcmp(source_text, "figure_delivery") != 0) {
                log_error("Unsupported ProductionMethod output source", source_text, 0);
                g_parse_state.error = 1;
                return 0;
            }
            if (strcmp(source_text, "figure_delivery") == 0) {
                g_parse_state.definition->set_output_source(ProductionOutputSource::FigureDelivery);
            }
        }
    } else {
        if (xml_parser_has_attribute("source")) {
            log_error("ProductionMethod effect output cannot declare source", xml_parser_get_attribute_string("source"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        if (xml_parser_has_attribute("destination")) {
            log_error("ProductionMethod effect output cannot declare destination", xml_parser_get_attribute_string("destination"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        const char *effect_text = xml_parser_get_attribute_string("effect");
        const ProductionOutputEffect effect = parse_output_effect_name(effect_text);
        if (effect == ProductionOutputEffect::None) {
            log_error("Unsupported ProductionMethod output effect", effect_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_output_effect(effect);
    }

    g_parse_state.definition->set_base_monthly_production(xml_parser_get_attribute_int("production_per_month"));
    g_parse_state.saw_output = 1;
    return 1;
}

int parse_batch_size()
{
    if (reject_tombstone_content("batch_size")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod batch_size before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_batch_size) {
        log_error("ProductionMethod xml contains duplicate batch_size nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("ProductionMethod batch_size is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int batch_size = xml_parser_get_attribute_int("value");
    if (batch_size <= 0) {
        log_error("Unsupported ProductionMethod batch_size", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_batch_size(batch_size);
    g_parse_state.saw_batch_size = 1;
    return 1;
}

int parse_cart_loads()
{
    if (reject_tombstone_content("cart_loads")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod cart_loads before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_cart_loads) {
        log_error("ProductionMethod xml contains duplicate cart_loads nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int numerator = 0;
    int denominator = 1;
    const int has_value = xml_parser_has_attribute("value");
    const int has_numerator = xml_parser_has_attribute("numerator");
    const int has_denominator = xml_parser_has_attribute("denominator");
    if (has_value) {
        if (has_numerator || has_denominator) {
            log_error("ProductionMethod cart_loads cannot mix value with numerator/denominator", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        numerator = xml_parser_get_attribute_int("value");
    } else {
        if (!has_numerator || !has_denominator) {
            log_error("ProductionMethod cart_loads is missing required attributes", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        numerator = xml_parser_get_attribute_int("numerator");
        denominator = xml_parser_get_attribute_int("denominator");
    }
    if (numerator <= 0 || denominator <= 0) {
        log_error("Unsupported ProductionMethod cart_loads", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_cart_loads(numerator, denominator);
    g_parse_state.saw_cart_loads = 1;
    return 1;
}

int parse_cart_capacity()
{
    if (reject_tombstone_content("cart_capacity")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod cart_capacity before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_cart_capacity) {
        log_error("ProductionMethod xml contains duplicate cart_capacity nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("ProductionMethod cart_capacity is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    const int value = xml_parser_get_attribute_int("value");
    if (value <= 0) {
        log_error("Unsupported ProductionMethod cart_capacity", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_cart_capacity(value);
    g_parse_state.saw_cart_capacity = 1;
    return 1;
}

int parse_treasury_cost()
{
    if (reject_tombstone_content("treasury_cost")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod treasury_cost before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_treasury_cost) {
        log_error("ProductionMethod xml contains duplicate treasury_cost nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("amount")) {
        log_error("ProductionMethod treasury_cost is missing required attribute 'amount'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int cost = xml_parser_get_attribute_int("amount");
    if (cost <= 0) {
        log_error("Unsupported ProductionMethod treasury_cost amount", xml_parser_get_attribute_string("amount"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_treasury_cost_per_cycle(cost);
    g_parse_state.saw_treasury_cost = 1;
    return 1;
}

int parse_climate_bonuses()
{
    if (reject_tombstone_content("climate_bonuses")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod climate_bonuses before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    return 1;
}

int parse_climate_bonus()
{
    if (reject_tombstone_content("bonus")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod climate bonus before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("climate")) {
        log_error("ProductionMethod climate bonus is missing required attribute 'climate'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("percent")) {
        log_error("ProductionMethod climate bonus is missing required attribute 'percent'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    scenario_climate climate = CLIMATE_CENTRAL;
    const char *climate_text = xml_parser_get_attribute_string("climate");
    if (!parse_scenario_climate_name(climate_text, &climate)) {
        log_error("Unsupported ProductionMethod climate bonus climate", climate_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    ClimateProductionBonus bonus;
    bonus.climate = climate;
    bonus.percent_delta = xml_parser_get_attribute_int("percent");
    if (!g_parse_state.definition->add_climate_bonus(bonus)) {
        log_error("ProductionMethod xml contains duplicate climate bonus entries", climate_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    return 1;
}

int parse_input()
{
    if (reject_tombstone_content("input")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod input before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("resource")) {
        log_error("ProductionMethod input is missing required attribute 'resource'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("amount")) {
        log_error("ProductionMethod input is missing required attribute 'amount'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *resource_text = xml_parser_get_attribute_string("resource");
    const std::string resource = xml_value::trim_copy(resource_text ? resource_text : "");
    if (resource.empty()) {
        log_error("ProductionMethod input resource must not be empty", resource_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int amount = xml_parser_get_attribute_int("amount");
    if (amount <= 0) {
        log_error("Unsupported ProductionMethod input amount", xml_parser_get_attribute_string("amount"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.inputs.push_back({resource, amount});
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "production_method", parse_root, nullptr, nullptr, nullptr },
    { "kind", parse_kind, nullptr, "production_method", nullptr },
    { "output", parse_output, nullptr, "production_method", nullptr },
    { "batch_size", parse_batch_size, nullptr, "production_method", nullptr },
    { "cart_loads", parse_cart_loads, nullptr, "production_method", nullptr },
    { "cart_capacity", parse_cart_capacity, nullptr, "production_method", nullptr },
    { "treasury_cost", parse_treasury_cost, nullptr, "production_method", nullptr },
    { "climate_bonuses", parse_climate_bonuses, nullptr, "production_method", nullptr },
    { "bonus", parse_climate_bonus, nullptr, "climate_bonuses", nullptr },
    { "input", parse_input, nullptr, "production_method", nullptr }
};

struct ParsedDefinition {
    std::unique_ptr<ProductionMethod> definition;
    std::string output_resource;
    std::vector<PendingProductionInput> inputs;
    bool disabled = false;
};

int parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("production_method_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<ProductionMethod>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_buffer(
        filename,
        "ProductionMethod",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    const bool complete_definition = g_parse_state.saw_kind && g_parse_state.saw_output;
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || !g_parse_state.definition ||
        (!g_parse_state.disabled && !complete_definition)) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, definition_path ? definition_path : "");
        error_context_report_error("Unable to parse ProductionMethod xml.", detail);
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to parse ProductionMethod xml.", detail);
        }
        return 0;
    }

    result.definition = std::move(g_parse_state.definition);
    result.output_resource = std::move(g_parse_state.output_resource);
    result.inputs = std::move(g_parse_state.inputs);
    result.disabled = g_parse_state.disabled;
    return 1;
}

int parse_definition_file(
    const char *filename,
    const char *definition_path,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(filename, buffer, "ProductionMethod")) {
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to load ProductionMethod xml.", filename);
        }
        return 0;
    }
    return parse_definition_buffer(filename, definition_path, buffer, result, failure_reason);
}

struct LayeredDefinition {
    ParsedDefinition parsed;
    mod_definition::DefinitionSource source;
};

struct StagedRegistry {
    mod_definition::DefinitionOverlayTracker overlay;
    std::map<std::string, LayeredDefinition> winners;
    std::unordered_map<std::string, std::unique_ptr<ProductionMethod>> definitions;
};

bool stage_definition(
    StagedRegistry &staged,
    ParsedDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->path() : "";
    if (!staged.overlay.apply(stable_id, parsed.disabled, source)) {
        log_error("Unable to layer ProductionMethod definition", staged.overlay.failure_reason().c_str(), 0);
        error_context_report_error(
            "Unable to layer ProductionMethod definition.", staged.overlay.failure_reason().c_str());
        if (failure_reason) {
            *failure_reason = staged.overlay.failure_reason();
        }
        return false;
    }
    staged.winners[stable_id] = {std::move(parsed), source};
    return true;
}

bool fail_reference(
    const char *kind,
    const std::string &name,
    const LayeredDefinition &winner,
    std::string *failure_reason)
{
    const std::string detail = "ProductionMethod '" + std::string(winner.parsed.definition->path()) +
        "' from " + winner.source.describe() + " has an unknown " + kind + " resource '" + name + "'.";
    log_error("Unsupported ProductionMethod resource reference", detail.c_str(), 0);
    error_context_report_error("Unsupported ProductionMethod resource reference.", detail.c_str());
    if (failure_reason) {
        *failure_reason = detail;
    }
    return false;
}

bool resolve_winners(StagedRegistry &staged, std::string *failure_reason)
{
    for (auto &entry : staged.winners) {
        LayeredDefinition &winner = entry.second;
        ParsedDefinition &parsed = winner.parsed;
        if (parsed.disabled) {
            continue;
        }

        if (!parsed.output_resource.empty()) {
            const resource_type resource = resource_type_from_xml_attr(parsed.output_resource.c_str());
            if (resource == RESOURCE_NONE) {
                return fail_reference("output", parsed.output_resource, winner, failure_reason);
            }
            parsed.definition->set_output_resource(resource);
        }
        for (const PendingProductionInput &input : parsed.inputs) {
            const resource_type resource = resource_type_from_xml_attr(input.resource.c_str());
            if (resource == RESOURCE_NONE) {
                return fail_reference("input", input.resource, winner, failure_reason);
            }
            parsed.definition->add_input({resource, input.amount});
        }

        if (!validate_definition(
                *parsed.definition,
                winner.source.full_path.c_str(),
                winner.source.normalized_definition_path.c_str())) {
            if (failure_reason) {
                *failure_reason = "Invalid ProductionMethod winner: " + winner.source.describe();
            }
            return false;
        }
        staged.definitions.emplace(entry.first, std::move(parsed.definition));
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
            {"ProductionMethod"},
            "ProductionMethod",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                ParsedDefinition parsed;
                return parse_definition_file(
                           source.full_path.c_str(),
                           source.normalized_definition_path.c_str(),
                           parsed,
                           failure_reason) &&
                    stage_definition(staged, std::move(parsed), source, failure_reason);
            },
            nullptr,
            failure_reason)) {
        return false;
    }
    return resolve_winners(staged, failure_reason);
}

} // namespace

ProductionMethod *find_production_method_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_production_methods.find(normalized);
    return found != g_production_methods.end() ? found->second.get() : nullptr;
}

int production_per_month_for_resource(resource_type resource)
{
    for (const auto &entry : g_production_methods) {
        const ProductionMethod *method = entry.second.get();
        if (method && method->output_resource() == resource) {
            return method->base_monthly_production();
        }
    }
    return 0;
}

int default_production_per_month_for_resource(resource_type resource)
{
    for (const auto &entry : g_production_methods) {
        const ProductionMethod *method = entry.second.get();
        if (method && method->output_resource() == resource) {
            return method->default_base_monthly_production();
        }
    }
    return 0;
}

int set_production_per_month_for_resource(resource_type resource, int production)
{
    int changed = 0;
    for (const auto &entry : g_production_methods) {
        ProductionMethod *method = entry.second.get();
        if (method && method->output_resource() == resource) {
            method->override_base_monthly_production(production);
            changed = 1;
        }
    }
    return changed;
}

int adjust_production_per_month_for_resource(resource_type resource, int delta)
{
    int changed = 0;
    for (const auto &entry : g_production_methods) {
        ProductionMethod *method = entry.second.get();
        if (method && method->output_resource() == resource) {
            method->override_base_monthly_production(static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(method->base_monthly_production()) + delta, 0, INT_MAX)));
            changed = 1;
        }
    }
    return changed;
}

void reset_production_overrides()
{
    for (const auto &entry : g_production_methods) {
        if (entry.second) {
            entry.second->reset_base_monthly_production_override();
        }
    }
}

} // namespace building_type_registry_impl

int production_method_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure ProductionMethod definition layers", failure_reason.c_str(), 0);
        return 0;
    }
    return production_method_registry_load_layers(layers, &failure_reason);
}

int production_method_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        return 0;
    }
    g_production_methods = std::move(staged.definitions);
    g_production_method_overlays = std::move(staged.overlay);
    return 1;
}

const char *production_method_definition_source_path(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_production_method_overlays.find(normalized);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int production_method_definition_is_suppressed(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_production_method_overlays.find(normalized);
    return entry && entry->disabled;
}

#ifdef STARTUP_PARSER_TEST
namespace {

void fill_layer_test_result(
    const building_type_registry_impl::StagedRegistry &staged,
    const char *query_path,
    production_method_layer_test_result *result)
{
    if (!result) {
        return;
    }
    *result = {};
    result->active_count = static_cast<int>(staged.overlay.active_count());
    result->suppressed_count = static_cast<int>(staged.overlay.suppressed_count());
    result->queried_source_layer = -1;

    const std::string path = xml_definition::normalize_path(query_path);
    const mod_definition::DefinitionOverlayEntry *overlay = staged.overlay.find(path);
    if (overlay) {
        result->queried_disabled = overlay->disabled ? 1 : 0;
        result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
    }
    const auto found = staged.definitions.find(path);
    if (found != staged.definitions.end() && found->second) {
        result->queried_production_per_month = found->second->base_monthly_production();
        result->queried_input_count = static_cast<int>(found->second->inputs().size());
    }
}

} // namespace

int production_method_layered_definition_buffers_are_valid_for_test(
    const production_method_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    production_method_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedRegistry staged;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const production_method_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        mod_definition::DefinitionSource source;
        source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.category = "ProductionMethod";
        source.full_path = input.source_path ? input.source_path : "ProductionMethodLayerTest.xml";
        source.file_name = source.full_path;
        source.normalized_definition_path = xml_definition::normalize_path(input.definition_path);
        source.registry_relative_path = "ProductionMethod\\" + source.normalized_definition_path;

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

int production_method_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    production_method_layer_test_result *result,
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

int production_method_registry_production_per_month_for_resource(resource_type resource)
{
    return building_type_registry_impl::production_per_month_for_resource(resource);
}

int production_method_registry_default_production_per_month_for_resource(resource_type resource)
{
    return building_type_registry_impl::default_production_per_month_for_resource(resource);
}

int production_method_registry_set_production_per_month_for_resource(resource_type resource, int production)
{
    return building_type_registry_impl::set_production_per_month_for_resource(resource, production);
}

int production_method_registry_adjust_production_per_month_for_resource(resource_type resource, int delta)
{
    return building_type_registry_impl::adjust_production_per_month_for_resource(resource, delta);
}

void production_method_registry_reset_production_overrides(void)
{
    building_type_registry_impl::reset_production_overrides();
}

void production_method_registry_save_overrides(buffer *buf)
{
    using namespace building_type_registry_impl;
    std::map<std::string, int> overrides;
    size_t bytes = 8;
    for (const auto &entry : g_production_methods) {
        if (!entry.second || !entry.second->has_production_override()) continue;
        overrides.emplace(entry.first, entry.second->base_monthly_production());
        bytes += 8 + entry.first.size();
    }
    buffer_init_dynamic(buf, bytes);
    buffer_write_u32(buf, 0x31504d56); // VMP1
    buffer_write_u32(buf, static_cast<uint32_t>(overrides.size()));
    for (const auto &entry : overrides) {
        buffer_write_u32(buf, static_cast<uint32_t>(entry.first.size()));
        buffer_write_raw(buf, entry.first.data(), entry.first.size());
        buffer_write_i32(buf, entry.second);
    }
}

int production_method_registry_load_overrides(buffer *buf)
{
    using namespace building_type_registry_impl;
    if (!buf || buf->size < 12) return 0;
    buffer source = *buf;
    buffer_reset(&source);
    if (buffer_read_u32(&source) != source.size || buffer_read_u32(&source) != 0x31504d56) return 0;
    const uint32_t count = buffer_read_u32(&source);
    if (count > (source.size - source.index) / 9) return 0;
    std::map<std::string, int> overrides;
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t size = buffer_read_u32(&source);
        if (!size || size > 4096 || size > source.size - source.index || source.size - source.index - size < 4) return 0;
        std::string path(size, '\0'); buffer_read_raw(&source, path.data(), size);
        const int value = buffer_read_i32(&source);
        if (value < 0 || path.find('\0') != std::string::npos || !overrides.emplace(path, value).second) return 0;
    }
    if (source.overflow || source.index != source.size) return 0;
    reset_production_overrides();
    for (const auto &entry : overrides) {
        auto *method = find_production_method_definition(entry.first.c_str());
        if (method) method->override_base_monthly_production(entry.second);
        else log_warning("Scenario production override references an unavailable method", entry.first.c_str(), 0);
    }
    return 1;
}

int production_method_registry_supply_chain_for_good(
    resource_supply_chain *chain,
    resource_type good,
    int max_entries)
{
    using namespace building_type_registry_impl;

    int count = 0;
    for (const auto &entry : g_production_methods) {
        const ProductionMethod *method = entry.second.get();
        if (!method || method->output_resource() != good) {
            continue;
        }
        for (const ProductionResourceAmount &input : method->inputs()) {
            if (!resource_is_declared(input.resource)) {
                continue;
            }
            if (chain) {
                if (count >= max_entries) {
                    continue;
                }
                chain[count] = { input.amount, input.resource, good };
            }
            count++;
        }
    }
    return count;
}

int production_method_registry_supply_chain_for_raw_material(
    resource_supply_chain *chain,
    resource_type raw_material,
    int max_entries)
{
    using namespace building_type_registry_impl;

    int count = 0;
    for (const auto &entry : g_production_methods) {
        const ProductionMethod *method = entry.second.get();
        if (!method || !resource_is_declared(method->output_resource())) {
            continue;
        }
        for (const ProductionResourceAmount &input : method->inputs()) {
            if (input.resource != raw_material) {
                continue;
            }
            if (chain) {
                if (count >= max_entries) {
                    continue;
                }
                chain[count] = { input.amount, raw_material, method->output_resource() };
            }
            count++;
        }
    }
    return count;
}
