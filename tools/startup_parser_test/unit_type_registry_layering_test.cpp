#include "unit_type_registry_layering_test.h"

#include "figure/unit_type.h"

#include <ostream>

namespace {

constexpr const char *LOWER_XML =
    "<unit key=\"archer\"><figure type=\"fort_archer\"/><combat><health value=\"70\"/>"
    "<attack value=\"6\"/><defense value=\"3\"/><morale value=\"60\"/><armor value=\"1\"/>"
    "</combat><movement pathing=\"soldier_land\"/><recruit type=\"archer\" requires_weapon=\"false\"/>"
    "<abilities><melee/></abilities><graphics figure_type=\"archer\"/></unit>";
constexpr const char *UPPER_XML =
    "<unit key=\"archer\"><figure type=\"fort_archer\"/><combat><health value=\"90\"/>"
    "<attack value=\"8\"/><defense value=\"4\"/><morale value=\"70\"/><armor value=\"2\"/>"
    "</combat><movement pathing=\"soldier_land\"/><recruit type=\"archer\" requires_weapon=\"true\"/>"
    "<abilities><melee engages_adjacent=\"false\" exposed_back_bonus=\"4\"/>"
    "<ranged range=\"12\" cooldown=\"50\" damage=\"10\" launch_frame=\"1\" "
    "projectile=\"friendly_arrow\" requires_double_line=\"true\"/></abilities>"
    "<graphics figure_type=\"archer\"/></unit>";
constexpr const char *DISABLED_XML = "<unit key=\"archer\" disabled=\"true\"></unit>";

unit_type_layer_test_input input(const char *xml, int layer, const char *mod, const char *path)
{
    return {xml, layer, mod, path};
}

bool valid(
    const unit_type_layer_test_input *inputs,
    int count,
    const char *query,
    unit_type_layer_test_result *result = nullptr)
{
    return unit_type_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

} // namespace

bool validate_unit_type_registry_layering_contract(std::ostream &errors)
{
    const unit_type_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "Julius/UnitType/archer.xml"),
        input(UPPER_XML, 1, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    unit_type_layer_test_result result;
    if (!valid(replacement, 2, "archer", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        !result.queried_requires_weapon || result.queried_source_layer != 1 ||
        result.queried_health != 90 || result.queried_attack != 8 ||
        result.queried_defense != 4 || result.queried_morale != 70 ||
        !result.queried_has_morale || result.queried_armor != 2 || !result.queried_has_melee ||
        result.queried_engages_adjacent || !result.queried_has_ranged ||
        result.queried_range != 12 || result.queried_cooldown != 50 ||
        result.queried_damage != 10 || result.queried_launch_frame != 1 ||
        !result.queried_requires_double_line ||
        result.queried_projectile_type != FIGURE_FRIENDLY_ARROW) {
        errors << "UnitType upper-layer replacement lost winner data or provenance.\n";
        return false;
    }

    const unit_type_layer_test_input inherited_only[] = {
        input(LOWER_XML, 0, "Julius", "Julius/UnitType/archer.xml")
    };
    if (!valid(inherited_only, 1, "archer", &result) || result.queried_source_layer != 0) {
        errors << "UnitType lower definition was not inherited when the upper layer was absent.\n";
        return false;
    }

    constexpr const char *NON_RECRUIT_XML =
        "<unit key=\"prefect\"><figure type=\"prefect\"/><combat><health value=\"50\"/>"
        "<attack value=\"5\"/><defense value=\"0\"/><armor value=\"0\"/>"
        "</combat><abilities><melee exposed_back_bonus=\"4\"/></abilities></unit>";
    const unit_type_layer_test_input non_recruit[] = {
        input(NON_RECRUIT_XML, 0, "Julius", "Julius/UnitType/prefect.xml")
    };
    if (!valid(non_recruit, 1, "prefect", &result) ||
        result.queried_recruit_type != LEGION_RECRUIT_NONE || result.queried_requires_weapon ||
        result.queried_has_morale || result.queried_health != 50 ||
        result.queried_attack != 5 || !result.queried_has_melee) {
        errors << "UnitType rejected a valid non-recruitable city combat actor.\n";
        return false;
    }

    const unit_type_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "Julius/UnitType/archer.xml"),
        input(DISABLED_XML, 1, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    if (!valid(suppression, 2, "archer", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1) {
        errors << "UnitType disabled tombstone did not suppress its inherited key.\n";
        return false;
    }

    const unit_type_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "Julius/UnitType/archer.xml"),
        input(UPPER_XML, 0, "Julius", "Julius/UnitType/archer-copy.xml")
    };
    if (valid(duplicate, 2, "archer")) {
        errors << "UnitType same-layer duplicate key was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_DISABLED_CONTENT =
        "<unit key=\"archer\" disabled=\"true\"><figure type=\"fort_archer\"/></unit>";
    const unit_type_layer_test_input invalid_tombstone[] = {
        input(INVALID_DISABLED_CONTENT, 0, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    if (valid(invalid_tombstone, 1, "archer")) {
        errors << "UnitType disabled tombstone accepted authored content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_FIGURE_XML =
        "<unit key=\"archer\"><figure type=\"missing_figure\"/><combat><health value=\"70\"/>"
        "<attack value=\"6\"/><defense value=\"3\"/><morale value=\"60\"/><armor value=\"1\"/>"
        "</combat><recruit type=\"archer\"/><abilities><melee/></abilities></unit>";
    const unit_type_layer_test_input deferred_reference[] = {
        input(INVALID_LOWER_FIGURE_XML, 0, "Julius", "Julius/UnitType/archer.xml"),
        input(UPPER_XML, 1, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    if (!valid(deferred_reference, 2, "archer", &result) || !result.queried_requires_weapon) {
        errors << "UnitType figure references were resolved before final winners were collected.\n";
        return false;
    }

    const unit_type_layer_test_input unresolved_winner[] = {
        input(INVALID_LOWER_FIGURE_XML, 0, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    if (valid(unresolved_winner, 1, "archer")) {
        errors << "UnitType final winner accepted an unresolved figure reference.\n";
        return false;
    }

    constexpr const char *DUPLICATE_FIGURE_XML =
        "<unit key=\"archer_variant\"><figure type=\"fort_archer\"/><combat><health value=\"90\"/>"
        "<attack value=\"6\"/><defense value=\"0\"/><morale value=\"60\"/><armor value=\"0\"/>"
        "</combat><recruit type=\"archer\"/><abilities><melee/></abilities></unit>";
    const unit_type_layer_test_input duplicate_figure[] = {
        input(LOWER_XML, 0, "Pharaoh", "Pharaoh/UnitType/archer.xml"),
        input(DUPLICATE_FIGURE_XML, 0, "Pharaoh", "Pharaoh/UnitType/archer_variant.xml")
    };
    if (valid(duplicate_figure, 2, "archer")) {
        errors << "UnitType accepted two definitions for one runtime figure identity.\n";
        return false;
    }

    constexpr const char *INVALID_PROJECTILE_XML =
        "<unit key=\"archer\"><figure type=\"fort_archer\"/><combat><health value=\"90\"/>"
        "<attack value=\"6\"/><defense value=\"0\"/><morale value=\"60\"/><armor value=\"0\"/>"
        "</combat><recruit type=\"archer\"/><abilities><ranged range=\"12\" cooldown=\"50\" "
        "damage=\"10\" launch_frame=\"1\" projectile=\"unknown\"/></abilities></unit>";
    const unit_type_layer_test_input invalid_projectile[] = {
        input(INVALID_PROJECTILE_XML, 0, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    if (valid(invalid_projectile, 1, "archer")) {
        errors << "UnitType accepted an unresolved ranged projectile.\n";
        return false;
    }

    constexpr const char *INVALID_LAUNCH_FRAME_XML =
        "<unit key=\"archer\"><figure type=\"fort_archer\"/><combat><health value=\"90\"/>"
        "<attack value=\"6\"/><defense value=\"0\"/><morale value=\"60\"/><armor value=\"0\"/>"
        "</combat><recruit type=\"archer\"/><abilities><ranged range=\"12\" cooldown=\"50\" "
        "damage=\"10\" launch_frame=\"0\" projectile=\"friendly_arrow\"/></abilities></unit>";
    const unit_type_layer_test_input invalid_launch_frame[] = {
        input(INVALID_LAUNCH_FRAME_XML, 0, "Pharaoh", "Pharaoh/UnitType/archer.xml")
    };
    if (valid(invalid_launch_frame, 1, "archer")) {
        errors << "UnitType accepted a zero ranged launch frame.\n";
        return false;
    }

    constexpr const char *INVALID_ENEMY_PROJECTILE_XML =
        "<unit key=\"enemy43_spear\"><figure type=\"enemy43_spear\"/>"
        "<combat><health value=\"70\"/><attack value=\"5\"/><defense value=\"0\"/>"
        "<armor value=\"0\"/></combat><abilities><melee/><ranged range=\"10\" cooldown=\"70\" "
        "damage=\"10\" launch_frame=\"1\" projectile=\"spear\">"
        "<projectile enemy=\"unknown\" type=\"arrow\" damage=\"12\"/>"
        "</ranged></abilities></unit>";
    const unit_type_layer_test_input invalid_enemy_projectile[] = {
        input(INVALID_ENEMY_PROJECTILE_XML, 0, "Julius", "Julius/UnitType/enemy43_spear.xml")
    };
    if (valid(invalid_enemy_projectile, 1, "enemy43_spear")) {
        errors << "UnitType accepted a ranged projectile override for an unknown enemy family.\n";
        return false;
    }

    constexpr const char *DUPLICATE_ENEMY_PROJECTILE_XML =
        "<unit key=\"enemy43_spear\"><figure type=\"enemy43_spear\"/>"
        "<combat><health value=\"70\"/><attack value=\"5\"/><defense value=\"0\"/>"
        "<armor value=\"0\"/></combat><abilities><melee/><ranged range=\"10\" cooldown=\"70\" "
        "damage=\"10\" launch_frame=\"1\" projectile=\"spear\">"
        "<projectile enemy=\"goth\" type=\"arrow\" damage=\"12\"/>"
        "<projectile enemy=\"goth\" type=\"arrow\" damage=\"12\"/>"
        "</ranged></abilities></unit>";
    const unit_type_layer_test_input duplicate_enemy_projectile[] = {
        input(DUPLICATE_ENEMY_PROJECTILE_XML, 0, "Julius", "Julius/UnitType/enemy43_spear.xml")
    };
    if (valid(duplicate_enemy_projectile, 1, "enemy43_spear")) {
        errors << "UnitType accepted duplicate projectile overrides for one enemy family.\n";
        return false;
    }

    constexpr const char *INVALID_DIFFICULTY_ATTACK_XML =
        "<unit key=\"wolf\"><figure type=\"wolf\"/><combat><health value=\"80\"/>"
        "<attack value=\"8\"><difficulty level=\"nightmare\" value=\"12\"/></attack>"
        "<defense value=\"0\"/><armor value=\"0\"/></combat>"
        "<abilities><melee exposed_back_bonus=\"4\"/></abilities></unit>";
    const unit_type_layer_test_input invalid_difficulty_attack[] = {
        input(INVALID_DIFFICULTY_ATTACK_XML, 0, "Julius", "Julius/UnitType/wolf.xml")
    };
    if (valid(invalid_difficulty_attack, 1, "wolf")) {
        errors << "UnitType accepted an attack override for an unknown difficulty.\n";
        return false;
    }

    constexpr const char *DUPLICATE_DIFFICULTY_ATTACK_XML =
        "<unit key=\"wolf\"><figure type=\"wolf\"/><combat><health value=\"80\"/>"
        "<attack value=\"8\"><difficulty level=\"easy\" value=\"4\"/>"
        "<difficulty level=\"easy\" value=\"5\"/></attack>"
        "<defense value=\"0\"/><armor value=\"0\"/></combat>"
        "<abilities><melee exposed_back_bonus=\"4\"/></abilities></unit>";
    const unit_type_layer_test_input duplicate_difficulty_attack[] = {
        input(DUPLICATE_DIFFICULTY_ATTACK_XML, 0, "Julius", "Julius/UnitType/wolf.xml")
    };
    if (valid(duplicate_difficulty_attack, 1, "wolf")) {
        errors << "UnitType accepted duplicate attack overrides for one difficulty.\n";
        return false;
    }

    struct ExpectedRomanUnit {
        const char *key;
        figure_type figure;
        UnitCombatStats stats;
        bool engages_adjacent;
        int exposed_back_bonus;
        int halted_attack_bonus;
        int charge_attack_bonus;
        int exposed_defense_penalty;
        int column_defense_bonus;
        int double_line_defense_bonus;
        int single_line_missile_defense_bonus;
        int column_missile_damage;
        int range;
        int cooldown;
        int damage;
        int launch_frame;
        bool requires_double_line;
        figure_type projectile;
    };
    const ExpectedRomanUnit roman_units[] = {
        {"legionary", FIGURE_FORT_LEGIONARY, {150, 10, 2, 80, 0}, true,
            0, 4, 0, 4, 5, 2, 0, 1, 6, 150, 20, 19, true, FIGURE_JAVELIN},
        {"javelin", FIGURE_FORT_JAVELIN, {80, 4, 0, 60, 0}, false,
            4, 0, 0, 0, 0, 0, 4, 0, 10, 100, 20, 1, false, FIGURE_JAVELIN},
        {"mounted", FIGURE_FORT_MOUNTED, {120, 8, 0, 60, 0}, true,
            4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, FIGURE_NONE},
        {"infantry", FIGURE_FORT_INFANTRY, {110, 8, 1, 60, 2}, true,
            4, 2, 2, 2, 3, 1, 0, 0, 0, 0, 0, 0, false, FIGURE_NONE},
        {"archer", FIGURE_FORT_ARCHER, {90, 6, 0, 60, 0}, false,
            4, 0, 0, 0, 0, 0, 4, 0, 12, 50, 10, 1, false, FIGURE_FRIENDLY_ARROW}
    };
    for (const ExpectedRomanUnit &expected : roman_units) {
        const UnitType *unit = unit_type_registry_impl::find_unit_type(expected.key);
        const UnitMeleeAbility *melee = unit ? unit->melee_ability() : nullptr;
        const UnitRangedAbility *ranged = unit ? unit->ranged_ability() : nullptr;
        const UnitCombatStats *stats = unit ? &unit->combat_stats() : nullptr;
        if (!unit || unit->figure_type_id() != expected.figure || !stats || !melee ||
            stats->health != expected.stats.health || stats->attack != expected.stats.attack ||
            stats->defense != expected.stats.defense || stats->morale != expected.stats.morale ||
            stats->armor != expected.stats.armor ||
            melee->engages_adjacent != expected.engages_adjacent ||
            melee->exposed_back_bonus != expected.exposed_back_bonus ||
            melee->halted_attack_bonus != expected.halted_attack_bonus ||
            melee->charge_attack_bonus != expected.charge_attack_bonus ||
            melee->exposed_defense_penalty != expected.exposed_defense_penalty ||
            melee->column_defense_bonus != expected.column_defense_bonus ||
            melee->double_line_defense_bonus != expected.double_line_defense_bonus ||
            melee->single_line_missile_defense_bonus != expected.single_line_missile_defense_bonus ||
            melee->column_missile_damage != expected.column_missile_damage ||
            ((expected.projectile == FIGURE_NONE) != (ranged == nullptr)) ||
            (ranged && (ranged->range != expected.range || ranged->cooldown != expected.cooldown ||
                ranged->damage != expected.damage || ranged->launch_frame != expected.launch_frame ||
                ranged->requires_double_line != expected.requires_double_line ||
                ranged->projectile_type != expected.projectile))) {
            errors << "Parsed Roman UnitType combat contract differs from legacy gameplay for "
                << expected.key << ".\n";
            return false;
        }
    }

    struct ExpectedCityUnit {
        const char *key;
        figure_type figure;
        UnitCombatStats stats;
        bool has_melee;
        int range;
        int cooldown;
        int damage;
        figure_type projectile;
    };
    const ExpectedCityUnit city_units[] = {
        {"prefect", FIGURE_PREFECT, {50, 5, 0, 0, 0}, true, 0, 0, 0, FIGURE_NONE},
        {"gladiator", FIGURE_GLADIATOR, {100, 9, 2, 0, 0}, true, 0, 0, 0, FIGURE_NONE},
        {"lion_tamer", FIGURE_LION_TAMER, {100, 15, 0, 0, 0}, true, 0, 0, 0, FIGURE_NONE},
        {"indigenous_native", FIGURE_INDIGENOUS_NATIVE, {40, 6, 0, 0, 0}, true,
            0, 0, 0, FIGURE_NONE},
        {"tower_sentry", FIGURE_TOWER_SENTRY, {50, 6, 0, 0, 0}, true,
            10, 50, 20, FIGURE_JAVELIN},
        {"watchman", FIGURE_WATCHMAN, {50, 6, 0, 0, 0}, true,
            10, 50, 20, FIGURE_JAVELIN},
        {"watchtower_archer", FIGURE_WATCHTOWER_ARCHER, {10, 0, 0, 0, 0}, false,
            12, 40, 10, FIGURE_FRIENDLY_ARROW}
    };
    for (const ExpectedCityUnit &expected : city_units) {
        const UnitType *unit = unit_type_registry_impl::find_unit_type(expected.key);
        const UnitCombatStats *stats = unit ? &unit->combat_stats() : nullptr;
        const UnitMeleeAbility *melee = unit ? unit->melee_ability() : nullptr;
        const UnitRangedAbility *ranged = unit ? unit->ranged_ability() : nullptr;
        if (!unit || unit->figure_type_id() != expected.figure || !stats ||
            unit->recruit_type() != LEGION_RECRUIT_NONE || unit->requires_weapon() ||
            unit->has_morale() ||
            stats->health != expected.stats.health || stats->attack != expected.stats.attack ||
            stats->defense != expected.stats.defense || stats->morale != expected.stats.morale ||
            stats->armor != expected.stats.armor || (melee != nullptr) != expected.has_melee ||
            (melee && (!melee->engages_adjacent || melee->exposed_back_bonus != 4)) ||
            ((expected.projectile == FIGURE_NONE) != (ranged == nullptr)) ||
            (ranged && (ranged->range != expected.range || ranged->cooldown != expected.cooldown ||
                ranged->damage != expected.damage || ranged->launch_frame != 1 ||
                ranged->requires_double_line || ranged->projectile_type != expected.projectile))) {
            errors << "Parsed city UnitType combat contract differs from legacy gameplay for "
                << expected.key << ".\n";
            return false;
        }
    }

    struct ExpectedEnemyUnit {
        const char *key;
        figure_type figure;
        UnitCombatStats stats;
        bool has_morale;
        int ranged_cooldown;
    };
    const ExpectedEnemyUnit enemy_units[] = {
        {"enemy43_spear", FIGURE_ENEMY43_SPEAR, {70, 5, 0, 0, 0}, false, 70},
        {"enemy44_sword", FIGURE_ENEMY44_SWORD, {90, 7, 1, 0, 1}, false, 0},
        {"enemy45_sword", FIGURE_ENEMY45_SWORD, {120, 12, 2, 0, 2}, false, 0},
        {"enemy46_camel", FIGURE_ENEMY46_CAMEL, {120, 7, 1, 0, 0}, false, 70},
        {"enemy47_elephant", FIGURE_ENEMY47_ELEPHANT, {200, 20, 5, 0, 8}, false, 0},
        {"enemy48_chariot", FIGURE_ENEMY48_CHARIOT, {120, 15, 4, 0, 4}, false, 0},
        {"enemy49_fast_sword", FIGURE_ENEMY49_FAST_SWORD, {90, 7, 1, 0, 0}, false, 0},
        {"enemy50_sword", FIGURE_ENEMY50_SWORD, {110, 10, 2, 0, 2}, false, 0},
        {"enemy51_spear", FIGURE_ENEMY51_SPEAR, {70, 5, 0, 0, 0}, false, 100},
        {"enemy52_mounted_archer", FIGURE_ENEMY52_MOUNTED_ARCHER, {100, 6, 1, 0, 0}, false, 70},
        {"enemy53_axe", FIGURE_ENEMY53_AXE, {120, 15, 2, 0, 0}, false, 0},
        {"enemy54_gladiator", FIGURE_ENEMY54_GLADIATOR, {100, 9, 2, 0, 0}, false, 0},
        {"enemy_caesar_javelin", FIGURE_ENEMY_CAESAR_JAVELIN, {90, 4, 0, 0, 0}, false, 0},
        {"enemy_caesar_mounted", FIGURE_ENEMY_CAESAR_MOUNTED, {100, 8, 0, 0, 0}, false, 0},
        {"enemy_caesar_legionary", FIGURE_ENEMY_CAESAR_LEGIONARY, {150, 13, 2, 100, 2}, true, 0},
        {"enemy_catapult", FIGURE_ENEMY_CATAPULT, {200, 1, 0, 0, 20}, false, 200}
    };
    for (const ExpectedEnemyUnit &expected : enemy_units) {
        const UnitType *unit = unit_type_registry_impl::find_unit_type(expected.key);
        const UnitCombatStats *stats = unit ? &unit->combat_stats() : nullptr;
        const UnitMeleeAbility *melee = unit ? unit->melee_ability() : nullptr;
        const UnitRangedAbility *ranged = unit ? unit->ranged_ability() : nullptr;
        if (!unit || unit->figure_type_id() != expected.figure || !stats || !melee ||
            unit->has_morale() != expected.has_morale ||
            stats->health != expected.stats.health || stats->attack != expected.stats.attack ||
            stats->defense != expected.stats.defense || stats->morale != expected.stats.morale ||
            stats->armor != expected.stats.armor || !melee->engages_adjacent ||
            melee->exposed_back_bonus != 4 ||
            ((expected.ranged_cooldown == 0) != (ranged == nullptr)) ||
            (ranged && (ranged->range != 10 || ranged->cooldown != expected.ranged_cooldown ||
                ranged->launch_frame != 1 ||
                ranged->projectile_for_enemy(ENEMY_8_GREEK).type != FIGURE_SPEAR ||
                ranged->projectile_for_enemy(ENEMY_8_GREEK).damage != 10))) {
            errors << "Parsed enemy UnitType combat contract differs from legacy gameplay for "
                << expected.key << ".\n";
            return false;
        }
        if (expected.figure == FIGURE_ENEMY_CAESAR_LEGIONARY &&
            (melee->exposed_defense_penalty != 4 || melee->column_defense_bonus != 5 ||
                melee->double_line_defense_bonus != 2 || melee->column_missile_damage != 1)) {
            errors << "Caesar legionary formation defense modifiers were not parsed from UnitType.\n";
            return false;
        }
        if (ranged && expected.figure != FIGURE_ENEMY_CATAPULT &&
            (ranged->projectile_for_enemy(ENEMY_4_GOTH).type != FIGURE_ARROW ||
                ranged->projectile_for_enemy(ENEMY_4_GOTH).damage != 12 ||
                ranged->projectile_for_enemy(ENEMY_5_PERGAMUM).type != FIGURE_ARROW ||
                ranged->projectile_for_enemy(ENEMY_9_EGYPTIAN).type != FIGURE_ARROW ||
                ranged->projectile_for_enemy(ENEMY_10_CARTHAGINIAN).type != FIGURE_ARROW)) {
            errors << "Enemy ranged UnitType lost its faction projectile alternatives for "
                << expected.key << ".\n";
            return false;
        }
        if (expected.figure == FIGURE_ENEMY_CATAPULT && ranged &&
            (ranged->projectile_for_enemy(ENEMY_4_GOTH).type != FIGURE_SPEAR ||
                ranged->projectile_for_enemy(ENEMY_4_GOTH).damage != 10)) {
            errors << "Enemy catapult no longer preserves its live spear projectile policy.\n";
            return false;
        }
    }
    if (unit_type_registry_impl::find_unit_type(FIGURE_CATAPULT_MISSILE)) {
        errors << "Unreachable compatibility catapult projectile was incorrectly published as a UnitType.\n";
        return false;
    }

    const UnitType *wolf = unit_type_registry_impl::find_unit_type("wolf");
    const UnitCombatStats *wolf_stats = wolf ? &wolf->combat_stats() : nullptr;
    const UnitMeleeAbility *wolf_melee = wolf ? wolf->melee_ability() : nullptr;
    if (!wolf || wolf->figure_type_id() != FIGURE_WOLF || !wolf_stats || !wolf_melee ||
        wolf->has_morale() || wolf->ranged_ability() ||
        wolf_stats->health != 80 || wolf_stats->attack != 8 || wolf_stats->defense != 0 ||
        wolf_stats->armor != 0 || !wolf_melee->engages_adjacent ||
        wolf_melee->exposed_back_bonus != 4 ||
        wolf_stats->attack_for_difficulty(DIFFICULTY_VERY_EASY) != 2 ||
        wolf_stats->attack_for_difficulty(DIFFICULTY_EASY) != 4 ||
        wolf_stats->attack_for_difficulty(DIFFICULTY_NORMAL) != 6 ||
        wolf_stats->attack_for_difficulty(DIFFICULTY_HARD) != 8 ||
        wolf_stats->attack_for_difficulty(DIFFICULTY_VERY_HARD) != 8) {
        errors << "Parsed wolf UnitType combat contract differs from stable live gameplay.\n";
        return false;
    }

    struct ExpectedCrimeUnit {
        const char *key;
        figure_type figure;
    };
    const ExpectedCrimeUnit crime_units[] = {
        {"protester", FIGURE_PROTESTER},
        {"criminal", FIGURE_CRIMINAL},
        {"rioter", FIGURE_RIOTER},
        {"criminal_robber", FIGURE_CRIMINAL_ROBBER},
        {"criminal_looter", FIGURE_CRIMINAL_LOOTER}
    };
    for (const ExpectedCrimeUnit &expected : crime_units) {
        const UnitType *unit = unit_type_registry_impl::find_unit_type(expected.key);
        const UnitCombatStats *stats = unit ? &unit->combat_stats() : nullptr;
        const UnitMeleeAbility *melee = unit ? unit->melee_ability() : nullptr;
        if (!unit || unit->figure_type_id() != expected.figure || !stats || !melee ||
            unit->recruit_type() != LEGION_RECRUIT_NONE || unit->has_morale() ||
            unit->ranged_ability() || stats->health != 12 || stats->attack != 0 ||
            stats->defense != 0 || stats->armor != 0 || !melee->engages_adjacent ||
            melee->exposed_back_bonus != 4 ||
            stats->attack_for_difficulty(DIFFICULTY_VERY_EASY) != 0 ||
            stats->attack_for_difficulty(DIFFICULTY_VERY_HARD) != 0) {
            errors << "Parsed crime-family UnitType contract differs from stable live gameplay for "
                << expected.key << ".\n";
            return false;
        }
    }

    const UnitType *armed_supplier = unit_type_registry_impl::find_unit_type("mess_hall_supplier");
    const UnitCombatStats *supplier_stats = armed_supplier ? &armed_supplier->combat_stats() : nullptr;
    const UnitMeleeAbility *supplier_melee = armed_supplier ? armed_supplier->melee_ability() : nullptr;
    if (!armed_supplier || armed_supplier->figure_type_id() != FIGURE_MESS_HALL_SUPPLIER ||
        !supplier_stats || !supplier_melee || armed_supplier->has_morale() ||
        armed_supplier->ranged_ability() || armed_supplier->recruit_type() != LEGION_RECRUIT_NONE ||
        supplier_stats->health != 70 || supplier_stats->attack != 8 ||
        supplier_stats->defense != 1 || supplier_stats->armor != 0 ||
        !supplier_melee->engages_adjacent || supplier_melee->exposed_back_bonus != 4 ||
        supplier_stats->attack_for_difficulty(DIFFICULTY_VERY_EASY) != 8 ||
        supplier_stats->attack_for_difficulty(DIFFICULTY_VERY_HARD) != 8) {
        errors << "Parsed armed mess-hall supplier UnitType differs from stable live gameplay.\n";
        return false;
    }

    const UnitType *ballista = unit_type_registry_impl::find_unit_type("ballista");
    const UnitCombatStats *ballista_stats = ballista ? &ballista->combat_stats() : nullptr;
    const UnitRangedAbility *ballista_ranged = ballista ? ballista->ranged_ability() : nullptr;
    if (!ballista || ballista->figure_type_id() != FIGURE_BALLISTA || !ballista_stats ||
        !ballista_ranged || ballista->melee_ability() || ballista->has_morale() ||
        ballista->recruit_type() != LEGION_RECRUIT_NONE || ballista_stats->health != 100 ||
        ballista_stats->attack != 0 || ballista_stats->defense != 0 ||
        ballista_stats->armor != 0 || ballista_ranged->range != 15 ||
        ballista_ranged->cooldown != 200 || ballista_ranged->damage != 200 ||
        ballista_ranged->launch_frame != 1 || ballista_ranged->projectile_type != FIGURE_BOLT ||
        ballista_ranged->projectile_for_enemy(ENEMY_0_BARBARIAN).type != FIGURE_BOLT ||
        ballista_ranged->projectile_for_enemy(ENEMY_0_BARBARIAN).damage != 200) {
        errors << "Parsed city ballista UnitType differs from stable live siege gameplay.\n";
        return false;
    }
    if (unit_type_registry_impl::find_unit_type(FIGURE_BOLT)) {
        errors << "Saved in-flight bolt projectile was incorrectly published as a UnitType.\n";
        return false;
    }

    return true;
}
