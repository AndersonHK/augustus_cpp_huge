#include "building/building_type_registry_internal.h"
#include "building/building_type_id_bridge.h"
#include "building/building_type_legacy_migration.h"
#include "building/production_method_registry.h"
#include "building/storage_type_registry.h"
#include "assets/image_group_payload.h"
#include "core/crash_context.h"
#include "core/xml_value.h"

extern "C" {
#include "building/building_runtime_api.h"
#include "building/menu.h"
#include "building/properties.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "figure/action.h"
#include "game/resource.h"
#include "scenario/property.h"
#include "sound/city.h"
}

#include <cstdio>
#include <utility>

namespace building_type_registry_impl {

static building_type g_next_dynamic_building_type = BUILDING_DYNAMIC_TYPE_FIRST;

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
    if (building_type_legacy_migration_text_id_is_xml_owned(type_attr)) {
        uint16_t legacy_type = building_type_legacy_migration_enum_for_text_id(type_attr);
        if (legacy_type == BUILDING_LEGACY_SLOT_THEATER || legacy_type == BUILDING_LEGACY_SLOT_WELL) {
            return BUILDING_NONE;
        }
        return static_cast<building_type>(legacy_type);
    }

    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_properties *properties = building_properties_for_type(type);
        if (!properties->event_data.attr) {
            continue;
        }
        if (xml_parser_compare_multiple(properties->event_data.attr, type_attr)) {
            return type;
        }
    }
    return BUILDING_NONE;
}

static building_type allocate_dynamic_building_type(const char *type_attr)
{
    if (!type_attr || !*type_attr) {
        return BUILDING_NONE;
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (definition && compare_text(definition->attr(), type_attr) == 0) {
            return BUILDING_NONE;
        }
    }

    while (g_next_dynamic_building_type < BUILDING_TYPE_MAX && g_building_types[g_next_dynamic_building_type]) {
        g_next_dynamic_building_type = static_cast<building_type>(g_next_dynamic_building_type + 1);
    }
    if (g_next_dynamic_building_type >= BUILDING_TYPE_MAX) {
        return BUILDING_NONE;
    }

    building_type allocated_type = g_next_dynamic_building_type;
    g_next_dynamic_building_type = static_cast<building_type>(g_next_dynamic_building_type + 1);
    return allocated_type;
}


static figure_type parse_figure_type_name(const char *name)
{
    struct named_figure {
        const char *name;
        figure_type type;
    };
    static const named_figure figure_names[] = {
        { "actor", FIGURE_ACTOR },
        { "barber", FIGURE_BARBER },
        { "bathhouse_worker", FIGURE_BATHHOUSE_WORKER },
        { "doctor", FIGURE_DOCTOR },
        { "engineer", FIGURE_ENGINEER },
        { "gladiator", FIGURE_GLADIATOR },
        { "librarian", FIGURE_LIBRARIAN },
        { "lion_tamer", FIGURE_LION_TAMER },
        { "prefect", FIGURE_PREFECT },
        { "priest", FIGURE_PRIEST },
        { "school_child", FIGURE_SCHOOL_CHILD },
        { "surgeon", FIGURE_SURGEON },
        { "teacher", FIGURE_TEACHER },
        { "tax_collector", FIGURE_TAX_COLLECTOR },
        { "work_camp_architect", FIGURE_WORK_CAMP_ARCHITECT },
        { "work_camp_worker", FIGURE_WORK_CAMP_WORKER }
    };

    for (size_t i = 0; i < sizeof(figure_names) / sizeof(figure_names[0]); i++) {
        if (name && compare_text(name, figure_names[i].name) == 0) {
            return figure_names[i].type;
        }
    }
    return FIGURE_NONE;
}

static int parse_action_state_name(const char *name)
{
    struct named_action_state {
        const char *name;
        int action_state;
    };
    static const named_action_state action_names[] = {
        { "roaming", FIGURE_ACTION_125_ROAMING },
        { "engineer_created", FIGURE_ACTION_60_ENGINEER_CREATED },
        { "prefect_created", FIGURE_ACTION_70_PREFECT_CREATED },
        { "tax_collector_created", FIGURE_ACTION_40_TAX_COLLECTOR_CREATED },
        { "entertainer_roaming", FIGURE_ACTION_94_ENTERTAINER_ROAMING },
        { "entertainer_school_created", FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED },
        { "work_camp_worker_created", FIGURE_ACTION_203_WORK_CAMP_WORKER_CREATED },
        { "work_camp_architect_created", FIGURE_ACTION_206_WORK_CAMP_ARCHITECT_CREATED }
    };

    for (size_t i = 0; i < sizeof(action_names) / sizeof(action_names[0]); i++) {
        if (name && compare_text(name, action_names[i].name) == 0) {
            return action_names[i].action_state;
        }
    }
    return 0;
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
            figure_type type = parse_figure_type_name(token.c_str());
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
    if (value && compare_text(value, "none") == 0) {
        return LaborSeekerMethod::None;
    }
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

static WaterAccessType parse_provider_water_access_type(const char *value)
{
    if (value && compare_text(value, "well") == 0) {
        return WaterAccessType::Well;
    }
    if (value && compare_text(value, "fountain") == 0) {
        return WaterAccessType::Fountain;
    }
    if (value && compare_text(value, "reservoir") == 0) {
        return WaterAccessType::Reservoir;
    }
    return WaterAccessType::None;
}

static WaterAccessRequirement parse_provider_water_access_requirement(const char *value)
{
    if (!value || compare_text(value, "none") == 0) {
        return WaterAccessRequirement::None;
    }
    if (compare_text(value, "reservoir_network") == 0) {
        return WaterAccessRequirement::ReservoirNetwork;
    }
    if (compare_text(value, "water_source_any") == 0) {
        return WaterAccessRequirement::WaterSourceAny;
    }
    if (compare_text(value, "water_source_fresh_only") == 0) {
        return WaterAccessRequirement::WaterSourceFreshOnly;
    }
    return WaterAccessRequirement::None;
}

static WaterAccessNodeKind parse_provider_water_access_node_kind(const char *value)
{
    if (value && compare_text(value, "aqueduct_connection") == 0) {
        return WaterAccessNodeKind::AqueductConnection;
    }
    return WaterAccessNodeKind::None;
}

static int parse_spawn_direction(const char *value)
{
    if (value && compare_text(value, "bottom") == 0) {
        return DIR_4_BOTTOM;
    }
    return DIR_0_TOP;
}

static int equals_ignore_case_ascii(char left, char right)
{
    if (left >= 'A' && left <= 'Z') {
        left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = static_cast<char>(right - 'A' + 'a');
    }
    return left == right;
}

static int starts_with_ignore_case_ascii(const std::string &value, const char *prefix)
{
    if (!prefix) {
        return 0;
    }

    size_t prefix_length = strlen(prefix);
    if (value.size() < prefix_length) {
        return 0;
    }

    for (size_t i = 0; i < prefix_length; i++) {
        if (!equals_ignore_case_ascii(value[i], prefix[i])) {
            return 0;
        }
    }
    return 1;
}

static int ends_with_ignore_case_ascii(const std::string &value, const char *suffix)
{
    if (!suffix) {
        return 0;
    }

    size_t suffix_length = strlen(suffix);
    if (value.size() < suffix_length) {
        return 0;
    }

    size_t start = value.size() - suffix_length;
    for (size_t i = 0; i < suffix_length; i++) {
        if (!equals_ignore_case_ascii(value[start + i], suffix[i])) {
            return 0;
        }
    }
    return 1;
}

static std::string normalize_graphics_path(const char *value)
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

    if (starts_with_ignore_case_ascii(normalized, "graphics\\") ||
        starts_with_ignore_case_ascii(normalized, "\\") ||
        ends_with_ignore_case_ascii(normalized, ".xml") ||
        normalized.back() == '\\') {
        return std::string();
    }

    return normalized;
}

static std::string normalize_runtime_definition_path(const char *value)
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

static int parse_graphics_comparison(const char *comparison_text, GraphicComparison *out_comparison)
{
    if (!out_comparison) {
        return 0;
    }
    if (comparison_text && compare_text(comparison_text, "lt") == 0) {
        *out_comparison = GraphicComparison::LessThan;
        return 1;
    }
    if (comparison_text && (compare_text(comparison_text, "lte") == 0 || compare_text(comparison_text, "let") == 0)) {
        *out_comparison = GraphicComparison::LessThanOrEqual;
        return 1;
    }
    if (comparison_text && compare_text(comparison_text, "eq") == 0) {
        *out_comparison = GraphicComparison::Equal;
        return 1;
    }
    if (comparison_text && compare_text(comparison_text, "gt") == 0) {
        *out_comparison = GraphicComparison::GreaterThan;
        return 1;
    }
    if (comparison_text && compare_text(comparison_text, "gte") == 0) {
        *out_comparison = GraphicComparison::GreaterThanOrEqual;
        return 1;
    }
    return 0;
}

static int parse_state()
{
    if (!g_parse_state.definition) {
        log_error("Encountered state definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_state) {
        log_error("BuildingType xml contains duplicate state nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_state = 1;
    g_parse_state.parsing_state = 1;
    return 1;
}

static void finish_state()
{
    g_parse_state.parsing_state = 0;
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
    g_parse_state.saw_provider_water_access_type = 0;
    g_parse_state.saw_provider_water_access_range = 0;
    g_parse_state.saw_provider_water_access_requirement = 0;
    return 1;
}

static void finish_provider_water_access()
{
    if (!g_parse_state.parsing_provider_water_access) {
        return;
    }

    if (!g_parse_state.saw_provider_water_access_type || !g_parse_state.saw_provider_water_access_range) {
        log_error("BuildingType provider water_access is missing required child nodes", g_parse_state.definition ? g_parse_state.definition->attr() : 0, 0);
        g_parse_state.error = 1;
    }
    g_parse_state.parsing_provider_water_access = 0;
}

static int parse_building_root()
{
    if (!xml_parser_has_attribute("type")) {
        log_error("BuildingType xml is missing required attribute 'type'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_attr = xml_parser_get_attribute_string("type");
    building_type type = find_building_type_by_attr(type_attr);
    if (type == BUILDING_NONE) {
        type = allocate_dynamic_building_type(type_attr);
        if (type == BUILDING_NONE) {
            log_error("Unable to allocate dynamic BuildingType xml type", type_attr, 0);
            g_parse_state.error = 1;
            return 0;
        }
    }
    if (g_building_types[type]) {
        log_error("Duplicate BuildingType xml type", type_attr, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition = std::make_unique<BuildingType>(type, type_attr);
    return 1;
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

    if (!parse_optional_int_attribute("Unsupported BuildingType model numeric attribute", "size", &value, &has_value)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_value) {
        if (value <= 0) {
            log_error("Unsupported BuildingType model size", g_parse_state.definition->attr(), value);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_model_size(value);
        any_value = 1;
    }

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
    if (!xml_parser_has_attribute("policy")) {
        log_error("BuildingType foundation is missing required attribute 'policy'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string policy = xml_value::trim_copy(xml_parser_get_attribute_string("policy"));
    if (policy.empty()) {
        log_error("Unsupported BuildingType foundation policy", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_foundation_policy(std::move(policy));
    g_parse_state.saw_foundation = 1;
    return 1;
}

static int parse_button()
{
    if (!g_parse_state.definition) {
        log_error("Encountered button definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_button) {
        log_error("BuildingType xml contains duplicate button/menu nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    int any_value = 0;
    if (xml_parser_has_attribute("group")) {
        std::string group = xml_value::trim_copy(xml_parser_get_attribute_string("group"));
        if (group.empty()) {
            log_error("Unsupported BuildingType button group", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_button_group(std::move(group));
        any_value = 1;
    }

    int order = 0;
    int has_order = 0;
    if (!parse_optional_int_attribute("Unsupported BuildingType button numeric attribute", "order", &order, &has_order)) {
        g_parse_state.error = 1;
        return 0;
    }
    if (has_order) {
        g_parse_state.definition->set_button_order(order);
        any_value = 1;
    }

    if (xml_parser_has_attribute("icon")) {
        std::string icon = xml_value::trim_copy(xml_parser_get_attribute_string("icon"));
        if (icon.empty()) {
            log_error("Unsupported BuildingType button icon", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_button_icon(std::move(icon));
        any_value = 1;
    }

    if (xml_parser_has_attribute("text_key")) {
        std::string text_key = xml_value::trim_copy(xml_parser_get_attribute_string("text_key"));
        if (text_key.empty()) {
            log_error("Unsupported BuildingType button text_key", g_parse_state.definition->attr(), 0);
            g_parse_state.error = 1;
            return 0;
        }
        g_parse_state.definition->set_button_text_key(std::move(text_key));
        any_value = 1;
    }

    if (!any_value) {
        log_error("BuildingType button is missing supported attributes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.saw_button = 1;
    return 1;
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
        {"clinic", SOUND_CITY_CLINIC},
        {"colosseum", SOUND_CITY_COLOSSEUM},
        {"concrete_maker", SOUND_CITY_CONCRETE_MAKER},
        {"engineers_post", SOUND_CITY_ENGINEERS_POST},
        {"forum", SOUND_CITY_FORUM},
        {"fountain", SOUND_CITY_FOUNTAIN},
        {"fruit_farm", SOUND_CITY_FRUIT_FARM},
        {"furniture_workshop", SOUND_CITY_FURNITURE_WORKSHOP},
        {"gladiator_school", SOUND_CITY_GLADIATOR_SCHOOL},
        {"hospital", SOUND_CITY_HOSPITAL},
        {"hippodrome", SOUND_CITY_HIPPODROME},
        {"library", SOUND_CITY_LIBRARY},
        {"lighthouse", SOUND_CITY_LIGHTHOUSE},
        {"lion_pit", SOUND_CITY_LION_PIT},
        {"market", SOUND_CITY_MARKET},
        {"native_decoration", SOUND_CITY_NATIVE_DECORATION},
        {"native_hut", SOUND_CITY_NATIVE_HUT},
        {"oil_workshop", SOUND_CITY_OIL_WORKSHOP},
        {"olive_farm", SOUND_CITY_OLIVE_FARM},
        {"oracle", SOUND_CITY_ORACLE},
        {"palace", SOUND_CITY_PALACE},
        {"pig_farm", SOUND_CITY_PIG_FARM},
        {"pottery_workshop", SOUND_CITY_POTTERY_WORKSHOP},
        {"prefecture", SOUND_CITY_PREFECTURE},
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
        {"vegetable_farm", SOUND_CITY_VEGETABLE_FARM},
        {"vine_farm", SOUND_CITY_VINE_FARM},
        {"watchtower", SOUND_CITY_WATCHTOWER},
        {"weapons_workshop", SOUND_CITY_WEAPONS_WORKSHOP},
        {"well", SOUND_CITY_WELL},
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

static int parse_event_data()
{
    if (!g_parse_state.definition) {
        log_error("Encountered event_data definition before building root", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_event_data) {
        log_error("BuildingType xml contains duplicate event_data nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("attr")) {
        log_error("BuildingType event_data is missing required attribute 'attr'", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    std::string attr = xml_value::trim_copy(xml_parser_get_attribute_string("attr"));
    if (attr.empty()) {
        log_error("Unsupported BuildingType event_data attr", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_event_attr(std::move(attr));
    g_parse_state.saw_event_data = 1;
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

static resource_type parse_construction_requirement_type_name(const char *name);

static void finish_graphics()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }

    if (g_parse_state.current_graphics_target_scope == GraphicsParseTargetScope::ConstructionPhase) {
        ConstructionPhase *phase = g_parse_state.definition->last_construction_phase();
        if (!phase || !phase->graphics.has_path()) {
            log_error("BuildingType construction phase graphics is missing required child node 'path'", 0, 0);
            g_parse_state.error = 1;
        }
    } else if (!g_parse_state.definition->graphics().has_default_node()) {
        log_error("BuildingType graphics is missing required child node 'default'", 0, 0);
        g_parse_state.error = 1;
    } else if (!g_parse_state.definition->graphics().default_target().has_path()) {
        log_error("BuildingType graphics is missing required child node 'path'", 0, 0);
        g_parse_state.error = 1;
    }

    g_parse_state.parsing_graphics = 0;
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

    g_parse_state.saw_construction = 1;
    g_parse_state.parsing_construction = 1;
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
    ConstructionPhase *phase = g_parse_state.definition ? g_parse_state.definition->last_construction_phase() : nullptr;
    if (!g_parse_state.saw_construction_phase_graphics || !phase || !phase->graphics.has_path()) {
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
    resource_type resource = parse_construction_requirement_type_name(type_text);
    if (resource == RESOURCE_NONE && compare_text(type_text, "architects") != 0) {
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
    if (g_parse_state.definition->graphics().has_default_node()) {
        log_error("BuildingType graphics contains duplicate default nodes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->mark_graphics_default_node();
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::Default;
    return 1;
}

static void finish_graphics_default()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }
    if (!g_parse_state.definition->graphics().default_target().has_path()) {
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
    g_parse_state.definition->add_graphics_variant();
    g_parse_state.has_current_graphics_variant = 1;
    g_parse_state.current_graphics_variant_index = g_parse_state.definition->graphics().variants().size() - 1;
    g_parse_state.current_graphics_target_scope = GraphicsParseTargetScope::Variant;
    return 1;
}

static void finish_graphics_variant()
{
    if (!g_parse_state.parsing_graphics) {
        return;
    }

    GraphicsVariant *variant = g_parse_state.definition->last_graphics_variant();
    if (!variant || !variant->target.has_path()) {
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
            g_parse_state.definition->default_graphics_target().set_path(std::move(normalized_path));
            break;
        case GraphicsParseTargetScope::Variant:
        {
            GraphicsVariant *variant = g_parse_state.definition->last_graphics_variant();
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
            ConstructionPhase *phase = g_parse_state.definition->last_construction_phase();
            if (!phase) {
                log_error("Encountered construction phase graphics path without an active phase", 0, 0);
                g_parse_state.error = 1;
                return 0;
            }
            phase->graphics.set_path(std::move(normalized_path));
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
            g_parse_state.definition->default_graphics_target().set_image(std::move(image_id));
            break;
        case GraphicsParseTargetScope::Variant:
        {
            GraphicsVariant *variant = g_parse_state.definition->last_graphics_variant();
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
            ConstructionPhase *phase = g_parse_state.definition->last_construction_phase();
            if (!phase) {
                log_error("Encountered construction phase graphics image without an active phase", 0, 0);
                g_parse_state.error = 1;
                return 0;
            }
            phase->graphics.set_image(std::move(image_id));
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

static resource_type parse_resource_type_name(const char *name)
{
    if (!name || !*name) {
        return RESOURCE_NONE;
    }

    const std::string normalized_name = xml_value::trim_copy(name);
    if (normalized_name.empty()) {
        return RESOURCE_NONE;
    }

    for (resource_type type = RESOURCE_MIN; type < RESOURCE_MAX; type = static_cast<resource_type>(type + 1)) {
        resource_data *data = resource_get_data(type);
        if (!data || !data->xml_attr_name) {
            continue;
        }
        if (xml_parser_compare_multiple(data->xml_attr_name, normalized_name.c_str())) {
            return type;
        }
    }
    return RESOURCE_NONE;
}

static resource_type parse_construction_requirement_type_name(const char *name)
{
    if (name && compare_text(name, "architects") == 0) {
        return RESOURCE_NONE;
    }
    return parse_resource_type_name(name);
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
    if (!g_parse_state.definition || !g_parse_state.parsing_graphics || !g_parse_state.has_current_graphics_variant) {
        log_error("Encountered graphics condition outside graphics variant", 0, 0);
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
        condition.resource = parse_resource_type_name(resource_text);
        if (condition.resource == RESOURCE_NONE) {
            log_error("Unsupported BuildingType graphics resource_positive resource", resource_text, 0);
            g_parse_state.error = 1;
            return 0;
        }
        condition.type = GraphicsConditionType::ResourcePositive;
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

    g_parse_state.definition->add_graphics_variant_condition(condition);
    return 1;
}

static int parse_state_water_access()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_state) {
        log_error("Encountered water_access outside state node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("mode")) {
        log_error("BuildingType state water_access is missing required attribute 'mode'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *mode_text = xml_parser_get_attribute_string("mode");
    if (!mode_text || compare_text(mode_text, "reservoir_range") != 0) {
        log_error("Unsupported BuildingType water_access mode", mode_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_state_water_access_mode(WaterAccessMode::ReservoirRange);
    return 1;
}

static int parse_provider_water_access_type_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered provider water_access type outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_provider_water_access_type) {
        log_error("BuildingType provider water_access contains duplicate type nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType provider water_access type is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *type_text = xml_parser_get_attribute_string("value");
    WaterAccessType type = parse_provider_water_access_type(type_text);
    if (type == WaterAccessType::None) {
        log_error("Unsupported BuildingType provider water_access type", type_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_water_access_type(type);
    g_parse_state.saw_provider_water_access_type = 1;
    return 1;
}

static int parse_provider_water_access_range_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered provider water_access range outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_provider_water_access_range) {
        log_error("BuildingType provider water_access contains duplicate range nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType provider water_access range is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    int range = xml_parser_get_attribute_int("value");
    if (range < 0) {
        log_error("Unsupported BuildingType provider water_access range", xml_parser_get_attribute_string("value"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_water_access_range(range);
    g_parse_state.saw_provider_water_access_range = 1;
    return 1;
}

static int parse_provider_water_access_requirement_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered provider water_access requirement outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (g_parse_state.saw_provider_water_access_requirement) {
        log_error("BuildingType provider water_access contains duplicate requirement nodes", g_parse_state.definition->attr(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("value")) {
        log_error("BuildingType provider water_access requirement is missing required attribute 'value'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *requirement_text = xml_parser_get_attribute_string("value");
    WaterAccessRequirement requirement = parse_provider_water_access_requirement(requirement_text);
    if (requirement == WaterAccessRequirement::None && compare_text(requirement_text, "none") != 0) {
        log_error("Unsupported BuildingType provider water_access requirement", requirement_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->set_water_access_requirement(requirement);
    g_parse_state.saw_provider_water_access_requirement = 1;
    return 1;
}

static int parse_provider_water_access_node()
{
    if (!g_parse_state.definition || !g_parse_state.parsing_provider_water_access) {
        log_error("Encountered provider water_access node outside water_access node", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("kind") || !xml_parser_has_attribute("x") || !xml_parser_has_attribute("y")) {
        log_error("BuildingType provider water_access node is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *kind_text = xml_parser_get_attribute_string("kind");
    WaterAccessNodeKind kind = parse_provider_water_access_node_kind(kind_text);
    if (kind == WaterAccessNodeKind::None) {
        log_error("Unsupported BuildingType provider water_access node kind", kind_text, 0);
        g_parse_state.error = 1;
        return 0;
    }

    WaterAccessNode node;
    node.kind = kind;
    node.x = xml_parser_get_attribute_int("x");
    node.y = xml_parser_get_attribute_int("y");
    g_parse_state.definition->add_water_access_node(node);
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

    if (!g_parse_state.saw_labor_seeker_amount) {
        if (g_parse_state.definition->labor().has_employee_count()) {
            g_parse_state.current_labor_seeker_policy.amount = g_parse_state.definition->labor().employee_count();
            g_parse_state.saw_labor_seeker_amount = 1;
        } else {
            log_error("BuildingType labor_seeker amount is missing and labor has no employees count", 0, 0);
            g_parse_state.error = 1;
        }
    }

    if (!g_parse_state.saw_labor_seeker_method ||
        !g_parse_state.saw_labor_seeker_amount) {
        log_error("BuildingType labor_seeker is missing required child nodes", 0, 0);
        g_parse_state.error = 1;
    } else {
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
    if (g_parse_state.current_labor_seeker_policy.method == LaborSeekerMethod::None &&
        (!method_text || compare_text(method_text, "none") != 0)) {
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

    std::string normalized_path = normalize_runtime_definition_path(xml_parser_get_attribute_string("path"));
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

    std::string normalized_path = normalize_runtime_definition_path(xml_parser_get_attribute_string("path"));
    if (normalized_path.empty()) {
        log_error("Unsupported BuildingType production_method reference path", xml_parser_get_attribute_string("path"), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_production_method_reference(std::move(normalized_path));
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

    if (!xml_parser_has_attribute("mode")) {
        log_error("BuildingType spawn is missing required attribute 'mode'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *mode_text = xml_parser_get_attribute_string("mode");
    SpawnPolicy policy;
    if (mode_text && compare_text(mode_text, "service_roamer") == 0) {
        policy.mode = SpawnMode::ServiceRoamer;
    } else if (mode_text && compare_text(mode_text, "temple_supplier") == 0) {
        policy.mode = SpawnMode::TempleSupplier;
    } else if (mode_text && compare_text(mode_text, "temple_destination_priest") == 0) {
        policy.mode = SpawnMode::TempleDestinationPriest;
    } else if (mode_text && compare_text(mode_text, "temple_mars_mess_hall_priest") == 0) {
        policy.mode = SpawnMode::TempleMarsMessHallPriest;
    } else if (mode_text && compare_text(mode_text, "temple_neptune_chariot") == 0) {
        policy.mode = SpawnMode::TempleNeptuneChariot;
    } else if (mode_text && compare_text(mode_text, "grand_temple_mars_recruit") == 0) {
        policy.mode = SpawnMode::GrandTempleMarsRecruit;
    } else {
        log_error("Unsupported BuildingType spawn mode", mode_text, 0);
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

    if (policy.mode == SpawnMode::ServiceRoamer && !xml_parser_has_attribute("spawn_figure")) {
        log_error("BuildingType spawn is missing required attribute 'spawn_figure'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("spawn_figure")) {
        policy.spawn_figure = parse_figure_type_name(xml_parser_get_attribute_string("spawn_figure"));
        if (policy.spawn_figure == FIGURE_NONE) {
            log_error("Unsupported BuildingType spawn spawn_figure", xml_parser_get_attribute_string("spawn_figure"), 0);
            g_parse_state.error = 1;
            return 0;
        }
    }

    const int has_profile_attribute = xml_parser_has_attribute("profile");

    if (policy.mode == SpawnMode::ServiceRoamer && !has_profile_attribute && !xml_parser_has_attribute("action_state")) {
        log_error("BuildingType spawn is missing required attribute 'action_state'", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("action_state")) {
        policy.action_state = parse_action_state_name(xml_parser_get_attribute_string("action_state"));
        if (!policy.action_state) {
            log_error("Unsupported BuildingType spawn action_state", xml_parser_get_attribute_string("action_state"), 0);
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

    if (xml_parser_has_attribute("init_roaming")) {
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("init_roaming"), &policy.init_roaming)) {
            log_error("Unsupported BuildingType spawn init_roaming", xml_parser_get_attribute_string("init_roaming"), 0);
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

static const xml_parser_element XML_ELEMENTS[] = {
    { "building", parse_building_root, nullptr, nullptr, nullptr },
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
    { "sound", parse_sound, nullptr, "building", nullptr },
    { "event_data", parse_event_data, nullptr, "building", nullptr },
    { "flags", parse_flags, nullptr, "building", nullptr },
    { "state", parse_state, finish_state, "building", nullptr },
    { "water_access", parse_provider_water_access, finish_provider_water_access, "building", nullptr },
    { "graphics", parse_graphics, finish_graphics, "building|phase", nullptr },
    { "construction", parse_construction, finish_construction, "building", nullptr },
    { "phase", parse_construction_phase, finish_construction_phase, "construction", nullptr },
    { "type", parse_provider_water_access_type_node, nullptr, "water_access", nullptr },
    { "range", parse_provider_water_access_range_node, nullptr, "water_access", nullptr },
    { "requirement", parse_provider_water_access_requirement_node, nullptr, "water_access", nullptr },
    { "requirement", parse_construction_requirement, nullptr, "phase|construction", nullptr },
    { "node", parse_provider_water_access_node, nullptr, "water_access", nullptr },
    { "default", parse_graphics_default, finish_graphics_default, "graphics", nullptr },
    { "variant", parse_graphics_variant, finish_graphics_variant, "graphics", nullptr },
    { "path", parse_graphics_path, nullptr, "default|variant|graphics", nullptr },
    { "image", parse_graphics_image, nullptr, "default|variant|graphics", nullptr },
    { "condition", parse_graphics_condition, nullptr, "variant", nullptr },
    { "water_access", parse_state_water_access, nullptr, "state", nullptr },
    { "labor", parse_labor, finish_labor, "building", nullptr },
    { "employees", parse_labor_employees, nullptr, "labor", nullptr },
    { "labor_seeker", parse_labor_seeker, finish_labor_seeker, "labor", nullptr },
    { "method", parse_labor_seeker_method_node, nullptr, "labor_seeker", nullptr },
    { "amount", parse_labor_seeker_amount_node, nullptr, "labor_seeker", nullptr },
    { "storages", parse_storages, finish_storages, "building", nullptr },
    { "storage", parse_storage_reference, nullptr, "storages", nullptr },
    { "production_methods", parse_production_methods, finish_production_methods, "building", nullptr },
    { "production_method", parse_production_method_reference, nullptr, "production_methods", nullptr },
    { "spawn_group", parse_spawn_group, nullptr, "building", nullptr },
    { "spawn", parse_spawn, nullptr, "spawn_group", nullptr }
};

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open BuildingType xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek BuildingType xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size BuildingType xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read BuildingType xml", filename, 0);
        return 0;
    }
    return 1;
}

// Input: one authored graphics target from a BuildingType plus a short scope label for logging.
// Output: true when the referenced runtime group/image namespace exists up front, false when the BuildingType should lose native graphics.
static int validate_graphics_target_entry(
    const BuildingType &definition,
    const GraphicsTarget &target,
    const char *target_scope)
{
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

    return 1;
}

// Input: one parsed BuildingType definition that may contain native runtime graphics targets.
// Output: true when all required targets resolve; invalid top-level graphics are cleared so runtime rendering falls back safely.
static int validate_runtime_graphics_or_clear(BuildingType &definition)
{
    int valid = 1;
    if (definition.has_graphic()) {
        valid = validate_graphics_target_entry(definition, definition.graphics().default_target(), "default");

        if (valid) {
            const std::vector<GraphicsVariant> &variants = definition.graphics().variants();
            for (size_t i = 0; i < variants.size(); i++) {
                char scope[64];
                snprintf(scope, sizeof(scope), "variant[%u]", static_cast<unsigned int>(i));
                if (!validate_graphics_target_entry(definition, variants[i].target, scope)) {
                    valid = 0;
                    break;
                }
            }
        }

        if (!valid) {
            definition.clear_graphics();
        }
    }

    if (definition.has_phased_construction()) {
        const std::vector<ConstructionPhase> &phases = definition.construction().phases();
        for (const ConstructionPhase &phase : phases) {
            char scope[64];
            snprintf(scope, sizeof(scope), "construction.phase[%d]", phase.index);
            if (!validate_graphics_target_entry(definition, phase.graphics, scope)) {
                return 0;
            }
        }
    }

    return 1;
}

static int resolve_runtime_references(BuildingType &definition, const char *filename)
{
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
        const ProductionMethod *production_method = find_production_method_definition(production_path.c_str());
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

    return 1;
}

static int parse_definition_file(const char *filename)
{
    ErrorContextScope error_scope("building_type_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize BuildingType xml parser", filename, 0);
        return 0;
    }

    int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    // TEMPORARY: metadata-only BuildingType XML is accepted while build authority moves out of legacy code.
    // This must tighten again once metadata, graphics, placement, and runtime behavior are all XML-owned.
    // Live bad XML should fail at load time instead of quietly registering incomplete building definitions.
    int has_supported_node = g_parse_state.saw_identity || g_parse_state.saw_model || g_parse_state.saw_foundation ||
        g_parse_state.saw_button || g_parse_state.saw_sound || g_parse_state.saw_event_data ||
        g_parse_state.saw_flags || g_parse_state.saw_desirability || g_parse_state.saw_graphic ||
        g_parse_state.saw_construction || g_parse_state.saw_spawn ||
        g_parse_state.saw_storages || g_parse_state.saw_production_methods ||
        g_parse_state.saw_labor || g_parse_state.saw_state || g_parse_state.saw_provider_water_access;
    if (!parsed || g_parse_state.error || !g_parse_state.definition ||
        !has_supported_node) {
        if (!has_supported_node) {
            log_error("BuildingType xml is missing a supported node", filename, 0);
        }
        return 0;
    }

    if (!validate_runtime_graphics_or_clear(*g_parse_state.definition)) {
        return 0;
    }
    if (!resolve_runtime_references(*g_parse_state.definition, filename)) {
        return 0;
    }
    g_building_types[g_parse_state.definition->type()] = std::move(g_parse_state.definition);
    return 1;
}

}

extern "C" int building_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    refresh_building_type_path();

    clear_xml_runtime_property_fields();
    for (std::unique_ptr<BuildingType> &definition : g_building_types) {
        definition.reset();
    }
    g_next_dynamic_building_type = BUILDING_DYNAMIC_TYPE_FIRST;

    if (!storage_type_registry_load()) {
        log_error("Unable to load StorageType xml definitions", 0, 0);
        return 0;
    }
    if (!production_method_registry_load()) {
        log_error("Unable to load ProductionMethod xml definitions", 0, 0);
        return 0;
    }

    const dir_listing *files = dir_find_files_with_extension(g_building_type_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No BuildingType xml files found in", g_building_type_path.c_str(), 0);
        return 0;
    }

    // Load every live BuildingType definition from the selected mod folder.
    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_building_type_path.c_str(), files->files[i].name);
        if (!parse_definition_file(full_path)) {
            log_error("Unable to parse BuildingType xml", full_path, 0);
            return 0;
        }
    }

    building_type_registry_apply_model_overrides();
    refresh_known_building_type_ids();
    building_type_id_bridge_reset_for_runtime();
    building_runtime_reset();
    building_menu_invalidate_catalog();
    return 1;
}
