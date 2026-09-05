#include "figure/formation_type.h"

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

namespace formation_type_registry_impl {

std::string g_failure_reason;
std::vector<std::unique_ptr<FormationType>> g_formation_types;
mod_definition::DefinitionOverlayTracker g_formation_type_overlays;
std::array<const FormationType *, FIGURE_TYPE_MAX> g_spawn_formations = {};
std::array<std::vector<const FormationType *>, 3> g_herd_spawns_by_climate;

namespace {

struct SlotSpec {
    int x = 0;
    int y = 0;
    std::string unit_key;
};

struct FormationTypeSpec {
    std::string key;
    int width = 0;
    int height = 0;
    int recruit_capacity = 0;
    FormationStationAlignment station_alignment = FormationStationAlignment::Unspecified;
    FormationCombatModifiers combat_modifiers;
    std::vector<SlotSpec> slots;
    FormationSpawn spawn;
    bool disabled = false;
};

struct StagedFormationType : FormationTypeSpec {
    std::unique_ptr<FormationType> definition;
    mod_definition::DefinitionSource source;
};

struct ParseState : FormationTypeSpec {
    bool saw_root = false;
    bool saw_grid = false;
    bool saw_combat = false;
    bool saw_slot = false;
    bool saw_herd_behavior = false;
    bool saw_herd_member = false;
    bool error = false;
};

struct StagedFormationTypes {
    std::map<std::string, StagedFormationType> winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

ParseState g_parse_state;

int parse_enabled_content(const char *element)
{
    if (!g_parse_state.saw_root || g_parse_state.disabled) {
        log_error("Disabled FormationType definition must contain only its root identity", element, 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
}

bool parse_row_range(const char *text, int *first_row, int *last_row)
{
    if (!text || !first_row || !last_row) {
        return false;
    }

    const std::string value = xml_value::trim_copy(text);
    const size_t separator = value.find("..");
    if (separator == std::string::npos) {
        int row = 0;
        if (!xml_value::parse_int_strict(value, &row) || row < 0) {
            return false;
        }
        *first_row = row;
        *last_row = row;
        return true;
    }

    int begin = 0;
    int end = 0;
    if (!xml_value::parse_int_strict(value.substr(0, separator), &begin) ||
        !xml_value::parse_int_strict(value.substr(separator + 2), &end) ||
        begin < 0 || end < begin) {
        return false;
    }
    *first_row = begin;
    *last_row = end;
    return true;
}

bool parse_combat_modifier(const char *attribute, int *out_value, bool *saw_attribute)
{
    if (!xml_parser_has_attribute(attribute)) {
        return true;
    }

    int value = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string(attribute), &value) ||
        value < -100 || value > 100) {
        log_error("FormationType combat modifier must be an integer from -100 through 100", attribute, 0);
        return false;
    }
    *out_value = value;
    *saw_attribute = true;
    return true;
}

bool parse_combat_policy(const char *attribute, int minimum, int *out_value, bool *saw_attribute)
{
    if (!xml_parser_has_attribute(attribute)) return true;
    int value = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string(attribute), &value) ||
        value < minimum || value > 100) {
        log_error("FormationType strategic combat value is outside its supported range", attribute, value);
        return false;
    }
    *out_value = value;
    *saw_attribute = true;
    return true;
}

int parse_root()
{
    if (g_parse_state.saw_root) {
        log_error("FormationType contains duplicate root nodes", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    std::string key;
    if (!xml_definition::parse_required_nonempty_string_attribute("key", &key)) {
        log_error("FormationType xml requires a non-empty key", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("FormationType has invalid Boolean attribute 'disabled'", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    int recruit_capacity = 0;
    if (xml_parser_has_attribute("recruit_capacity") &&
        !xml_definition::parse_required_positive_int_attribute("recruit_capacity", &recruit_capacity)) {
        log_error("FormationType has invalid positive recruit_capacity", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    if (disabled && recruit_capacity) {
        log_error("Disabled FormationType cannot declare recruit_capacity", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.key = std::move(key);
    g_parse_state.recruit_capacity = recruit_capacity;
    g_parse_state.disabled = disabled != 0;
    g_parse_state.saw_root = true;
    return 1;
}

int parse_grid()
{
    if (!parse_enabled_content("grid")) {
        return 0;
    }

    int width = 0;
    int height = 0;
    if (!xml_definition::parse_required_positive_int_attribute("width", &width) ||
        !xml_definition::parse_required_positive_int_attribute("height", &height)) {
        log_error("FormationType grid requires positive width and height", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    const char *alignment_value = xml_parser_get_attribute_string("station_alignment");
    FormationStationAlignment station_alignment;
    if (xml_value::equals(alignment_value, "scaled_tile_anchor")) {
        station_alignment = FormationStationAlignment::ScaledTileAnchor;
    } else if (xml_value::equals(alignment_value, "tile_anchor")) {
        station_alignment = FormationStationAlignment::TileAnchor;
    } else {
        log_error("FormationType grid requires station_alignment 'scaled_tile_anchor' or 'tile_anchor'", alignment_value, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.width = width;
    g_parse_state.height = height;
    g_parse_state.station_alignment = station_alignment;
    g_parse_state.saw_grid = true;
    return 1;
}

int parse_combat()
{
    if (!parse_enabled_content("combat")) {
        return 0;
    }
    if (g_parse_state.saw_combat) {
        log_error("FormationType contains duplicate combat nodes", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }

    bool saw_attribute = false;
    FormationCombatModifiers modifiers;
    if (!parse_combat_modifier("melee_attack_modifier", &modifiers.melee_attack, &saw_attribute) ||
        !parse_combat_modifier("melee_defense_modifier", &modifiers.melee_defense, &saw_attribute) ||
        !parse_combat_modifier("morale_modifier", &modifiers.morale, &saw_attribute) ||
        !parse_combat_modifier("missile_damage_modifier", &modifiers.missile_damage, &saw_attribute) ||
        !parse_combat_policy("distant_battle_strength", 1, &modifiers.distant_battle_strength, &saw_attribute) ||
        !parse_combat_policy("trained_distant_battle_bonus", 0, &modifiers.trained_distant_battle_bonus, &saw_attribute) ||
        !parse_combat_policy("curse_weight_per_figure", 1, &modifiers.curse_weight_per_figure, &saw_attribute) ||
        !saw_attribute) {
        if (!saw_attribute) {
            log_error("FormationType combat node requires at least one modifier", g_parse_state.key.c_str(), 0);
        }
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.combat_modifiers = modifiers;
    g_parse_state.saw_combat = true;
    return 1;
}

int parse_slot()
{
    if (!parse_enabled_content("slot") || !g_parse_state.saw_grid) {
        log_error("FormationType slot requires a preceding grid", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (!xml_parser_has_attribute("unit")) {
        log_error("FormationType slot is missing required unit", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const std::string unit_key = xml_value::trim_copy(xml_parser_get_attribute_string("unit"));
    if (unit_key.empty()) {
        log_error("FormationType slot unit is empty", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (xml_parser_has_attribute("fill") &&
        xml_value::equals(xml_parser_get_attribute_string("fill"), "all")) {
        for (int y = 0; y < g_parse_state.height; ++y) {
            for (int x = 0; x < g_parse_state.width; ++x) {
                g_parse_state.slots.push_back({x, y, unit_key});
            }
        }
        g_parse_state.saw_slot = true;
        return 1;
    }

    if (xml_parser_has_attribute("row") || xml_parser_has_attribute("rows")) {
        int first_row = 0;
        int last_row = 0;
        const char *row_text = xml_parser_has_attribute("rows") ?
            xml_parser_get_attribute_string("rows") :
            xml_parser_get_attribute_string("row");
        if (!parse_row_range(row_text, &first_row, &last_row) ||
            first_row < 0 || last_row < first_row || last_row >= g_parse_state.height) {
            log_error("FormationType slot row range is invalid", row_text, 0);
            g_parse_state.error = true;
            return 0;
        }
        for (int y = first_row; y <= last_row; ++y) {
            for (int x = 0; x < g_parse_state.width; ++x) {
                g_parse_state.slots.push_back({x, y, unit_key});
            }
        }
        g_parse_state.saw_slot = true;
        return 1;
    }

    int x = 0;
    int y = 0;
    if (!xml_definition::parse_required_nonnegative_int_attribute("x", &x) ||
        !xml_definition::parse_required_nonnegative_int_attribute("y", &y) ||
        x >= g_parse_state.width || y >= g_parse_state.height) {
        log_error("FormationType explicit slot requires valid x, y, and unit", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.slots.push_back({x, y, unit_key});
    g_parse_state.saw_slot = true;
    return 1;
}

int parse_spawn()
{
    if (!parse_enabled_content("spawn") || g_parse_state.spawn.role != FormationSpawnRole::None) {
        log_error("FormationType contains an invalid or duplicate spawn node", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    const char *role = xml_parser_get_attribute_string("role");
    if (xml_value::equals(role, "enemy")) {
        if (xml_parser_has_attribute("climate") || xml_parser_has_attribute("count")) {
            log_error("Enemy FormationType spawn cannot declare herd population data", g_parse_state.key.c_str(), 0);
            g_parse_state.error = true;
            return 0;
        }
        g_parse_state.spawn.role = FormationSpawnRole::Enemy;
        return 1;
    }
    if (!xml_value::equals(role, "herd")) {
        log_error("FormationType spawn role must be 'enemy' or 'herd'", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    const char *climate = xml_parser_get_attribute_string("climate");
    if (xml_value::equals(climate, "central")) g_parse_state.spawn.climate = 0;
    else if (xml_value::equals(climate, "northern")) g_parse_state.spawn.climate = 1;
    else if (xml_value::equals(climate, "desert")) g_parse_state.spawn.climate = 2;
    else {
        log_error("Herd FormationType spawn requires central, northern, or desert climate", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    if (!xml_definition::parse_required_positive_int_attribute("count", &g_parse_state.spawn.initial_count)) {
        log_error("Herd FormationType spawn requires a positive count", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.spawn.role = FormationSpawnRole::Herd;
    return 1;
}

int parse_herd_behavior()
{
    if (!parse_enabled_content("herd") || g_parse_state.saw_herd_behavior) {
        log_error("FormationType contains invalid or duplicate herd behavior", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.saw_herd_behavior = true;
    int aggressive = 0;
    int allow_negative_desirability = 0;
    const char *invalid_attribute = nullptr;
    if (!xml_definition::parse_required_positive_int_attribute("roam_distance", &g_parse_state.spawn.herd.roam_distance)) invalid_attribute = "roam_distance";
    else if (!xml_definition::parse_required_positive_int_attribute("roam_delay", &g_parse_state.spawn.herd.roam_delay)) invalid_attribute = "roam_delay";
    else if (!xml_definition::parse_required_nonnegative_int_attribute("reproduction_delay", &g_parse_state.spawn.herd.reproduction_delay)) invalid_attribute = "reproduction_delay";
    else if (!xml_value::parse_bool(xml_parser_get_attribute_string("aggressive"), &aggressive)) invalid_attribute = "aggressive";
    else if (!xml_value::parse_bool(xml_parser_get_attribute_string("allow_negative_desirability"), &allow_negative_desirability)) invalid_attribute = "allow_negative_desirability";
    if (invalid_attribute) {
        log_error("Herd FormationType spawn requires a valid behavior attribute", invalid_attribute, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.spawn.herd.aggressive = aggressive != 0;
    g_parse_state.spawn.herd.allow_negative_desirability = allow_negative_desirability != 0;
    const char *movement_sound = xml_parser_get_attribute_string("movement_sound");
    if (xml_value::equals(movement_sound, "none")) {
        g_parse_state.spawn.herd.movement_sound = FormationHerdMovementSound::None;
    } else if (xml_value::equals(movement_sound, "wolf_howl")) {
        g_parse_state.spawn.herd.movement_sound = FormationHerdMovementSound::WolfHowl;
    } else {
        log_error("Herd FormationType movement_sound must be none or wolf_howl", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
}

int parse_herd_member()
{
    if (!parse_enabled_content("herd_member") || g_parse_state.saw_herd_member) {
        log_error("FormationType contains invalid or duplicate herd member behavior", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.saw_herd_member = true;
    int alternate_rest_animation = 0;
    const char *invalid_attribute = nullptr;
    if (!xml_definition::parse_required_positive_int_attribute("rest_delay", &g_parse_state.spawn.herd.member_rest_delay)) invalid_attribute = "rest_delay";
    else if (!xml_definition::parse_required_positive_int_attribute("move_speed", &g_parse_state.spawn.herd.member_move_speed)) invalid_attribute = "move_speed";
    else if (!xml_definition::parse_required_positive_int_attribute("animation_frames", &g_parse_state.spawn.herd.member_animation_frames)) invalid_attribute = "animation_frames";
    else if (!xml_value::parse_bool(xml_parser_get_attribute_string("alternate_rest_animation"), &alternate_rest_animation)) invalid_attribute = "alternate_rest_animation";
    if (invalid_attribute) {
        log_error("Herd FormationType member requires a valid behavior attribute", invalid_attribute, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.spawn.herd.alternate_rest_animation = alternate_rest_animation != 0;
    const char *combat_animation = xml_parser_get_attribute_string("combat_animation");
    if (xml_value::equals(combat_animation, "move")) {
        g_parse_state.spawn.herd.combat_animation = FormationHerdCombatAnimation::Move;
    } else if (xml_value::equals(combat_animation, "attack")) {
        g_parse_state.spawn.herd.combat_animation = FormationHerdCombatAnimation::Attack;
    } else if (xml_value::equals(combat_animation, "rest")) {
        g_parse_state.spawn.herd.combat_animation = FormationHerdCombatAnimation::Rest;
    } else {
        log_error("Herd FormationType combat_animation must be move, attack, or rest", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "formation", parse_root, nullptr, nullptr, nullptr },
    { "grid", parse_grid, nullptr, "formation", nullptr },
    { "combat", parse_combat, nullptr, "formation", nullptr },
    { "spawn", parse_spawn, nullptr, "formation", nullptr },
    { "herd", parse_herd_behavior, nullptr, "formation", nullptr },
    { "herd_member", parse_herd_member, nullptr, "formation", nullptr },
    { "slot", parse_slot, nullptr, "formation", nullptr }
};

bool validate_definition(const FormationType &definition, const char *filename)
{
    if (!definition.has_grid() || definition.capacity() <= 0) {
        log_error("FormationType is missing a valid grid", definition.key(), 0);
        error_context_report_error("FormationType is missing a valid grid.", filename);
        return false;
    }
    if (definition.recruit_capacity() <= 0 ||
        definition.recruit_capacity() > definition.capacity()) {
        log_error("FormationType recruit_capacity exceeds its formation capacity", definition.key(), 0);
        error_context_report_error("FormationType has invalid recruit_capacity.", filename);
        return false;
    }
    if (!definition.has_slots()) {
        log_error("FormationType requires at least one declared slot", definition.key(), 0);
        error_context_report_error("FormationType requires declared slots.", filename);
        return false;
    }

    if (!definition.has_valid_slots()) {
        log_error("FormationType contains invalid slot", definition.key(), 0);
        error_context_report_error("FormationType contains invalid slot.", filename);
        return false;
    }
    if (definition.has_duplicate_slots()) {
        log_error("FormationType contains duplicate slot coordinates", definition.key(), 0);
        error_context_report_error("FormationType contains duplicate slot coordinates.", filename);
        return false;
    }
    return true;
}

int parse_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    StagedFormationType *out_definition)
{
    ErrorContextScope error_scope("formation_type_registry.parse_definition", filename);

    g_parse_state = {};
    const int parsed = xml_definition::parse_buffer(
        filename,
        "FormationType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    const bool has_complete_herd_data = g_parse_state.saw_herd_behavior && g_parse_state.saw_herd_member;
    const bool herd_schema_matches_role = g_parse_state.spawn.role == FormationSpawnRole::Herd ?
        has_complete_herd_data : !g_parse_state.saw_herd_behavior && !g_parse_state.saw_herd_member;
    if (!herd_schema_matches_role) {
        char detail[160];
        snprintf(detail, sizeof(detail), "definition=%s role=%d grid=%d slot=%d herd=%d herd_member=%d",
            g_parse_state.key.c_str(), static_cast<int>(g_parse_state.spawn.role),
            g_parse_state.saw_grid, g_parse_state.saw_slot,
            g_parse_state.saw_herd_behavior, g_parse_state.saw_herd_member);
        log_error("FormationType herd role requires exactly one herd and herd_member declaration", detail, 0);
    }
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || g_parse_state.key.empty() || !herd_schema_matches_role ||
        (!g_parse_state.disabled && (!g_parse_state.saw_grid || !g_parse_state.saw_slot))) {
        log_error("Unable to parse FormationType xml", filename, 0);
        return 0;
    }

    if (out_definition) {
        static_cast<FormationTypeSpec &>(*out_definition) =
            std::move(static_cast<FormationTypeSpec &>(g_parse_state));
    }
    return 1;
}

int parse_definition_file(const mod_definition::DefinitionSource &source, StagedFormationType *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "FormationType")) {
        return 0;
    }
    return parse_definition_buffer(source.full_path.c_str(), buffer, out_definition);
}

int stage_definition(StagedFormationTypes &staged, StagedFormationType definition)
{
    if (definition.key.empty()) {
        staged.failure_reason = "FormationType key must not be empty.";
        return 0;
    }
    if (!staged.overlays.apply(definition.key, definition.disabled, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }
    staged.winners.insert_or_assign(definition.key, std::move(definition));
    return 1;
}

int resolve_and_validate_winners(StagedFormationTypes &staged)
{
    std::array<const FormationType *, FIGURE_TYPE_MAX> spawn_owners = {};
    for (auto &entry : staged.winners) {
        StagedFormationType &winner = entry.second;
        if (winner.disabled) {
            continue;
        }

        winner.definition = std::make_unique<FormationType>(winner.key);
        winner.definition->set_grid(winner.width, winner.height, winner.station_alignment);
        winner.definition->set_recruit_capacity(winner.recruit_capacity);
        winner.definition->set_combat_modifiers(winner.combat_modifiers);
        winner.definition->spawn = winner.spawn;
        for (const SlotSpec &slot : winner.slots) {
            if (!winner.definition->add_slot(slot.x, slot.y, slot.unit_key.c_str())) {
                staged.failure_reason = "FormationType '" + entry.first + "' from " + winner.source.full_path +
                    " references unsupported UnitType '" + slot.unit_key + "'.";
                return 0;
            }
        }
        if (!validate_definition(*winner.definition, winner.source.full_path.c_str())) {
            staged.failure_reason = "Invalid FormationType definition: " + winner.source.full_path;
            return 0;
        }
        if (winner.spawn.role == FormationSpawnRole::Herd &&
            (winner.spawn.climate < 0 || winner.spawn.climate >= 3 || winner.spawn.initial_count <= 0 ||
             winner.spawn.initial_count > winner.definition->capacity())) {
            staged.failure_reason = "Invalid herd spawn metadata in FormationType '" + entry.first + "'.";
            return 0;
        }
        if (winner.spawn.role != FormationSpawnRole::None && !winner.definition->can_spawn_figures_directly()) {
            staged.failure_reason = "Spawnable FormationType '" + entry.first +
                "' must fill its grid with one UnitType because the formation owns direct figure creation.";
            return 0;
        }
        if (winner.spawn.role != FormationSpawnRole::None) {
            const figure_type figure = winner.definition->primary_unit()->figure_type_id();
            const size_t figure_index = static_cast<size_t>(figure);
            if (figure <= FIGURE_NONE || figure >= FIGURE_TYPE_MAX || spawn_owners[figure_index]) {
                staged.failure_reason = "Spawnable FormationType definitions must own unique figure types across all roles.";
                return 0;
            }
            spawn_owners[figure_index] = winner.definition.get();
        }
    }
    return 1;
}

} // namespace

const FormationType *find_formation_type(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    for (const std::unique_ptr<FormationType> &definition : g_formation_types) {
        if (definition && definition->matches_key(key)) {
            return definition.get();
        }
    }
    return nullptr;
}

const FormationType *find_spawn_formation(figure_type type)
{
    return type > FIGURE_NONE && type < FIGURE_TYPE_MAX ?
        g_spawn_formations[static_cast<size_t>(type)] : nullptr;
}

const FormationType *find_herd_spawn_for_location(int climate, int x, int y)
{
    if (climate < 0 || climate >= static_cast<int>(g_herd_spawns_by_climate.size())) {
        return nullptr;
    }
    const std::vector<const FormationType *> &spawns = g_herd_spawns_by_climate[static_cast<size_t>(climate)];
    if (spawns.empty()) {
        return nullptr;
    }
    const unsigned int location_key = static_cast<unsigned int>(x) * 73856093u ^ static_cast<unsigned int>(y) * 19349663u;
    return spawns[location_key % spawns.size()];
}

const std::vector<std::unique_ptr<FormationType>> &formation_types()
{
    return g_formation_types;
}

} // namespace formation_type_registry_impl

const char *formation_type_registry_get_failure_reason(void)
{
    return formation_type_registry_impl::g_failure_reason.c_str();
}

const char *formation_type_definition_source_path(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        formation_type_registry_impl::g_formation_type_overlays.find(key);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int formation_type_definition_is_suppressed(const char *key)
{
    if (!key || !*key) {
        return 0;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        formation_type_registry_impl::g_formation_type_overlays.find(key);
    return entry && entry->disabled;
}

void formation_type_registry_reset(void)
{
    formation_type_registry_impl::g_formation_types.clear();
    formation_type_registry_impl::g_formation_type_overlays.clear();
    formation_type_registry_impl::g_failure_reason.clear();
    formation_type_registry_impl::g_spawn_formations.fill(nullptr);
    for (std::vector<const FormationType *> &spawns : formation_type_registry_impl::g_herd_spawns_by_climate) {
        spawns.clear();
    }
}

int formation_type_registry_load(void)
{
    using namespace formation_type_registry_impl;

    StagedFormationTypes staged;
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"FormationType"},
            "FormationType",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedFormationType definition;
                definition.source = source;
                if (!parse_definition_file(source, &definition)) {
                    staged.failure_reason = "Unable to parse FormationType xml: " + source.full_path;
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

    std::vector<std::unique_ptr<FormationType>> published;
    std::array<const FormationType *, FIGURE_TYPE_MAX> spawn_formations = {};
    std::array<std::vector<const FormationType *>, 3> herd_spawns;
    published.reserve(staged.overlays.active_count());
    for (auto &entry : staged.winners) {
        if (!entry.second.disabled) {
            StagedFormationType &winner = entry.second;
            FormationType *definition = winner.definition.get();
            const figure_type figure = definition->primary_unit()->figure_type_id();
            if (winner.spawn.role != FormationSpawnRole::None) {
                const size_t figure_index = static_cast<size_t>(figure);
                spawn_formations[figure_index] = definition;
                if (winner.spawn.role == FormationSpawnRole::Herd) {
                    herd_spawns[static_cast<size_t>(winner.spawn.climate)].push_back(definition);
                }
            }
            published.push_back(std::move(entry.second.definition));
        }
    }
    g_formation_types = std::move(published);
    g_spawn_formations = spawn_formations;
    g_herd_spawns_by_climate = herd_spawns;
    g_formation_type_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int formation_type_layered_definition_buffers_are_valid_for_test(
    const formation_type_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    formation_type_layer_test_result *result)
{
    using namespace formation_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedFormationTypes staged;
    for (int index = 0; index < input_count; ++index) {
        const formation_type_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedFormationType definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.full_path = input.source_path ? input.source_path : "FormationTypeLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.category = "FormationType";
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
                result->queried_capacity = winner->second.definition->capacity();
                result->queried_recruit_capacity = winner->second.definition->recruit_capacity();
                constexpr int TEST_BASE_VALUE = 100;
                result->queried_melee_attack_modifier = winner->second.definition->modified_combat_value(
                    FormationCombatStat::MeleeAttack, TEST_BASE_VALUE) - TEST_BASE_VALUE;
                result->queried_melee_defense_modifier = winner->second.definition->modified_combat_value(
                    FormationCombatStat::MeleeDefense, TEST_BASE_VALUE) - TEST_BASE_VALUE;
                result->queried_morale_modifier = winner->second.definition->modified_combat_value(
                    FormationCombatStat::Morale, TEST_BASE_VALUE) - TEST_BASE_VALUE;
                result->queried_missile_damage_modifier = winner->second.definition->modified_combat_value(
                    FormationCombatStat::MissileDamage, TEST_BASE_VALUE) - TEST_BASE_VALUE;
                const FormationSpawn &spawn = winner->second.definition->spawn;
                result->queried_spawn_role = static_cast<int>(spawn.role);
                result->queried_spawn_climate = spawn.climate;
                result->queried_spawn_count = spawn.initial_count;
                result->queried_herd_roam_distance = spawn.herd.roam_distance;
                result->queried_herd_roam_delay = spawn.herd.roam_delay;
                result->queried_herd_reproduction_delay = spawn.herd.reproduction_delay;
                result->queried_herd_member_rest_delay = spawn.herd.member_rest_delay;
                result->queried_herd_member_move_speed = spawn.herd.member_move_speed;
                result->queried_herd_member_animation_frames = spawn.herd.member_animation_frames;
                result->queried_herd_aggressive = spawn.herd.aggressive;
                result->queried_herd_allow_negative_desirability = spawn.herd.allow_negative_desirability;
                result->queried_herd_alternate_rest_animation = spawn.herd.alternate_rest_animation;
                result->queried_herd_movement_sound = static_cast<int>(spawn.herd.movement_sound);
                result->queried_herd_combat_animation = static_cast<int>(spawn.herd.combat_animation);
            }
        }
    }
    return 1;
}
#endif
