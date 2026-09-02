#include "building/building_record.h"
#include "building/building_type_registry.h"
#include "building/building_type_registry_internal.h"
#include "building/building_type_startup_bridge.h"
#include "building/building_type_id_bridge.h"
#include "building/building_type_legacy_migration.h"
#include "building/culture_module_registry.h"
#include "building/distribution.h"
#include "building/FoundationRegistry.h"
#include "building/god_registry.h"
#include "building/god_id_bridge.h"
#include "building/housing_profile_registry.h"
#include "building/production_method_registry.h"
#include "building/religion_registry.h"
#include "building/storage_type_registry.h"
#include "building/water_access_type.h"
#include "building/water_access_type_id_bridge.h"
#include "assets/image_group_payload.h"
#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include "building/menu.h"
#include "building/monument.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/resource.h"
#include "figure/formation_type.h"
#include "figure/figure_type_registry_internal.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "sound/city.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>

namespace building_type_registry_impl {

constexpr uint16_t LEGACY_BUILDING_THEATER = 31;
constexpr uint16_t LEGACY_BUILDING_WELL = 92;

static int compare_text(const char *left, const char *right)
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

static building_type find_building_type_by_attr(const char *type_attr)
{
    if (find_housing_profile_definition_for_legacy_building_path(type_attr)) {
        return BUILDING_NONE;
    }
    if (building_type_legacy_migration_text_id_is_xml_owned(type_attr)) {
        uint16_t legacy_type = building_type_legacy_migration_enum_for_text_id(type_attr);
        if (legacy_type == LEGACY_BUILDING_THEATER || legacy_type == LEGACY_BUILDING_WELL) {
            return BUILDING_NONE;
        }
        return static_cast<building_type>(legacy_type);
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (definition && definition->matches_identity(type_attr ? type_attr : "")) {
            return definition->type();
        }
    }
    return BUILDING_NONE;
}

static HousingTransitionKind parse_housing_transition_kind(const char *attribute)
{
    if (attribute && compare_text(attribute, "evolve_to") == 0) {
        return HousingTransitionKind::EvolveTo;
    }
    if (attribute && compare_text(attribute, "devolve_to") == 0) {
        return HousingTransitionKind::DevolveTo;
    }
    if (attribute && compare_text(attribute, "merge_to") == 0) {
        return HousingTransitionKind::MergeTo;
    }
    return HousingTransitionKind::SplitTo;
}

static int parse_figure_list_attribute(const char *value, std::vector<figure_type> &out_figures)
{
    if (!value || !*value) {
        return 1;
    }

    std::string list = value;
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        std::string token = xml_value::trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!token.empty()) {
            figure_type type = figure_type_from_xml_name(token.c_str());
            if (type == FIGURE_NONE) {
                return 0;
            }
            out_figures.push_back(type);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return 1;
}

static int parse_optional_int_attribute(const char *node_name, const char *attribute_name, int *out_value, int *out_has_value)
{
    *out_has_value = 0;
    if (!xml_parser_has_attribute(attribute_name)) {
        return 1;
    }

    const char *text = xml_parser_get_attribute_string(attribute_name);
    if (!text || !xml_value::parse_int_strict(text, out_value)) {
        log_error(node_name, attribute_name, 0);
        return 0;
    }

    *out_has_value = 1;
    return 1;
}

static RoadAccessMode parse_road_access_mode(const char *value)
{
    if (value && compare_text(value, "normal") == 0) {
        return RoadAccessMode::Normal;
    }
    return RoadAccessMode::None;
}

static LaborSeekerMethod parse_labor_seeker_method(const char *value)
{
    if (value && compare_text(value, "houses_spawn_if_below") == 0) {
        return LaborSeekerMethod::HousesSpawnIfBelow;
    }
    if (value && compare_text(value, "houses_generate_if_below") == 0) {
        return LaborSeekerMethod::HousesGenerateIfBelow;
    }
    if (value && compare_text(value, "workforce") == 0) {
        return LaborSeekerMethod::Workforce;
    }
    return LaborSeekerMethod::None;
}

static int parse_delay_bands_attribute(const char *value, std::vector<DelayBand> &out_delay_bands)
{
    if (!value || !*value) {
        return 0;
    }

    std::string list = value;
    size_t start = 0;
    int previous_percentage = 101;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        std::string token = xml_value::trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!token.empty()) {
            size_t separator = token.find(':');
            if (separator == std::string::npos) {
                return 0;
            }

            std::string percentage_text = xml_value::trim_copy(token.substr(0, separator));
            std::string delay_text = xml_value::trim_copy(token.substr(separator + 1));
            int percentage = 0;
            int delay = 0;
            if (!xml_value::parse_int_strict(percentage_text, &percentage) || !xml_value::parse_int_strict(delay_text, &delay)) {
                return 0;
            }
            if (percentage < 1 || percentage > 100 || delay < 0) {
                return 0;
            }
            if (percentage >= previous_percentage) {
                return 0;
            }

            out_delay_bands.push_back({ percentage, delay });
            previous_percentage = percentage;
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return !out_delay_bands.empty();
}

static int parse_chance_bands_attribute(const char *value, std::vector<ChanceBand> &out_chance_bands)
{
    if (!value || !*value) {
        return 0;
    }

    // Runtime chooses the first band whose minimum is <= the live source value,
    // so authors must list bands from highest minimum to lowest minimum.
    std::string list = value;
    size_t start = 0;
    int previous_value = INT_MAX;
    while (start <= list.size()) {
        size_t end = list.find(',', start);
        std::string token = xml_value::trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (!token.empty()) {
            size_t separator = token.find(':');
            if (separator == std::string::npos) {
                return 0;
            }

            std::string value_text = xml_value::trim_copy(token.substr(0, separator));
            std::string chance_text = xml_value::trim_copy(token.substr(separator + 1));
            int min_value = 0;
            int chance = 0;
            if (!xml_value::parse_int_strict(value_text, &min_value) || !xml_value::parse_int_strict(chance_text, &chance)) {
                return 0;
            }
            if (min_value < 0 || chance < 0 || chance > 1000000 || min_value >= previous_value) {
                return 0;
            }

            out_chance_bands.push_back({ min_value, chance });
            previous_value = min_value;
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return !out_chance_bands.empty();
}

static GraphicTiming parse_graphic_timing(const char *value)
{
    if (value && compare_text(value, "on_spawn_entry") == 0) {
        return GraphicTiming::OnSpawnEntry;
    }
    if (value && compare_text(value, "before_delay_check") == 0) {
        return GraphicTiming::BeforeDelayCheck;
    }
    if (value && compare_text(value, "before_successful_spawn") == 0) {
        return GraphicTiming::BeforeSuccessfulSpawn;
    }
    return GraphicTiming::None;
}

static SpawnChanceSource parse_spawn_chance_source(const char *value)
{
    if (!value || compare_text(value, "none") == 0) {
        return SpawnChanceSource::None;
    }
    if (compare_text(value, "city_unemployment_percent") == 0) {
        return SpawnChanceSource::CityUnemploymentPercent;
    }
    if (compare_text(value, "house_unemployed_workers") == 0) {
        return SpawnChanceSource::HouseUnemployedWorkers;
    }
    return SpawnChanceSource::None;
}

static FigureSlot parse_figure_slot(const char *value)
{
    if (value && compare_text(value, "primary") == 0) {
        return FigureSlot::Primary;
    }
    if (value && compare_text(value, "secondary") == 0) {
        return FigureSlot::Secondary;
    }
    if (value && compare_text(value, "quaternary") == 0) {
        return FigureSlot::Quaternary;
    }
    if (value && compare_text(value, "none") == 0) {
        return FigureSlot::None;
    }
    return FigureSlot::Primary;
}

static GuardTiming parse_guard_timing(const char *value)
{
    if (value && compare_text(value, "after_labor_seeker") == 0) {
        return GuardTiming::AfterLaborSeeker;
    }
    return GuardTiming::BeforeRoadAccess;
}

static SpawnCondition parse_spawn_condition(const char *value)
{
    if (!value || compare_text(value, "always") == 0) {
        return SpawnCondition::Always;
    }
    if (compare_text(value, "days1_positive") == 0) {
        return SpawnCondition::Days1Positive;
    }
    if (compare_text(value, "days1_not_positive") == 0) {
        return SpawnCondition::Days1NotPositive;
    }
    if (compare_text(value, "days2_positive") == 0) {
        return SpawnCondition::Days2Positive;
    }
    if (compare_text(value, "days1_or_days2_positive") == 0) {
        return SpawnCondition::Days1OrDays2Positive;
    }
    return SpawnCondition::Always;
}

static WaterAccessNodeKind parse_provider_water_access_node_kind(const char *value)
{
    if (!value || compare_text(value, "aqueduct_connection") == 0) {
        return WaterAccessNodeKind::AqueductConnection;
    }
    return WaterAccessNodeKind::None;
}

static WaterAccessOrigin parse_water_access_origin(const char *value)
{
    if (value && compare_text(value, "nodes") == 0) {
        return WaterAccessOrigin::Nodes;
    }
    return WaterAccessOrigin::Footprint;
}

static WaterAccessRequirementWhere parse_water_access_where(const char *value)
{
    if (value && compare_text(value, "nodes") == 0) {
        return WaterAccessRequirementWhere::Nodes;
    }
    return WaterAccessRequirementWhere::Footprint;
}

static WaterAccessRequirementMode parse_water_access_requirement_mode(const char *value)
{
    if (value && compare_text(value, "any") == 0) {
        return WaterAccessRequirementMode::Any;
    }
    return WaterAccessRequirementMode::All;
}

enum class WaterAccessNodeRole {
    Both,
    Provider,
    Requirement
};

static WaterAccessNodeRole parse_water_access_node_role(const char *value)
{
    if (value && compare_text(value, "provide") == 0) {
        return WaterAccessNodeRole::Provider;
    }
    if (value && compare_text(value, "require") == 0) {
        return WaterAccessNodeRole::Requirement;
    }
    return WaterAccessNodeRole::Both;
}

static int parse_spawn_direction(const char *value)
{
    if (value && compare_text(value, "bottom") == 0) {
        return DIR_4_BOTTOM;
    }
    return DIR_0_TOP;
}

static std::string normalize_graphics_path(const char *value)
{
    std::string text = xml_value::trim_copy(value ? value : "");
    if (text.empty() || xml_definition::ends_with_ignore_case_ascii(text, ".xml")) {
        return std::string();
    }

    std::string normalized = xml_definition::normalize_path(text.c_str());
    if (normalized.empty() || xml_definition::starts_with_ignore_case_ascii(normalized, "graphics\\")) {
        return std::string();
    }

    return normalized;
}

static int parse_graphics_comparison(const char *comparison_text, GraphicComparison *out_comparison)
{
    if (!out_comparison) {
        return 0;
    }
    if (comparison_text && (compare_text(comparison_text, "lt") == 0 ||
        compare_text(comparison_text, "less_than") == 0)) {
        *out_comparison = GraphicComparison::LessThan;
        return 1;
    }
    if (comparison_text && (compare_text(comparison_text, "lte") == 0 ||
        compare_text(comparison_text, "le") == 0 ||
        compare_text(comparison_text, "less_than_or_equal") == 0)) {
        *out_comparison = GraphicComparison::LessThanOrEqual;
        return 1;
    }
    if (comparison_text && (compare_text(comparison_text, "eq") == 0 ||
        compare_text(comparison_text, "equal") == 0)) {
        *out_comparison = GraphicComparison::Equal;
        return 1;
    }
    if (comparison_text && (compare_text(comparison_text, "gt") == 0 ||
        compare_text(comparison_text, "greater_than") == 0)) {
        *out_comparison = GraphicComparison::GreaterThan;
        return 1;
    }
    if (comparison_text && (compare_text(comparison_text, "gte") == 0 ||
        compare_text(comparison_text, "ge") == 0 ||
        compare_text(comparison_text, "greater_than_or_equal") == 0)) {
        *out_comparison = GraphicComparison::GreaterThanOrEqual;
        return 1;
    }
    return 0;
}

static int parse_provider_water_access()
{
    if (!g_parse_state.definition) {
        log_error("Encountered water_access definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_provider_water_access) {
        log_error("BuildingType xml contains duplicate provider water_access nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_provider_water_access = 1;
    g_parse_state.parsing_provider_water_access = 1;
    g_parse_state.saw_water_access_rule = 0;
    if (xml_parser_has_attribute("requires_open_water")) {
        int required = 0;
        const char *value = xml_parser_get_attribute_string("requires_open_water");
        if (!xml_value::parse_bool(value, &required)) {
            log_error("BuildingType water_access has invalid requires_open_water value", value, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_water_access_requires_open_water(required);
        g_parse_state.saw_water_access_rule = required;
    }
    return 1;
}

int building_type_water_access_open_water_attribute_is_valid_for_test(const char *value)
{
    int required = 0;
    return value && xml_value::parse_bool(value, &required);
}

static void finish_provider_water_access()
{
    if (!g_parse_state.parsing_provider_water_access) {
        return;
    }

    if (!g_parse_state.saw_water_access_rule) {
        log_error("BuildingType water_access is missing provide/require rules", g_parse_state.definition ? g_parse_state.definition->attr() : 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_provider_water_access = 0;
}

static int parse_building_root()
{
    if (g_parse_state.saw_root || !xml_parser_has_attribute("type")) {
        log_error("BuildingType xml is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string type_attr = xml_definition::normalize_path(xml_parser_get_attribute_string("type"));
    int disabled = 0;
    if (type_attr.empty() || (xml_parser_has_attribute("disabled") &&
            !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled))) {
        log_error("BuildingType xml has invalid identity or disabled value", type_attr.c_str(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition = std::make_unique<BuildingType>(
        static_cast<building_type>(BUILDING_NONE), std::move(type_attr));
    g_parse_state.disabled = disabled;
    g_parse_state.saw_root = 1;
    return 1;
}

static void parse_building_root_text(const char *text)
{
    if (g_parse_state.disabled && !xml_value::trim_copy(text ? text : "").empty()) {
        g_parse_state.saw_root_text = 1;
        g_parse_state.error = 1;
        log_error("Disabled BuildingType tombstone contains text data", g_parse_state.definition ?
            g_parse_state.definition->attr() : 0, 0);
    }
}

static int parse_identity()
{
    if (!g_parse_state.definition) {
        log_error("Encountered identity definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_identity) {
        log_error("BuildingType xml contains duplicate identity nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *name_key = 0;
    if (xml_parser_has_attribute("name_key")) {
        name_key = xml_parser_get_attribute_string("name_key");
    } else if (xml_parser_has_attribute("translation_key")) {
        name_key = xml_parser_get_attribute_string("translation_key");
    }

    std::string key = xml_value::trim_copy(name_key ? name_key : "");
    if (key.empty()) {
        log_error("BuildingType identity is missing required attribute 'name_key'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_identity_name_key(std::move(key));
    if (xml_parser_has_attribute("aliases")) {
        const std::string aliases = xml_parser_get_attribute_string("aliases");
        size_t start = 0;
        while (start <= aliases.size()) {
            const size_t separator = aliases.find('|', start);
            const std::string segment = aliases.substr(
                start,
                separator == std::string::npos ? std::string::npos : separator - start);
            std::string alias = xml_value::trim_copy(segment.c_str());
            if (alias.empty() || alias == g_parse_state.definition->attr() ||
                !g_parse_state.definition->add_identity_alias(std::move(alias))) {
                log_error("BuildingType identity contains an empty, canonical, or duplicate alias",
                    g_parse_state.definition->attr(), 0);
                g_parse_state.error = 1;
                return 0;
            }
            if (separator == std::string::npos) {
                break;
            }
            start = separator + 1;
        }
    }
    g_parse_state.saw_identity = 1;
    return 1;
}

static int parse_model()
{
    if (!g_parse_state.definition) {
        log_error("Encountered model definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_model) {
        log_error("BuildingType xml contains duplicate model nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (xml_parser_has_attribute("size")) {
        log_error("BuildingType model size was removed; geometry belongs to the external FoundationDef",
            g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("desirability_value") || xml_parser_has_attribute("desirability_step") ||
        xml_parser_has_attribute("desirability_step_size") || xml_parser_has_attribute("desirability_range") ||
        xml_parser_has_attribute("laborers")) {
        log_error("BuildingType model contains attributes that must be child groups", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int value = 0;
    int has_value = 0;
    int any_value = 0;

    if (!parse_optional_int_attribute("Unsupported BuildingType model numeric attribute", "cost", &value, &has_value)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_value) {
        if (value < 0) {
            log_error("Unsupported BuildingType model cost", g_parse_state.definition->attr(), value);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_model_cost(value);
        any_value = 1;
    }

    if (!parse_optional_int_attribute("Unsupported BuildingType model numeric attribute", "hp", &value, &has_value)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_value) {
        if (value <= 0) {
            log_error("Unsupported BuildingType model hp", g_parse_state.definition->attr(), value);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_model_hit_points(value);
        any_value = 1;
    }

    if (!any_value) {
        log_error("BuildingType model is missing supported attributes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_model = 1;
    return 1;
}

static int parse_desirability()
{
    if (!g_parse_state.definition) {
        log_error("Encountered desirability definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_desirability) {
        log_error("BuildingType xml contains duplicate desirability nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_desirability = 1;
    g_parse_state.parsing_desirability = 1;
    g_parse_state.saw_desirability_value = 0;
    g_parse_state.saw_desirability_step = 0;
    g_parse_state.saw_desirability_step_size = 0;
    g_parse_state.saw_desirability_range = 0;
    return 1;
}

static void finish_desirability()
{
    if (!g_parse_state.parsing_desirability) {
        return;
    }
    if (!g_parse_state.saw_desirability_value || !g_parse_state.saw_desirability_step ||
        !g_parse_state.saw_desirability_step_size || !g_parse_state.saw_desirability_range) {
        log_error("BuildingType desirability is missing required child nodes", g_parse_state.definition ? g_parse_state.definition->attr() : 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_desirability = 0;
}

static int parse_desirability_numeric_child(const char *node_name, int *saw_node, int allow_negative,
    void (BuildingType::*setter)(int))
{
    if (!g_parse_state.definition || !g_parse_state.parsing_desirability) {
        log_error("Encountered desirability child outside desirability node", node_name, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (*saw_node) {
        log_error("BuildingType desirability contains duplicate child nodes", node_name, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType desirability child is missing required attribute 'value'", node_name, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *value_text = xml_parser_get_attribute_string("value");
    int value = 0;
    if (!xml_value::parse_int_strict(value_text, &value) || (!allow_negative && value < 0)) {
        log_error("Unsupported BuildingType desirability numeric value", node_name, value);
        g_parse_state.error = 1;
        return 0;
    }

    (g_parse_state.definition.get()->*setter)(value);
    *saw_node = 1;
    return 1;
}

static int parse_desirability_value()
{
    return parse_desirability_numeric_child("value", &g_parse_state.saw_desirability_value, 1,
        &BuildingType::set_model_desirability_value);
}

static int parse_desirability_step()
{
    return parse_desirability_numeric_child("step", &g_parse_state.saw_desirability_step, 0,
        &BuildingType::set_model_desirability_step);
}

static int parse_desirability_step_size()
{
    return parse_desirability_numeric_child("step_size", &g_parse_state.saw_desirability_step_size, 1,
        &BuildingType::set_model_desirability_step_size);
}

static int parse_desirability_range()
{
    return parse_desirability_numeric_child("range", &g_parse_state.saw_desirability_range, 0,
        &BuildingType::set_model_desirability_range);
}

static int parse_foundation()
{
    if (!g_parse_state.definition) {
        log_error("Encountered foundation definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_foundation) {
        log_error("BuildingType xml contains duplicate foundation nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    std::string path;
    if (!xml_definition::parse_required_nonempty_string_attribute("path", &path) ||
        xml_parser_has_attribute("policy") || xml_parser_has_attribute("open_water")) {
        log_error("BuildingType foundation requires only an external path; inline policy attributes were removed",
            g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_foundation_reference(xml_definition::normalize_path(path.c_str()));
    if (xml_parser_has_attribute("replaces")) {
        const std::string replacements = xml_parser_get_attribute_string("replaces");
        size_t start = 0;
        while (start <= replacements.size()) {
            const size_t separator = replacements.find('|', start);
            std::string replacement = xml_value::trim_copy(replacements.substr(
                start,
                separator == std::string::npos ? std::string::npos : separator - start));
            if (!g_parse_state.definition->add_foundation_replacement_reference(std::move(replacement))) {
                log_error("BuildingType foundation contains an empty or duplicate replacement type",
                    g_parse_state.definition->attr(), 0);
                g_parse_state.error = 1;
                return 0;
            }
            if (separator == std::string::npos) {
                break;
            }
            start = separator + 1;
        }
    }
    g_parse_state.saw_foundation = 1;
    return 1;
}

int building_type_geometry_attributes_are_valid_for_test(
    const char *foundation_path,
    const char *foundation_policy,
    const char *foundation_open_water,
    const char *model_size)
{
    return foundation_path && *foundation_path &&
        !foundation_policy && !foundation_open_water && !model_size;
}

static int parse_button()
{
    if (!g_parse_state.definition) {
        log_error("Encountered button definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    BuildButtonDefinition button;
    int any_value = 0;
    if (xml_parser_has_attribute("group")) {
        std::string group = xml_value::trim_copy(xml_parser_get_attribute_string("group"));
        if (group.empty()) {
            log_error("Unsupported BuildingType button group", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        button.set_group(std::move(group));
        any_value = 1;
    }

    int order = 0;
    int has_order = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType button numeric attribute", "order", &order, &has_order)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_order) {
        button.set_order(order);
        any_value = 1;
    }

    if (xml_parser_has_attribute("icon")) {
        std::string icon = xml_value::trim_copy(xml_parser_get_attribute_string("icon"));
        if (icon.empty()) {
            log_error("Unsupported BuildingType button icon", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        button.set_icon(std::move(icon));
        any_value = 1;
    }

    if (xml_parser_has_attribute("icon_image")) {
        if (!xml_parser_has_attribute("icon")) {
            log_error("BuildingType button icon_image requires icon", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        std::string icon_image = xml_value::trim_copy(xml_parser_get_attribute_string("icon_image"));
        if (icon_image.empty()) {
            log_error("Unsupported BuildingType button icon_image", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        button.set_icon_image(std::move(icon_image));
        any_value = 1;
    }

    if (xml_parser_has_attribute("text_key")) {
        std::string text_key = xml_value::trim_copy(xml_parser_get_attribute_string("text_key"));
        if (text_key.empty()) {
            log_error("Unsupported BuildingType button text_key", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        button.set_text_key(std::move(text_key));
        any_value = 1;
    }

    if (!any_value) {
        log_error("BuildingType button is missing supported attributes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_button(std::move(button));
    g_parse_state.saw_button = 1;
    return 1;
}

static int parse_cycle()
{
    if (!g_parse_state.definition) {
        log_error("Encountered cycle definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_cycle) {
        log_error("BuildingType xml contains duplicate cycle nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("group") || !xml_parser_has_attribute("order")) {
        log_error("BuildingType cycle requires group and order", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string group = xml_value::trim_copy(xml_parser_get_attribute_string("group"));
    if (group.empty()) {
        log_error("Unsupported BuildingType cycle group", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int order = 0;
    int has_order = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType cycle order", "order", &order, &has_order) ||
        !has_order || order < 0) {
        g_parse_state.error = 1;
        return 0;
    }

    int steps = 1;
    int has_steps = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType cycle steps", "steps", &steps, &has_steps)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_steps && steps <= 0) {
        log_error("Unsupported BuildingType cycle steps", g_parse_state.definition->attr(), steps);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_cycle_group(std::move(group));
    g_parse_state.definition->set_cycle_order(order);
    g_parse_state.definition->set_cycle_steps(steps);
    g_parse_state.saw_cycle = 1;
    return 1;
}

static BridgeType parse_bridge_type(const char *text)
{
    if (compare_text(text, "low") == 0) {
        return BridgeType::Low;
    }
    if (compare_text(text, "ship") == 0) {
        return BridgeType::Ship;
    }
    return BridgeType::None;
}

int bridge_definition_attribute_is_valid_for_test(const char *type)
{
    return type && parse_bridge_type(type) != BridgeType::None;
}

static RubbleType parse_rubble_type(const char *text)
{
    if (compare_text(text, "rubble") == 0) {
        return RubbleType::Rubble;
    }
    if (compare_text(text, "burning_ruin") == 0) {
        return RubbleType::BurningRubble;
    }
    return RubbleType::None;
}

static int parse_rubble()
{
    if (!g_parse_state.definition) {
        log_error("Encountered rubble definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_rubble) {
        log_error("BuildingType xml contains duplicate rubble nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("kind")) {
        log_error("BuildingType rubble is missing required attribute 'kind'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *kind_text = xml_parser_get_attribute_string("kind");
    RubbleType type = parse_rubble_type(kind_text);
    if (type == RubbleType::None) {
        log_error("Unsupported BuildingType rubble kind", kind_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_rubble(type);
    int burn_days = 0;
    int has_burn_days = 0;
    if (!parse_optional_int_attribute("BuildingType rubble burn_days must be an integer", "burn_days", &burn_days, &has_burn_days)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_burn_days) {
        if (burn_days <= 0) {
            log_error("BuildingType rubble burn_days must be positive", g_parse_state.definition->attr(), burn_days);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_rubble_burn_days(burn_days);
    }

    if (xml_parser_has_attribute("decays_to")) {
        std::string decay_attr = xml_value::trim_copy(xml_parser_get_attribute_string("decays_to"));
        if (decay_attr.empty()) {
            log_error("BuildingType rubble decays_to cannot be empty", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_rubble_decay_reference(std::move(decay_attr));
    }

    if (type == RubbleType::BurningRubble && (!has_burn_days || g_parse_state.definition->rubble().decays_to.empty())) {
        log_error("Burning rubble requires burn_days and decays_to", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_rubble = 1;
    return 1;
}

static int parse_bridge()
{
    if (!g_parse_state.definition) {
        log_error("Encountered bridge definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_bridge) {
        log_error("BuildingType xml contains duplicate bridge nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("type")) {
        log_error("BuildingType bridge is missing required attribute 'type'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    const char *type_text = xml_parser_get_attribute_string("type");
    const BridgeType bridge_type = parse_bridge_type(type_text);
    if (bridge_type == BridgeType::None) {
        log_error("Unsupported BuildingType bridge type", type_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_bridge_type(bridge_type);
    g_parse_state.saw_bridge = 1;
    return 1;
}

static TileKind parse_tile_kind(const char *text)
{
    if (compare_text(text, "garden") == 0) {
        return TileKind::Garden;
    }
    if (compare_text(text, "plaza") == 0) {
        return TileKind::Plaza;
    }
    if (compare_text(text, "roadblock") == 0) {
        return TileKind::Roadblock;
    }
    return TileKind::None;
}

static TileRefreshBehavior parse_tile_refresh_behavior(const char *text, int *ok)
{
    *ok = 1;
    if (compare_text(text, "none") == 0) {
        return TileRefreshBehavior::None;
    }
    if (compare_text(text, "garden") == 0) {
        return TileRefreshBehavior::Garden;
    }
    if (compare_text(text, "plaza") == 0) {
        return TileRefreshBehavior::Plaza;
    }
    *ok = 0;
    return TileRefreshBehavior::None;
}

static TilePlacementBehavior parse_tile_placement_behavior(const char *text, int *ok)
{
    *ok = 1;
    if (compare_text(text, "none") == 0) {
        return TilePlacementBehavior::None;
    }
    if (compare_text(text, "garden") == 0) {
        return TilePlacementBehavior::Garden;
    }
    if (compare_text(text, "plaza") == 0) {
        return TilePlacementBehavior::Plaza;
    }
    *ok = 0;
    return TilePlacementBehavior::None;
}

static int parse_tile()
{
    if (!g_parse_state.definition) {
        log_error("Encountered tile definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_tile) {
        log_error("BuildingType xml contains duplicate tile nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("kind")) {
        log_error("BuildingType tile is missing required attribute 'kind'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string kind_key = xml_value::trim_copy(xml_parser_get_attribute_string("kind"));
    if (kind_key.empty()) {
        log_error("BuildingType tile kind is empty", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    const char *kind_text = kind_key.c_str();
    TileKind kind = parse_tile_kind(kind_text);

    g_parse_state.definition->set_tile_kind_key(std::move(kind_key));
    g_parse_state.definition->set_tile_kind(kind);
    if (xml_parser_has_attribute("refresh")) {
        int ok = 0;
        TileRefreshBehavior behavior =
            parse_tile_refresh_behavior(xml_parser_get_attribute_string("refresh"), &ok);
        if (!ok) {
            log_error("Unsupported BuildingType tile refresh behavior",
                xml_parser_get_attribute_string("refresh"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_tile_refresh_behavior(behavior);
    }
    if (xml_parser_has_attribute("placement")) {
        int ok = 0;
        TilePlacementBehavior behavior =
            parse_tile_placement_behavior(xml_parser_get_attribute_string("placement"), &ok);
        if (!ok) {
            log_error("Unsupported BuildingType tile placement behavior",
                xml_parser_get_attribute_string("placement"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_tile_placement_behavior(behavior);
    }
    if (xml_parser_has_attribute("overgrown")) {
        int overgrown = 0;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("overgrown"), &overgrown)) {
            log_error("BuildingType tile overgrown attribute must be true or false",
                g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_tile_overgrown(overgrown);
    }
    g_parse_state.saw_tile = 1;
    g_parse_state.parsing_tile = 1;
    return 1;
}

static ConstructionToolKind parse_tool_kind(const char *text)
{
    if (compare_text(text, "clear_land") == 0) {
        return ConstructionToolKind::ClearLand;
    }
    if (compare_text(text, "clear_trees") == 0) {
        return ConstructionToolKind::ClearTrees;
    }
    if (compare_text(text, "repair_land") == 0) {
        return ConstructionToolKind::RepairLand;
    }
    if (compare_text(text, "road") == 0) {
        return ConstructionToolKind::Road;
    }
    if (compare_text(text, "highway") == 0) {
        return ConstructionToolKind::Highway;
    }
    if (compare_text(text, "wall") == 0) {
        return ConstructionToolKind::Wall;
    }
    if (compare_text(text, "roadblock") == 0) {
        return ConstructionToolKind::Roadblock;
    }
    if (compare_text(text, "aqueduct") == 0) {
        return ConstructionToolKind::Aqueduct;
    }
    if (compare_text(text, "draggable_reservoir") == 0) {
        return ConstructionToolKind::DraggableReservoir;
    }
    if (compare_text(text, "draggable_building") == 0) {
        return ConstructionToolKind::DraggableBuilding;
    }
    return ConstructionToolKind::None;
}

static ConstructionDragTerrain parse_tool_drag_terrain(const char *text)
{
    if (compare_text(text, "land") == 0) {
        return ConstructionDragTerrain::Land;
    }
    if (compare_text(text, "land_or_road") == 0) {
        return ConstructionDragTerrain::LandOrRoad;
    }
    return ConstructionDragTerrain::Land;
}

static ConstructionDragRotation parse_tool_drag_rotation(const char *text)
{
    if (compare_text(text, "none") == 0) {
        return ConstructionDragRotation::None;
    }
    if (compare_text(text, "path") == 0) {
        return ConstructionDragRotation::Path;
    }
    if (compare_text(text, "hedge") == 0) {
        return ConstructionDragRotation::Hedge;
    }
    if (compare_text(text, "variant") == 0) {
        return ConstructionDragRotation::Variant;
    }
    if (compare_text(text, "pair") == 0) {
        return ConstructionDragRotation::Pair;
    }
    return ConstructionDragRotation::None;
}

static SmartToolModifier parse_smart_tool_modifier(const char *text, int *ok)
{
    *ok = 0;
    if (!text) {
        return SmartToolModifier::Any;
    }
    if (compare_text(text, "any") == 0) {
        *ok = 1;
        return SmartToolModifier::Any;
    }
    if (compare_text(text, "ctrl") == 0 || compare_text(text, "control") == 0) {
        *ok = 1;
        return SmartToolModifier::Control;
    }
    if (compare_text(text, "shift") == 0) {
        *ok = 1;
        return SmartToolModifier::Shift;
    }
    return SmartToolModifier::Any;
}

static SmartToolTarget parse_smart_tool_target(const char *text, int *ok)
{
    *ok = 0;
    if (!text) {
        return SmartToolTarget::DragArea;
    }
    if (compare_text(text, "drag") == 0) {
        *ok = 1;
        return SmartToolTarget::DragArea;
    }
    if (compare_text(text, "single") == 0) {
        *ok = 1;
        return SmartToolTarget::SingleTarget;
    }
    return SmartToolTarget::DragArea;
}

static SmartToolContext parse_smart_tool_context(const char *text, int *ok)
{
    *ok = 0;
    if (!text) {
        return SmartToolContext::Default;
    }
    if (compare_text(text, "default") == 0) {
        *ok = 1;
        return SmartToolContext::Default;
    }
    if (compare_text(text, "water") == 0) {
        *ok = 1;
        return SmartToolContext::HoverWater;
    }
    if (compare_text(text, "route") == 0) {
        *ok = 1;
        return SmartToolContext::RoutedSegment;
    }
    return SmartToolContext::Default;
}

int smart_tool_mode_attributes_are_valid_for_test(
    const char *modifier,
    const char *type,
    const char *target,
    int footprint_size,
    const char *context)
{
    int modifier_ok = 0;
    int target_ok = 0;
    int context_ok = 0;
    const SmartToolModifier parsed_modifier = parse_smart_tool_modifier(modifier, &modifier_ok);
    const SmartToolTarget parsed_target = parse_smart_tool_target(target, &target_ok);
    const SmartToolContext parsed_context = parse_smart_tool_context(context, &context_ok);
    (void) parsed_modifier;
    (void) parsed_context;
    return modifier_ok && target_ok && context_ok && type && *type && footprint_size > 0 &&
        (parsed_target != SmartToolTarget::SingleTarget || footprint_size == 1);
}

static int parse_tool()
{
    if (!g_parse_state.definition) {
        log_error("Encountered tool definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_tool) {
        log_error("BuildingType xml contains duplicate tool nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("kind")) {
        log_error("BuildingType tool is missing required attribute 'kind'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *kind_text = xml_parser_get_attribute_string("kind");
    ConstructionToolKind kind = parse_tool_kind(kind_text);
    if (kind == ConstructionToolKind::None) {
        log_error("Unsupported BuildingType tool kind", kind_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_tool_kind(kind);
    if (kind == ConstructionToolKind::DraggableBuilding) {
        if (!xml_parser_has_attribute("drag_terrain") || !xml_parser_has_attribute("rotation")) {
            log_error("Draggable BuildingType tool requires drag_terrain and rotation", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        const char *terrain_text = xml_parser_get_attribute_string("drag_terrain");
        const char *rotation_text = xml_parser_get_attribute_string("rotation");
        ConstructionDragTerrain terrain = parse_tool_drag_terrain(terrain_text);
        if (terrain == ConstructionDragTerrain::Land && compare_text(terrain_text, "land") != 0) {
            log_error("Unsupported draggable BuildingType tool terrain", terrain_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        ConstructionDragRotation rotation = parse_tool_drag_rotation(rotation_text);
        if (rotation == ConstructionDragRotation::None && compare_text(rotation_text, "none") != 0) {
            log_error("Unsupported draggable BuildingType tool rotation", rotation_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_tool_drag_terrain(terrain);
        g_parse_state.definition->set_tool_drag_rotation(rotation);
    } else if (xml_parser_has_attribute("drag_terrain") || xml_parser_has_attribute("rotation")) {
        log_error("Only draggable BuildingType tools may define drag_terrain or rotation", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_tool = 1;
    return 1;
}

static int parse_smart_tool_mode()
{
    if (!g_parse_state.definition || !g_parse_state.saw_tool) {
        log_error("Encountered smart-tool mode before tool definition", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("modifier") || !xml_parser_has_attribute("type")) {
        log_error("Smart-tool mode requires modifier and type", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *modifier_text = xml_parser_get_attribute_string("modifier");
    const char *target_text = xml_parser_has_attribute("target") ?
        xml_parser_get_attribute_string("target") : "drag";
    const char *context_text = xml_parser_has_attribute("context") ?
        xml_parser_get_attribute_string("context") : "default";
    std::string type_reference = xml_value::trim_copy(xml_parser_get_attribute_string("type"));
    int modifier_ok = 0;
    int target_ok = 0;
    int context_ok = 0;
    SmartToolModeDefinition mode;
    mode.modifier = parse_smart_tool_modifier(modifier_text, &modifier_ok);
    mode.target = parse_smart_tool_target(target_text, &target_ok);
    mode.context = parse_smart_tool_context(context_text, &context_ok);
    int has_footprint = 0;
    if (!parse_optional_int_attribute(
            "Smart-tool footprint must be an integer", "footprint", &mode.footprint_size, &has_footprint)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (!has_footprint) {
        mode.footprint_size = 1;
    }
    if (!modifier_ok || !target_ok || !context_ok || type_reference.empty() || mode.footprint_size <= 0 ||
        (mode.target == SmartToolTarget::SingleTarget && mode.footprint_size != 1)) {
        log_error("Invalid smart-tool mode", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    for (const SmartToolModeDefinition &existing : g_parse_state.definition->tool().smart_tool().modes()) {
        if (existing.context == mode.context && existing.modifier == mode.modifier) {
            log_error("Smart-tool contains duplicate context/modifier modes", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }
    mode.type_reference = std::move(type_reference);
    g_parse_state.definition->add_tool_smart_mode(std::move(mode));
    return 1;
}

static int parse_temple()
{
    if (!g_parse_state.definition) {
        log_error("Encountered temple definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_temple) {
        log_error("BuildingType xml contains duplicate temple nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("religion")) {
        log_error("BuildingType temple is missing required attribute 'religion'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = xml_definition::normalize_path(xml_parser_get_attribute_string("religion"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType temple religion path", xml_parser_get_attribute_string("religion"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_temple_religion_reference(std::move(normalized_path));
    g_parse_state.saw_temple = 1;
    return 1;
}

static void finish_tile()
{
    if (!g_parse_state.parsing_tile) {
        return;
    }
    g_parse_state.parsing_tile = 0;
}

static int parse_sound_city_name(const char *name)
{
    struct named_sound {
        const char *name;
        int sound;
    };
    static const named_sound sound_names[] = {
        {"none", SOUND_CITY_NONE},
        {"academy", SOUND_CITY_ACADEMY},
        {"actor_colony", SOUND_CITY_ACTOR_COLONY},
        {"amphitheater", SOUND_CITY_AMPHITHEATER},
        {"aqueduct", SOUND_CITY_AQUEDUCT},
        {"arena", SOUND_CITY_ARENA},
        {"armoury", SOUND_CITY_ARMOURY},
        {"barber", SOUND_CITY_BARBER},
        {"bathhouse", SOUND_CITY_BATHHOUSE},
        {"brickworks", SOUND_CITY_BRICKWORKS},
        {"caravanserai", SOUND_CITY_CARAVANSERAI},
        {"chariot_maker", SOUND_CITY_CHARIOT_MAKER},
        {"clay_pit", SOUND_CITY_CLAY_PIT},
        {"clinic", SOUND_CITY_CLINIC},
        {"colosseum", SOUND_CITY_COLOSSEUM},
        {"concrete_maker", SOUND_CITY_CONCRETE_MAKER},
        {"city_mint", SOUND_CITY_CITY_MINT},
        {"city_depot", SOUND_CITY_DEPOT},
        {"engineers_post", SOUND_CITY_ENGINEERS_POST},
        {"forum", SOUND_CITY_FORUM},
        {"fountain", SOUND_CITY_FOUNTAIN},
        {"fruit_farm", SOUND_CITY_FRUIT_FARM},
        {"furniture_workshop", SOUND_CITY_FURNITURE_WORKSHOP},
        {"gladiator_school", SOUND_CITY_GLADIATOR_SCHOOL},
        {"house_slum", SOUND_CITY_HOUSE_SLUM},
        {"house_poor", SOUND_CITY_HOUSE_POOR},
        {"house_medium", SOUND_CITY_HOUSE_MEDIUM},
        {"house_good", SOUND_CITY_HOUSE_GOOD},
        {"house_posh", SOUND_CITY_HOUSE_POSH},
        {"iron_mine", SOUND_CITY_IRON_MINE},
        {"hospital", SOUND_CITY_HOSPITAL},
        {"hippodrome", SOUND_CITY_HIPPODROME},
        {"library", SOUND_CITY_LIBRARY},
        {"lighthouse", SOUND_CITY_LIGHTHOUSE},
        {"lion_pit", SOUND_CITY_LION_PIT},
        {"market", SOUND_CITY_MARKET},
        {"marble_quarry", SOUND_CITY_QUARRY},
        {"native_decoration", SOUND_CITY_NATIVE_DECORATION},
        {"native_hut", SOUND_CITY_NATIVE_HUT},
        {"oil_workshop", SOUND_CITY_OIL_WORKSHOP},
        {"olive_farm", SOUND_CITY_OLIVE_FARM},
        {"oracle", SOUND_CITY_ORACLE},
        {"palace", SOUND_CITY_PALACE},
        {"pig_farm", SOUND_CITY_PIG_FARM},
        {"pottery_workshop", SOUND_CITY_POTTERY_WORKSHOP},
        {"prefecture", SOUND_CITY_PREFECTURE},
        {"quarry", SOUND_CITY_QUARRY},
        {"reservoir", SOUND_CITY_RESERVOIR},
        {"school", SOUND_CITY_SCHOOL},
        {"senate", SOUND_CITY_SENATE},
        {"tavern", SOUND_CITY_TAVERN},
        {"temple_ceres", SOUND_CITY_TEMPLE_CERES},
        {"temple_mars", SOUND_CITY_TEMPLE_MARS},
        {"temple_mercury", SOUND_CITY_TEMPLE_MERCURY},
        {"temple_neptune", SOUND_CITY_TEMPLE_NEPTUNE},
        {"temple_venus", SOUND_CITY_TEMPLE_VENUS},
        {"theater", SOUND_CITY_THEATER},
        {"timber_yard", SOUND_CITY_TIMBER_YARD},
        {"vegetable_farm", SOUND_CITY_VEGETABLE_FARM},
        {"vine_farm", SOUND_CITY_VINE_FARM},
        {"watchtower", SOUND_CITY_WATCHTOWER},
        {"weapons_workshop", SOUND_CITY_WEAPONS_WORKSHOP},
        {"well", SOUND_CITY_WELL},
        {"wharf", SOUND_CITY_WHARF},
        {"wheat_farm", SOUND_CITY_WHEAT_FARM},
        {"wine_workshop", SOUND_CITY_WINE_WORKSHOP},
        {"workcamp", SOUND_CITY_WORKCAMP}
    };

    for (const named_sound &sound : sound_names) {
        if (name && compare_text(name, sound.name) == 0) {
            return sound.sound;
        }
    }
    return -1;
}

static int parse_sound()
{
    if (!g_parse_state.definition) {
        log_error("Encountered sound definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_sound) {
        log_error("BuildingType xml contains duplicate sound nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *sound_attr = 0;
    if (xml_parser_has_attribute("id")) {
        sound_attr = xml_parser_get_attribute_string("id");
    } else if (xml_parser_has_attribute("value")) {
        sound_attr = xml_parser_get_attribute_string("value");
    } else if (xml_parser_has_attribute("city")) {
        sound_attr = xml_parser_get_attribute_string("city");
    }

    std::string sound_name = xml_value::trim_copy(sound_attr ? sound_attr : "");
    int sound = parse_sound_city_name(sound_name.c_str());
    if (sound < 0) {
        log_error("Unsupported BuildingType sound id", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_sound_id(sound);

    int flag_value = 0;
    if (xml_parser_has_attribute("mute_on_enemies")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("mute_on_enemies"), &flag_value)) {
            log_error("Unsupported BuildingType sound mute_on_enemies", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_sound_mute_on_enemies(flag_value);
    }
    if (xml_parser_has_attribute("always_play")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("always_play"), &flag_value)) {
            log_error("Unsupported BuildingType sound always_play", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_sound_always_play(flag_value);
    }

    g_parse_state.saw_sound = 1;
    return 1;
}

static int parse_market()
{
    if (!g_parse_state.definition) {
        log_error("Encountered market definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_market) {
        log_error("BuildingType xml contains duplicate market nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int max_distance = 0;
    int has_max_distance = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType market numeric attribute", "max_distance",
            &max_distance, &has_max_distance)) {
        g_parse_state.error = 1;
        return 0;
    }
    int max_food_stock = 0;
    int has_max_food_stock = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType market numeric attribute", "max_food_stock",
            &max_food_stock, &has_max_food_stock)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (!has_max_distance || max_distance <= 0) {
        log_error("BuildingType market is missing positive required attribute 'max_distance'",
            g_parse_state.definition->attr(), max_distance);
        g_parse_state.error = 1;
        return 0;
    }
    if (!has_max_food_stock || max_food_stock <= 0) {
        log_error("BuildingType market is missing positive required attribute 'max_food_stock'",
            g_parse_state.definition->attr(), max_food_stock);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_market_max_distance(max_distance);
    g_parse_state.definition->set_market_max_food_stock(max_food_stock);
    g_parse_state.saw_market = 1;
    return 1;
}

static int parse_flags()
{
    if (!g_parse_state.definition) {
        log_error("Encountered flags definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_flags) {
        log_error("BuildingType xml contains duplicate flags nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int value = 0;
    int has_value = 0;
    int any_value = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType flags numeric attribute", "fire_proof", &value, &has_value)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_value) {
        g_parse_state.definition->set_fire_proof(value ? 1 : 0);
        any_value = 1;
    }
    if (!parse_optional_int_attribute("Unsupported BuildingType flags numeric attribute", "draw_desirability_range", &value, &has_value)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_value) {
        g_parse_state.definition->set_draw_desirability_range(value ? 1 : 0);
        any_value = 1;
    }
    if (!parse_optional_int_attribute("Unsupported BuildingType flags numeric attribute", "venus_gt_bonus", &value, &has_value)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_value) {
        g_parse_state.definition->set_venus_gt_bonus(value ? 1 : 0);
        any_value = 1;
    }
    if (!any_value) {
        log_error("BuildingType flags is missing supported attributes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_flags = 1;
    return 1;
}

static int parse_military()
{
    if (!g_parse_state.definition) {
        log_error("Encountered military definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_military || g_parse_state.parsing_military) {
        log_error("BuildingType xml contains duplicate military nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_military = 1;
    g_parse_state.parsing_military = 1;
    return 1;
}

static void finish_military()
{
    if (!g_parse_state.parsing_military) {
        return;
    }
    if (!g_parse_state.saw_military_formation) {
        log_error("BuildingType military node is missing formation", g_parse_state.definition ? g_parse_state.definition->attr() : 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_military = 0;
}

static int parse_military_formation()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_military) {
        log_error("Encountered military formation outside military node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_military_formation) {
        log_error("BuildingType military node contains duplicate formation", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    std::string key;
    if (!xml_definition::parse_required_nonempty_string_attribute("key", &key)) {
        log_error("BuildingType military formation requires a non-empty key", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->set_military_formation_reference(std::move(key));
    g_parse_state.saw_military_formation = 1;
    return 1;
}

static int parse_composed_offset_attributes(const char *scope, int *out_x, int *out_y, int *out_has_offset)
{
    *out_x = 0;
    *out_y = 0;
    *out_has_offset = 0;
    const int has_x = xml_parser_has_attribute("x");
    const int has_y = xml_parser_has_attribute("y");
    if (has_x != has_y) {
        log_error("BuildingType composed offset requires both x and y", scope, 0);
        return 0;
    }
    if (!has_x) {
        return 1;
    }
    const char *x_text = xml_parser_get_attribute_string("x");
    const char *y_text = xml_parser_get_attribute_string("y");
    if (!xml_value::parse_int_strict(x_text, out_x) || !xml_value::parse_int_strict(y_text, out_y)) {
        log_error("Unsupported BuildingType composed offset", scope, 0);
        return 0;
    }
    *out_has_offset = 1;
    return 1;
}

static int parse_composed_rotation(int *out_rotation)
{
    if (!xml_parser_has_attribute("rotation")) {
        log_error("BuildingType composed offset is missing required attribute 'rotation'", 0, 0);
        return 0;
    }
    const char *rotation_text = xml_parser_get_attribute_string("rotation");
    if (!xml_value::parse_int_strict(rotation_text, out_rotation) || *out_rotation < 0 || *out_rotation > 3) {
        log_error("Unsupported BuildingType composed offset rotation", rotation_text, 0);
        return 0;
    }
    return 1;
}

static int parse_composition()
{
    if (!g_parse_state.definition || g_parse_state.saw_composed) {
        log_error("Invalid or duplicate BuildingType composition node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_composed = 1;
    g_parse_state.parsing_composed = 1;
    return 1;
}

static void finish_composition()
{
    if (!g_parse_state.definition || !g_parse_state.definition->composition().has_any()) {
        log_error("BuildingType composition is missing child nodes", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_composed = 0;
}

static int parse_composition_child()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_composed ||
        g_parse_state.current_composition_child ||
        !xml_parser_has_attribute("type") || !xml_parser_has_attribute("role")) {
        log_error("Invalid BuildingType composition child", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    std::string type_reference =
        xml_definition::normalize_path(xml_parser_get_attribute_string("type"));
    std::string role = xml_value::trim_copy(xml_parser_get_attribute_string("role"));
    if (type_reference.empty() || role.empty()) {
        log_error("BuildingType composition child has empty type or role", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    CompositionChildDef &child =
        g_parse_state.definition->composition().add_child(std::move(type_reference), std::move(role));
    int x = 0;
    int y = 0;
    int has_offset = 0;
    if (!parse_composed_offset_attributes(g_parse_state.definition->attr(), &x, &y, &has_offset)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_offset) {
        child.set_canonical_offset(x, y);
    }
    if (xml_parser_has_attribute("orientation")) {
        const char *orientation = xml_parser_get_attribute_string("orientation");
        if (xml_value::equals(orientation, "inherit_owner")) {
            child.orientation = CompositionChildOrientation::InheritOwner;
        } else if (!xml_value::equals(orientation, "canonical")) {
            log_error("Unsupported composition child orientation", orientation, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }
    g_parse_state.current_composition_child = &child;
    return 1;
}

static void finish_composition_child()
{
    CompositionChildDef *child = g_parse_state.current_composition_child;
    if (!child || !child->has_canonical_offset() || child->has_partial_rotation_overrides()) {
        log_error("Composition child requires a canonical offset and complete optional overrides", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.current_composition_child = nullptr;
}

static int parse_composition_offset()
{
    if (!g_parse_state.current_composition_child) {
        log_error("Composition offset appears outside a child", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    int rotation = 0;
    int x = 0;
    int y = 0;
    int has_offset = 0;
    if (!parse_composed_rotation(&rotation) ||
        !parse_composed_offset_attributes(g_parse_state.definition->attr(), &x, &y, &has_offset) ||
        !has_offset) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_composition_child->set_rotation_override(rotation, x, y);
    return 1;
}

static int parse_graphics()
{
    if (!g_parse_state.definition) {
        log_error("Encountered graphics definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.parsing_construction_phase) {
        if (g_parse_state.saw_construction_phase_graphics) {
            log_error("BuildingType construction phase contains duplicate graphics nodes", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.saw_construction_phase_graphics = 1;
        g_parse_state.parsing_graphics = 1;
        g_parse_state.has_current_graphics_variant = 0;
        g_parse_state.current_graphics_variant_index = 0;
        g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::ConstructionPhase;
        const ConstructionPhase *phase = g_parse_state.definition->last_construction_phase();
        if (!phase) {
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.graphics_definition.add_construction_phase(phase->index);
        return 1;
    }
    if (g_parse_state.saw_graphic) {
        log_error("BuildingType xml contains duplicate graphics nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_graphic = 1;
    g_parse_state.parsing_graphics = 1;
    g_parse_state.has_current_graphics_variant = 0;
    g_parse_state.current_graphics_variant_index = 0;
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::None;
    return 1;
}

static ConstructionConfigFlag parse_construction_config_flag(const char *text)
{
    if (compare_text(text, "none") == 0) {
        return ConstructionConfigFlag::None;
    }
    if (compare_text(text, "multiple_barracks") == 0) {
        return ConstructionConfigFlag::MultipleBarracks;
    }
    return ConstructionConfigFlag::None;
}

static void finish_graphics()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }

    if (g_parse_state.current_graphics_target_scope == GraphicsParseTargetScope::ConstructionPhase) {
        const GraphicsTarget *phase = g_parse_state.graphics_definition.last_construction_phase();
        if (!phase || (!phase->has_path() && !phase->has_options() && !phase->is_resource_storage())) {
            log_error("BuildingType construction phase graphics is missing required child node 'path'", 0, 0);
            g_parse_state.error = 1;
        }
    } else if (!g_parse_state.graphics_definition.has_default_node()) {
        log_error("BuildingType graphics is missing required child node 'default'", 0, 0);
        g_parse_state.error = 1;
    } else if (!g_parse_state.graphics_definition.default_target().has_path() &&
        !g_parse_state.graphics_definition.default_target().has_options() &&
        !g_parse_state.graphics_definition.default_target().is_resource_storage()) {
        log_error("BuildingType graphics is missing required child node 'path'", 0, 0);
        g_parse_state.error = 1;
    }

    g_parse_state.parsing_graphics = 0;
    g_parse_state.parsing_graphics_options = 0;
    g_parse_state.has_current_graphics_variant = 0;
    g_parse_state.current_graphics_variant_index = 0;
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::None;
}

static int parse_construction()
{
    if (!g_parse_state.definition) {
        log_error("Encountered construction definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_construction) {
        log_error("BuildingType xml contains duplicate construction nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *mode_text = xml_parser_has_attribute("mode") ? xml_parser_get_attribute_string("mode") : "instant";
    if (compare_text(mode_text, "instant") == 0) {
        g_parse_state.definition->set_construction_mode(ConstructionMode::Instant);
    } else if (compare_text(mode_text, "phased") == 0) {
        g_parse_state.definition->set_construction_mode(ConstructionMode::Phased);
    } else {
        log_error("Unsupported BuildingType construction mode", mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int radius = 0;
    int has_radius = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType construction numeric attribute", "road_update_radius", &radius, &has_radius)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_radius) {
        if (radius < 0) {
            log_error("Unsupported BuildingType construction road_update_radius", g_parse_state.definition->attr(), radius);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_construction_road_update_radius(radius);
    }

    int free_when_broke_limit = 0;
    int has_free_when_broke_limit = 0;
    if (!parse_optional_int_attribute(
            "Unsupported BuildingType construction numeric attribute",
            "free_when_broke_limit",
            &free_when_broke_limit,
            &has_free_when_broke_limit)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_free_when_broke_limit) {
        if (free_when_broke_limit < 0) {
            log_error(
                "Unsupported BuildingType construction free_when_broke_limit",
                g_parse_state.definition->attr(),
                free_when_broke_limit);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_construction_free_when_broke_limit(
            free_when_broke_limit);
    }

    int max_count = 0;
    int has_max_count = 0;
    if (!parse_optional_int_attribute(
            "Unsupported BuildingType construction numeric attribute",
            "max_count",
            &max_count,
            &has_max_count)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_max_count) {
        if (max_count < 1) {
            log_error("Unsupported BuildingType construction max_count",
                g_parse_state.definition->attr(), max_count);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_construction_max_count(max_count);
    }
    if (xml_parser_has_attribute("max_count_unless_config")) {
        if (!has_max_count) {
            log_error("BuildingType construction max_count_unless_config requires max_count",
                g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        const char *config_text = xml_parser_get_attribute_string("max_count_unless_config");
        ConstructionConfigFlag flag = parse_construction_config_flag(config_text);
        if (flag == ConstructionConfigFlag::None && compare_text(config_text, "none") != 0) {
            log_error("Unsupported BuildingType construction max_count_unless_config",
                config_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_construction_max_count_unless_config(flag);
    }
    if (xml_parser_has_attribute("requires_building")) {
        std::string type_attr = xml_value::trim_copy(
            xml_parser_get_attribute_string("requires_building"));
        if (type_attr.empty()) {
            log_error("Unsupported BuildingType construction requires_building",
                xml_parser_get_attribute_string("requires_building"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_construction_required_building_reference(
            std::move(type_attr));
    }

    g_parse_state.saw_construction = 1;
    g_parse_state.parsing_construction = 1;
    return 1;
}

static int parse_graphics_status_icon_values(const char *x_text, const char *y_text, int *x, int *y)
{
    return x_text && y_text && x && y &&
        xml_value::parse_int_strict(x_text, x) &&
        xml_value::parse_int_strict(y_text, y);
}

int graphics_status_icon_attributes_are_valid_for_test(const char *x, const char *y)
{
    int parsed_x = 0;
    int parsed_y = 0;
    return parse_graphics_status_icon_values(x, y, &parsed_x, &parsed_y);
}

static int parse_graphics_overlay_summary_policy(
    const char *text,
    GraphicsOverlaySummaryPolicy *policy)
{
    if (!text || !*text || !policy) {
        return 0;
    }
    if (compare_text(text, "composition_owner") == 0) {
        *policy = GraphicsOverlaySummaryPolicy::CompositionOwner;
        return 1;
    }
    return 0;
}

int graphics_overlay_summary_policy_is_valid_for_test(const char *mode)
{
    GraphicsOverlaySummaryPolicy policy = GraphicsOverlaySummaryPolicy::EveryDrawTile;
    return parse_graphics_overlay_summary_policy(mode, &policy);
}

static int parse_graphics_overlay_summary()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics ||
        g_parse_state.parsing_construction_phase ||
        g_parse_state.current_graphics_target_scope != GraphicsParseTargetScope::None) {
        log_error("BuildingType graphics overlay_summary must be a direct child of top-level graphics", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.graphics_definition.has_overlay_summary_policy()) {
        log_error("BuildingType graphics contains duplicate overlay_summary nodes",
            g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    GraphicsOverlaySummaryPolicy policy = GraphicsOverlaySummaryPolicy::EveryDrawTile;
    const char *mode = xml_parser_has_attribute("mode") ?
        xml_parser_get_attribute_string("mode") : nullptr;
    if (!parse_graphics_overlay_summary_policy(mode, &policy)) {
        log_error("BuildingType graphics overlay_summary has unsupported mode", mode, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.graphics_definition.set_overlay_summary_policy(policy);
    return 1;
}

static int parse_graphics_status_icon()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics ||
        g_parse_state.parsing_construction_phase ||
        g_parse_state.current_graphics_target_scope != GraphicsParseTargetScope::None) {
        log_error("BuildingType graphics status_icon must be a direct child of top-level graphics", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int existing_x = 0;
    int existing_y = 0;
    if (g_parse_state.graphics_definition.status_icon_anchor(&existing_x, &existing_y)) {
        log_error("BuildingType graphics contains duplicate status_icon nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *x_text = xml_parser_has_attribute("x") ? xml_parser_get_attribute_string("x") : nullptr;
    const char *y_text = xml_parser_has_attribute("y") ? xml_parser_get_attribute_string("y") : nullptr;
    int x = 0;
    int y = 0;
    if (!parse_graphics_status_icon_values(x_text, y_text, &x, &y)) {
        log_error("BuildingType graphics status_icon requires strict integer x and y attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.graphics_definition.set_status_icon_anchor(x, y);
    return 1;
}

static void finish_construction()
{
    if (!g_parse_state.parsing_construction) {
        return;
    }
    if (g_parse_state.definition->construction().is_phased() &&
        g_parse_state.definition->construction().phase_count() <= 0) {
        log_error("BuildingType phased construction is missing phase nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_construction = 0;
}

static int parse_construction_phase()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_construction) {
        log_error("Encountered construction phase outside construction node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!g_parse_state.definition->construction().is_phased()) {
        log_error("BuildingType construction phase is only supported in phased mode", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("index")) {
        log_error("BuildingType construction phase is missing required attribute 'index'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int index = xml_parser_get_attribute_int("index");
    int expected_index = g_parse_state.definition->construction().phase_count() + 1;
    if (index != expected_index) {
        log_error("BuildingType construction phase index must be contiguous from 1", g_parse_state.definition->attr(), index);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_construction_phase(index);
    g_parse_state.parsing_construction_phase = 1;
    g_parse_state.saw_construction_phase_graphics = 0;
    return 1;
}

static void finish_construction_phase()
{
    if (!g_parse_state.parsing_construction_phase) {
        return;
    }
    const GraphicsTarget *phase_graphics = g_parse_state.graphics_definition.last_construction_phase();
    if (!g_parse_state.saw_construction_phase_graphics || !phase_graphics ||
        (!phase_graphics->has_path() && !phase_graphics->has_options() && !phase_graphics->is_resource_storage())) {
        log_error("BuildingType construction phase is missing required graphics", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_construction_phase = 0;
    g_parse_state.saw_construction_phase_graphics = 0;
}

static int parse_construction_requirement()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_construction) {
        log_error("Encountered construction requirement outside construction node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("type") || !xml_parser_has_attribute("amount")) {
        log_error("BuildingType construction requirement is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_text = xml_parser_get_attribute_string("type");
    const bool requires_architects = type_text && compare_text(type_text, "architects") == 0;
    resource_type resource = requires_architects ? RESOURCE_NONE : resource_type_from_xml_attr(type_text);
    if (resource == RESOURCE_NONE && !requires_architects) {
        log_error("Unsupported BuildingType construction requirement type", type_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int amount = xml_parser_get_attribute_int("amount");
    if (amount < 0) {
        log_error("Unsupported BuildingType construction requirement amount", g_parse_state.definition->attr(), amount);
        g_parse_state.error = 1;
        return 0;
    }

    if (g_parse_state.parsing_construction_phase) {
        g_parse_state.definition->add_construction_requirement(resource, amount);
        return 1;
    }
    if (g_parse_state.definition->construction().is_phased()) {
        log_error("BuildingType phased construction requirements must be inside a phase", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (resource == RESOURCE_NONE) {
        log_error("BuildingType instant construction requirement must be a resource", type_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_instant_construction_requirement(resource, amount);
    return 1;
}

static int parse_graphics_default()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics default outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.graphics_definition.has_default_node()) {
        log_error("BuildingType graphics contains duplicate default nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.graphics_definition.mark_default_node();
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::Default;
    GraphicsTarget &target = g_parse_state.graphics_definition.default_target();
    if (xml_parser_has_attribute("animation")) {
        int enabled = 1;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("animation"), &enabled)) {
            log_error("Unsupported BuildingType graphics default animation flag", xml_parser_get_attribute_string("animation"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        target.set_animation_enabled(enabled);
    }
    if (xml_parser_has_attribute("use_terrain_as_foundation")) {
        int enabled = 1;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("use_terrain_as_foundation"), &enabled)) {
            log_error("Unsupported BuildingType graphics default terrain foundation flag",
                xml_parser_get_attribute_string("use_terrain_as_foundation"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        target.set_terrain_foundation(enabled);
    }
    return 1;
}

static GraphicsTarget *current_graphics_target()
{
    if (!g_parse_state.definition) {
        return nullptr;
    }

    switch (g_parse_state.current_graphics_target_scope) {
        case GraphicsParseTargetScope::Default:
            return &g_parse_state.graphics_definition.default_target();
        case GraphicsParseTargetScope::Variant:
            return g_parse_state.graphics_definition.last_variant() ?
                &g_parse_state.graphics_definition.last_variant()->target :
                nullptr;
        case GraphicsParseTargetScope::ConstructionPhase:
            return g_parse_state.graphics_definition.last_construction_phase();
        case GraphicsParseTargetScope::None:
        default:
            return nullptr;
    }
}

static int parse_graphics_resource_storage()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics resource_storage outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    GraphicsTarget *target = current_graphics_target();
    if (!target) {
        log_error("Encountered graphics resource_storage without an active target", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (target->has_path() || target->has_image() || target->has_options()) {
        log_error("BuildingType graphics resource_storage cannot be combined with path/image/options", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    target->set_resource_storage(1);
    return 1;
}

static GraphicsLayerStage parse_graphics_layer_stage(const char *text, int *valid)
{
    *valid = 1;
    if (!text || compare_text(text, "auto") == 0) {
        return GraphicsLayerStage::Auto;
    }
    if (compare_text(text, "footprint") == 0) {
        return GraphicsLayerStage::Footprint;
    }
    if (compare_text(text, "top") == 0) {
        return GraphicsLayerStage::Top;
    }
    if (compare_text(text, "animation") == 0) {
        return GraphicsLayerStage::Animation;
    }
    *valid = 0;
    return GraphicsLayerStage::Auto;
}

static int parse_graphics_layer()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics layer outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    GraphicsTarget *target = current_graphics_target();
    if (!target || g_parse_state.parsing_graphics_layer) {
        log_error("Encountered graphics layer without an active target", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    GraphicsLayer &layer = target->add_layer();
    if (xml_parser_has_attribute("path")) {
        std::string normalized_path = normalize_graphics_path(xml_parser_get_attribute_string("path"));
        if (normalized_path.empty()) {
            log_error("Unsupported BuildingType graphics layer path", xml_parser_get_attribute_string("path"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        layer.set_path(std::move(normalized_path));
    }
    if (xml_parser_has_attribute("image")) {
        std::string image_id = xml_value::trim_copy(xml_parser_get_attribute_string("image"));
        if (image_id.empty()) {
            log_error("Unsupported BuildingType graphics layer image", xml_parser_get_attribute_string("image"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        layer.set_image(std::move(image_id));
    }
    if (xml_parser_has_attribute("role")) {
        std::string role = xml_value::trim_copy(xml_parser_get_attribute_string("role"));
        if (role.empty()) {
            log_error("Unsupported BuildingType graphics layer role", xml_parser_get_attribute_string("role"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        layer.set_role(std::move(role));
    }
    if (xml_parser_has_attribute("stage")) {
        int valid = 0;
        GraphicsLayerStage stage = parse_graphics_layer_stage(xml_parser_get_attribute_string("stage"), &valid);
        if (!valid) {
            log_error("Unsupported BuildingType graphics layer stage", xml_parser_get_attribute_string("stage"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        layer.set_stage(stage);
    }
    if (xml_parser_has_attribute("animation")) {
        int enabled = 1;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("animation"), &enabled)) {
            log_error("Unsupported BuildingType graphics layer animation flag", xml_parser_get_attribute_string("animation"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        layer.set_animation_enabled(enabled);
    }
    int x = 0;
    int y = 0;
    int has_offset = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType graphics layer offset", "x", &x, &has_offset) ||
        !parse_optional_int_attribute("Unsupported BuildingType graphics layer offset", "y", &y, &has_offset)) {
        g_parse_state.error = 1;
        return 0;
    }
    layer.set_offset(x, y);
    g_parse_state.parsing_graphics_layer = 1;
    g_parse_state.current_graphics_layer = &layer;
    return 1;
}

static void finish_graphics_layer()
{
    if (!g_parse_state.parsing_graphics_layer) {
        return;
    }
    GraphicsLayer *layer = g_parse_state.current_graphics_layer;
    if (!layer || (!layer->has_image() && !layer->has_options())) {
        log_error("BuildingType graphics layer is missing image/options", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_graphics_layer = 0;
    g_parse_state.current_graphics_layer = nullptr;
}

static int parse_graphics_options()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics options outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!g_parse_state.parsing_graphics_layer &&
        g_parse_state.current_graphics_target_scope != GraphicsParseTargetScope::Default &&
        g_parse_state.current_graphics_target_scope != GraphicsParseTargetScope::Variant &&
        g_parse_state.current_graphics_target_scope != GraphicsParseTargetScope::ConstructionPhase) {
        log_error("BuildingType graphics options must appear inside default, variant, or construction phase", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *selection = xml_parser_has_attribute("selection") ? xml_parser_get_attribute_string("selection") : "stable_variant";
    GraphicsOptionSelection option_selection = GraphicsOptionSelection::StableVariant;
    if (!selection || compare_text(selection, "stable_variant") == 0) {
        option_selection = GraphicsOptionSelection::StableVariant;
    } else if (compare_text(selection, "build_rotation") == 0) {
        option_selection = GraphicsOptionSelection::BuildRotation;
    } else if (compare_text(selection, "connectable") == 0) {
        option_selection = GraphicsOptionSelection::Connectable;
    } else if (compare_text(selection, "orientation") == 0) {
        option_selection = GraphicsOptionSelection::Orientation;
    } else if (compare_text(selection, "production_progress") == 0) {
        option_selection = GraphicsOptionSelection::ProductionProgress;
    } else if (compare_text(selection, "storage_permission") == 0) {
        option_selection = GraphicsOptionSelection::StoragePermission;
    } else if (compare_text(selection, "gatehouse_orientation") == 0) {
        option_selection = GraphicsOptionSelection::GatehouseOrientation;
    } else if (compare_text(selection, "road_crossing") == 0) {
        option_selection = GraphicsOptionSelection::RoadCrossing;
    } else if (compare_text(selection, "rubble") == 0) {
        option_selection = GraphicsOptionSelection::Rubble;
    } else {
        log_error("Unsupported BuildingType graphics options selection", selection, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (g_parse_state.parsing_graphics_layer) {
        GraphicsLayer *layer = g_parse_state.current_graphics_layer;
        if (!layer || layer->has_options()) {
            log_error("BuildingType graphics layer contains duplicate options nodes", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        layer->set_option_selection(option_selection);
        g_parse_state.parsing_graphics_options = 1;
        return 1;
    }

    GraphicsTarget *target = current_graphics_target();
    if (!target || target->has_options()) {
        log_error("BuildingType graphics target contains duplicate options nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    target->set_option_selection(option_selection);
    g_parse_state.parsing_graphics_options = 1;
    return 1;
}

static void finish_graphics_options()
{
    if (!g_parse_state.parsing_graphics_options) {
        return;
    }

    GraphicsTarget *target = current_graphics_target();
    if (g_parse_state.parsing_graphics_layer) {
        GraphicsLayer *layer = g_parse_state.current_graphics_layer;
        if (!layer || !layer->has_options()) {
            log_error("BuildingType graphics layer options is missing option nodes", 0, 0);
            g_parse_state.error = 1;
        }
        g_parse_state.parsing_graphics_options = 0;
        return;
    }
    if (!target || !target->has_options()) {
        log_error("BuildingType graphics options is missing option nodes", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_graphics_options = 0;
}

static int parse_graphics_option()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics_options) {
        log_error("Encountered graphics option outside options node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    int draws = 1;
    if (xml_parser_has_attribute("draw")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("draw"), &draws)) {
            log_error("Unsupported BuildingType graphics option draw flag", xml_parser_get_attribute_string("draw"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }
    if (!draws && g_parse_state.parsing_graphics_layer) {
        log_error("BuildingType graphics layer option cannot use draw=0", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (draws && !xml_parser_has_attribute("image")) {
        log_error("BuildingType graphics option is missing required attribute 'image'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (g_parse_state.parsing_graphics_layer) {
        GraphicsLayer *layer = g_parse_state.current_graphics_layer;
        if (!layer) {
            log_error("Encountered graphics layer option without an active layer", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        GraphicsLayerOption &option = layer->add_option();
        if (xml_parser_has_attribute("path")) {
            std::string normalized_path = normalize_graphics_path(xml_parser_get_attribute_string("path"));
            if (normalized_path.empty()) {
                log_error("Unsupported BuildingType graphics layer option path", xml_parser_get_attribute_string("path"), 0);
                g_parse_state.error = 1;
                return 0;
            }
            option.path = std::move(normalized_path);
        }
        option.image = xml_value::trim_copy(xml_parser_get_attribute_string("image"));
        if (option.image.empty()) {
            log_error("Unsupported BuildingType graphics layer option image", xml_parser_get_attribute_string("image"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        int has_offset = 0;
        if (!parse_optional_int_attribute(
                "Unsupported BuildingType graphics layer option offset",
                "x",
                &option.x_offset,
                &has_offset)) {
            g_parse_state.error = 1;
            return 0;
        }
        option.has_x_offset = has_offset;
        if (!parse_optional_int_attribute(
                "Unsupported BuildingType graphics layer option offset",
                "y",
                &option.y_offset,
                &has_offset)) {
            g_parse_state.error = 1;
            return 0;
        }
        option.has_y_offset = has_offset;
        return 1;
    }

    GraphicsTarget *target = current_graphics_target();
    if (!target) {
        log_error("Encountered graphics option without an active target", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    GraphicsTarget &option = target->add_option();
    if (xml_parser_has_attribute("use_terrain_as_foundation")) {
        int enabled = 1;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("use_terrain_as_foundation"), &enabled)) {
            log_error("Unsupported BuildingType graphics option terrain foundation flag",
                xml_parser_get_attribute_string("use_terrain_as_foundation"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        option.set_terrain_foundation(enabled);
    }
    if (!draws) {
        option.set_no_draw(1);
        return 1;
    }
    if (xml_parser_has_attribute("path")) {
        std::string normalized_path = normalize_graphics_path(xml_parser_get_attribute_string("path"));
        if (normalized_path.empty()) {
            log_error("Unsupported BuildingType graphics option path", xml_parser_get_attribute_string("path"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        option.set_path(std::move(normalized_path));
    }

    std::string image_id = xml_value::trim_copy(xml_parser_get_attribute_string("image"));
    if (image_id.empty()) {
        log_error("Unsupported BuildingType graphics option image", xml_parser_get_attribute_string("image"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    option.set_image(std::move(image_id));
    return 1;
}

static void finish_graphics_default()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }
    if (!g_parse_state.graphics_definition.default_target().has_path() &&
        !g_parse_state.graphics_definition.default_target().has_options() &&
        !g_parse_state.graphics_definition.default_target().is_resource_storage()) {
        log_error("BuildingType graphics default is missing required child node 'path'", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::None;
}

static int parse_graphics_variant()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics variant outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    GraphicsVariant &variant = g_parse_state.graphics_definition.add_variant();
    if (xml_parser_has_attribute("animation")) {
        int enabled = 1;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("animation"), &enabled)) {
            log_error("Unsupported BuildingType graphics variant animation flag", xml_parser_get_attribute_string("animation"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        variant.target.set_animation_enabled(enabled);
    }
    if (xml_parser_has_attribute("use_terrain_as_foundation")) {
        int enabled = 1;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("use_terrain_as_foundation"), &enabled)) {
            log_error("Unsupported BuildingType graphics variant terrain foundation flag",
                xml_parser_get_attribute_string("use_terrain_as_foundation"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        variant.target.set_terrain_foundation(enabled);
    }
    if (xml_parser_has_attribute("role")) {
        std::string role = xml_value::trim_copy(xml_parser_get_attribute_string("role"));
        if (role.empty()) {
            log_error("Unsupported BuildingType graphics variant role", xml_parser_get_attribute_string("role"), 0);
            g_parse_state.error = 1;
            return 0;
        }
        variant.role = std::move(role);
    }
    g_parse_state.has_current_graphics_variant = 1;
    g_parse_state.current_graphics_variant_index = g_parse_state.graphics_definition.variants().size() - 1;
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::Variant;
    return 1;
}

static void finish_graphics_variant()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }

    GraphicsVariant *variant = g_parse_state.graphics_definition.last_variant();
    if (!variant || (!variant->target.has_path() && !variant->target.has_options() &&
        !variant->target.is_resource_storage())) {
        log_error("BuildingType graphics variant is missing required child node 'path'", 0, 0);
        g_parse_state.error = 1;
    }

    g_parse_state.has_current_graphics_variant = 0;
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::None;
}

static int parse_graphics_path()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics path outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType graphics path is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = normalize_graphics_path(xml_parser_get_attribute_string("value"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType graphics path", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    switch (g_parse_state.current_graphics_target_scope) {
        case GraphicsParseTargetScope::Default:
            g_parse_state.graphics_definition.default_target().set_path(std::move(normalized_path));
            break;
        case GraphicsParseTargetScope::Variant:
        {
            GraphicsVariant *variant = g_parse_state.graphics_definition.last_variant();
            if (!variant) {
                log_error("Encountered graphics path without an active variant", 0, 0);
                g_parse_state.error = 1;
                return 0;
            }
            variant->target.set_path(std::move(normalized_path));
            break;
        }
        case GraphicsParseTargetScope::ConstructionPhase:
        {
            GraphicsTarget *phase = g_parse_state.graphics_definition.last_construction_phase();
            if (!phase) {
                log_error("Encountered construction phase graphics path without an active phase", 0, 0);
                g_parse_state.error = 1;
                return 0;
            }
            phase->set_path(std::move(normalized_path));
            break;
        }
        case GraphicsParseTargetScope::None:
        default:
            log_error("BuildingType graphics path must appear inside default or variant", 0, 0);
            g_parse_state.error = 1;
            return 0;
    }
    return 1;
}

static int parse_graphics_image()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics) {
        log_error("Encountered graphics image outside graphics node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType graphics image is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string image_id = xml_value::trim_copy(xml_parser_get_attribute_string("value"));
    if (image_id.empty()) {
        log_error("Unsupported BuildingType graphics image", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    switch (g_parse_state.current_graphics_target_scope) {
        case GraphicsParseTargetScope::Default:
            g_parse_state.graphics_definition.default_target().set_image(std::move(image_id));
            break;
        case GraphicsParseTargetScope::Variant:
        {
            GraphicsVariant *variant = g_parse_state.graphics_definition.last_variant();
            if (!variant) {
                log_error("Encountered graphics image without an active variant", 0, 0);
                g_parse_state.error = 1;
                return 0;
            }
            variant->target.set_image(std::move(image_id));
            break;
        }
        case GraphicsParseTargetScope::ConstructionPhase:
        {
            GraphicsTarget *phase = g_parse_state.graphics_definition.last_construction_phase();
            if (!phase) {
                log_error("Encountered construction phase graphics image without an active phase", 0, 0);
                g_parse_state.error = 1;
                return 0;
            }
            phase->set_image(std::move(image_id));
            break;
        }
        case GraphicsParseTargetScope::None:
        default:
            log_error("BuildingType graphics image must appear inside default or variant", 0, 0);
            g_parse_state.error = 1;
            return 0;
    }
    return 1;
}

static int parse_climate_name(const char *name, int *out_climate)
{
    if (!out_climate) {
        return 0;
    }
    if (name && compare_text(name, "central") == 0) {
        *out_climate = CLIMATE_CENTRAL;
        return 1;
    }
    if (name && compare_text(name, "northern") == 0) {
        *out_climate = CLIMATE_NORTHERN;
        return 1;
    }
    if (name && compare_text(name, "desert") == 0) {
        *out_climate = CLIMATE_DESERT;
        return 1;
    }
    return 0;
}

static int parse_graphics_condition()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics ||
        (!g_parse_state.has_current_graphics_variant && !g_parse_state.parsing_graphics_layer)) {
        log_error("Encountered graphics condition outside graphics variant/layer", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("type")) {
        log_error("BuildingType graphics condition is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_text = xml_parser_get_attribute_string("type");
    GraphicsCondition condition;
    if (type_text && compare_text(type_text, "has_workers") == 0) {
        condition.type = GraphicsConditionType::HasWorkers;
    } else if (type_text && compare_text(type_text, "working") == 0) {
        condition.type = GraphicsConditionType::Working;
    } else if (type_text && compare_text(type_text, "water_access") == 0) {
        condition.type = GraphicsConditionType::WaterAccess;
    } else if (type_text && compare_text(type_text, "figure_slot_occupied") == 0) {
        if (!xml_parser_has_attribute("slot")) {
            log_error("BuildingType graphics figure_slot_occupied condition is missing required attribute 'slot'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *figure_slot_text = xml_parser_get_attribute_string("slot");
        condition.figure_slot = parse_figure_slot(figure_slot_text);
        if (condition.figure_slot == FigureSlot::None || (compare_text(figure_slot_text, "primary") != 0 &&
            compare_text(figure_slot_text, "secondary") != 0 && compare_text(figure_slot_text, "quaternary") != 0)) {
            log_error("Unsupported BuildingType graphics figure_slot_occupied slot", figure_slot_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::FigureSlotOccupied;
    } else if (type_text && compare_text(type_text, "resource_positive") == 0) {
        if (!xml_parser_has_attribute("resource")) {
            log_error("BuildingType graphics resource_positive condition is missing required attribute 'resource'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *resource_text = xml_parser_get_attribute_string("resource");
        condition.resource = resource_type_from_xml_attr(resource_text);
        if (condition.resource == RESOURCE_NONE) {
            log_error("Unsupported BuildingType graphics resource_positive resource", resource_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::ResourcePositive;
    } else if (type_text && compare_text(type_text, "resource_amount") == 0) {
        if (!xml_parser_has_attribute("resource") || !xml_parser_has_attribute("operator") ||
            !xml_parser_has_attribute("threshold")) {
            log_error("BuildingType graphics resource_amount condition is missing required attributes", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *resource_text = xml_parser_get_attribute_string("resource");
        condition.resource = resource_text && compare_text(resource_text, "none") == 0 ?
            RESOURCE_NONE :
            resource_type_from_xml_attr(resource_text);
        if (condition.resource == RESOURCE_NONE && compare_text(resource_text, "none") != 0) {
            log_error("Unsupported BuildingType graphics resource_amount resource", resource_text, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *comparison_text = xml_parser_get_attribute_string("operator");
        if (!parse_graphics_comparison(comparison_text, &condition.comparison)) {
            log_error("Unsupported BuildingType graphics resource_amount operator", comparison_text, 0);
            g_parse_state.error = 1;
            return 0;
        }

        int threshold = 0;
        const char *threshold_text = xml_parser_get_attribute_string("threshold");
        if (!threshold_text || !xml_value::parse_int_strict(threshold_text, &threshold)) {
            log_error("Unsupported BuildingType graphics resource_amount threshold", threshold_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::ResourceAmount;
        condition.threshold = threshold;
    } else if (type_text && compare_text(type_text, "terrain") == 0) {
        if (!xml_parser_has_attribute("value")) {
            log_error("BuildingType graphics terrain condition is missing required attribute 'value'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *terrain_text = xml_parser_get_attribute_string("value");
        if (compare_text(terrain_text, "road") == 0) {
            condition.terrain_mask = TERRAIN_ROAD;
        } else if (compare_text(terrain_text, "aqueduct") == 0) {
            condition.terrain_mask = TERRAIN_AQUEDUCT;
        } else if (compare_text(terrain_text, "highway") == 0) {
            condition.terrain_mask = TERRAIN_HIGHWAY;
        } else if (compare_text(terrain_text, "water") == 0) {
            condition.terrain_mask = TERRAIN_WATER;
        } else if (compare_text(terrain_text, "building") == 0) {
            condition.terrain_mask = TERRAIN_BUILDING;
        } else if (compare_text(terrain_text, "garden") == 0) {
            condition.terrain_mask = TERRAIN_GARDEN;
        } else if (compare_text(terrain_text, "rubble") == 0) {
            condition.terrain_mask = TERRAIN_RUBBLE;
        } else if (compare_text(terrain_text, "wall") == 0) {
            condition.terrain_mask = TERRAIN_WALL;
        } else {
            log_error("Unsupported BuildingType graphics terrain condition", terrain_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::Terrain;
    } else if (type_text && compare_text(type_text, "climate") == 0) {
        if (!xml_parser_has_attribute("value")) {
            log_error("BuildingType graphics climate condition is missing required attribute 'value'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *climate_text = xml_parser_get_attribute_string("value");
        if (!parse_climate_name(climate_text, &condition.climate)) {
            log_error("Unsupported BuildingType graphics climate condition", climate_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::Climate;
    } else if (type_text && compare_text(type_text, "monument_upgrade") == 0) {
        if (!xml_parser_has_attribute("value")) {
            log_error("BuildingType graphics monument_upgrade condition is missing required attribute 'value'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        int upgrade = 0;
        const char *upgrade_text = xml_parser_get_attribute_string("value");
        if (!upgrade_text || !xml_value::parse_int_strict(upgrade_text, &upgrade) || upgrade <= 0) {
            log_error("Unsupported BuildingType graphics monument_upgrade condition value", upgrade_text, 0);
            g_parse_state.error = 1;
            return 0;
        }

        condition.type = GraphicsConditionType::MonumentUpgrade;
        condition.monument_upgrade = upgrade;
    } else if (type_text && compare_text(type_text, "race_active") == 0) {
        condition.type = GraphicsConditionType::RaceActive;
    } else if (type_text && compare_text(type_text, "population") == 0) {
        const char *operator_text = xml_parser_get_attribute_string("operator");
        const char *threshold_text = xml_parser_get_attribute_string("threshold");
        if (!operator_text || !threshold_text || !parse_graphics_comparison(operator_text, &condition.comparison) ||
            !xml_value::parse_int_strict(threshold_text, &condition.threshold) || condition.threshold < 0) {
            log_error("BuildingType graphics population condition is invalid", threshold_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::Population;
    } else if (type_text && compare_text(type_text, "orientation") == 0) {
        const char *value = xml_parser_get_attribute_string("value");
        if (value && compare_text(value, "top") == 0) condition.orientation = DIR_0_TOP;
        else if (value && compare_text(value, "right") == 0) condition.orientation = DIR_2_RIGHT;
        else if (value && compare_text(value, "bottom") == 0) condition.orientation = DIR_4_BOTTOM;
        else if (value && compare_text(value, "left") == 0) condition.orientation = DIR_6_LEFT;
        else {
            log_error("BuildingType graphics orientation condition is invalid", value, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::Orientation;
    } else if (type_text && compare_text(type_text, "festival_games") == 0) {
        if (!xml_parser_has_attribute("value")) {
            log_error("BuildingType graphics festival_games condition is missing required attribute 'value'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        int games = 0;
        const char *games_text = xml_parser_get_attribute_string("value");
        if (!games_text || !xml_value::parse_int_strict(games_text, &games) || games < 1 || games > 3) {
            log_error("Unsupported BuildingType graphics festival_games condition value", games_text, 0);
            g_parse_state.error = 1;
            return 0;
        }

        condition.type = GraphicsConditionType::FestivalGames;
        condition.festival_games = games;
    } else if (type_text && compare_text(type_text, "days1_positive") == 0) {
        condition.type = GraphicsConditionType::Days1Positive;
    } else if (type_text && compare_text(type_text, "days1_not_positive") == 0) {
        condition.type = GraphicsConditionType::Days1NotPositive;
    } else if (type_text && compare_text(type_text, "days2_positive") == 0) {
        condition.type = GraphicsConditionType::Days2Positive;
    } else if (type_text && compare_text(type_text, "days1_or_days2_positive") == 0) {
        condition.type = GraphicsConditionType::Days1OrDays2Positive;
    } else if (type_text && compare_text(type_text, "desirability") == 0) {
        if (!xml_parser_has_attribute("operator")) {
            log_error("BuildingType graphics desirability condition is missing required attribute 'operator'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        if (!xml_parser_has_attribute("threshold")) {
            log_error("BuildingType graphics desirability condition is missing required attribute 'threshold'", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }

        const char *comparison_text = xml_parser_get_attribute_string("operator");
        if (!parse_graphics_comparison(comparison_text, &condition.comparison)) {
            log_error("Unsupported BuildingType graphics condition operator", comparison_text, 0);
            g_parse_state.error = 1;
            return 0;
        }

        int threshold = 0;
        const char *threshold_text = xml_parser_get_attribute_string("threshold");
        if (!threshold_text || !xml_value::parse_int_strict(threshold_text, &threshold)) {
            log_error("Unsupported BuildingType graphics condition threshold", threshold_text, 0);
            g_parse_state.error = 1;
            return 0;
        }

        condition.type = GraphicsConditionType::Desirability;
        condition.threshold = threshold;
    } else {
        log_error("Unsupported BuildingType graphics condition type", type_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (g_parse_state.parsing_graphics_layer) {
        if (!g_parse_state.current_graphics_layer) {
            log_error("Encountered graphics layer condition without an active layer", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.current_graphics_layer->add_condition(condition);
        return 1;
    }
    GraphicsVariant *variant = g_parse_state.graphics_definition.last_variant();
    if (variant) variant->conditions.push_back(std::move(condition));
    return 1;
}

static int parse_water_access_provides()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered water_access provides outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("type") || !xml_parser_has_attribute("range")) {
        log_error("BuildingType water_access provides is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_text = xml_parser_get_attribute_string("type");
    const uint8_t mask = water_access_mask_from_text(type_text);
    if (!mask) {
        log_error("Unsupported BuildingType water_access provides type", type_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    int range = xml_parser_get_attribute_int("range");
    if (range < 0) {
        log_error("Unsupported BuildingType provider water_access range", xml_parser_get_attribute_string("range"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("origin")) {
        const char *origin_text = xml_parser_get_attribute_string("origin");
        if (compare_text(origin_text, "footprint") != 0 && compare_text(origin_text, "nodes") != 0) {
            log_error("Unsupported BuildingType water_access provides origin", origin_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    WaterAccessProvideRule rule;
    rule.mask = mask;
    rule.range = range;
    rule.origin = parse_water_access_origin(xml_parser_has_attribute("origin") ? xml_parser_get_attribute_string("origin") : nullptr);
    g_parse_state.definition->add_water_access_provide_rule(rule);
    g_parse_state.saw_water_access_rule = 1;
    return 1;
}

static int parse_water_access_requires()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered water_access requires outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.parsing_water_access_requirement) {
        log_error("BuildingType water_access contains nested requires nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("mode")) {
        log_error("BuildingType water_access requires is missing required attribute 'mode'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *mode_text = xml_parser_get_attribute_string("mode");
    if (compare_text(mode_text, "any") != 0 && compare_text(mode_text, "all") != 0) {
        log_error("Unsupported BuildingType water_access requires mode", mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("where")) {
        const char *where_text = xml_parser_get_attribute_string("where");
        if (compare_text(where_text, "footprint") != 0 && compare_text(where_text, "nodes") != 0) {
            log_error("Unsupported BuildingType water_access requires where", where_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    g_parse_state.current_water_access_requirement_rule = WaterAccessRequirementRule();
    g_parse_state.current_water_access_requirement_rule.mode = parse_water_access_requirement_mode(mode_text);
    g_parse_state.current_water_access_requirement_where =
        parse_water_access_where(xml_parser_has_attribute("where") ? xml_parser_get_attribute_string("where") : nullptr);
    g_parse_state.parsing_water_access_requirement = 1;
    g_parse_state.saw_current_water_access_requirement_term = 0;
    return 1;
}

static void finish_water_access_requires()
{
    if (!g_parse_state.parsing_water_access_requirement) {
        return;
    }
    if (!g_parse_state.saw_current_water_access_requirement_term) {
        log_error("BuildingType water_access requires is missing access/source terms",
            g_parse_state.definition ? g_parse_state.definition->attr() : 0, 0);
        g_parse_state.error = 1;
    } else if (g_parse_state.definition) {
        g_parse_state.definition->add_water_access_requirement_rule(std::move(g_parse_state.current_water_access_requirement_rule));
        g_parse_state.saw_water_access_rule = 1;
    }
    g_parse_state.parsing_water_access_requirement = 0;
}

static int add_water_access_requirement_term(WaterAccessRequirementTerm term)
{
    if (!g_parse_state.definition || !g_parse_state.parsing_water_access_requirement) {
        log_error("Encountered water_access requirement term outside requires node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("where")) {
        const char *where_text = xml_parser_get_attribute_string("where");
        if (compare_text(where_text, "footprint") != 0 && compare_text(where_text, "nodes") != 0) {
            log_error("Unsupported BuildingType water_access requirement where", where_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        term.where = parse_water_access_where(where_text);
    } else {
        term.where = g_parse_state.current_water_access_requirement_where;
    }
    g_parse_state.current_water_access_requirement_rule.terms.push_back(term);
    g_parse_state.saw_current_water_access_requirement_term = 1;
    return 1;
}

static int parse_water_access_requirement_access()
{
    if (!xml_parser_has_attribute("type")) {
        log_error("BuildingType water_access requirement access is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    WaterAccessRequirementTerm term;
    term.kind = WaterAccessRequirementTermKind::Access;
    term.mask = water_access_mask_from_text(xml_parser_get_attribute_string("type"));
    if (!term.mask) {
        log_error("Unsupported BuildingType water_access requirement access type", xml_parser_get_attribute_string("type"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    return add_water_access_requirement_term(term);
}

static int parse_water_access_requirement_source()
{
    if (!xml_parser_has_attribute("type")) {
        log_error("BuildingType water_access requirement source is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    const char *source_text = xml_parser_get_attribute_string("type");
    WaterAccessRequirementTerm term;
    if (compare_text(source_text, "water_source_any") == 0) {
        term.kind = WaterAccessRequirementTermKind::WaterSourceAny;
    } else if (compare_text(source_text, "water_source_fresh_only") == 0) {
        term.kind = WaterAccessRequirementTermKind::WaterSourceFreshOnly;
    } else {
        log_error("Unsupported BuildingType water_access requirement source type", source_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    term.where = WaterAccessRequirementWhere::Footprint;
    return add_water_access_requirement_term(term);
}

static int parse_provider_water_access_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered provider water_access node outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("x") || !xml_parser_has_attribute("y")) {
        log_error("BuildingType provider water_access node is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *kind_text = xml_parser_has_attribute("kind") ? xml_parser_get_attribute_string("kind") : nullptr;
    WaterAccessNodeKind kind = parse_provider_water_access_node_kind(kind_text);
    if (kind == WaterAccessNodeKind::None) {
        log_error("Unsupported BuildingType provider water_access node kind", kind_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("role")) {
        const char *role_text = xml_parser_get_attribute_string("role");
        if (compare_text(role_text, "provide") != 0 &&
            compare_text(role_text, "require") != 0 &&
            compare_text(role_text, "both") != 0) {
            log_error("Unsupported BuildingType provider water_access node role", role_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    WaterAccessNode node;
    node.kind = kind;
    node.x = xml_parser_get_attribute_int("x");
    node.y = xml_parser_get_attribute_int("y");
    switch (parse_water_access_node_role(xml_parser_has_attribute("role") ? xml_parser_get_attribute_string("role") : nullptr)) {
        case WaterAccessNodeRole::Provider:
            g_parse_state.definition->add_water_access_provider_node(node);
            break;
        case WaterAccessNodeRole::Requirement:
            g_parse_state.definition->add_water_access_requirement_node(node);
            break;
        case WaterAccessNodeRole::Both:
        default:
            g_parse_state.definition->add_water_access_node(node);
            break;
    }
    return 1;
}

static int parse_labor()
{
    if (!g_parse_state.definition) {
        log_error("Encountered labor definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_labor) {
        log_error("BuildingType xml contains duplicate labor nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_labor = 1;
    g_parse_state.parsing_labor = 1;
    g_parse_state.saw_labor_employees = 0;
    g_parse_state.saw_labor_seeker = 0;
    g_parse_state.saw_labor_seeker_method = 0;
    g_parse_state.saw_labor_seeker_amount = 0;
    g_parse_state.current_labor_seeker_policy = LaborSeekerPolicy();
    return 1;
}

static void finish_labor()
{
    if (!g_parse_state.parsing_labor) {
        return;
    }
    if (!g_parse_state.saw_labor_employees && !g_parse_state.saw_labor_seeker) {
        log_error("BuildingType labor is missing a supported child node", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_labor = 0;
}

static int parse_labor_employees()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_labor) {
        log_error("Encountered labor employees outside labor node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_labor_employees) {
        log_error("BuildingType labor contains duplicate employees nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("count")) {
        log_error("BuildingType labor employees is missing required attribute 'count'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int count = xml_parser_get_attribute_int("count");
    if (count < 0) {
        log_error("Unsupported BuildingType labor employees count", xml_parser_get_attribute_string("count"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_labor_employee_count(count);
    g_parse_state.saw_labor_employees = 1;
    return 1;
}

static int parse_labor_seeker()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_labor) {
        log_error("Encountered labor seeker outside labor node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_labor_seeker) {
        log_error("BuildingType labor contains duplicate labor_seeker nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_labor_seeker = 1;
    g_parse_state.parsing_labor_seeker = 1;
    g_parse_state.saw_labor_seeker_method = 0;
    g_parse_state.saw_labor_seeker_amount = 0;
    g_parse_state.current_labor_seeker_policy = LaborSeekerPolicy();
    return 1;
}

static void finish_labor_seeker()
{
    if (!g_parse_state.parsing_labor_seeker) {
        return;
    }

    if (!g_parse_state.saw_labor_seeker_method) {
        log_error("BuildingType labor_seeker is missing required child nodes", 0, 0);
        g_parse_state.error = 1;
        g_parse_state.parsing_labor_seeker = 0;
        return;
    }

    if (!g_parse_state.saw_labor_seeker_amount) {
        switch (g_parse_state.current_labor_seeker_policy.method) {
            case LaborSeekerMethod::Workforce:
                if (g_parse_state.definition->labor().has_employee_count()) {
                    g_parse_state.current_labor_seeker_policy.amount =
                        g_parse_state.definition->labor().employee_count();
                    g_parse_state.saw_labor_seeker_amount = 1;
                } else {
                    log_error("BuildingType workforce labor_seeker amount is missing and labor has no employees count", 0, 0);
                    g_parse_state.error = 1;
                }
                break;
            case LaborSeekerMethod::HousesSpawnIfBelow:
            case LaborSeekerMethod::HousesGenerateIfBelow:
                log_error("BuildingType house-coverage labor_seeker is missing required amount", 0, 0);
                g_parse_state.error = 1;
                break;
            case LaborSeekerMethod::None:
            default:
                break;
        }
    }

    if (g_parse_state.saw_labor_seeker_amount) {
        g_parse_state.definition->set_labor_seeker_policy(g_parse_state.current_labor_seeker_policy);
    }
    g_parse_state.parsing_labor_seeker = 0;
}

static int parse_labor_seeker_method_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_labor_seeker) {
        log_error("Encountered labor_seeker method outside labor_seeker node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_labor_seeker_method) {
        log_error("BuildingType labor_seeker contains duplicate method nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType labor_seeker method is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *method_text = xml_parser_get_attribute_string("value");
    g_parse_state.current_labor_seeker_policy.method = parse_labor_seeker_method(method_text);
    if (g_parse_state.current_labor_seeker_policy.method == LaborSeekerMethod::None) {
        log_error("Unsupported BuildingType labor seeker method", method_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_labor_seeker_method = 1;
    return 1;
}

static int parse_labor_seeker_amount_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_labor_seeker) {
        log_error("Encountered labor_seeker amount outside labor_seeker node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_labor_seeker_amount) {
        log_error("BuildingType labor_seeker contains duplicate amount nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType labor_seeker amount is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int amount = xml_parser_get_attribute_int("value");
    if (amount < 0) {
        log_error("Unsupported BuildingType labor seeker amount", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_labor_seeker_policy.amount = amount;
    g_parse_state.saw_labor_seeker_amount = 1;
    return 1;
}

static int parse_culture_modules()
{
    if (!g_parse_state.definition) {
        log_error("Encountered culture_modules definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_culture_modules) {
        log_error("BuildingType xml contains duplicate culture_modules nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_culture_modules = 1;
    g_parse_state.parsing_culture_modules = 1;
    return 1;
}

static void finish_culture_modules()
{
    g_parse_state.parsing_culture_modules = 0;
}

static int parse_culture_module_count_mode(const char *value, CultureModuleCountMode *out_count_mode)
{
    if (!out_count_mode) {
        return 0;
    }

    std::string text = xml_value::trim_copy(value ? value : "");
    if (text.empty() || compare_text(text.c_str(), "total") == 0) {
        *out_count_mode = CultureModuleCountMode::Total;
        return 1;
    }
    if (compare_text(text.c_str(), "active") == 0) {
        *out_count_mode = CultureModuleCountMode::Active;
        return 1;
    }
    if (compare_text(text.c_str(), "working") == 0) {
        *out_count_mode = CultureModuleCountMode::Working;
        return 1;
    }
    return 0;
}

static int parse_culture_module_reference()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_culture_modules) {
        log_error("Encountered culture_module reference outside culture_modules node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("path")) {
        log_error("BuildingType culture_module reference is missing required attribute 'path'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("capacity")) {
        log_error("BuildingType culture_module reference is missing required attribute 'capacity'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    std::string normalized_path = xml_definition::normalize_path(xml_parser_get_attribute_string("path"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType culture_module reference path", xml_parser_get_attribute_string("path"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int capacity = 0;
    const char *capacity_text = xml_parser_get_attribute_string("capacity");
    if (!capacity_text || !xml_value::parse_int_strict(capacity_text, &capacity) || capacity < 0) {
        log_error("Unsupported BuildingType culture_module capacity", capacity_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int upgrade_bonus = 0;
    if (xml_parser_has_attribute("upgrade_bonus")) {
        const char *upgrade_bonus_text = xml_parser_get_attribute_string("upgrade_bonus");
        if (!upgrade_bonus_text || !xml_value::parse_int_strict(upgrade_bonus_text, &upgrade_bonus) ||
            upgrade_bonus < 0) {
            log_error("Unsupported BuildingType culture_module upgrade_bonus", upgrade_bonus_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    CultureModuleCountMode count_mode = CultureModuleCountMode::Total;
    if (xml_parser_has_attribute("count") &&
        !parse_culture_module_count_mode(xml_parser_get_attribute_string("count"), &count_mode)) {
        log_error("Unsupported BuildingType culture_module count mode", xml_parser_get_attribute_string("count"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_culture_module_reference(std::move(normalized_path), capacity, upgrade_bonus, count_mode);
    return 1;
}

static int parse_storages()
{
    if (!g_parse_state.definition) {
        log_error("Encountered storages definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_storages) {
        log_error("BuildingType xml contains duplicate storages nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_storages = 1;
    g_parse_state.parsing_storages = 1;
    return 1;
}

static void finish_storages()
{
    g_parse_state.parsing_storages = 0;
}

static int parse_storage_reference()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_storages) {
        log_error("Encountered storage reference outside storages node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("path")) {
        log_error("BuildingType storage reference is missing required attribute 'path'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = xml_definition::normalize_path(xml_parser_get_attribute_string("path"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType storage reference path", xml_parser_get_attribute_string("path"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_storage_reference(std::move(normalized_path));
    return 1;
}

static int parse_production_methods()
{
    if (!g_parse_state.definition) {
        log_error("Encountered production_methods definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_production_methods) {
        log_error("BuildingType xml contains duplicate production_methods nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_production_methods = 1;
    g_parse_state.parsing_production_methods = 1;
    return 1;
}

static void finish_production_methods()
{
    g_parse_state.parsing_production_methods = 0;
}

static int parse_production_method_reference()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_production_methods) {
        log_error("Encountered production_method reference outside production_methods node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("path")) {
        log_error("BuildingType production_method reference is missing required attribute 'path'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = xml_definition::normalize_path(xml_parser_get_attribute_string("path"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType production_method reference path", xml_parser_get_attribute_string("path"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_production_method_reference(std::move(normalized_path));
    return 1;
}

static int parse_distribution()
{
    if (!g_parse_state.definition) {
        log_error("Encountered distribution definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_distribution) {
        log_error("BuildingType xml contains duplicate distribution nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("path")) {
        log_error("BuildingType distribution is missing required attribute 'path'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = xml_definition::normalize_path(xml_parser_get_attribute_string("path"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType distribution path", xml_parser_get_attribute_string("path"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_distribution_reference(std::move(normalized_path));
    g_parse_state.saw_distribution = 1;
    return 1;
}

static int parse_housing()
{
    if (!g_parse_state.definition) {
        log_error("Encountered housing definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_housing) {
        log_error("BuildingType xml contains duplicate housing nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("path")) {
        log_error("BuildingType housing is missing required attribute 'path'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string normalized_path = xml_definition::normalize_path(xml_parser_get_attribute_string("path"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType housing path", xml_parser_get_attribute_string("path"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    HousingDef &housing = g_parse_state.definition->housing_def();
    housing.profile_path = std::move(normalized_path);

    int capacity = 0;
    int has_capacity = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType housing capacity", "capacity", &capacity, &has_capacity)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (!has_capacity || capacity <= 0) {
        log_error("BuildingType housing is missing positive required attribute 'capacity'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    housing.capacity = capacity;

    int goods_consumption_events_per_month = 0;
    int mars_offering_amount = 0;
    if (!xml_definition::parse_required_positive_int_attribute(
            "goods_consumption_events_per_month", &goods_consumption_events_per_month) ||
        !xml_definition::parse_required_positive_int_attribute(
            "mars_offering_amount", &mars_offering_amount) ||
        goods_consumption_events_per_month > 2) {
        log_error("BuildingType housing is missing required positive balance attributes",
            g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    housing.goods_consumption_events_per_month = goods_consumption_events_per_month;
    housing.mars_offering_amount = mars_offering_amount;

    const char *transition_attributes[] = {"evolve_to", "devolve_to", "merge_to", "split_to"};
    for (const char *attribute : transition_attributes) {
        if (!xml_parser_has_attribute(attribute)) {
            continue;
        }
        std::string transition = xml_value::trim_copy(xml_parser_get_attribute_string(attribute));
        if (!transition.empty()) {
            housing.transition(parse_housing_transition_kind(attribute)).type_path = std::move(transition);
        }
    }

    g_parse_state.saw_housing = 1;
    return 1;
}

static int parse_vacant_lot()
{
    if (!g_parse_state.definition) {
        log_error("Encountered vacant_lot definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_vacant_lot) {
        log_error("BuildingType xml contains duplicate vacant_lot nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("fill_to")) {
        log_error("BuildingType vacant_lot is missing required attribute 'fill_to'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string fill_to = xml_value::trim_copy(xml_parser_get_attribute_string("fill_to"));
    if (fill_to.empty()) {
        log_error("Unsupported BuildingType vacant_lot fill_to", xml_parser_get_attribute_string("fill_to"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.definition->housing_def().transition(HousingTransitionKind::VacantLot).type_path = std::move(fill_to);
    g_parse_state.saw_vacant_lot = 1;
    return 1;
}

static int parse_spawn_group()
{
    if (!g_parse_state.definition) {
        log_error("Encountered spawn_group definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("road_access")) {
        log_error("BuildingType spawn_group is missing required attribute 'road_access'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("delay_bands")) {
        log_error("BuildingType spawn_group is missing required attribute 'delay_bands'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    SpawnDelayGroup group;
    const char *road_access_text = xml_parser_get_attribute_string("road_access");
    group.road_access_mode = parse_road_access_mode(road_access_text);
    if (group.road_access_mode == RoadAccessMode::None) {
        log_error("Unsupported BuildingType spawn_group road_access", road_access_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *delay_bands_text = xml_parser_get_attribute_string("delay_bands");
    if (!parse_delay_bands_attribute(delay_bands_text, group.delay_bands)) {
        log_error("Unsupported BuildingType spawn_group delay_bands", delay_bands_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (xml_parser_has_attribute("existing_figure")) {
        const char *existing_figure_text = xml_parser_get_attribute_string("existing_figure");
        if (!parse_figure_list_attribute(existing_figure_text, group.existing_figures)) {
            log_error("Unsupported BuildingType spawn_group existing_figure", existing_figure_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("guard_timing")) {
        const char *guard_timing_text = xml_parser_get_attribute_string("guard_timing");
        group.guard_timing = parse_guard_timing(guard_timing_text);
        if (compare_text(guard_timing_text, "before_road_access") != 0 &&
            compare_text(guard_timing_text, "after_labor_seeker") != 0) {
            log_error("Unsupported BuildingType spawn_group guard_timing", guard_timing_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    g_parse_state.definition->add_spawn_group(std::move(group));
    g_parse_state.current_spawn_group_index = g_parse_state.definition->spawn_groups().size() - 1;
    g_parse_state.has_current_spawn_group = 1;
    return 1;
}

static int parse_spawn()
{
    if (!g_parse_state.definition || !g_parse_state.has_current_spawn_group) {
        log_error("Encountered spawn definition before spawn_group", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    SpawnPolicy policy;
    const int has_special_mode = xml_parser_has_attribute("mode");
    const char *mode_text = has_special_mode ? xml_parser_get_attribute_string("mode") : nullptr;
    if (!has_special_mode) {
        policy.special_mode = SpecialSpawnMode::None;
    } else if (compare_text(mode_text, "temple_supplier") == 0) {
        policy.special_mode = SpecialSpawnMode::TempleSupplier;
    } else if (mode_text && compare_text(mode_text, "temple_destination_priest") == 0) {
        policy.special_mode = SpecialSpawnMode::TempleDestinationPriest;
    } else if (mode_text && compare_text(mode_text, "temple_mars_mess_hall_priest") == 0) {
        policy.special_mode = SpecialSpawnMode::TempleMarsMessHallPriest;
    } else if (mode_text && compare_text(mode_text, "temple_neptune_chariot") == 0) {
        policy.special_mode = SpecialSpawnMode::TempleNeptuneChariot;
    } else if (mode_text && compare_text(mode_text, "grand_temple_mars_recruit") == 0) {
        policy.special_mode = SpecialSpawnMode::GrandTempleMarsRecruit;
    } else if (mode_text && compare_text(mode_text, "fishing_boat") == 0) {
        policy.special_mode = SpecialSpawnMode::FishingBoat;
    } else {
        log_error("Unsupported BuildingType special spawn mode", mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const int has_spawn_figure = xml_parser_has_attribute("spawn_figure");
    const int has_profile_attribute = xml_parser_has_attribute("profile");
    const int has_generic_only_attribute = has_spawn_figure || has_profile_attribute ||
        xml_parser_has_attribute("action_state") || xml_parser_has_attribute("init_roaming") ||
        xml_parser_has_attribute("direction") || xml_parser_has_attribute("figure_slot") ||
        xml_parser_has_attribute("spawn_count");
    if (has_special_mode && has_generic_only_attribute) {
        log_error("BuildingType special spawn contains generic figure spawn attributes", mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!has_special_mode && (!has_spawn_figure || !has_profile_attribute)) {
        log_error("BuildingType generic spawn requires spawn_figure and profile", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *graphic_timing_text = xml_parser_has_attribute("graphic_timing") ?
        xml_parser_get_attribute_string("graphic_timing") : "none";
    policy.graphic_timing = parse_graphic_timing(graphic_timing_text);
    if (graphic_timing_text && compare_text(graphic_timing_text, "none") != 0 && policy.graphic_timing == GraphicTiming::None) {
        log_error("Unsupported BuildingType spawn graphic_timing", graphic_timing_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (has_spawn_figure) {
        policy.spawn_figure = figure_type_from_xml_name(xml_parser_get_attribute_string("spawn_figure"));
        if (policy.spawn_figure == FIGURE_NONE) {
            log_error("Unsupported BuildingType spawn spawn_figure", xml_parser_get_attribute_string("spawn_figure"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("direction")) {
        const char *direction_text = xml_parser_get_attribute_string("direction");
        policy.spawn_direction = parse_spawn_direction(direction_text);
        if (compare_text(direction_text, "top") != 0 && compare_text(direction_text, "bottom") != 0) {
            log_error("Unsupported BuildingType spawn direction", direction_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("figure_slot")) {
        const char *figure_slot_text = xml_parser_get_attribute_string("figure_slot");
        policy.figure_slot = parse_figure_slot(figure_slot_text);
        if (compare_text(figure_slot_text, "primary") != 0 && compare_text(figure_slot_text, "secondary") != 0 &&
            compare_text(figure_slot_text, "quaternary") != 0 && compare_text(figure_slot_text, "none") != 0) {
            log_error("Unsupported BuildingType spawn figure_slot", figure_slot_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("require_water_access")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("require_water_access"), &policy.require_water_access)) {
            log_error("Unsupported BuildingType spawn require_water_access", xml_parser_get_attribute_string("require_water_access"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("mark_problem_if_no_water")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("mark_problem_if_no_water"), &policy.mark_problem_if_no_water)) {
            log_error("Unsupported BuildingType spawn mark_problem_if_no_water", xml_parser_get_attribute_string("mark_problem_if_no_water"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("spawn_count")) {
        policy.spawn_count = xml_parser_get_attribute_int("spawn_count");
        if (policy.spawn_count <= 0) {
            log_error("Unsupported BuildingType spawn spawn_count", xml_parser_get_attribute_string("spawn_count"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("condition")) {
        const char *condition_text = xml_parser_get_attribute_string("condition");
        policy.condition = parse_spawn_condition(condition_text);
        if (compare_text(condition_text, "always") != 0 &&
            compare_text(condition_text, "days1_positive") != 0 &&
            compare_text(condition_text, "days1_not_positive") != 0 &&
            compare_text(condition_text, "days2_positive") != 0 &&
            compare_text(condition_text, "days1_or_days2_positive") != 0) {
            log_error("Unsupported BuildingType spawn condition", condition_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("block_on_success")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("block_on_success"), &policy.block_on_success)) {
            log_error("Unsupported BuildingType spawn block_on_success", xml_parser_get_attribute_string("block_on_success"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("spawn_source")) {
        const char *source_text = xml_parser_get_attribute_string("spawn_source");
        if (compare_text(source_text, "self") == 0) {
            policy.spawn_source = SpawnSource::Self;
        } else if (compare_text(source_text, "shipyard") == 0) {
            policy.spawn_source = SpawnSource::Shipyard;
        } else {
            log_error("Unsupported BuildingType spawn spawn_source", source_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("capacity")) {
        policy.capacity = xml_parser_get_attribute_int("capacity");
        if (policy.capacity <= 0) {
            log_error("Unsupported BuildingType spawn capacity", xml_parser_get_attribute_string("capacity"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }
    if (policy.special_mode == SpecialSpawnMode::FishingBoat &&
        (policy.spawn_source == SpawnSource::None || policy.capacity <= 0)) {
        log_error("BuildingType fishing_boat spawn requires spawn_source and positive capacity",
            mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (policy.special_mode != SpecialSpawnMode::FishingBoat &&
        (policy.spawn_source != SpawnSource::None || policy.capacity > 0)) {
        log_error("BuildingType spawn_source and capacity are valid only for fishing_boat special spawns",
            mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (xml_parser_has_attribute("chance_source")) {
        const char *chance_source_text = xml_parser_get_attribute_string("chance_source");
        policy.chance_source = parse_spawn_chance_source(chance_source_text);
        if (compare_text(chance_source_text, "none") != 0 &&
            compare_text(chance_source_text, "city_unemployment_percent") != 0 &&
            compare_text(chance_source_text, "house_unemployed_workers") != 0) {
            log_error("Unsupported BuildingType spawn chance_source", chance_source_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("chance_per_million")) {
        policy.chance_per_million = xml_parser_get_attribute_int("chance_per_million");
        if (policy.chance_per_million < 0 || policy.chance_per_million > 1000000) {
            log_error("Unsupported BuildingType spawn chance_per_million", xml_parser_get_attribute_string("chance_per_million"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("chance_divisor")) {
        policy.chance_divisor = xml_parser_get_attribute_int("chance_divisor");
        if (policy.chance_divisor <= 0) {
            log_error("Unsupported BuildingType spawn chance_divisor", xml_parser_get_attribute_string("chance_divisor"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    if (xml_parser_has_attribute("chance_per_million_bands")) {
        const char *bands_text = xml_parser_get_attribute_string("chance_per_million_bands");
        if (!parse_chance_bands_attribute(bands_text, policy.chance_bands)) {
            log_error("Unsupported BuildingType spawn chance_per_million_bands", bands_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    // A policy can have no chance gate, or exactly one chance gate. Combining
    // constants, source bands, and divisors would make spawn rates ambiguous.
    const int chance_policy_count =
        (policy.chance_per_million >= 0 ? 1 : 0) +
        (policy.chance_divisor > 0 ? 1 : 0) +
        (!policy.chance_bands.empty() ? 1 : 0);
    if (chance_policy_count > 1) {
        log_error("BuildingType spawn has multiple chance policies", xml_parser_get_attribute_string("spawn_figure"), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if ((policy.chance_divisor > 0 || !policy.chance_bands.empty()) &&
        policy.chance_source == SpawnChanceSource::None) {
        log_error("BuildingType spawn chance policy requires chance_source", xml_parser_get_attribute_string("spawn_figure"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    if (has_profile_attribute) {
        policy.profile = xml_value::trim_copy(xml_parser_get_attribute_string("profile"));
        if (policy.profile.empty()) {
            log_error("Unsupported BuildingType spawn profile", xml_parser_get_attribute_string("profile"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    SpawnDelayGroup *group = g_parse_state.definition->last_spawn_group();
    if (!group) {
        log_error("BuildingType spawn has no active spawn_group", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    group->policies.push_back(std::move(policy));
    g_parse_state.saw_spawn = 1;
    return 1;
}

static int parse_race_positive_attribute(const char *name, int *out_value, int minimum, int maximum)
{
    const char *text = xml_parser_get_attribute_string(name);
    int value = 0;
    if (!text || !xml_value::parse_int_strict(text, &value) || value < minimum || value > maximum) {
        log_error("BuildingType race has an invalid numeric attribute", name, value);
        g_parse_state.error = 1;
        return 0;
    }
    *out_value = value;
    return 1;
}

static int parse_race()
{
    if (!g_parse_state.definition || g_parse_state.saw_race) {
        log_error("BuildingType xml contains an invalid or duplicate race module",
            g_parse_state.definition ? g_parse_state.definition->attr() : nullptr, 0);
        g_parse_state.error = 1;
        return 0;
    }
    std::string participant;
    if (!xml_definition::parse_required_nonempty_string_attribute("participant", &participant)) {
        log_error("BuildingType race requires participant", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    const figure_type participant_type = figure_type_from_xml_name(participant.c_str());
    if (participant_type == FIGURE_NONE) {
        log_error("BuildingType race participant is unknown", participant.c_str(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    RaceDefinition &race = g_parse_state.race_definition;
    race.participant_figure_reference = std::move(participant);
    race.participant_figure = participant_type;
    const char *start_delay_bands = xml_parser_get_attribute_string("start_delay_bands");
    if (!parse_delay_bands_attribute(start_delay_bands, race.start_delay_bands)) {
        log_error("BuildingType race requires valid start_delay_bands", start_delay_bands, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!parse_race_positive_attribute("laps", &race.laps, 1, 100) ||
        !parse_race_positive_attribute("ready_ticks", &race.ready_ticks, 0, 10000) ||
        !parse_race_positive_attribute("minimum_speed", &race.minimum_speed, 1, 32) ||
        !parse_race_positive_attribute("maximum_speed", &race.maximum_speed, 1, 32) ||
        !parse_race_positive_attribute("speed_roll", &race.speed_roll, 1, 10000) ||
        !parse_race_positive_attribute("track_margin", &race.track_margin, 0, 64) ||
        !parse_race_positive_attribute("lane_spacing", &race.lane_spacing, 1, 64) ||
        race.maximum_speed < race.minimum_speed) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_race = 1;
    g_parse_state.parsing_race = 1;
    return 1;
}

static void finish_race()
{
    RaceDefinition &race = g_parse_state.race_definition;
    if (!g_parse_state.saw_race_spawn || !g_parse_state.saw_race_finish ||
        !g_parse_state.saw_race_route || !g_parse_state.saw_race_teams || race.route.size() < 2 || race.teams.empty()) {
        log_error("BuildingType race is missing spawn, finish, route, or teams",
            g_parse_state.definition ? g_parse_state.definition->attr() : nullptr, 0);
        g_parse_state.error = 1;
    }
    if (race.route.size() >= 2) {
        const RaceRoutePoint &first = race.route[0];
        const RaceRoutePoint &second = race.route[1];
        const int finish_x = first.x - std::clamp(second.x - first.x, -1, 1);
        const int finish_y = first.y - std::clamp(second.y - first.y, -1, 1);
        if (race.spawn_x != first.x || race.spawn_y != first.y) {
            log_error("BuildingType race spawn must equal its first route waypoint", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
        }
        if (race.finish_x != finish_x || race.finish_y != finish_y) {
            log_error("BuildingType race finish must be one step behind its first route waypoint", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
        }
    }
    if (race.betting.enabled) {
        if (race.betting.window.empty()) {
            log_error("BuildingType betting race requires a window", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
        }
        for (const RaceTeamDefinition &team : race.teams) {
            if (team.portrait_path.empty() || team.portrait_image.empty()) {
                log_error("BuildingType betting race team requires a portrait", team.id.c_str(), 0);
                g_parse_state.error = 1;
            }
        }
    }
    for (const RaceTeamDefinition &team : race.teams) {
        if (race.track_margin + team.lane * race.lane_spacing > 64) {
            log_error("BuildingType race lane exceeds the supported visual track width", team.id.c_str(), team.lane);
            g_parse_state.error = 1;
        }
    }
    if (!g_parse_state.error) {
        g_parse_state.definition->set_race(std::move(race));
    }
    g_parse_state.parsing_race = 0;
}

static int parse_race_spawn_point()
{
    if (!g_parse_state.parsing_race || g_parse_state.saw_race_spawn ||
        !parse_race_positive_attribute("x", &g_parse_state.race_definition.spawn_x, -64, 64) ||
        !parse_race_positive_attribute("y", &g_parse_state.race_definition.spawn_y, -64, 64)) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_race_spawn = 1;
    return 1;
}

static int parse_race_finish_point()
{
    if (!g_parse_state.parsing_race || g_parse_state.saw_race_finish ||
        !parse_race_positive_attribute("x", &g_parse_state.race_definition.finish_x, -64, 64) ||
        !parse_race_positive_attribute("y", &g_parse_state.race_definition.finish_y, -64, 64)) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_race_finish = 1;
    return 1;
}

static int parse_race_route()
{
    if (!g_parse_state.parsing_race || g_parse_state.saw_race_route) {
        log_error("BuildingType race contains an invalid or duplicate route", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_race_route = 1;
    g_parse_state.parsing_race_route = 1;
    return 1;
}

static void finish_race_route()
{
    if (g_parse_state.race_definition.route.size() < 2) {
        log_error("BuildingType race route requires at least two waypoints", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_race_route = 0;
}

static int parse_race_waypoint()
{
    RaceRoutePoint point;
    if (!g_parse_state.parsing_race_route ||
        !parse_race_positive_attribute("x", &point.x, -64, 64) ||
        !parse_race_positive_attribute("y", &point.y, -64, 64)) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.race_definition.route.push_back(point);
    return 1;
}

static int parse_race_teams()
{
    if (!g_parse_state.parsing_race || g_parse_state.saw_race_teams) {
        log_error("BuildingType race contains invalid or duplicate teams", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_race_teams = 1;
    g_parse_state.parsing_race_teams = 1;
    return 1;
}

static void finish_race_teams()
{
    if (g_parse_state.race_definition.teams.empty()) {
        log_error("BuildingType race requires at least one team", 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_race_teams = 0;
}

static int parse_race_team()
{
    RaceTeamDefinition team;
    if (!g_parse_state.parsing_race_teams || g_parse_state.parsing_race_team ||
        !xml_definition::parse_required_nonempty_string_attribute("id", &team.id) ||
        !xml_definition::parse_required_nonempty_string_attribute("name_key", &team.name_key) ||
        !xml_definition::parse_required_nonempty_string_attribute("description_key", &team.description_key) ||
        !xml_definition::parse_required_nonempty_string_attribute("tooltip_key", &team.tooltip_key) ||
        !parse_race_positive_attribute("lane", &team.lane, 0, 255)) {
        log_error("BuildingType race team is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    for (const RaceTeamDefinition &existing : g_parse_state.race_definition.teams) {
        if (existing.id == team.id || existing.lane == team.lane) {
            log_error("BuildingType race team id or lane is duplicated", team.id.c_str(), team.lane);
            g_parse_state.error = 1;
            return 0;
        }
    }
    g_parse_state.current_race_team = std::move(team);
    g_parse_state.parsing_race_team = 1;
    return 1;
}

static void finish_race_team()
{
    RaceTeamDefinition &team = g_parse_state.current_race_team;
    if (team.graphics_path.empty() || team.body_entry.empty()) {
        log_error("BuildingType race team requires racer graphics", team.id.c_str(), 0);
        g_parse_state.error = 1;
    } else {
        g_parse_state.race_definition.teams.push_back(std::move(team));
    }
    g_parse_state.current_race_team = {};
    g_parse_state.parsing_race_team = 0;
}

static int parse_race_vehicle_offsets(const char *text, std::array<RaceRoutePoint, 8> &offsets)
{
    if (!text || !*text) return 0;
    std::string list(text);
    size_t start = 0;
    for (int direction = 0; direction < 8; ++direction) {
        const size_t end = list.find(',', start);
        const std::string token = xml_value::trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        const size_t separator = token.find(':');
        if (separator == std::string::npos || !xml_value::parse_int_strict(token.substr(0, separator).c_str(), &offsets[direction].x) ||
            !xml_value::parse_int_strict(token.substr(separator + 1).c_str(), &offsets[direction].y) ||
            offsets[direction].x < -64 || offsets[direction].x > 64 || offsets[direction].y < -64 || offsets[direction].y > 64 ||
            (direction < 7 && end == std::string::npos) || (direction == 7 && end != std::string::npos)) {
            return 0;
        }
        start = end + 1;
    }
    return 1;
}

static int parse_race_vehicle_behind(const char *text, std::array<int, 8> &behind)
{
    if (!text || !*text) return 1;
    static constexpr const char *directions[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    std::string list(text);
    size_t start = 0;
    while (start <= list.size()) {
        const size_t end = list.find(',', start);
        const std::string token = xml_value::trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        int found = -1;
        for (int direction = 0; direction < 8; ++direction) {
            if (token == directions[direction]) found = direction;
        }
        if (found < 0 || behind[found]) return 0;
        behind[found] = 1;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return 1;
}

static int parse_race_racer()
{
    if (!g_parse_state.parsing_race_team ||
        !xml_definition::parse_required_nonempty_string_attribute("path", &g_parse_state.current_race_team.graphics_path) ||
        !xml_definition::parse_required_nonempty_string_attribute("body", &g_parse_state.current_race_team.body_entry)) {
        log_error("BuildingType race racer requires path and body", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_race_team.graphics_path =
        xml_definition::normalize_path(g_parse_state.current_race_team.graphics_path.c_str());
    if (xml_parser_has_attribute("vehicle")) {
        g_parse_state.current_race_team.vehicle_entry =
            xml_value::trim_copy(xml_parser_get_attribute_string("vehicle"));
        if (g_parse_state.current_race_team.vehicle_entry.empty()) {
            log_error("BuildingType race racer vehicle cannot be empty", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
        if (!parse_race_vehicle_offsets(xml_parser_get_attribute_string("vehicle_offsets"),
                g_parse_state.current_race_team.vehicle_offsets) ||
            !parse_race_vehicle_behind(xml_parser_get_attribute_string("vehicle_behind"),
                g_parse_state.current_race_team.vehicle_behind)) {
            log_error("BuildingType race racer has invalid vehicle offsets or draw order", 0, 0);
            g_parse_state.error = 1;
            return 0;
        }
    } else if (xml_parser_has_attribute("vehicle_offsets") || xml_parser_has_attribute("vehicle_behind")) {
        log_error("BuildingType race racer cannot define vehicle placement without a vehicle", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    return 1;
}

static int parse_race_portrait()
{
    if (!g_parse_state.parsing_race_team ||
        !xml_definition::parse_required_nonempty_string_attribute("path", &g_parse_state.current_race_team.portrait_path) ||
        !xml_definition::parse_required_nonempty_string_attribute("image", &g_parse_state.current_race_team.portrait_image)) {
        log_error("BuildingType race portrait requires path and image", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_race_team.portrait_path =
        xml_definition::normalize_path(g_parse_state.current_race_team.portrait_path.c_str());
    return 1;
}

static int parse_race_betting()
{
    RaceBettingDefinition &betting = g_parse_state.race_definition.betting;
    if (!g_parse_state.parsing_race || g_parse_state.saw_race_betting || !xml_parser_has_attribute("enabled") ||
        !xml_value::parse_bool(xml_parser_get_attribute_string("enabled"), &betting.enabled)) {
        log_error("BuildingType race betting has an invalid enabled attribute", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.saw_race_betting = 1;
    if (betting.enabled) {
        if (!xml_definition::parse_required_nonempty_string_attribute("window", &betting.window) ||
            !parse_race_positive_attribute("wager_step", &betting.wager_step, 1, 1000000) ||
            !parse_race_positive_attribute("normal_multiplier", &betting.normal_multiplier, 1, 100) ||
            !parse_race_positive_attribute("festival_multiplier", &betting.festival_multiplier, 1, 100)) {
            g_parse_state.error = 1;
            return 0;
        }
    } else if (xml_parser_has_attribute("window") || xml_parser_has_attribute("wager_step") ||
        xml_parser_has_attribute("normal_multiplier") || xml_parser_has_attribute("festival_multiplier")) {
        log_error("Disabled BuildingType race betting cannot define betting parameters", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "building", parse_building_root, nullptr, nullptr, parse_building_root_text },
    { "identity", parse_identity, nullptr, "building", nullptr },
    { "model", parse_model, nullptr, "building", nullptr },
    { "desirability", parse_desirability, finish_desirability, "building", nullptr },
    { "value", parse_desirability_value, nullptr, "desirability", nullptr },
    { "step", parse_desirability_step, nullptr, "desirability", nullptr },
    { "step_size", parse_desirability_step_size, nullptr, "desirability", nullptr },
    { "range", parse_desirability_range, nullptr, "desirability", nullptr },
    { "foundation", parse_foundation, nullptr, "building", nullptr },
    { "button", parse_button, nullptr, "building", nullptr },
    { "menu", parse_button, nullptr, "building", nullptr },
    { "cycle", parse_cycle, nullptr, "building", nullptr },
    { "bridge", parse_bridge, nullptr, "building", nullptr },
    { "rubble", parse_rubble, nullptr, "building", nullptr },
    { "tile", parse_tile, finish_tile, "building", nullptr },
    { "tool", parse_tool, nullptr, "building", nullptr },
    { "mode", parse_smart_tool_mode, nullptr, "tool", nullptr },
    { "temple", parse_temple, nullptr, "building", nullptr },
    { "sound", parse_sound, nullptr, "building", nullptr },
    { "market", parse_market, nullptr, "building", nullptr },
    { "flags", parse_flags, nullptr, "building", nullptr },
    { "military", parse_military, finish_military, "building", nullptr },
    { "formation", parse_military_formation, nullptr, "military", nullptr },
    { "water_access", parse_provider_water_access, finish_provider_water_access, "building", nullptr },
    { "composition", parse_composition, finish_composition, "building", nullptr },
    { "child", parse_composition_child, finish_composition_child, "composition", nullptr },
    { "offset", parse_composition_offset, nullptr, "child", nullptr },
    { "graphics", parse_graphics, finish_graphics, "building|phase", nullptr },
    { "construction", parse_construction, finish_construction, "building", nullptr },
    { "phase", parse_construction_phase, finish_construction_phase, "construction", nullptr },
    { "provides", parse_water_access_provides, nullptr, "water_access", nullptr },
    { "requires", parse_water_access_requires, finish_water_access_requires, "water_access", nullptr },
    { "access", parse_water_access_requirement_access, nullptr, "requires", nullptr },
    { "source", parse_water_access_requirement_source, nullptr, "requires", nullptr },
    { "requirement", parse_construction_requirement, nullptr, "phase|construction", nullptr },
    { "node", parse_provider_water_access_node, nullptr, "water_access", nullptr },
    { "default", parse_graphics_default, finish_graphics_default, "graphics", nullptr },
    { "status_icon", parse_graphics_status_icon, nullptr, "graphics", nullptr },
    { "overlay_summary", parse_graphics_overlay_summary, nullptr, "graphics", nullptr },
    { "variant", parse_graphics_variant, finish_graphics_variant, "graphics", nullptr },
    { "path", parse_graphics_path, nullptr, "default|variant|graphics", nullptr },
    { "image", parse_graphics_image, nullptr, "default|variant|graphics", nullptr },
    { "resource_storage", parse_graphics_resource_storage, nullptr, "default|variant", nullptr },
    { "layer", parse_graphics_layer, finish_graphics_layer, "default|variant", nullptr },
    { "options", parse_graphics_options, finish_graphics_options, "default|variant|layer|graphics", nullptr },
    { "option", parse_graphics_option, nullptr, "options", nullptr },
    { "condition", parse_graphics_condition, nullptr, "variant|layer", nullptr },
    { "labor", parse_labor, finish_labor, "building", nullptr },
    { "employees", parse_labor_employees, nullptr, "labor", nullptr },
    { "labor_seeker", parse_labor_seeker, finish_labor_seeker, "labor", nullptr },
    { "method", parse_labor_seeker_method_node, nullptr, "labor_seeker", nullptr },
    { "amount", parse_labor_seeker_amount_node, nullptr, "labor_seeker", nullptr },
    { "culture_modules", parse_culture_modules, finish_culture_modules, "building", nullptr },
    { "culture_module", parse_culture_module_reference, nullptr, "culture_modules", nullptr },
    { "storages", parse_storages, finish_storages, "building", nullptr },
    { "storage", parse_storage_reference, nullptr, "storages", nullptr },
    { "production_methods", parse_production_methods, finish_production_methods, "building", nullptr },
    { "production_method", parse_production_method_reference, nullptr, "production_methods", nullptr },
    { "distribution", parse_distribution, nullptr, "building", nullptr },
    { "housing", parse_housing, nullptr, "building", nullptr },
    { "vacant_lot", parse_vacant_lot, nullptr, "building", nullptr },
    { "spawn_group", parse_spawn_group, nullptr, "building", nullptr },
    { "spawn", parse_spawn, nullptr, "spawn_group", nullptr },
    { "race", parse_race, finish_race, "building", nullptr },
    { "spawn_point", parse_race_spawn_point, nullptr, "race", nullptr },
    { "finish_point", parse_race_finish_point, nullptr, "race", nullptr },
    { "route", parse_race_route, finish_race_route, "race", nullptr },
    { "waypoint", parse_race_waypoint, nullptr, "route", nullptr },
    { "teams", parse_race_teams, finish_race_teams, "race", nullptr },
    { "team", parse_race_team, finish_race_team, "teams", nullptr },
    { "racer", parse_race_racer, nullptr, "team", nullptr },
    { "portrait", parse_race_portrait, nullptr, "team", nullptr },
    { "betting", parse_race_betting, nullptr, "race", nullptr }
};

static int starts_with_token(const char *cursor, const char *end, const char *token)
{
    size_t token_length = strlen(token);
    return end - cursor >= static_cast<long>(token_length) &&
        strncmp(cursor, token, token_length) == 0;
}

static const char *find_token(const char *cursor, const char *end, const char *token)
{
    while (cursor < end) {
        if (starts_with_token(cursor, end, token)) {
            return cursor;
        }
        cursor++;
    }
    return nullptr;
}

static const char *skip_xml_preamble(const char *cursor, const char *end)
{
    if (end - cursor >= 3 &&
        static_cast<unsigned char>(cursor[0]) == 0xef &&
        static_cast<unsigned char>(cursor[1]) == 0xbb &&
        static_cast<unsigned char>(cursor[2]) == 0xbf) {
        cursor += 3;
    }

    while (cursor < end) {
        while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) {
            cursor++;
        }
        if (starts_with_token(cursor, end, "<?")) {
            const char *close = find_token(cursor, end, "?>");
            if (!close) {
                return cursor;
            }
            cursor = close + 2;
            continue;
        }
        if (starts_with_token(cursor, end, "<!--")) {
            const char *close = find_token(cursor, end, "-->");
            if (!close) {
                return cursor;
            }
            cursor = close + 3;
            continue;
        }
        return cursor;
    }
    return cursor;
}

static int buffer_has_building_root(const std::vector<char> &buffer)
{
    const char *start = buffer.data();
    const char *end = start + buffer.size();
    const char *cursor = skip_xml_preamble(start, end);
    if (!starts_with_token(cursor, end, "<building")) {
        return 0;
    }

    cursor += strlen("<building");
    return cursor < end &&
        (*cursor == '>' || *cursor == '/' || std::isspace(static_cast<unsigned char>(*cursor)));
}

static int disabled_building_root_has_only_identity_attributes(const std::vector<char> &buffer)
{
    const char *start = buffer.data();
    const char *end = start + buffer.size();
    const char *cursor = skip_xml_preamble(start, end);
    if (!starts_with_token(cursor, end, "<building")) {
        return 0;
    }
    cursor += strlen("<building");
    int saw_type = 0;
    int saw_disabled = 0;
    while (cursor < end) {
        while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (cursor >= end || *cursor == '>' || *cursor == '/') {
            return cursor < end && saw_type && saw_disabled;
        }

        const char *name_start = cursor;
        while (cursor < end && !std::isspace(static_cast<unsigned char>(*cursor)) &&
            *cursor != '=' && *cursor != '>' && *cursor != '/') {
            ++cursor;
        }
        const std::string name(name_start, cursor);
        if (name == "type") {
            if (saw_type) {
                return 0;
            }
            saw_type = 1;
        } else if (name == "disabled") {
            if (saw_disabled) {
                return 0;
            }
            saw_disabled = 1;
        } else {
            return 0;
        }
        while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (cursor >= end || *cursor != '=') {
            return 0;
        }
        ++cursor;
        while (cursor < end && std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (cursor >= end || (*cursor != '\'' && *cursor != '"')) {
            return 0;
        }
        const char quote = *cursor++;
        while (cursor < end && *cursor != quote) {
            ++cursor;
        }
        if (cursor >= end) {
            return 0;
        }
        ++cursor;
    }
    return 0;
}

// Input: one authored graphics target from a BuildingType plus a short scope label for logging.
// Output: true when the referenced runtime group/image namespace exists up front, false when the BuildingType should lose native graphics.
static int validate_graphics_target_entry(
    const BuildingType &definition,
    const GraphicsTarget &target,
    const char *target_scope)
{
    if (target.is_resource_storage()) {
        return 1;
    }
    if (target.has_options()) {
        if (target.has_image()) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s scope=%s",
                definition.attr(),
                target_scope ? target_scope : "graphics");
            log_error("Disabling invalid runtime graphics because an option target also has a direct image", detail, 0);
            return 0;
        }

        const int option_count = target.option_count();
        for (int i = 0; i < option_count; i++) {
            // Validate the exact target the renderer will see after path inheritance.
            GraphicsTarget resolved = target.resolved_option(static_cast<unsigned char>(i));
            char scope[96];
            snprintf(
                scope,
                sizeof(scope),
                "%s.option[%u]",
                target_scope ? target_scope : "graphics",
                static_cast<unsigned int>(i));
            if (resolved.no_draw()) {
                continue;
            }
            if (!resolved.has_image()) {
                char detail[512];
                snprintf(
                    detail,
                    sizeof(detail),
                    "building=%s scope=%s",
                    definition.attr(),
                    scope);
                log_error("Disabling invalid runtime graphics because an option is missing image", detail, 0);
                return 0;
            }
            if (!resolved.has_path()) {
                char detail[512];
                snprintf(
                    detail,
                    sizeof(detail),
                    "building=%s scope=%s",
                    definition.attr(),
                    scope);
                log_error("Disabling invalid runtime graphics because an option is missing path", detail, 0);
                return 0;
            }
            if (!validate_graphics_target_entry(definition, resolved, scope)) {
                return 0;
            }
        }
        return 1;
    }

    if (!target.has_path()) {
        return 1;
    }
    if (!image_group_payload_load(target.path())) {
        char detail[512];
        snprintf(
            detail,
            sizeof(detail),
            "building=%s scope=%s path=%s",
            definition.attr(),
            target_scope ? target_scope : "graphics",
            target.path());
        log_error("Disabling invalid runtime graphics because the group could not be loaded", detail, 0);
        return 0;
    }

    const ImageGroupPayload *payload = image_group_payload_get(target.path());
    if (!payload) {
        char detail[512];
        snprintf(
            detail,
            sizeof(detail),
            "building=%s scope=%s path=%s",
            definition.attr(),
            target_scope ? target_scope : "graphics",
            target.path());
        log_error("Disabling invalid runtime graphics because the payload could not be found", detail, 0);
        return 0;
    }

    const ImageGroupEntry *entry = target.has_image() ? payload->entry_for(target.image()) : payload->default_entry();
    if (!entry) {
        char detail[768];
        if (target.has_image()) {
            snprintf(
                detail,
                sizeof(detail),
                "building=%s scope=%s path=%s image=%s",
                definition.attr(),
                target_scope ? target_scope : "graphics",
                target.path(),
                target.image());
            log_error("Disabling invalid runtime graphics because the referenced image id could not be resolved", detail, 0);
        } else {
            snprintf(
                detail,
                sizeof(detail),
                "building=%s scope=%s path=%s",
                definition.attr(),
                target_scope ? target_scope : "graphics",
                target.path());
            log_error("Disabling invalid runtime graphics because the group has no default entry", detail, 0);
        }
        return 0;
    }

    GraphicsTarget resolved_target_for_layers = target.resolved_option(0);
    const std::vector<GraphicsLayer> &layers = resolved_target_for_layers.layers();
    for (size_t i = 0; i < layers.size(); i++) {
        const GraphicsLayer &layer = layers[i];
        GraphicsLayer resolved_layer = layer.has_options() ? layer.resolved_option(0) : layer;
        char scope[96];
        snprintf(
            scope,
            sizeof(scope),
            "%s.layer[%u]",
            target_scope ? target_scope : "graphics",
            static_cast<unsigned int>(i));
        if (!resolved_layer.has_path()) {
            char detail[512];
            snprintf(detail, sizeof(detail), "building=%s scope=%s", definition.attr(), scope);
            log_error("Disabling invalid runtime graphics because a layer is missing path", detail, 0);
            return 0;
        }
        if (layer.has_options()) {
            const int option_count = layer.option_count();
            for (int option_index = 0; option_index < option_count; option_index++) {
                GraphicsLayer resolved_option = layer.resolved_option(static_cast<unsigned char>(option_index));
                if (!resolved_option.has_path() || !resolved_option.has_image()) {
                    char detail[512];
                    snprintf(detail, sizeof(detail), "building=%s scope=%s option=%d",
                        definition.attr(), scope, option_index);
                    log_error("Disabling invalid runtime graphics because a layer option is incomplete", detail, 0);
                    return 0;
                }
                if (!image_group_payload_load(resolved_option.path())) {
                    char detail[512];
                    snprintf(detail, sizeof(detail), "building=%s scope=%s path=%s",
                        definition.attr(), scope, resolved_option.path());
                    log_error("Disabling invalid runtime graphics because a layer group could not be loaded", detail, 0);
                    return 0;
                }
                const ImageGroupPayload *option_payload = image_group_payload_get(resolved_option.path());
                if (!option_payload || !option_payload->entry_for(resolved_option.image())) {
                    char detail[768];
                    snprintf(detail, sizeof(detail), "building=%s scope=%s path=%s image=%s",
                        definition.attr(), scope, resolved_option.path(), resolved_option.image());
                    log_error("Disabling invalid runtime graphics because a layer image id could not be resolved", detail, 0);
                    return 0;
                }
            }
            continue;
        }
        if (!resolved_layer.has_image()) {
            char detail[512];
            snprintf(detail, sizeof(detail), "building=%s scope=%s", definition.attr(), scope);
            log_error("Disabling invalid runtime graphics because a layer is missing image", detail, 0);
            return 0;
        }
        if (!image_group_payload_load(resolved_layer.path())) {
            char detail[512];
            snprintf(detail, sizeof(detail), "building=%s scope=%s path=%s",
                definition.attr(), scope, resolved_layer.path());
            log_error("Disabling invalid runtime graphics because a layer group could not be loaded", detail, 0);
            return 0;
        }
        const ImageGroupPayload *layer_payload = image_group_payload_get(resolved_layer.path());
        if (!layer_payload || !layer_payload->entry_for(resolved_layer.image())) {
            char detail[768];
            snprintf(detail, sizeof(detail), "building=%s scope=%s path=%s image=%s",
                definition.attr(), scope, resolved_layer.path(), resolved_layer.image());
            log_error("Disabling invalid runtime graphics because a layer image id could not be resolved", detail, 0);
            return 0;
        }
    }
    return 1;
}

// Input: one parsed BuildingType definition with module-owned native graphics targets.
// Output: true only when every target resolves. Invalid authored graphics reject startup.
static int validate_runtime_graphics(const BuildingType &definition)
{
    if (definition.has_graphic()) {
        if (!validate_graphics_target_entry(definition, definition.graphics().default_target(), "default")) return 0;
        const std::vector<GraphicsVariant> &variants = definition.graphics().variants();
        for (size_t i = 0; i < variants.size(); i++) {
            char scope[64];
            snprintf(scope, sizeof(scope), "variant[%u]", static_cast<unsigned int>(i));
            if (!validate_graphics_target_entry(definition, variants[i].target, scope)) {
                return 0;
            }
        }
    }

    for (const ConstructionPhaseGraphics &phase : definition.graphics().construction_phases()) {
        char scope[64];
        snprintf(scope, sizeof(scope), "construction.phase[%d]", phase.phase);
        if (!validate_graphics_target_entry(definition, phase.target, scope)) {
            return 0;
        }
    }

    return 1;
}

static int resolve_runtime_references(BuildingType &definition, const char *filename)
{
    for (const BuildingCultureModule &culture_module_reference : definition.culture_modules()) {
        const std::string &culture_module_path = culture_module_reference.reference_path;
        const CultureModule *culture_module = find_culture_module_definition(culture_module_path.c_str());
        if (!culture_module) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s culture_module_path=%s file=%s",
                definition.attr(),
                culture_module_path.c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType culture_module reference.", detail);
            log_error("Unable to resolve BuildingType culture_module reference", detail, 0);
            return 0;
        }
        definition.resolve_culture_module(culture_module_path, culture_module);
    }

    for (const std::string &storage_path : definition.storage_reference_paths()) {
        const StorageType *storage_type = find_storage_type_definition(storage_path.c_str());
        if (!storage_type) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s storage_path=%s file=%s",
                definition.attr(),
                storage_path.c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType storage reference.", detail);
            log_error("Unable to resolve BuildingType storage reference", detail, 0);
            return 0;
        }
        definition.add_storage_type(storage_type);
    }

    for (const std::string &production_path : definition.production_method_reference_paths()) {
        ProductionMethod *production_method = find_production_method_definition(production_path.c_str());
        if (!production_method) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s production_method_path=%s file=%s",
                definition.attr(),
                production_path.c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType production_method reference.", detail);
            log_error("Unable to resolve BuildingType production_method reference", detail, 0);
            return 0;
        }
        definition.add_production_method(production_method);
    }

    if (!definition.distribution_reference_path().empty()) {
        const Distribution *distribution = find_distribution_definition(definition.distribution_reference_path().c_str());
        if (!distribution) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s distribution_path=%s file=%s",
                definition.attr(),
                definition.distribution_reference_path().c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType distribution reference.", detail);
            log_error("Unable to resolve BuildingType distribution reference", detail, 0);
            return 0;
        }
        definition.set_distribution(distribution);
    }

    HousingDef &housing = definition.housing_def();
    if (!housing.profile_path.empty()) {
        const HousingProfileDef *profile = find_housing_profile_definition(housing.profile_path.c_str());
        if (!profile) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s housing_path=%s file=%s",
                definition.attr(),
                housing.profile_path.c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType housing reference.", detail);
            log_error("Unable to resolve BuildingType housing reference", detail, 0);
            return 0;
        }
        housing.profile = profile;
    }

    if (!definition.temple_religion_reference_path().empty()) {
        const Religion *religion = find_religion_definition(definition.temple_religion_reference_path().c_str());
        if (!religion) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s religion_path=%s file=%s",
                definition.attr(),
                definition.temple_religion_reference_path().c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType temple religion reference.", detail);
            log_error("Unable to resolve BuildingType temple religion reference", detail, 0);
            return 0;
        }
        definition.set_temple_religion(religion);
    }

    if (definition.has_military()) {
        const FormationType *formation =
            formation_type_registry_impl::find_formation_type(definition.military().formation_reference().c_str());
        if (!formation) {
            char detail[512];
            snprintf(
                detail,
                sizeof(detail),
                "building=%s formation=%s file=%s",
                definition.attr(),
                definition.military().formation_reference().c_str(),
                filename ? filename : "");
            error_context_report_error("Unable to resolve BuildingType military formation reference.", detail);
            log_error("Unable to resolve BuildingType military formation reference", detail, 0);
            return 0;
        }
        definition.set_military_formation_type(formation);
    }

    return 1;
}

static int validate_runtime_class_nodes(const BuildingType &definition)
{
    if (compare_text(definition.attr(), "market") == 0 && !definition.has_market()) {
        log_error("BuildingType market is missing required market node", definition.attr(), 0);
        return 0;
    }
    if ((compare_text(definition.attr(), "market") == 0 ||
         compare_text(definition.attr(), "tavern") == 0) &&
        !definition.has_distribution()) {
        log_error("BuildingType class is missing required distribution node", definition.attr(), 0);
        return 0;
    }
    return 1;
}

struct ParsedBuildingTypeDefinition {
    std::unique_ptr<BuildingType> definition;
    int disabled = 0;
};

static int parse_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    ParsedBuildingTypeDefinition &result)
{
    ErrorContextScope error_scope("building_type_registry.parse_definition", filename);

    g_parse_state = {};
    const bool parsed = xml_definition::parse_buffer(
        filename,
        "BuildingType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    // TEMPORARY: metadata-only BuildingType XML is accepted while build authority moves out of legacy code.
    // This must tighten again once metadata, graphics, placement, and runtime behavior are all XML-owned.
    // Live bad XML should fail at load time instead of quietly registering incomplete building definitions.
    int has_supported_node = g_parse_state.saw_identity || g_parse_state.saw_model || g_parse_state.saw_foundation ||
        g_parse_state.saw_button || g_parse_state.saw_cycle || g_parse_state.saw_bridge ||
        g_parse_state.saw_rubble || g_parse_state.saw_tile || g_parse_state.saw_tool ||
        g_parse_state.saw_temple || g_parse_state.saw_sound ||
        g_parse_state.saw_market || g_parse_state.saw_flags || g_parse_state.saw_military ||
        g_parse_state.saw_desirability || g_parse_state.saw_graphic ||
        g_parse_state.saw_construction || g_parse_state.saw_spawn ||
        g_parse_state.saw_culture_modules ||
        g_parse_state.saw_storages || g_parse_state.saw_production_methods || g_parse_state.saw_distribution ||
        g_parse_state.saw_housing || g_parse_state.saw_vacant_lot ||
        g_parse_state.saw_labor || g_parse_state.saw_provider_water_access || g_parse_state.saw_composed ||
        g_parse_state.saw_race;
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || !g_parse_state.definition ||
        (!g_parse_state.disabled && !has_supported_node) ||
        (g_parse_state.disabled && (has_supported_node || g_parse_state.saw_root_text ||
            !disabled_building_root_has_only_identity_attributes(buffer)))) {
        if (!g_parse_state.disabled && !has_supported_node) {
            log_error("BuildingType xml is missing a supported node", filename, 0);
        } else if (g_parse_state.disabled && has_supported_node) {
            log_error("Disabled BuildingType tombstone must contain only type and disabled", filename, 0);
        }
        return 0;
    }
    g_parse_state.definition->set_graphics(std::move(g_parse_state.graphics_definition));
    result.definition = std::move(g_parse_state.definition);
    result.disabled = g_parse_state.disabled;
    return 1;
}

static int resolve_housing_transition(BuildingType &definition, HousingTransitionKind kind, const char *name)
{
    HousingTransitionDef &transition = definition.housing_def().transition(kind);
    const std::string &text_id = transition.type_path;
    if (text_id.empty()) {
        return 1;
    }

    building_type target = runtime_id_from_text(text_id.c_str());
    if (target == BUILDING_NONE) {
        target = find_building_type_by_attr(text_id.c_str());
        if (target == BUILDING_NONE) {
            char detail[512];
            snprintf(detail, sizeof(detail), "building=%s transition=%s target=%s",
                definition.attr(), name ? name : "", text_id.c_str());
            error_context_report_error("BuildingType housing transition target does not exist.", detail);
            return 0;
        }
    }

    transition.type = definition_for_type(target);
    if (!transition.type || !transition.type->has_housing()) {
        error_context_report_error("BuildingType housing transition target has no HousingDef.", text_id.c_str());
        return 0;
    }
    if (kind == HousingTransitionKind::SplitTo) {
        const FoundationDef *foundation = transition.type->foundation_def();
        if (!foundation || foundation->cells().size() != 1) {
            error_context_report_error(
                "BuildingType housing split_to target must have a one-cell FoundationDef.",
                text_id.c_str());
            return 0;
        }
    }
    return 1;
}

static int resolve_vacant_lot_fill_type(BuildingType &definition)
{
    HousingTransitionDef &transition = definition.housing_def().transition(HousingTransitionKind::VacantLot);
    const std::string &text_id = transition.type_path;
    if (text_id.empty()) {
        return 1;
    }

    building_type target = runtime_id_from_text(text_id.c_str());
    if (target == BUILDING_NONE) {
        target = find_building_type_by_attr(text_id.c_str());
    }

    const BuildingType *target_definition = definition_for_type(target);
    const FoundationDef *target_foundation = target_definition ? target_definition->foundation_def() : nullptr;
    if (target == BUILDING_NONE || !target_definition || !target_definition->has_housing() ||
        !target_foundation || target_foundation->cells().size() != 1) {
        char detail[512];
        snprintf(detail, sizeof(detail), "building=%s fill_to=%s",
            definition.attr(), text_id.c_str());
        error_context_report_error("BuildingType vacant_lot fill target does not exist.", detail);
        return 0;
    }

    transition.type = definition_for_type(target);
    return 1;
}

static building_type resolve_building_type_reference(const std::string &text_id)
{
    building_type target = runtime_id_from_text(text_id.c_str());
    if (target == BUILDING_NONE) {
        target = find_building_type_by_attr(text_id.c_str());
    }
    return target;
}

static int resolve_smart_tool_references()
{
    ErrorContextScope error_scope("building_type_registry.resolve_smart_tools");

    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }
        const std::vector<SmartToolModeDefinition> &modes = definition->tool().smart_tool().modes();
        for (std::size_t index = 0; index < modes.size(); ++index) {
            const building_type target = resolve_building_type_reference(modes[index].type_reference);
            const BuildingType *target_definition = definition_for_type(target);
            if (target == BUILDING_NONE || !target_definition) {
                char detail[512];
                snprintf(detail, sizeof(detail), "tool=%s target=%s",
                    definition->attr(), modes[index].type_reference.c_str());
                error_context_report_error("SmartToolDef target type does not exist.", detail);
                return 0;
            }
            definition->resolve_tool_smart_mode_type(index, target_definition);
        }
    }
    return 1;
}

static int resolve_rubble_decay_references()
{
    ErrorContextScope error_scope("building_type_registry.resolve_rubble_decay_references");

    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->has_rubble() || definition->rubble().decays_to.empty()) {
            continue;
        }

        const building_type target_type = resolve_building_type_reference(definition->rubble().decays_to);
        const BuildingType *target_definition = definition_for_type(target_type);
        if (target_type == BUILDING_NONE || !target_definition || !target_definition->has_rubble()) {
            char detail[512];
            snprintf(detail, sizeof(detail), "building=%s decays_to=%s",
                definition->attr(), definition->rubble().decays_to.c_str());
            error_context_report_error("BuildingType rubble decay target does not exist or is not rubble.", detail);
            log_error("Unable to resolve BuildingType rubble decay target", detail, 0);
            return 0;
        }
        definition->set_rubble_decay_type(target_definition);
    }
    return 1;
}

static int resolve_construction_references()
{
    ErrorContextScope error_scope("building_type_registry.resolve_construction_references");

    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->has_construction()) {
            continue;
        }
        const std::string &required_building =
            definition->construction().required_building_reference();
        if (required_building.empty()) {
            continue;
        }
        building_type type = resolve_building_type_reference(required_building);
        if (type == BUILDING_NONE || !definition_for_type(type)) {
            char detail[512];
            snprintf(detail, sizeof(detail), "building=%s requires_building=%s",
                definition->attr(), required_building.c_str());
            error_context_report_error(
                "BuildingType construction required building does not exist.", detail);
            log_error("Unable to resolve BuildingType construction required building",
                detail, 0);
            return 0;
        }
        definition->set_construction_required_building_type(type);
    }
    return 1;
}

static int resolve_native_composition_references()
{
    ErrorContextScope error_scope("building_type_registry.resolve_native_compositions");

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (definition &&
            definition->graphics().overlay_summary_policy() ==
                GraphicsOverlaySummaryPolicy::CompositionOwner &&
            !definition->has_composition()) {
            log_error("BuildingType composition_owner overlay summary requires a composition",
                definition->attr(), definition->type());
            return 0;
        }
        if (!definition || !definition->has_composition()) {
            continue;
        }
        if (definition->has_housing()) {
            log_error("BuildingType cannot combine housing with fixed composition",
                definition->attr(), definition->type());
            return 0;
        }
        for (CompositionChildDef &child : definition->composition().children()) {
            const building_type child_type = resolve_building_type_reference(child.type_reference);
            child.type = definition_for_type(child_type);
            if (!child.type) {
                log_error("Unable to resolve CompositionDef child type",
                    child.type_reference.c_str(), definition->type());
                return 0;
            }
            if (child.type->has_housing()) {
                log_error("BuildingType composition cannot contain a housing child",
                    child.type_reference.c_str(), definition->type());
                return 0;
            }
            definition->inherit_labor_category(child.type->labor_category());
        }
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->has_composition()) {
            continue;
        }
        for (int rotation = 0; rotation < 4; ++rotation) {
            const CompositionLayoutResult layout = build_composition_layout(
                definition.get(), definition->composition(), 0, 0, rotation, resolve_composition_foundation);
            if (!layout.valid()) {
                log_error("Invalid BuildingType CompositionDef", layout.detail.c_str(), definition->type());
                return 0;
            }
        }
    }
    return 1;
}

static int resolve_housing_transitions()
{
    ErrorContextScope error_scope("building_type_registry.resolve_housing_transitions");

    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->has_housing()) {
            continue;
        }
        if (!resolve_housing_transition(*definition, HousingTransitionKind::EvolveTo, "evolve_to") ||
            !resolve_housing_transition(*definition, HousingTransitionKind::DevolveTo, "devolve_to") ||
            !resolve_housing_transition(*definition, HousingTransitionKind::MergeTo, "merge_to") ||
            !resolve_housing_transition(*definition, HousingTransitionKind::SplitTo, "split_to")) {
            return 0;
        }
    }

    int saw_vacant_lot = 0;
    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->is_vacant_lot()) {
            continue;
        }
        if (saw_vacant_lot) {
            error_context_report_error(
                "BuildingType contains more than one vacant_lot definition.",
                definition->attr());
            return 0;
        }
        saw_vacant_lot = 1;
        if (!resolve_vacant_lot_fill_type(*definition)) {
            return 0;
        }
    }

    building_type vacant_lot_target = building_type_registry_impl::vacant_lot_occupancy_type();
    const BuildingType *target_definition = definition_for_type(vacant_lot_target);
    const FoundationDef *target_foundation = target_definition ? target_definition->foundation_def() : nullptr;
    if (vacant_lot_target == BUILDING_NONE || !target_definition || !target_definition->has_housing() ||
        !target_foundation || target_foundation->cells().size() != 1) {
        error_context_report_error(
            "BuildingType housing vacant-lot fill target does not exist.",
            "target=first_housing_profile foundation_cells=1");
        return 0;
    }

    return 1;
}

static int resolve_foundation_references()
{
    ErrorContextScope error_scope("building_type_registry.resolve_foundation_references");
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || definition->foundation_reference_path().empty()) {
            continue;
        }
        const FoundationDef *foundation =
            find_foundation_definition(definition->foundation_reference_path().c_str());
        if (!foundation) {
            log_error("Unable to resolve BuildingType foundation reference",
                definition->foundation_reference_path().c_str(), definition->type());
            return 0;
        }
        if (definition->water_access().requires_open_water() && !foundation->has_water_requirement()) {
            log_error("BuildingType requires open water but its Foundation has no water cells",
                definition->attr(), definition->type());
            return 0;
        }
        definition->set_foundation_definition(foundation);
    }
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }
        for (const std::string &reference : definition->foundation_replacement_reference_paths()) {
            const building_type replacement_type = resolve_building_type_reference(reference);
            const BuildingType *replacement = definition_for_type(replacement_type);
            const FoundationDef *replacement_foundation = replacement ? replacement->foundation_def() : nullptr;
            if (!replacement || !replacement_foundation || replacement_foundation->cells().size() != 1) {
                char detail[512];
                snprintf(detail, sizeof(detail), "building=%s replaces=%s",
                    definition->attr(), reference.c_str());
                error_context_report_error(
                    "BuildingType foundation replacement target must resolve to a one-cell BuildingType.", detail);
                return 0;
            }
            definition->add_foundation_replacement_type(replacement);
        }
    }
    return 1;
}

static int validate_identity_uniqueness()
{
    ErrorContextScope error_scope("building_type_registry.validate_identity_uniqueness");
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }
        for (const std::string &alias : definition->identity().aliases()) {
            for (const std::unique_ptr<BuildingType> &other : g_building_types) {
                if (!other || other.get() == definition.get()) {
                    continue;
                }
                if (other->matches_identity(alias)) {
                    char detail[512];
                    snprintf(detail, sizeof(detail), "identity=%s alias=%s conflicts_with=%s",
                        definition->attr(), alias.c_str(), other->attr());
                    error_context_report_error("BuildingType identity alias is not globally unique.", detail);
                    return 0;
                }
            }
        }
    }
    return 1;
}

struct LayeredBuildingTypeDefinition {
    ParsedBuildingTypeDefinition parsed;
    mod_definition::DefinitionSource source;
    int allocation_category = 0;
};

using LayeredBuildingTypeDefinitions = std::map<std::string, LayeredBuildingTypeDefinition>;

struct StagedBuildingTypeRegistry {
    std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> definitions;
    mod_definition::DefinitionOverlayTracker overlays;
};

static int category_order(const std::string &category)
{
    if (category == "BuildingType") {
        return 0;
    }
    if (category == "Tiles") {
        return 1;
    }
    return 2;
}

static building_type fixed_runtime_type_for_identity(const char *type_attr)
{
    if (!type_attr || !*type_attr || find_housing_profile_definition_for_legacy_building_path(type_attr) ||
        !building_type_legacy_migration_text_id_is_xml_owned(type_attr)) {
        return BUILDING_NONE;
    }
    const uint16_t legacy_type = building_type_legacy_migration_enum_for_text_id(type_attr);
    if (legacy_type == LEGACY_BUILDING_THEATER || legacy_type == LEGACY_BUILDING_WELL ||
        legacy_type <= BUILDING_NONE || legacy_type >= BUILDING_TYPE_MAX) {
        return BUILDING_NONE;
    }
    return static_cast<building_type>(legacy_type);
}

static int stage_building_type_definition(
    LayeredBuildingTypeDefinitions &winners,
    mod_definition::DefinitionOverlayTracker &overlays,
    ParsedBuildingTypeDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->attr() : "";
    if (!overlays.apply(stable_id, parsed.disabled != 0, source)) {
        if (failure_reason) {
            *failure_reason = overlays.failure_reason();
        }
        log_error("Unable to layer BuildingType definition", overlays.failure_reason().c_str(), 0);
        return 0;
    }
    const auto existing = winners.find(stable_id);
    const int allocation_category = existing == winners.end() ?
        category_order(source.category) : existing->second.allocation_category;
    winners[stable_id] = {std::move(parsed), source, allocation_category};
    return 1;
}

static int materialize_building_type_winners(
    LayeredBuildingTypeDefinitions &winners,
    mod_definition::DefinitionOverlayTracker overlays,
    StagedBuildingTypeRegistry &staged,
    std::string *failure_reason)
{
    std::vector<LayeredBuildingTypeDefinition *> ordered;
    ordered.reserve(winners.size());
    for (auto &entry : winners) {
        if (!entry.second.parsed.disabled) {
            ordered.push_back(&entry.second);
        }
    }
    std::sort(ordered.begin(), ordered.end(), [](const LayeredBuildingTypeDefinition *left,
        const LayeredBuildingTypeDefinition *right) {
        const int left_category = left->allocation_category;
        const int right_category = right->allocation_category;
        if (left_category != right_category) {
            return left_category < right_category;
        }
        const int identity_order = compare_text(
            left->parsed.definition->attr(), right->parsed.definition->attr());
        if (identity_order != 0) {
            return identity_order < 0;
        }
        return left->source.normalized_definition_path < right->source.normalized_definition_path;
    });

    building_type next_dynamic = BUILDING_DYNAMIC_TYPE_FIRST;
    for (LayeredBuildingTypeDefinition *winner : ordered) {
        BuildingType *definition = winner->parsed.definition.get();
        building_type type = fixed_runtime_type_for_identity(definition->attr());
        if (type == BUILDING_NONE) {
            while (next_dynamic < BUILDING_TYPE_MAX && staged.definitions[next_dynamic]) {
                next_dynamic = static_cast<building_type>(next_dynamic + 1);
            }
            type = next_dynamic;
            if (next_dynamic < BUILDING_TYPE_MAX) {
                next_dynamic = static_cast<building_type>(next_dynamic + 1);
            }
        }
        if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX || staged.definitions[type]) {
            const std::string detail = std::string("Unable to assign effective BuildingType runtime id: ") +
                definition->attr() + " from " + winner->source.describe();
            if (failure_reason) {
                *failure_reason = detail;
            }
            log_error("Unable to materialize BuildingType winner", detail.c_str(), 0);
            return 0;
        }
        definition->assign_runtime_type(type);
        staged.definitions[type] = std::move(winner->parsed.definition);
    }
    staged.overlays = std::move(overlays);
    return 1;
}

static int build_layered_building_type_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedBuildingTypeRegistry &staged,
    std::string *failure_reason)
{
    LayeredBuildingTypeDefinitions winners;
    mod_definition::DefinitionOverlayTracker overlays;
    int saw_primary_category_definition = 0;
    if (!mod_definition::for_each_definition_file(
            layers,
            {"BuildingType", "Tiles", "BuildingTypeMenu"},
            "BuildingType",
            true,
            [&](const mod_definition::DefinitionSource &source) {
            std::vector<char> buffer;
            if (!xml_definition::load_file_to_buffer(
                    source.full_path.c_str(), buffer, source.full_path.c_str())) {
                return false;
            }
            if (!buffer_has_building_root(buffer)) {
                const std::string detail = "BuildingType xml root is not <building>: " + source.describe();
                if (failure_reason) {
                    *failure_reason = detail;
                }
                log_error("BuildingType xml root is not <building>", source.full_path.c_str(), 0);
                return false;
            }
            ParsedBuildingTypeDefinition parsed;
            if (!parse_definition_buffer(source.full_path.c_str(), buffer, parsed)) {
                if (failure_reason) {
                    *failure_reason = "Unable to parse BuildingType xml: " + source.describe();
                }
                log_error("Unable to parse BuildingType xml", source.full_path.c_str(), 0);
                return false;
            }
            if (source.category == "BuildingType") {
                saw_primary_category_definition = 1;
            }
            return stage_building_type_definition(
                winners, overlays, std::move(parsed), source, failure_reason) != 0;
        },
        nullptr,
        failure_reason)) {
        return 0;
    }
    if (!saw_primary_category_definition) {
        const std::string detail = "No BuildingType xml files found in the BuildingType category of the mod stack.";
        if (failure_reason) {
            *failure_reason = detail;
        }
        log_error(detail.c_str(), 0, 0);
        return 0;
    }
    return materialize_building_type_winners(
        winners, std::move(overlays), staged, failure_reason);
}

static int validate_final_winner_definitions(
    StagedBuildingTypeRegistry &staged,
    std::string *failure_reason)
{
    for (std::unique_ptr<BuildingType> &definition : staged.definitions) {
        if (!definition) {
            continue;
        }
        const mod_definition::DefinitionOverlayEntry *source = staged.overlays.find(definition->attr());
        const char *filename = source ? source->source.full_path.c_str() : definition->attr();
        if (definition->has_race()) {
            const RaceDefinition &race = definition->race();
            static constexpr const char *direction_suffixes[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
            if (!figure_type_registry_impl::definition_for(race.participant_figure)) {
                if (failure_reason) *failure_reason = std::string("Race participant FigureType is unresolved: ") + race.participant_figure_reference;
                return 0;
            }
            for (const RaceTeamDefinition &team : race.teams) {
                for (const char *suffix : direction_suffixes) {
                    const std::string body_entry = team.body_entry + "_" + suffix;
                    if (!ImageGroupEntryRef::from_group(team.graphics_path, body_entry).runtime_slice().is_valid()) {
                        if (failure_reason) *failure_reason = std::string("Race team body graphics are unresolved: ") + team.id + " direction=" + suffix;
                        return 0;
                    }
                    if (!team.vehicle_entry.empty()) {
                        const std::string vehicle_entry = team.vehicle_entry + "_" + suffix;
                        if (!ImageGroupEntryRef::from_group(team.graphics_path, vehicle_entry).runtime_slice().is_valid()) {
                            if (failure_reason) *failure_reason = std::string("Race team vehicle graphics are unresolved: ") + team.id + " direction=" + suffix;
                            return 0;
                        }
                    }
                }
                if (race.betting.enabled &&
                    !ImageGroupEntryRef::from_group(team.portrait_path, team.portrait_image).runtime_slice().is_valid()) {
                    if (failure_reason) *failure_reason = std::string("Race team portrait is unresolved: ") + team.id;
                    return 0;
                }
            }
        }
        if (!validate_runtime_graphics(*definition) ||
            !resolve_runtime_references(*definition, filename) ||
            !validate_runtime_class_nodes(*definition)) {
            if (failure_reason && failure_reason->empty()) {
                *failure_reason = std::string("Unable to validate final BuildingType winner: ") +
                    definition->attr();
            }
            return 0;
        }
    }
    return 1;
}

static int resolve_staged_building_type_registry(
    StagedBuildingTypeRegistry &staged,
    std::string *failure_reason)
{
    if (!validate_final_winner_definitions(staged, failure_reason)) {
        return 0;
    }
    std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> previous = std::move(g_building_types);
    g_building_types = std::move(staged.definitions);
    const int resolved =
        validate_identity_uniqueness() &&
        resolve_foundation_references() &&
        resolve_smart_tool_references() &&
        resolve_housing_transitions() &&
        resolve_rubble_decay_references() &&
        resolve_construction_references() &&
        resolve_native_composition_references();
    staged.definitions = std::move(g_building_types);
    g_building_types = std::move(previous);
    if (!resolved && failure_reason && failure_reason->empty()) {
        *failure_reason = "Unable to resolve final BuildingType overlay winners.";
    }
    return resolved;
}

}

namespace {

std::string g_building_type_registry_failure_reason;

int fail_building_type_registry(const char *summary, const char *detail = nullptr)
{
    g_building_type_registry_failure_reason = summary ? summary : "Unable to load BuildingType definitions.";
    if (detail && *detail) {
        g_building_type_registry_failure_reason += "\n\n";
        g_building_type_registry_failure_reason += detail;
    }
    return 0;
}

} // namespace

int building_type_registry_load(void)
{
    using namespace building_type_registry_impl;
    g_building_type_registry_failure_reason.clear();

    if (!water_access_type_registry_load()) {
        log_error("Unable to load WaterAccessType xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load WaterAccessType definitions.",
            water_access_type_registry_get_failure_reason());
    }
    if (!god_registry_load()) {
        log_error("Unable to load God xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load God definitions.", god_registry_get_failure_reason());
    }
    if (!religion_registry_load()) {
        log_error("Unable to load Religion xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load Religion definitions.", religion_registry_get_failure_reason());
    }
    if (!culture_module_registry_load()) {
        log_error("Unable to load CultureModule xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load CultureModule definitions.",
            culture_module_registry_get_failure_reason());
    }
    if (!storage_type_registry_load()) {
        log_error("Unable to load StorageType xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load StorageType definitions.",
            storage_type_registry_get_failure_reason());
    }
    if (!distribution_registry_load()) {
        log_error("Unable to load Distribution xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load Distribution definitions.",
            distribution_registry_get_failure_reason());
    }
    if (!production_method_registry_load()) {
        log_error("Unable to load ProductionMethod xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load ProductionMethod definitions.");
    }
    if (!housing_profile_registry_load()) {
        log_error("Unable to load HousingProfile xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load HousingProfile definitions.");
    }
    if (!foundation_registry_load()) {
        log_error("Unable to load Foundation xml definitions", 0, 0);
        return fail_building_type_registry("Unable to load Foundation definitions.");
    }
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure BuildingType definition layers", failure_reason.c_str(), 0);
        return fail_building_type_registry("Unable to configure BuildingType definition layers.", failure_reason.c_str());
    }
    if (!building_type_registry_load_layers(layers, &failure_reason)) {
        return fail_building_type_registry("Unable to load BuildingType definitions.", failure_reason.c_str());
    }
    return 1;
}

const char *building_type_registry_get_failure_reason(void)
{
    return g_building_type_registry_failure_reason.c_str();
}

int building_type_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    g_building_type_registry_failure_reason.clear();
    StagedBuildingTypeRegistry staged;
    if (!build_layered_building_type_registry(layers, staged, failure_reason) ||
        !resolve_staged_building_type_registry(staged, failure_reason)) {
        if (failure_reason && !failure_reason->empty()) {
            g_building_type_registry_failure_reason = *failure_reason;
        } else {
            g_building_type_registry_failure_reason = "Unable to load BuildingType definitions.";
        }
        return 0;
    }

    clear_xml_runtime_property_fields();
    g_building_types = std::move(staged.definitions);
    g_building_type_overlays = std::move(staged.overlays);
    building_type_startup_bridge_apply_model_overrides();
    god_id_bridge_reset_for_runtime();
    building_type_id_bridge_reset_for_runtime();
    water_access_type_id_bridge_reset_for_runtime();
    building_monument_reset_runtime_bridge();
    building_menu_invalidate_catalog();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int building_type_registry_layers_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_identity,
    building_type_layer_test_result *result,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedBuildingTypeRegistry staged;
    if (!build_layered_building_type_registry(layers, staged, failure_reason) ||
        !validate_final_winner_definitions(staged, failure_reason)) {
        return 0;
    }
    std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> previous = std::move(g_building_types);
    g_building_types = std::move(staged.definitions);
    const int foundations_resolved = resolve_foundation_references();
    staged.definitions = std::move(g_building_types);
    g_building_types = std::move(previous);
    if (!foundations_resolved) {
        if (failure_reason && failure_reason->empty()) {
            *failure_reason = "Unable to resolve BuildingType foundation references.";
        }
        return 0;
    }

    if (result) {
        *result = {};
        result->queried_runtime_type = BUILDING_NONE;
        result->queried_source_layer = -1;
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        const std::string identity = xml_definition::normalize_path(query_identity);
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(identity);
        if (overlay) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
        }
        for (const std::unique_ptr<BuildingType> &definition : staged.definitions) {
            if (!definition || identity != definition->attr()) {
                continue;
            }
            result->queried_runtime_type = definition->type();
            result->queried_cost = definition->model().has_cost() ? definition->model().cost() : 0;
            break;
        }
    }
    return 1;
}
#endif
