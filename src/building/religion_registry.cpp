#

#include "building/religion_registry.h"

#include "building/god_registry.h"
#include "building/religion.h"
#include "core/crash_context.h"
#include "core/xml_definition.h"
#include "core/xml_value.h"
#include "game/mod_manager.h"

#include "core/log.h"
#include "core/xml_parser.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

std::string g_religion_path;

namespace {

struct ParseState {
    std::unique_ptr<Religion> definition;
    int saw_god = 0;
    int saw_tier = 0;
    int saw_capacity = 0;
    int saw_presentation = 0;
    int error = 0;
};

std::unordered_map<std::string, std::unique_ptr<Religion>> g_religions;
ParseState g_parse_state;

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
    if (!g_parse_state.definition) {
        g_parse_state.definition = std::make_unique<Religion>(std::string());
    }
    return 1;
}

int parse_presentation()
{
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
    if (god_path == "all") {
        g_parse_state.definition->set_all_gods();
        g_parse_state.saw_god = 1;
        return 1;
    }
    const God *god = god_path.empty() ? nullptr : find_god_definition(god_path.c_str());
    if (!god) {
        log_error("Unable to resolve Religion god reference", god_path.c_str(), 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition->add_god(god);
    g_parse_state.saw_god = 1;
    return 1;
}

int parse_tier()
{
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

int parse_definition_file(const char *filename, const char *definition_path)
{
    ErrorContextScope error_scope("religion_registry.parse_definition", filename);

    g_parse_state = {};
    g_parse_state.definition = std::make_unique<Religion>(definition_path ? definition_path : "");
    const int parsed = xml_definition::parse_file(
        filename,
        "Religion",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    const ReligionTier tier = g_parse_state.definition ? g_parse_state.definition->tier() : ReligionTier::None;
    const int needs_presentation = tier == ReligionTier::Grand;
    if (!parsed || g_parse_state.error || !g_parse_state.definition ||
        !g_parse_state.saw_god || !g_parse_state.saw_tier || !g_parse_state.saw_capacity ||
        (needs_presentation && !g_parse_state.definition->presentation().is_complete())) {
        log_error("Unable to parse Religion xml", filename, 0);
        error_context_report_error("Unable to parse Religion xml.", filename);
        return 0;
    }

    const std::string key = g_parse_state.definition->path();
    if (g_religions.find(key) != g_religions.end()) {
        log_error("Duplicate Religion definition path", key.c_str(), 0);
        error_context_report_error("Duplicate Religion definition path.", key.c_str());
        return 0;
    }

    g_religions.emplace(key, std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const Religion *find_religion_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_religions.find(normalized);
    return found != g_religions.end() ? found->second.get() : nullptr;
}

} // namespace building_type_registry_impl

const char *religion_registry_get_religion_path(void)
{
    building_type_registry_impl::g_religion_path = mod_manager::mod_path() + "Religions/";
    return building_type_registry_impl::g_religion_path.c_str();
}

int religion_registry_load(void)
{
    using namespace building_type_registry_impl;

    religion_registry_get_religion_path();
    g_religions.clear();

    return xml_definition::for_each_definition_file(
        g_religion_path,
        "Religion",
        true,
        [](const xml_definition::DefinitionFile &file, const std::string &normalized_path) {
            return parse_definition_file(file.full_path.c_str(), normalized_path.c_str());
        });
}
