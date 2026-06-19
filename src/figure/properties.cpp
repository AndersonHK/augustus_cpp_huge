#include "properties.h"

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
    .max_damage = 50, .attack_value = 5, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 80, .attack_value = 4, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 4, .missile_delay = 100
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 120, .attack_value = 8, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 150, .attack_value = 10, .defense_value = 2,
    .missile_defense_value = 0, .missile_attack_value = 2, .missile_delay = 150
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
    .max_damage = 100, .attack_value = 9, .defense_value = 2,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_ENTERTAINMENT,
    .max_damage = 100, .attack_value = 15, .defense_value = 0,
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
    .max_damage = 12, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 12, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 12, .attack_value = 0, .defense_value = 0,
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
    .max_damage = 40, .attack_value = 6, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 50, .attack_value = 6, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 6, .missile_delay = 50
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 70, .attack_value = 5, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 4, .missile_delay = 70
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 90, .attack_value = 7, .defense_value = 1,
    .missile_defense_value = 1, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 120, .attack_value = 12, .defense_value = 2,
    .missile_defense_value = 2, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 120, .attack_value = 7, .defense_value = 1,
    .missile_defense_value = 0, .missile_attack_value = 5, .missile_delay = 70
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 200, .attack_value = 20, .defense_value = 5,
    .missile_defense_value = 8, .missile_attack_value = 6, .missile_delay = 70
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 120, .attack_value = 15, .defense_value = 4,
    .missile_defense_value = 4, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 90, .attack_value = 7, .defense_value = 1,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 110, .attack_value = 10, .defense_value = 2,
    .missile_defense_value = 2, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 70, .attack_value = 5, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 3, .missile_delay = 100
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 100, .attack_value = 6, .defense_value = 1,
    .missile_defense_value = 0, .missile_attack_value = 4, .missile_delay = 70
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 120, .attack_value = 15, .defense_value = 2,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 100, .attack_value = 9, .defense_value = 2,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 90, .attack_value = 4, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 4, .missile_delay = 100
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 100, .attack_value = 8, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 150, .attack_value = 13, .defense_value = 2,
    .missile_defense_value = 2, .missile_attack_value = 2, .missile_delay = 150
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
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 200
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
    .max_damage = 80, .attack_value = 8, .defense_value = 0,
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
    .max_damage = 70, .attack_value = 8, .defense_value = 1,
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
    .max_damage = 50, .attack_value = 6, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 6, .missile_delay = 50
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 40
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
    .max_damage = 12, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_HOSTILE,
    .max_damage = 12, .attack_value = 0, .defense_value = 0,
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
    .max_damage = 110, .attack_value = 8, .defense_value = 1,
    .missile_defense_value = 2, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_CITIZEN,
    .max_damage = 10, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 0, .missile_delay = 0
    },
    {
    .category = FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CITIZEN,
    .max_damage = 90, .attack_value = 6, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 4, .missile_delay = 50
    },
    {
    .category = FIGURE_CATEGORY_HOSTILE,
    .max_damage = 200, .attack_value = 1, .defense_value = 0,
    .missile_defense_value = 20, .missile_attack_value = 100, .missile_delay = 200
    },
    {
    .category = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_PROJECTILE,
    .max_damage = 100, .attack_value = 0, .defense_value = 0,
    .missile_defense_value = 0, .missile_attack_value = 200, .missile_delay = 0
    },

};

const figure_properties *figure_properties_for_type(figure_type type)
{
    return &properties[type];
}
