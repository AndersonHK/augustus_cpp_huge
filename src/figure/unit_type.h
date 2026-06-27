#pragma once

#include "figure/type.h"

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
    int health = 0;
    int attack = 0;
    int defense = 0;
    int morale = 0;
    int armor = 0;
};

enum class UnitCombatStat {
    Health,
    Attack,
    Defense,
    Morale,
    Armor
};

struct UnitRangedAbility {
    int range = 0;
    int cooldown = 0;
    int damage = 0;
    std::string projectile;
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
    bool has_valid_combat_stats() const;

    void set_pathing_key(std::string key);
    const char *pathing_key() const;

    void set_graphics_figure_type(std::string figure_type);
    const char *graphics_figure_type() const;

    bool set_recruit_type_from_key(const char *key);
    int recruit_type() const;
    bool has_recruit_type() const;

    void set_requires_weapon(bool value);
    bool requires_weapon() const;

    void enable_melee();
    bool has_any_ability() const;

    bool set_ranged_ability(const UnitRangedAbility &ability);

    static int recruit_type_from_key(const char *key);

private:
    std::string key_;
    figure_type figure_type_ = FIGURE_NONE;
    UnitCombatStats combat_stats_;
    std::string pathing_key_;
    std::string graphics_figure_type_;
    int recruit_type_ = LEGION_RECRUIT_NONE;
    bool requires_weapon_ = false;
    bool has_melee_ = false;
    bool has_ranged_ = false;
    UnitRangedAbility ranged_ability_;
};

namespace unit_type_registry_impl {

const UnitType *find_unit_type(const char *key);
const std::vector<std::unique_ptr<UnitType>> &unit_types();

} // namespace unit_type_registry_impl

const char *unit_type_registry_get_failure_reason(void);
int unit_type_registry_load(void);
void unit_type_registry_reset(void);
