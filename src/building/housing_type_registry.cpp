#include "building/building_record.h"
#include "building/housing_type_registry.h"

#include "core/crash_context.h"
#include "core/xml_value.h"
#include "building/water_access_type.h"

extern "C" {
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
}

#include <cstdio>
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

int compare_text(const char *left, const char *right)
{
    if (!left || !right) {
        return left == right ? 0 : (left ? 1 : -1);
    }
    while (*left && *right && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
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
    std::string normalized = xml_value::trim_copy(value ? value : "");
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
        log_error("Unable to open HousingType xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek HousingType xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size HousingType xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read HousingType xml", filename, 0);
        return 0;
    }
    return 1;
}

int parse_non_negative_attribute(const char *node_name, const char *attribute_name, int *out_value)
{
    if (!xml_parser_has_attribute(attribute_name)) {
        log_error("HousingType node is missing required attribute", attribute_name, 0);
        return 0;
    }

    const char *text = xml_parser_get_attribute_string(attribute_name);
    int value = 0;
    if (!text || !xml_value::parse_int_strict(text, &value) || value < 0) {
        log_error("Unsupported HousingType numeric value", node_name, 0);
        return 0;
    }
    *out_value = value;
    return 1;
}

int parse_water_requirement(const char *value, int *out_water)
{
    if (value && compare_text(value, "none") == 0) {
        *out_water = 0;
        return 1;
    }
    if (value && compare_text(value, "well") == 0) {
        if (!water_access_mask_from_text("well")) {
            return 0;
        }
        *out_water = 1;
        return 1;
    }
    if (value && compare_text(value, "fountain") == 0) {
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

    std::string path = normalize_definition_path(xml_parser_get_attribute_string("type"));
    if (path.empty()) {
        log_error("Unsupported HousingType type", xml_parser_get_attribute_string("type"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int level = -1;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string("level"), &level) || level < 0) {
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
    if (resident_class && compare_text(resident_class, "plebeian") == 0) {
        g_parse_state.definition->set_resident_class(HousingResidentClass::Plebeian);
    } else if (resident_class && compare_text(resident_class, "patrician") == 0) {
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

    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        error_context_report_error("Failed to load HousingType definition.", filename);
        return 0;
    }

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<HousingType>(definition_path ? definition_path : "");
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize HousingType xml parser", filename, 0);
        error_context_report_error("Unable to initialize HousingType xml parser.", filename);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
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
    const std::string normalized = normalize_definition_path(path);
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

extern "C" const char *housing_type_registry_get_housing_type_path(void)
{
    building_type_registry_impl::g_housing_type_path = std::string(mod_manager_get_mod_path()) + "HousingType/";
    return building_type_registry_impl::g_housing_type_path.c_str();
}

extern "C" int housing_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    housing_type_registry_get_housing_type_path();
    g_housing_types.clear();
    g_housing_types_by_level.clear();
    g_housing_levels.clear();

    const dir_listing *files = dir_find_files_with_extension(g_housing_type_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        return 1;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_housing_type_path.c_str(), files->files[i].name);
        const std::string normalized_path = normalize_definition_path(files->files[i].name);
        if (normalized_path.empty()) {
            log_error("Unsupported HousingType file name", files->files[i].name, 0);
            return 0;
        }
        if (!parse_definition_file(full_path, normalized_path.c_str())) {
            return 0;
        }
    }

    std::sort(g_housing_levels.begin(), g_housing_levels.end());
    return 1;
}
