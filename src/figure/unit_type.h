#pragma once

#include "figure/type.h"
#include "game/settings.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

enum {
    LEGION_RECRUIT_NONE = 0,
    LEGION_RECRUIT_MOUNTED = 1,
    LEGION_RECRUIT_JAVELIN = 2,
    LEGION_RECRUIT_LEGIONARY = 3,
    LEGION_RECRUIT_INFANTRY = 4,
    LEGION_RECRUIT_ARCHER = 5,
};

struct UnitCombatStats {
    // Preserves the legacy combat contract: a figure dies when accumulated
    // damage is greater than this value.
    int health = 0;
    int attack = 0;
    int defense = 0;
    int morale = 0;
    int armor = 0;
    std::array<int, DIFFICULTY_VERY_HARD + 1> difficulty_attack = {-1, -1, -1, -1, -1};

    int attack_for_difficulty(set_difficulty difficulty) const;
};

struct UnitMeleeAbility {
    bool engages_adjacent = true;
    int exposed_back_bonus = 0;
    int halted_attack_bonus = 0;
    int charge_attack_bonus = 0;
    int exposed_defense_penalty = 0;
    int column_defense_bonus = 0;
    int double_line_defense_bonus = 0;
    int single_line_missile_defense_bonus = 0;
    int column_missile_damage = 0;
};

enum class UnitCombatStat {
    Health,
    Attack,
    Defense,
    Morale,
    Armor
};

struct UnitRangedAbility {
    struct Projectile {
        figure_type type = FIGURE_NONE;
        int damage = 0;
    };

    int range = 0;
    int cooldown = 0;
    int damage = 0;
    int launch_frame = 1;
    bool requires_double_line = false;
    figure_type projectile_type = FIGURE_NONE;
    std::array<Projectile, ENEMY_MAX> enemy_projectiles = {};

    Projectile projectile_for_enemy(int enemy_type) const;
};

class UnitType {
public:
    explicit UnitType(std::string key);

    const char *key() const;
    bool matches_key(const char *key) const;

    bool set_figure_type_from_key(const char *key);
    figure_type figure_type_id() const;
    bool has_figure_type() const;

    void set_combat_value(UnitCombatStat stat, int value);
    bool set_attack_for_difficulty(set_difficulty difficulty, int value);
    bool has_valid_combat_stats() const;
    const UnitCombatStats &combat_stats() const;
    bool has_morale() const;

    void set_pathing_key(std::string key);
    const char *pathing_key() const;

    bool set_recruit_type_from_key(const char *key);
    int recruit_type() const;

    void set_requires_weapon(bool value);
    bool requires_weapon() const;

    void set_melee_ability(const UnitMeleeAbility &ability);
    const UnitMeleeAbility *melee_ability() const;
    bool has_any_ability() const;

    bool set_ranged_ability(const UnitRangedAbility &ability, const char *projectile_key);
    bool set_ranged_projectile_for_enemy(int enemy_type, const char *projectile_key, int damage);
    const UnitRangedAbility *ranged_ability() const;

    static int recruit_type_from_key(const char *key);

private:
    std::string key_;
    figure_type figure_type_ = FIGURE_NONE;
    UnitCombatStats combat_stats_;
    bool has_morale_ = false;
    std::string pathing_key_;
    int recruit_type_ = LEGION_RECRUIT_NONE;
    bool requires_weapon_ = false;
    bool has_melee_ = false;
    bool has_ranged_ = false;
    UnitMeleeAbility melee_ability_;
    UnitRangedAbility ranged_ability_;
};

namespace unit_type_registry_impl {

const UnitType *find_unit_type(const char *key);
const UnitType *find_unit_type(figure_type type);
const std::vector<std::unique_ptr<UnitType>> &unit_types();

} // namespace unit_type_registry_impl

const char *unit_type_registry_get_failure_reason(void);
const char *unit_type_definition_source_path(const char *key);
int unit_type_definition_is_suppressed(const char *key);
int unit_type_registry_load(void);
void unit_type_registry_reset(void);

#ifdef STARTUP_PARSER_TEST
typedef struct {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
} unit_type_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_recruit_type;
    int queried_requires_weapon;
    int queried_health;
    int queried_attack;
    int queried_defense;
    int queried_morale;
    int queried_has_morale;
    int queried_armor;
    int queried_has_melee;
    int queried_engages_adjacent;
    int queried_has_ranged;
    int queried_range;
    int queried_cooldown;
    int queried_damage;
    int queried_launch_frame;
    int queried_requires_double_line;
    int queried_projectile_type;
    int queried_source_layer;
} unit_type_layer_test_result;

int unit_type_layered_definition_buffers_are_valid_for_test(
    const unit_type_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    unit_type_layer_test_result *result);
#endif
