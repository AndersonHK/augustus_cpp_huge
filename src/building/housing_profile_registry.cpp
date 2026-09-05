#include "building/housing_profile_registry.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

struct ParseState {
    std::unique_ptr<HousingProfileDef> definition;
    bool disabled = false;
    bool saw_root = false;
    bool saw_residents = false;
    bool saw_evolution = false;
    bool saw_requirements = false;
    bool saw_prosperity = false;
    bool saw_tax = false;
    bool error = false;
};

std::unordered_map<std::string, std::unique_ptr<HousingProfileDef>> g_profiles;
std::unordered_map<int, const HousingProfileDef *> g_profiles_by_level;
std::vector<int> g_compatibility_levels;
mod_definition::DefinitionOverlayTracker g_housing_profile_overlays;
ParseState g_parse;

bool parse_nonnegative(const char *node, const char *attribute, int &value)
{
    if (xml_definition::parse_required_nonnegative_int_attribute(attribute, &value)) {
        return true;
    }
    log_error("Unsupported HousingProfile numeric value", node, 0);
    return false;
}

int parse_root()
{
    if (g_parse.saw_root || !xml_parser_has_attribute("type")) {
        log_error("HousingProfile is missing its type or has duplicate roots", 0, 0);
        g_parse.error = true;
        return 0;
    }

    std::string path = xml_definition::normalize_path(xml_parser_get_attribute_string("type"));
    int disabled = 0;
    if (path.empty() || (xml_parser_has_attribute("disabled") &&
            !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled))) {
        log_error("Unsupported HousingProfile identity or disabled value", xml_parser_get_attribute_string("type"), 0);
        g_parse.error = true;
        return 0;
    }

    int level = -1;
    if ((!disabled && !xml_definition::parse_required_nonnegative_int_attribute("level", &level)) ||
        (disabled && xml_parser_has_attribute("level"))) {
        log_error("Unsupported HousingProfile identity", xml_parser_get_attribute_string("type"), level);
        g_parse.error = true;
        return 0;
    }

    g_parse.definition = std::make_unique<HousingProfileDef>(std::move(path));
    g_parse.definition->compatibility_level = level;
    g_parse.disabled = disabled != 0;
    g_parse.saw_root = true;
    return 1;
}

bool reject_tombstone_content(const char *node)
{
    if (!g_parse.disabled) {
        return false;
    }
    log_error("Disabled HousingProfile tombstone contains profile data", node, 0);
    g_parse.error = true;
    return true;
}

int parse_residents()
{
    if (reject_tombstone_content("residents") || !g_parse.definition ||
        g_parse.saw_residents || !xml_parser_has_attribute("class")) {
        g_parse.error = true;
        return 0;
    }

    const char *value = xml_parser_get_attribute_string("class");
    if (xml_value::equals(value, "plebeian")) {
        g_parse.definition->resident_class_value = HousingResidentClass::Plebeian;
    } else if (xml_value::equals(value, "patrician")) {
        g_parse.definition->resident_class_value = HousingResidentClass::Patrician;
    } else {
        log_error("Unsupported HousingProfile resident class", value, 0);
        g_parse.error = true;
        return 0;
    }
    g_parse.saw_residents = true;
    return 1;
}

int parse_evolution()
{
    if (reject_tombstone_content("evolution") || !g_parse.definition || g_parse.saw_evolution ||
        !xml_parser_has_attribute("devolve_desirability") ||
        !xml_parser_has_attribute("evolve_desirability") ||
        !xml_value::parse_int_strict(
            xml_parser_get_attribute_string("devolve_desirability"),
            &g_parse.definition->evolution.devolve_desirability) ||
        !xml_value::parse_int_strict(
            xml_parser_get_attribute_string("evolve_desirability"),
            &g_parse.definition->evolution.evolve_desirability)) {
        log_error("Invalid HousingProfile evolution", 0, 0);
        g_parse.error = true;
        return 0;
    }
    g_parse.saw_evolution = true;
    return 1;
}

int parse_requirements()
{
    if (reject_tombstone_content("requirements") || !g_parse.definition || g_parse.saw_requirements) {
        g_parse.error = true;
        return 0;
    }

    HousingRequirements &requirements = g_parse.definition->requirements;
    if (!parse_nonnegative("requirements", "entertainment", requirements.entertainment) ||
        !xml_parser_has_attribute("water")) {
        g_parse.error = true;
        return 0;
    }

    const char *water = xml_parser_get_attribute_string("water");
    if (xml_value::equals(water, "none")) {
        requirements.water = HousingWaterRequirement::None;
    } else if (xml_value::equals(water, "well")) {
        requirements.water = HousingWaterRequirement::Well;
    } else if (xml_value::equals(water, "fountain")) {
        requirements.water = HousingWaterRequirement::Fountain;
    } else {
        log_error("Unsupported HousingProfile water requirement", water, 0);
        g_parse.error = true;
        return 0;
    }

    struct NumericField {
        const char *attribute;
        int HousingRequirements::*member;
    };
    const NumericField fields[] = {
        { "religion", &HousingRequirements::religion },
        { "education", &HousingRequirements::education },
        { "barber", &HousingRequirements::barber },
        { "bathhouse", &HousingRequirements::bathhouse },
        { "health", &HousingRequirements::health },
        { "food_types", &HousingRequirements::food_types },
        { "pottery", &HousingRequirements::pottery },
        { "oil", &HousingRequirements::oil },
        { "furniture", &HousingRequirements::furniture },
        { "wine", &HousingRequirements::wine },
    };
    for (const NumericField &field : fields) {
        if (!parse_nonnegative("requirements", field.attribute, requirements.*field.member)) {
            g_parse.error = true;
            return 0;
        }
    }

    g_parse.saw_requirements = true;
    return 1;
}

int parse_prosperity()
{
    if (reject_tombstone_content("prosperity") || !g_parse.definition || g_parse.saw_prosperity ||
        !parse_nonnegative("prosperity", "value", g_parse.definition->prosperity)) {
        g_parse.error = true;
        return 0;
    }
    g_parse.saw_prosperity = true;
    return 1;
}

int parse_tax()
{
    if (reject_tombstone_content("tax") || !g_parse.definition || g_parse.saw_tax ||
        !parse_nonnegative("tax", "multiplier", g_parse.definition->tax_multiplier)) {
        g_parse.error = true;
        return 0;
    }
    g_parse.saw_tax = true;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "housing_profile", parse_root, nullptr, nullptr, nullptr },
    { "residents", parse_residents, nullptr, "housing_profile", nullptr },
    { "evolution", parse_evolution, nullptr, "housing_profile", nullptr },
    { "requirements", parse_requirements, nullptr, "housing_profile", nullptr },
    { "prosperity", parse_prosperity, nullptr, "housing_profile", nullptr },
    { "tax", parse_tax, nullptr, "housing_profile", nullptr },
};

struct ParsedDefinition {
    std::unique_ptr<HousingProfileDef> definition;
    bool disabled = false;
};

int parse_definition_file(
    const char *filename,
    const char *definition_path,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("housing_profile_registry.parse_definition", filename);
    g_parse = {};
    const int parsed = xml_definition::parse_file(
        filename,
        "HousingProfile",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    const bool complete_profile = g_parse.saw_residents && g_parse.saw_evolution &&
        g_parse.saw_requirements && g_parse.saw_prosperity && g_parse.saw_tax;
    if (!parsed || g_parse.error || !g_parse.saw_root || !g_parse.definition ||
        (!g_parse.disabled && !complete_profile)) {
        error_context_report_error("Unable to parse HousingProfile xml.", filename);
        if (failure_reason) {
            *failure_reason = std::string("Unable to parse HousingProfile xml: ") + filename;
        }
        return 0;
    }

    const std::string key = g_parse.definition->path_id;
    if (key != (definition_path ? definition_path : "")) {
        log_error("HousingProfile type does not match its definition path", key.c_str(), 0);
        error_context_report_error("HousingProfile type/path mismatch.", filename);
        if (failure_reason) {
            *failure_reason = std::string("HousingProfile type/path mismatch: ") + filename;
        }
        return 0;
    }

    result.definition = std::move(g_parse.definition);
    result.disabled = g_parse.disabled;
    return 1;
}

struct LayeredDefinition {
    ParsedDefinition parsed;
    mod_definition::DefinitionSource source;
};

struct StagedRegistry {
    std::unordered_map<std::string, std::unique_ptr<HousingProfileDef>> profiles;
    std::unordered_map<int, const HousingProfileDef *> profiles_by_level;
    std::vector<int> compatibility_levels;
    mod_definition::DefinitionOverlayTracker overlays;
};

bool build_layered_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedRegistry &staged,
    std::string *failure_reason)
{
    mod_definition::DefinitionOverlayTracker overlay;
    std::map<std::string, LayeredDefinition> winners;
    if (!mod_definition::for_each_definition_file(
            layers,
            {"HousingProfile"},
            "HousingProfile",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                ParsedDefinition parsed;
                if (!parse_definition_file(
                        source.full_path.c_str(),
                        source.normalized_definition_path.c_str(),
                        parsed,
                        failure_reason)) {
                    return false;
                }
                const std::string stable_id = parsed.definition->path_id;
                if (!overlay.apply(stable_id, parsed.disabled, source)) {
                    log_error("Unable to layer HousingProfile definition", overlay.failure_reason().c_str(), 0);
                    error_context_report_error("Unable to layer HousingProfile definition.", overlay.failure_reason().c_str());
                    if (failure_reason) {
                        *failure_reason = overlay.failure_reason();
                    }
                    return false;
                }
                winners[stable_id] = {std::move(parsed), source};
                return true;
            },
            nullptr,
            failure_reason)) {
        return false;
    }

    std::unordered_map<int, const LayeredDefinition *> levels;
    for (auto &entry : winners) {
        LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        const int level = winner.parsed.definition->compatibility_level;
        const auto existing = levels.find(level);
        if (existing != levels.end()) {
            const std::string detail = "HousingProfile compatibility level " + std::to_string(level) +
                " is claimed by both " + existing->second->source.describe() + " and " +
                winner.source.describe() + '.';
            log_error("Duplicate active HousingProfile compatibility level", detail.c_str(), level);
            error_context_report_error("Duplicate active HousingProfile compatibility level.", detail.c_str());
            if (failure_reason) {
                *failure_reason = detail;
            }
            return false;
        }
        levels.emplace(level, &winner);
    }

    for (auto &entry : winners) {
        LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        const int level = winner.parsed.definition->compatibility_level;
        HousingProfileDef *definition = winner.parsed.definition.get();
        staged.profiles_by_level.emplace(level, definition);
        staged.compatibility_levels.push_back(level);
        staged.profiles.emplace(entry.first, std::move(winner.parsed.definition));
    }
    std::sort(staged.compatibility_levels.begin(), staged.compatibility_levels.end());
    staged.overlays = std::move(overlay);
    return true;
}

std::string legacy_base_path(const char *path)
{
    std::string base = xml_value::trim_copy(path ? path : "");
    constexpr char suffix[] = "_2x2";
    if (base.size() > std::strlen(suffix) &&
        base.compare(base.size() - std::strlen(suffix), std::strlen(suffix), suffix) == 0) {
        base.resize(base.size() - std::strlen(suffix));
    }
    return base;
}

} // namespace

const HousingProfileDef *find_housing_profile_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_profiles.find(normalized);
    return found == g_profiles.end() ? nullptr : found->second.get();
}

const HousingProfileDef *find_housing_profile_definition_for_compatibility_level(int level)
{
    const auto found = g_profiles_by_level.find(level);
    return found == g_profiles_by_level.end() ? nullptr : found->second;
}

const HousingProfileDef *find_housing_profile_definition_for_legacy_building_path(const char *path)
{
    const std::string base = legacy_base_path(path);
    return find_housing_profile_definition(base.c_str());
}

int housing_profile_compatibility_level_count()
{
    return static_cast<int>(g_compatibility_levels.size());
}

int housing_profile_compatibility_level_at(int index)
{
    return index >= 0 && index < static_cast<int>(g_compatibility_levels.size())
        ? g_compatibility_levels[index]
        : -1;
}

const mod_definition::DefinitionOverlayEntry *find_housing_profile_definition_overlay(const char *path)
{
    return g_housing_profile_overlays.find(xml_definition::normalize_path(path));
}

} // namespace building_type_registry_impl

int housing_profile_registry_load()
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure HousingProfile definition layers", failure_reason.c_str(), 0);
        return 0;
    }
    return housing_profile_registry_load_layers(layers, &failure_reason);
}

int housing_profile_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        return 0;
    }
    g_profiles = std::move(staged.profiles);
    g_profiles_by_level = std::move(staged.profiles_by_level);
    g_compatibility_levels = std::move(staged.compatibility_levels);
    g_housing_profile_overlays = std::move(staged.overlays);
    return 1;
}
