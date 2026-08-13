#include "figure/unit_type.h"
#include "figure/figure_type_registry.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace unit_type_registry_impl {

std::string g_failure_reason;
std::vector<std::unique_ptr<UnitType>> g_unit_types;
std::array<const UnitType *, FIGURE_TYPE_MAX> g_unit_types_by_figure = {};
mod_definition::DefinitionOverlayTracker g_unit_type_overlays;

namespace {

struct ParseState {
    std::unique_ptr<UnitType> definition;
    std::string figure_reference;
    bool disabled = false;
    bool saw_root = false;
    bool saw_figure = false;
    bool saw_combat = false;
    bool saw_ability = false;
    bool error = false;
};

struct StagedUnitType {
    std::unique_ptr<UnitType> definition;
    std::string figure_reference;
    mod_definition::DefinitionSource source;
    bool disabled = false;
};

struct StagedUnitTypes {
    std::map<std::string, StagedUnitType> winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

ParseState g_parse_state;

int parse_enabled_content(const char *element)
{
    if (!g_parse_state.saw_root || g_parse_state.disabled) {
        log_error("Disabled UnitType definition must contain only its root identity", element, 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
}

int parse_root()
{
    if (g_parse_state.saw_root) {
        log_error("UnitType contains duplicate root nodes", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    std::string key;
    if (!xml_definition::parse_required_nonempty_string_attribute("key", &key)) {
        log_error("UnitType xml requires a non-empty key", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("UnitType has invalid Boolean attribute 'disabled'", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.saw_root = true;
    g_parse_state.disabled = disabled != 0;
    g_parse_state.definition = std::make_unique<UnitType>(key);
    return 1;
}

int parse_figure()
{
    if (!parse_enabled_content("figure") || !g_parse_state.definition || !xml_parser_has_attribute("type")) {
        log_error("UnitType figure node is missing required type", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.figure_reference = xml_value::trim_copy(xml_parser_get_attribute_string("type"));
    if (g_parse_state.figure_reference.empty()) {
        log_error("UnitType figure type is empty", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.saw_figure = true;
    return 1;
}

int parse_combat()
{
    if (!parse_enabled_content("combat")) {
        return 0;
    }
    g_parse_state.saw_combat = true;
    return 1;
}

int parse_combat_stat(UnitCombatStat stat)
{
    if (!parse_enabled_content("combat stat") || !g_parse_state.definition) {
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

set_difficulty difficulty_from_key(const char *key)
{
    if (xml_value::equals(key, "very_easy")) {
        return DIFFICULTY_VERY_EASY;
    }
    if (xml_value::equals(key, "easy")) {
        return DIFFICULTY_EASY;
    }
    if (xml_value::equals(key, "normal")) {
        return DIFFICULTY_NORMAL;
    }
    if (xml_value::equals(key, "hard")) {
        return DIFFICULTY_HARD;
    }
    if (xml_value::equals(key, "very_hard")) {
        return DIFFICULTY_VERY_HARD;
    }
    return static_cast<set_difficulty>(-1);
}

int parse_difficulty_attack()
{
    if (!parse_enabled_content("difficulty attack") || !g_parse_state.definition ||
        !xml_parser_has_attribute("level")) {
        log_error("UnitType difficulty attack requires level", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    int value = 0;
    const set_difficulty difficulty = difficulty_from_key(xml_parser_get_attribute_string("level"));
    if (!xml_definition::parse_required_nonnegative_int_attribute("value", &value) ||
        static_cast<int>(difficulty) < DIFFICULTY_VERY_EASY ||
        static_cast<int>(difficulty) > DIFFICULTY_VERY_HARD ||
        !g_parse_state.definition->set_attack_for_difficulty(difficulty, value)) {
        log_error("UnitType difficulty attack is invalid or duplicated", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
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
    if (!parse_enabled_content("movement") || !g_parse_state.definition || !xml_parser_has_attribute("pathing")) {
        log_error("UnitType movement node is missing required pathing", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->set_pathing_key(xml_value::trim_copy(xml_parser_get_attribute_string("pathing")));
    return 1;
}

int parse_recruit()
{
    if (!parse_enabled_content("recruit") || !g_parse_state.definition || !xml_parser_has_attribute("type")) {
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
    if (!parse_enabled_content("melee") || !g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    UnitMeleeAbility ability;
    if (xml_parser_has_attribute("engages_adjacent")) {
        int value = 0;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("engages_adjacent"), &value)) {
            log_error("UnitType melee ability has invalid engages_adjacent", 0, 0);
            g_parse_state.error = true;
            return 0;
        }
        ability.engages_adjacent = value != 0;
    }
    if (!xml_definition::parse_optional_nonnegative_int_attribute(
            "exposed_back_bonus", &ability.exposed_back_bonus) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "halted_attack_bonus", &ability.halted_attack_bonus) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "charge_attack_bonus", &ability.charge_attack_bonus) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "exposed_defense_penalty", &ability.exposed_defense_penalty) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "column_defense_bonus", &ability.column_defense_bonus) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "double_line_defense_bonus", &ability.double_line_defense_bonus) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "single_line_missile_defense_bonus", &ability.single_line_missile_defense_bonus) ||
        !xml_definition::parse_optional_nonnegative_int_attribute(
            "column_missile_damage", &ability.column_missile_damage)) {
        log_error("UnitType melee ability has an invalid modifier", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->set_melee_ability(ability);
    g_parse_state.saw_ability = true;
    return 1;
}

int parse_ranged()
{
    if (!parse_enabled_content("ranged") || !g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }

    UnitRangedAbility ability;
    if (!xml_definition::parse_required_nonnegative_int_attribute("range", &ability.range) ||
        !xml_definition::parse_required_nonnegative_int_attribute("cooldown", &ability.cooldown) ||
        !xml_definition::parse_required_nonnegative_int_attribute("damage", &ability.damage) ||
        !xml_definition::parse_required_nonnegative_int_attribute("launch_frame", &ability.launch_frame) ||
        !xml_parser_has_attribute("projectile")) {
        log_error("UnitType ranged ability requires range, cooldown, damage, launch_frame, and projectile", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    const std::string projectile = xml_value::trim_copy(xml_parser_get_attribute_string("projectile"));
    if (xml_parser_has_attribute("requires_double_line")) {
        int value = 0;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("requires_double_line"), &value)) {
            log_error("UnitType ranged ability has invalid requires_double_line", 0, 0);
            g_parse_state.error = true;
            return 0;
        }
        ability.requires_double_line = value != 0;
    }
    if (!g_parse_state.definition->set_ranged_ability(ability, projectile.c_str())) {
        log_error("UnitType ranged ability has invalid projectile or non-positive parameters", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.saw_ability = true;
    return 1;
}

int enemy_type_from_key(const char *key)
{
    static constexpr const char *KEYS[ENEMY_MAX] = {
        "barbarian", "numidian", "gaul", "celt", "goth", "pergamum",
        "seleucid", "etruscan", "greek", "egyptian", "carthaginian", "caesar"
    };
    for (int type = 0; type < ENEMY_MAX; ++type) {
        if (xml_value::equals(key, KEYS[type])) {
            return type;
        }
    }
    return ENEMY_UNDEFINED;
}

int parse_ranged_projectile()
{
    if (!parse_enabled_content("ranged projectile") || !g_parse_state.definition ||
        !xml_parser_has_attribute("enemy") || !xml_parser_has_attribute("type")) {
        log_error("UnitType ranged projectile override requires enemy and type", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    int damage = 0;
    if (!xml_definition::parse_required_positive_int_attribute("damage", &damage)) {
        log_error("UnitType ranged projectile override requires positive damage", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    const int enemy_type = enemy_type_from_key(xml_parser_get_attribute_string("enemy"));
    if (enemy_type == ENEMY_UNDEFINED ||
        !g_parse_state.definition->set_ranged_projectile_for_enemy(
            enemy_type, xml_parser_get_attribute_string("type"), damage)) {
        log_error("UnitType ranged projectile override is invalid", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
}

int parse_graphics()
{
    if (!parse_enabled_content("graphics") || !g_parse_state.definition || !xml_parser_has_attribute("figure_type")) {
        log_error("UnitType graphics node is missing required figure_type", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.definition->set_graphics_figure_type(
        xml_value::trim_copy(xml_parser_get_attribute_string("figure_type")));
    return 1;
}

int parse_abilities()
{
    return parse_enabled_content("abilities");
}

const xml_parser_element XML_ELEMENTS[] = {
    { "unit", parse_root, nullptr, nullptr, nullptr },
    { "figure", parse_figure, nullptr, "unit", nullptr },
    { "combat", parse_combat, nullptr, "unit", nullptr },
    { "health", parse_health, nullptr, "combat", nullptr },
    { "attack", parse_attack, nullptr, "combat", nullptr },
    { "difficulty", parse_difficulty_attack, nullptr, "attack", nullptr },
    { "defense", parse_defense, nullptr, "combat", nullptr },
    { "morale", parse_morale, nullptr, "combat", nullptr },
    { "armor", parse_armor, nullptr, "combat", nullptr },
    { "movement", parse_movement, nullptr, "unit", nullptr },
    { "recruit", parse_recruit, nullptr, "unit", nullptr },
    { "abilities", parse_abilities, nullptr, "unit", nullptr },
    { "melee", parse_melee, nullptr, "abilities", nullptr },
    { "ranged", parse_ranged, nullptr, "abilities", nullptr },
    { "projectile", parse_ranged_projectile, nullptr, "ranged", nullptr },
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
    return true;
}

int parse_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    StagedUnitType *out_definition)
{
    ErrorContextScope error_scope("unit_type_registry.parse_definition", filename);

    g_parse_state = {};
    const int parsed = xml_definition::parse_buffer(
        filename,
        "UnitType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || !g_parse_state.definition ||
        (!g_parse_state.disabled &&
            (!g_parse_state.saw_figure || !g_parse_state.saw_combat || !g_parse_state.saw_ability))) {
        log_error("Unable to parse UnitType xml", filename, 0);
        return 0;
    }

    if (out_definition) {
        out_definition->definition = std::move(g_parse_state.definition);
        out_definition->figure_reference = std::move(g_parse_state.figure_reference);
        out_definition->disabled = g_parse_state.disabled;
    }
    return 1;
}

int parse_definition_file(const mod_definition::DefinitionSource &source, StagedUnitType *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "UnitType")) {
        return 0;
    }
    return parse_definition_buffer(source.full_path.c_str(), buffer, out_definition);
}

int stage_definition(StagedUnitTypes &staged, StagedUnitType definition)
{
    const std::string key = definition.definition ? definition.definition->key() : "";
    if (key.empty()) {
        staged.failure_reason = "UnitType key must not be empty.";
        return 0;
    }
    if (!staged.overlays.apply(key, definition.disabled, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }
    staged.winners.insert_or_assign(key, std::move(definition));
    return 1;
}

int resolve_and_validate_winners(StagedUnitTypes &staged)
{
    std::map<figure_type, std::string> figure_owners;
    for (auto &entry : staged.winners) {
        StagedUnitType &winner = entry.second;
        if (winner.disabled) {
            continue;
        }
        if (!winner.definition->set_figure_type_from_key(winner.figure_reference.c_str()) ||
            figure_type_definition_is_suppressed(winner.figure_reference.c_str())) {
            staged.failure_reason = "UnitType '" + entry.first + "' from " + winner.source.full_path +
                " references unsupported or suppressed FigureType '" + winner.figure_reference + "'.";
            return 0;
        }
        if (!validate_definition(*winner.definition, winner.source.full_path.c_str())) {
            staged.failure_reason = "Invalid UnitType definition: " + winner.source.full_path;
            return 0;
        }
        const auto inserted = figure_owners.emplace(winner.definition->figure_type_id(), entry.first);
        if (!inserted.second) {
            staged.failure_reason = "UnitType '" + entry.first + "' from " + winner.source.full_path +
                " duplicates figure identity already owned by UnitType '" + inserted.first->second + "'.";
            return 0;
        }
    }
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

const UnitType *find_unit_type(figure_type type)
{
    return type > FIGURE_NONE && type < FIGURE_TYPE_MAX ?
        g_unit_types_by_figure[static_cast<size_t>(type)] : nullptr;
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

const UnitCombatStats &UnitType::combat_stats() const
{
    return combat_stats_;
}

int UnitCombatStats::attack_for_difficulty(set_difficulty difficulty) const
{
    const int index = static_cast<int>(difficulty);
    if (index >= DIFFICULTY_VERY_EASY && index <= DIFFICULTY_VERY_HARD &&
        difficulty_attack[static_cast<size_t>(index)] >= 0) {
        return difficulty_attack[static_cast<size_t>(index)];
    }
    return attack;
}

bool UnitType::has_morale() const
{
    return has_morale_;
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
            has_morale_ = true;
            break;
        case UnitCombatStat::Armor:
            combat_stats_.armor = value;
            break;
    }
}

bool UnitType::set_attack_for_difficulty(set_difficulty difficulty, int value)
{
    const int index = static_cast<int>(difficulty);
    if (index < DIFFICULTY_VERY_EASY || index > DIFFICULTY_VERY_HARD || value < 0 ||
        combat_stats_.difficulty_attack[static_cast<size_t>(index)] >= 0) {
        return false;
    }
    combat_stats_.difficulty_attack[static_cast<size_t>(index)] = value;
    return true;
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

void UnitType::set_requires_weapon(bool value)
{
    requires_weapon_ = value;
}

bool UnitType::requires_weapon() const
{
    return requires_weapon_;
}

void UnitType::set_melee_ability(const UnitMeleeAbility &ability)
{
    melee_ability_ = ability;
    has_melee_ = true;
}

const UnitMeleeAbility *UnitType::melee_ability() const
{
    return has_melee_ ? &melee_ability_ : nullptr;
}

bool UnitType::has_any_ability() const
{
    return has_melee_ || has_ranged_;
}

static figure_type projectile_type_from_key(const char *key)
{
    if (xml_value::equals(key, "arrow")) {
        return FIGURE_ARROW;
    }
    if (xml_value::equals(key, "spear")) {
        return FIGURE_SPEAR;
    }
    if (xml_value::equals(key, "bolt")) {
        return FIGURE_BOLT;
    }
    if (xml_value::equals(key, "javelin")) {
        return FIGURE_JAVELIN;
    }
    if (xml_value::equals(key, "friendly_arrow")) {
        return FIGURE_FRIENDLY_ARROW;
    }
    return FIGURE_NONE;
}

bool UnitType::set_ranged_ability(const UnitRangedAbility &ability, const char *projectile_key)
{
    const figure_type projectile_type = projectile_type_from_key(projectile_key);
    if (projectile_type == FIGURE_NONE || ability.range <= 0 || ability.damage <= 0 ||
        ability.launch_frame <= 0) {
        return false;
    }
    ranged_ability_ = ability;
    ranged_ability_.projectile_type = projectile_type;
    has_ranged_ = true;
    return true;
}

bool UnitType::set_ranged_projectile_for_enemy(
    int enemy_type, const char *projectile_key, int damage)
{
    if (!has_ranged_ || enemy_type < 0 || enemy_type >= ENEMY_MAX || damage <= 0) {
        return false;
    }
    const figure_type projectile_type = projectile_type_from_key(projectile_key);
    if (projectile_type == FIGURE_NONE ||
        ranged_ability_.enemy_projectiles[static_cast<size_t>(enemy_type)].type != FIGURE_NONE) {
        return false;
    }
    ranged_ability_.enemy_projectiles[static_cast<size_t>(enemy_type)] = {projectile_type, damage};
    return true;
}

UnitRangedAbility::Projectile UnitRangedAbility::projectile_for_enemy(int enemy_type) const
{
    if (enemy_type >= 0 && enemy_type < ENEMY_MAX) {
        const Projectile &override = enemy_projectiles[static_cast<size_t>(enemy_type)];
        if (override.type != FIGURE_NONE) {
            return override;
        }
    }
    return {projectile_type, damage};
}

const UnitRangedAbility *UnitType::ranged_ability() const
{
    return has_ranged_ ? &ranged_ability_ : nullptr;
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

const char *unit_type_definition_source_path(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        unit_type_registry_impl::g_unit_type_overlays.find(key);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int unit_type_definition_is_suppressed(const char *key)
{
    if (!key || !*key) {
        return 0;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        unit_type_registry_impl::g_unit_type_overlays.find(key);
    return entry && entry->disabled;
}

void unit_type_registry_reset(void)
{
    unit_type_registry_impl::g_unit_types.clear();
    unit_type_registry_impl::g_unit_types_by_figure.fill(nullptr);
    unit_type_registry_impl::g_unit_type_overlays.clear();
    unit_type_registry_impl::g_failure_reason.clear();
}

int unit_type_registry_load(void)
{
    using namespace unit_type_registry_impl;

    StagedUnitTypes staged;
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"UnitType"},
            "UnitType",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedUnitType definition;
                definition.source = source;
                if (!parse_definition_file(source, &definition)) {
                    staged.failure_reason = "Unable to parse UnitType xml: " + source.full_path;
                    return false;
                }
                return stage_definition(staged, std::move(definition)) != 0;
            },
            nullptr,
            &enumeration_failure)) {
        g_failure_reason = staged.failure_reason.empty() ? enumeration_failure : staged.failure_reason;
        return 0;
    }
    if (!resolve_and_validate_winners(staged)) {
        g_failure_reason = staged.failure_reason;
        return 0;
    }

    std::vector<std::unique_ptr<UnitType>> published;
    published.reserve(staged.overlays.active_count());
    for (auto &entry : staged.winners) {
        if (!entry.second.disabled) {
            published.push_back(std::move(entry.second.definition));
        }
    }
    g_unit_types = std::move(published);
    g_unit_types_by_figure.fill(nullptr);
    for (const std::unique_ptr<UnitType> &definition : g_unit_types) {
        g_unit_types_by_figure[static_cast<size_t>(definition->figure_type_id())] = definition.get();
    }
    g_unit_type_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int unit_type_layered_definition_buffers_are_valid_for_test(
    const unit_type_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    unit_type_layer_test_result *result)
{
    using namespace unit_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedUnitTypes staged;
    for (int index = 0; index < input_count; ++index) {
        const unit_type_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedUnitType definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.full_path = input.source_path ? input.source_path : "UnitTypeLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.category = "UnitType";
        if (!parse_definition_buffer(definition.source.full_path.c_str(), buffer, &definition) ||
            !stage_definition(staged, std::move(definition))) {
            return 0;
        }
    }
    if (!resolve_and_validate_winners(staged)) {
        return 0;
    }

    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        result->queried_source_layer = -1;
        const std::string key = query_key ? query_key : "";
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(key);
        const auto winner = staged.winners.find(key);
        if (overlay && winner != staged.winners.end()) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
            if (!winner->second.disabled && winner->second.definition) {
                const UnitType &definition = *winner->second.definition;
                const UnitCombatStats &stats = definition.combat_stats();
                const UnitMeleeAbility *melee = definition.melee_ability();
                const UnitRangedAbility *ranged = definition.ranged_ability();
                result->queried_recruit_type = definition.recruit_type();
                result->queried_requires_weapon = definition.requires_weapon() ? 1 : 0;
                result->queried_health = stats.health;
                result->queried_attack = stats.attack;
                result->queried_defense = stats.defense;
                result->queried_morale = stats.morale;
                result->queried_has_morale = definition.has_morale() ? 1 : 0;
                result->queried_armor = stats.armor;
                result->queried_has_melee = melee ? 1 : 0;
                result->queried_engages_adjacent = melee && melee->engages_adjacent ? 1 : 0;
                result->queried_has_ranged = ranged ? 1 : 0;
                if (ranged) {
                    result->queried_range = ranged->range;
                    result->queried_cooldown = ranged->cooldown;
                    result->queried_damage = ranged->damage;
                    result->queried_launch_frame = ranged->launch_frame;
                    result->queried_requires_double_line = ranged->requires_double_line ? 1 : 0;
                    result->queried_projectile_type = static_cast<int>(ranged->projectile_type);
                }
            }
        }
    }
    return 1;
}
#endif
