#include "figure/unit_type.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace unit_type_registry_impl {

std::string g_failure_reason;
std::vector<std::unique_ptr<UnitType>> g_unit_types;

namespace {

struct ParseState {
    std::unique_ptr<UnitType> definition;
    bool saw_figure = false;
    bool saw_combat = false;
    bool saw_ability = false;
    bool error = false;
};

ParseState g_parse_state;

void set_failure_reason(const char *message, const char *detail = nullptr)
{
    g_failure_reason = xml_definition::format_failure_reason(message, detail);
}

int parse_root()
{
    std::string key;
    if (!xml_definition::parse_required_nonempty_string_attribute("key", &key)) {
        log_error("UnitType xml requires a non-empty key", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.definition = std::make_unique<UnitType>(key);
    return 1;
}

int parse_figure()
{
    if (!g_parse_state.definition || !xml_parser_has_attribute("type")) {
        log_error("UnitType figure node is missing required type", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const char *type_text = xml_parser_get_attribute_string("type");
    if (!g_parse_state.definition->set_figure_type_from_key(type_text)) {
        log_error("Unsupported UnitType figure type", type_text, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.saw_figure = true;
    return 1;
}

int parse_combat()
{
    g_parse_state.saw_combat = true;
    return 1;
}

int parse_combat_stat(UnitCombatStat stat)
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    int value = 0;
    if (!xml_definition::parse_required_nonnegative_int_attribute("value", &value)) {
        log_error("UnitType combat value must be non-negative", "value", 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.definition->set_combat_value(stat, value);
    return 1;
}

int parse_health()
{
    return parse_combat_stat(UnitCombatStat::Health);
}

int parse_attack()
{
    return parse_combat_stat(UnitCombatStat::Attack);
}

int parse_defense()
{
    return parse_combat_stat(UnitCombatStat::Defense);
}

int parse_morale()
{
    return parse_combat_stat(UnitCombatStat::Morale);
}

int parse_armor()
{
    return parse_combat_stat(UnitCombatStat::Armor);
}

int parse_movement()
{
    if (!g_parse_state.definition || !xml_parser_has_attribute("pathing")) {
        log_error("UnitType movement node is missing required pathing", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->set_pathing_key(xml_value::trim_copy(xml_parser_get_attribute_string("pathing")));
    return 1;
}

int parse_recruit()
{
    if (!g_parse_state.definition || !xml_parser_has_attribute("type")) {
        log_error("UnitType recruit node is missing required type", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const char *recruit_type = xml_parser_get_attribute_string("type");
    if (!g_parse_state.definition->set_recruit_type_from_key(recruit_type)) {
        log_error("Unsupported UnitType recruit type", recruit_type, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (xml_parser_has_attribute("requires_weapon")) {
        const char *requires_weapon = xml_parser_get_attribute_string("requires_weapon");
        g_parse_state.definition->set_requires_weapon(
            xml_value::equals(requires_weapon, "true") ||
            xml_value::equals(requires_weapon, "1") ||
            xml_value::equals(requires_weapon, "yes"));
    }
    return 1;
}

int parse_melee()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->enable_melee();
    g_parse_state.saw_ability = true;
    return 1;
}

int parse_ranged()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }

    UnitRangedAbility ability;
    if (!xml_definition::parse_required_nonnegative_int_attribute("range", &ability.range) ||
        !xml_definition::parse_required_nonnegative_int_attribute("cooldown", &ability.cooldown) ||
        !xml_definition::parse_required_nonnegative_int_attribute("damage", &ability.damage) ||
        !xml_parser_has_attribute("projectile")) {
        log_error("UnitType ranged ability requires range, cooldown, damage, and projectile", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    ability.projectile = xml_value::trim_copy(xml_parser_get_attribute_string("projectile"));
    if (!g_parse_state.definition->set_ranged_ability(ability)) {
        log_error("UnitType ranged projectile is empty", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.saw_ability = true;
    return 1;
}

int parse_graphics()
{
    if (!g_parse_state.definition || !xml_parser_has_attribute("figure_type")) {
        log_error("UnitType graphics node is missing required figure_type", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->set_graphics_figure_type(
        xml_value::trim_copy(xml_parser_get_attribute_string("figure_type")));
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "unit", parse_root, nullptr, nullptr, nullptr },
    { "figure", parse_figure, nullptr, "unit", nullptr },
    { "combat", parse_combat, nullptr, "unit", nullptr },
    { "health", parse_health, nullptr, "combat", nullptr },
    { "attack", parse_attack, nullptr, "combat", nullptr },
    { "defense", parse_defense, nullptr, "combat", nullptr },
    { "morale", parse_morale, nullptr, "combat", nullptr },
    { "armor", parse_armor, nullptr, "combat", nullptr },
    { "movement", parse_movement, nullptr, "unit", nullptr },
    { "recruit", parse_recruit, nullptr, "unit", nullptr },
    { "abilities", nullptr, nullptr, "unit", nullptr },
    { "melee", parse_melee, nullptr, "abilities", nullptr },
    { "ranged", parse_ranged, nullptr, "abilities", nullptr },
    { "graphics", parse_graphics, nullptr, "unit", nullptr }
};

bool validate_definition(const UnitType &definition, const char *filename)
{
    if (!definition.has_figure_type()) {
        log_error("UnitType is missing required figure type", definition.key(), 0);
        error_context_report_error("UnitType is missing required figure type.", filename);
        return false;
    }
    if (!definition.has_valid_combat_stats()) {
        log_error("UnitType combat stats are invalid", definition.key(), 0);
        error_context_report_error("UnitType combat stats are invalid.", filename);
        return false;
    }
    if (!definition.has_any_ability()) {
        log_error("UnitType requires at least one ability", definition.key(), 0);
        error_context_report_error("UnitType requires at least one ability.", filename);
        return false;
    }
    if (!definition.has_recruit_type()) {
        log_error("UnitType is missing required recruit type", definition.key(), 0);
        error_context_report_error("UnitType is missing required recruit type.", filename);
        return false;
    }
    return true;
}

int parse_definition_file(const char *filename)
{
    ErrorContextScope error_scope("unit_type_registry.parse_definition", filename);

    g_parse_state = {};
    const int parsed = xml_definition::parse_file(
        filename,
        "UnitType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    if (!parsed) {
        log_error("Unable to parse UnitType xml", filename, 0);
        set_failure_reason("Unable to parse UnitType xml.", filename);
        return 0;
    }
    if (g_parse_state.error || !g_parse_state.definition ||
        !g_parse_state.saw_figure || !g_parse_state.saw_combat || !g_parse_state.saw_ability) {
        log_error("Unable to parse UnitType xml", filename, 0);
        set_failure_reason("Unable to parse UnitType xml.", filename);
        return 0;
    }
    if (!validate_definition(*g_parse_state.definition, filename)) {
        set_failure_reason("Invalid UnitType definition.", filename);
        return 0;
    }
    if (find_unit_type(g_parse_state.definition->key())) {
        log_error("Duplicate UnitType key", g_parse_state.definition->key(), 0);
        set_failure_reason("Duplicate UnitType key.", g_parse_state.definition->key());
        return 0;
    }

    g_unit_types.push_back(std::move(g_parse_state.definition));
    return 1;
}

} // namespace

const UnitType *find_unit_type(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    for (const std::unique_ptr<UnitType> &definition : g_unit_types) {
        if (definition && definition->matches_key(key)) {
            return definition.get();
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<UnitType>> &unit_types()
{
    return g_unit_types;
}

} // namespace unit_type_registry_impl

UnitType::UnitType(std::string key)
    : key_(std::move(key))
{
}

const char *UnitType::key() const
{
    return key_.c_str();
}

bool UnitType::matches_key(const char *key) const
{
    return xml_value::equals(key_.c_str(), key);
}

bool UnitType::set_figure_type_from_key(const char *key)
{
    const figure_type type = figure_type_from_xml_name(key);
    if (type == FIGURE_NONE) {
        return false;
    }
    figure_type_ = type;
    return true;
}

figure_type UnitType::figure_type_id() const
{
    return figure_type_;
}

bool UnitType::has_figure_type() const
{
    return figure_type_ != FIGURE_NONE;
}

bool UnitType::has_valid_combat_stats() const
{
    return combat_stats_.health > 0 &&
        combat_stats_.attack >= 0 &&
        combat_stats_.defense >= 0 &&
        combat_stats_.morale >= 0 &&
        combat_stats_.armor >= 0;
}

void UnitType::set_combat_value(UnitCombatStat stat, int value)
{
    switch (stat) {
        case UnitCombatStat::Health:
            combat_stats_.health = value;
            break;
        case UnitCombatStat::Attack:
            combat_stats_.attack = value;
            break;
        case UnitCombatStat::Defense:
            combat_stats_.defense = value;
            break;
        case UnitCombatStat::Morale:
            combat_stats_.morale = value;
            break;
        case UnitCombatStat::Armor:
            combat_stats_.armor = value;
            break;
    }
}

void UnitType::set_pathing_key(std::string key)
{
    pathing_key_ = std::move(key);
}

const char *UnitType::pathing_key() const
{
    return pathing_key_.c_str();
}

void UnitType::set_graphics_figure_type(std::string figure_type)
{
    graphics_figure_type_ = std::move(figure_type);
}

const char *UnitType::graphics_figure_type() const
{
    return graphics_figure_type_.c_str();
}

bool UnitType::set_recruit_type_from_key(const char *key)
{
    const int recruit_type = recruit_type_from_key(key);
    if (recruit_type == LEGION_RECRUIT_NONE) {
        return false;
    }
    recruit_type_ = recruit_type;
    return true;
}

int UnitType::recruit_type() const
{
    return recruit_type_;
}

bool UnitType::has_recruit_type() const
{
    return recruit_type_ != LEGION_RECRUIT_NONE;
}

void UnitType::set_requires_weapon(bool value)
{
    requires_weapon_ = value;
}

bool UnitType::requires_weapon() const
{
    return requires_weapon_;
}

void UnitType::enable_melee()
{
    has_melee_ = true;
}

bool UnitType::has_any_ability() const
{
    return has_melee_ || has_ranged_;
}

bool UnitType::set_ranged_ability(const UnitRangedAbility &ability)
{
    if (ability.projectile.empty()) {
        return false;
    }
    ranged_ability_ = ability;
    has_ranged_ = true;
    return true;
}

int UnitType::recruit_type_from_key(const char *key)
{
    if (xml_value::equals(key, "mounted")) {
        return LEGION_RECRUIT_MOUNTED;
    }
    if (xml_value::equals(key, "javelin")) {
        return LEGION_RECRUIT_JAVELIN;
    }
    if (xml_value::equals(key, "legionary")) {
        return LEGION_RECRUIT_LEGIONARY;
    }
    if (xml_value::equals(key, "infantry")) {
        return LEGION_RECRUIT_INFANTRY;
    }
    if (xml_value::equals(key, "archer")) {
        return LEGION_RECRUIT_ARCHER;
    }
    return LEGION_RECRUIT_NONE;
}

const char *unit_type_registry_get_failure_reason(void)
{
    return unit_type_registry_impl::g_failure_reason.c_str();
}

void unit_type_registry_reset(void)
{
    unit_type_registry_impl::g_unit_types.clear();
    unit_type_registry_impl::g_failure_reason.clear();
}

int unit_type_registry_load(void)
{
    using namespace unit_type_registry_impl;

    unit_type_registry_reset();
    const std::string path = mod_manager::mod_path() + "UnitType/";
    bool found_any = false;
    if (!xml_definition::for_each_definition_file(
        path,
        "UnitType",
        true,
        [](const xml_definition::DefinitionFile &file, const std::string &) {
            if (parse_definition_file(file.full_path.c_str())) {
                return true;
            }
            log_error("Unable to parse UnitType xml", file.full_path.c_str(), 0);
            return false;
        },
        &found_any)) {
        if (!found_any) {
            set_failure_reason("No UnitType xml files found in", path.c_str());
        }
        return 0;
    }
    return 1;
}
