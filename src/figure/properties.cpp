#include "properties.h"

#include "figure/unit_type.h"
#include "core/crash_context.h"
#include <exception>

static const figure_properties properties[FIGURE_TYPE_MAX] = {
    {
    .category = FIGURE_CATEGORY_INACTIVE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_EFFECT,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_EDUCATION,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_EDUCATION,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_EDUCATION,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_HEALTH,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_HEALTH,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_HEALTH,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_HEALTH,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_EFFECT,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_NATIVE | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_NATIVE | FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 40, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 12, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 20, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 200, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_EDUCATION,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_EFFECT,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ANIMAL,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_AGGRESSIVE_ANIMAL | FIGURE_CATEGORY_ANIMAL,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ANIMAL,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 10, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_EFFECT,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 10, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_INDUSTRY,
    .max_damage = 20, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 0, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 200, .missile_delay = 0
    },

};

const figure_properties *figure_properties_for_type(figure_type type)
{
    return &properties[figure_type_base(type)];
}

static const UnitCombatStats *unit_combat_stats(figure_type type)
{
    const UnitType *unit = unit_type_registry_impl::find_unit_type(type);
    if (!unit && (type == FIGURE_TRADE_CARAVAN || type == FIGURE_TRADE_CARAVAN_DONKEY)) {
        error_context_report_fatal_error_dialog("UnitType error", "Trade figure has no explicit combat stats.", "Load a complete UnitType definition for the figure.");
        std::terminate();
    }
    return unit ? &unit->combat_stats() : nullptr;
}

int figure_damage_limit_for_type(figure_type type)
{
    const UnitCombatStats *stats = unit_combat_stats(type);
    return stats ? stats->health : figure_properties_for_type(type)->max_damage;
}

int figure_attack_value_for_type(figure_type type)
{
    const UnitCombatStats *stats = unit_combat_stats(type);
    return stats ? stats->attack : figure_properties_for_type(type)->attack_value;
}

int figure_defense_value_for_type(figure_type type)
{
    const UnitCombatStats *stats = unit_combat_stats(type);
    return stats ? stats->defense : figure_properties_for_type(type)->defense_value;
}

int figure_missile_defense_for_type(figure_type type)
{
    const UnitCombatStats *stats = unit_combat_stats(type);
    return stats ? stats->armor : figure_properties_for_type(type)->missile_defense_value;
}
