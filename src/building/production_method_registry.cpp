#

#include "building/building_record.h"
#include "building/production_method_registry.h"

#include "core/crash_context.h"
#include "game/mod_manager.h"

#include "core/file.h"
#include "core/dir.h"
#include "core/log.h"
#include "core/xml_parser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_production_method_path;

namespace {

struct ParseState {
    std::unique_ptr<ProductionMethod> definition;
    int saw_kind = 0;
    int saw_output = 0;
    int saw_batch_size = 0;
    int saw_cart_loads = 0;
    int saw_treasury_cost = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<ProductionMethod>> g_production_methods;
ParseState g_parse_state;

std::string trim_copy(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        start++;
    }

    size_t end = value.size();
    while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        end--;
    }
    return value.substr(start, end - start);
}

int equals_ignore_case_ascii(char left, char right)
{
    if (left >= 'A' && left <= 'Z') {
        left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = static_cast<char>(right - 'A' + 'a');
    }
    return left == right;
}

int ends_with_ignore_case_ascii(const std::string &value, const char *suffix)
{
    if (!suffix) {
        return 0;
    }

    const size_t suffix_length = strlen(suffix);
    if (value.size() < suffix_length) {
        return 0;
    }

    const size_t start = value.size() - suffix_length;
    for (size_t i = 0; i < suffix_length; i++) {
        if (!equals_ignore_case_ascii(value[start + i], suffix[i])) {
            return 0;
        }
    }
    return 1;
}

std::string normalize_definition_path(const char *value)
{
    std::string normalized = trim_copy(value ? value : "");
    if (normalized.empty()) {
        return std::string();
    }

    for (char &ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }

    std::string collapsed;
    collapsed.reserve(normalized.size());
    char previous = '\0';
    for (char ch : normalized) {
        if (ch == '\\' && previous == '\\') {
            continue;
        }
        collapsed.push_back(ch);
        previous = ch;
    }
    normalized = collapsed;

    if (!normalized.empty() && normalized.front() == '\\') {
        return std::string();
    }
    if (!normalized.empty() && normalized.back() == '\\') {
        return std::string();
    }
    if (ends_with_ignore_case_ascii(normalized, ".xml")) {
        normalized.resize(normalized.size() - 4);
    }
    return normalized;
}

int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open ProductionMethod xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek ProductionMethod xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size ProductionMethod xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read ProductionMethod xml", filename, 0);
        return 0;
    }
    return 1;
}

resource_type parse_resource_type_name(const char *name)
{
    if (!name || !*name) {
        return RESOURCE_NONE;
    }

    return resource_type_from_xml_attr(name);
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

    const std::string normalized_name = trim_copy(name);
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
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<ProductionMethod>(std::string());
    }
    return 1;
}

int parse_kind()
{
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
        const resource_type resource = parse_resource_type_name(resource_text);
        if (resource == RESOURCE_NONE) {
            log_error("Unsupported ProductionMethod output resource", resource_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_output_resource(resource);
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

int parse_treasury_cost()
{
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
    if (!g_parse_state.definition) {
        log_error("Encountered ProductionMethod climate_bonuses before root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    return 1;
}

int parse_climate_bonus()
{
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
    const resource_type resource = parse_resource_type_name(resource_text);
    if (resource == RESOURCE_NONE) {
        log_error("Unsupported ProductionMethod input resource", resource_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int amount = xml_parser_get_attribute_int("amount");
    if (amount <= 0) {
        log_error("Unsupported ProductionMethod input amount", xml_parser_get_attribute_string("amount"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_input({ resource, amount });
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "production_method", parse_root, nullptr, nullptr, nullptr },
    { "kind", parse_kind, nullptr, "production_method", nullptr },
    { "output", parse_output, nullptr, "production_method", nullptr },
    { "batch_size", parse_batch_size, nullptr, "production_method", nullptr },
    { "cart_loads", parse_cart_loads, nullptr, "production_method", nullptr },
    { "treasury_cost", parse_treasury_cost, nullptr, "production_method", nullptr },
    { "climate_bonuses", parse_climate_bonuses, nullptr, "production_method", nullptr },
    { "bonus", parse_climate_bonus, nullptr, "climate_bonuses", nullptr },
    { "input", parse_input, nullptr, "production_method", nullptr }
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("production_method_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        error_context_report_error("Failed to load ProductionMethod definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<ProductionMethod>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize ProductionMethod xml parser", filename, 0);
        error_context_report_error("Unable to initialize ProductionMethod xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_kind || !g_parse_state.saw_output) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, definition_path ? definition_path : "");
        error_context_report_error("Unable to parse ProductionMethod xml.", detail);
        return 0;
    }
    if (!validate_definition(*g_parse_state.definition, filename, definition_path)) {
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_production_methods.find(key) != g_production_methods.end()) {
        char detail[512];
        snprintf(detail, sizeof(detail), "file=%s path=%s", filename, key.c_str());
        log_error("Duplicate ProductionMethod definition path", key.c_str(), 0);
        error_context_report_error("Duplicate ProductionMethod definition path.", detail);
        return 0;
    }

    g_production_methods.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

ProductionMethod *find_production_method_definition(const char *path)
{
    const std::string normalized = normalize_definition_path(path);
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
            method->override_base_monthly_production(method->base_monthly_production() + delta);
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

const char *production_method_registry_get_production_method_path(void)
{
    building_type_registry_impl::g_production_method_path = mod_manager::mod_path() + "ProductionMethod/";
    return building_type_registry_impl::g_production_method_path.c_str();
}

int production_method_registry_load(void)
{
    using namespace building_type_registry_impl;

    production_method_registry_get_production_method_path();
    g_production_methods.clear();

    const dir_listing *files = dir_find_files_with_extension(g_production_method_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        return 1;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_production_method_path.c_str(), files->files[i].name);
        const std::string normalized_path = normalize_definition_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported ProductionMethod file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    return 1;
}

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
