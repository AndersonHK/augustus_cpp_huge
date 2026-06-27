#

#include "building/building_record.h"
#include "building/housing_type_registry.h"

#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "building/water_access_type.h"
#include "game/mod_manager.h"

#include "core/log.h"
#include "core/xml_parser.h"

#include <cstring>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_housing_type_path;

namespace {

struct ParseState {
    std::unique_ptr<HousingType> definition;
    int saw_residents = 0;
    int saw_evolution = 0;
    int saw_requirements = 0;
    int saw_prosperity = 0;
    int saw_tax = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<HousingType>> g_housing_types;
std::unordered_map<int, const HousingType *> g_housing_types_by_level;
std::vector<int> g_housing_levels;
ParseState g_parse_state;

int parse_non_negative_attribute(const char *node_name, const char *attribute_name, int *out_value)
{
    if (!xml_definition::parse_required_nonnegative_int_attribute(attribute_name, out_value)) {
        log_error("Unsupported HousingType numeric value", node_name, 0);
        return 0;
    }
    return 1;
}

int parse_water_requirement(const char *value, int *out_water)
{
    if (xml_value::equals(value, "none")) {
        *out_water = 0;
        return 1;
    }
    if (xml_value::equals(value, "well")) {
        if (!water_access_mask_from_text("well")) {
            return 0;
        }
        *out_water = 1;
        return 1;
    }
    if (xml_value::equals(value, "fountain")) {
        if (!water_access_mask_from_text("fountain")) {
            return 0;
        }
        *out_water = 2;
        return 1;
    }
    return 0;
}

int parse_root()
{
    if (!xml_parser_has_attribute("type")) {
        log_error("HousingType xml is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("level")) {
        log_error("HousingType xml is missing required attribute 'level'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string path = xml_definition::normalize_path(xml_parser_get_attribute_string("type"));
    if (path.empty()) {
        log_error("Unsupported HousingType type", xml_parser_get_attribute_string("type"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int level = -1;
    if (!xml_definition::parse_required_nonnegative_int_attribute("level", &level)) {
        log_error("Unsupported HousingType level", xml_parser_get_attribute_string("level"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition = std::make_unique<HousingType>(std::move(path));
    g_parse_state.definition->set_level(level);
    return 1;
}

int parse_residents()
{
    if (!g_parse_state.definition || g_parse_state.saw_residents) {
        log_error("Invalid HousingType residents node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("class")) {
        log_error("HousingType residents is missing required attribute 'class'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *resident_class = xml_parser_get_attribute_string("class");
    if (xml_value::equals(resident_class, "plebeian")) {
        g_parse_state.definition->set_resident_class(HousingResidentClass::Plebeian);
    } else if (xml_value::equals(resident_class, "patrician")) {
        g_parse_state.definition->set_resident_class(HousingResidentClass::Patrician);
    } else {
        log_error("Unsupported HousingType residents class", resident_class, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_residents = 1;
    return 1;
}

int parse_evolution()
{
    if (!g_parse_state.definition || g_parse_state.saw_evolution) {
        log_error("Invalid HousingType evolution node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("devolve_desirability") || !xml_parser_has_attribute("evolve_desirability")) {
        log_error("HousingType evolution is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int value = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("devolve_desirability"), &value)) {
        log_error("Unsupported HousingType devolve_desirability", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_devolve_desirability(value);

    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("evolve_desirability"), &value)) {
        log_error("Unsupported HousingType evolve_desirability", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_evolve_desirability(value);
    g_parse_state.saw_evolution = 1;
    return 1;
}

int parse_requirements()
{
    if (!g_parse_state.definition || g_parse_state.saw_requirements) {
        log_error("Invalid HousingType requirements node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int value = 0;
    if (!parse_non_negative_attribute("requirements", "entertainment", &value)) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_entertainment(value);

    if (!xml_parser_has_attribute("water") ||
        !parse_water_requirement(xml_parser_get_attribute_string("water"), &value)) {
        log_error("Unsupported HousingType water requirement", xml_parser_get_attribute_string("water"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_water(value);

    struct NumericRequirement {
        const char *attribute;
        void (HousingType::*setter)(int);
    };
    const NumericRequirement requirements[] = {
        {"religion", &HousingType::set_religion},
        {"education", &HousingType::set_education},
        {"barber", &HousingType::set_barber},
        {"bathhouse", &HousingType::set_bathhouse},
        {"health", &HousingType::set_health},
        {"food_types", &HousingType::set_food_types},
        {"pottery", &HousingType::set_pottery},
        {"oil", &HousingType::set_oil},
        {"furniture", &HousingType::set_furniture},
        {"wine", &HousingType::set_wine}
    };

    for (const NumericRequirement &requirement : requirements) {
        if (!parse_non_negative_attribute("requirements", requirement.attribute, &value)) {
            g_parse_state.error = 1;
            return 0;
        }
        (g_parse_state.definition.get()->*requirement.setter)(value);
    }

    g_parse_state.saw_requirements = 1;
    return 1;
}

int parse_prosperity()
{
    int value = 0;
    if (!g_parse_state.definition || g_parse_state.saw_prosperity ||
        !parse_non_negative_attribute("prosperity", "value", &value)) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_prosperity(value);
    g_parse_state.saw_prosperity = 1;
    return 1;
}

int parse_tax()
{
    int value = 0;
    if (!g_parse_state.definition || g_parse_state.saw_tax ||
        !parse_non_negative_attribute("tax", "multiplier", &value)) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_tax_multiplier(value);
    g_parse_state.saw_tax = 1;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    {"housing_type", parse_root, nullptr, nullptr, nullptr},
    {"residents", parse_residents, nullptr, "housing_type", nullptr},
    {"evolution", parse_evolution, nullptr, "housing_type", nullptr},
    {"requirements", parse_requirements, nullptr, "housing_type", nullptr},
    {"prosperity", parse_prosperity, nullptr, "housing_type", nullptr},
    {"tax", parse_tax, nullptr, "housing_type", nullptr}
};

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("housing_type_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<HousingType>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_file(
        filename,
        "HousingType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    if (!parsed || g_parse_state.error || !g_parse_state.definition || !g_parse_state.saw_residents ||
        !g_parse_state.saw_evolution || !g_parse_state.saw_requirements || !g_parse_state.saw_prosperity ||
        !g_parse_state.saw_tax) {
        error_context_report_error("Unable to parse HousingType xml.", filename);
        return 0;
    }
    const std::string key = g_parse_state.definition->path();
    if (g_housing_types.find(key) != g_housing_types.end()) {
        log_error("Duplicate HousingType definition path", key.c_str(), 0);
        error_context_report_error("Duplicate HousingType definition path.", filename);
        return 0;
    }
    const int level = g_parse_state.definition->level();
    if (g_housing_types_by_level.find(level) != g_housing_types_by_level.end()) {
        log_error("Duplicate HousingType level", key.c_str(), level);
        error_context_report_error("Duplicate HousingType level.", filename);
        return 0;
    }

    HousingType *definition = g_parse_state.definition.get();
    g_housing_types_by_level.emplace(level, definition);
    g_housing_levels.push_back(level);
    g_housing_types.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

std::string compatibility_base_text_id(const char *text_id)
{
    std::string base = xml_value::trim_copy(text_id ? text_id : "");
    const char suffix[] = "_2x2";
    if (base.size() > strlen(suffix) && base.compare(base.size() - strlen(suffix), strlen(suffix), suffix) == 0) {
        base.resize(base.size() - strlen(suffix));
    }
    return base;
}

} // namespace

const HousingType *find_housing_type_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_housing_types.find(normalized);
    return found != g_housing_types.end() ? found->second.get() : nullptr;
}

const HousingType *find_housing_type_definition_for_building_path(const char *path)
{
    const std::string base = compatibility_base_text_id(path);
    return find_housing_type_definition(base.c_str());
}

const HousingType *find_housing_type_definition_for_level(int level)
{
    const auto found = g_housing_types_by_level.find(level);
    return found != g_housing_types_by_level.end() ? found->second : nullptr;
}

int housing_type_level_count()
{
    return static_cast<int>(g_housing_levels.size());
}

int housing_type_level_at(int index)
{
    return index >= 0 && index < static_cast<int>(g_housing_levels.size()) ? g_housing_levels[index] : -1;
}

} // namespace building_type_registry_impl

const char *housing_type_registry_get_housing_type_path(void)
{
    building_type_registry_impl::g_housing_type_path = mod_manager::mod_path() + "HousingType/";
    return building_type_registry_impl::g_housing_type_path.c_str();
}

int housing_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    housing_type_registry_get_housing_type_path();
    g_housing_types.clear();
    g_housing_types_by_level.clear();
    g_housing_levels.clear();

    if (!xml_definition::for_each_definition_file(
        g_housing_type_path,
        "HousingType",
        false,
        [](const xml_definition::DefinitionFile &file, const std::string &normalized_path) {
            return parse_definition_file(file.full_path.c_str(), normalized_path.c_str());
        })) {
        return 0;
    }

    std::sort(g_housing_levels.begin(), g_housing_levels.end());
    return 1;
}
