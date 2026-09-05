#pragma once

#include "figure/type.h"


enum figure_category_mask {
    FIGURE_CATEGORY_INACTIVE = 0,
    FIGURE_CATEGORY_CITIZEN = 1 << 0,
    FIGURE_CATEGORY_ARMED = 1 << 1,
    FIGURE_CATEGORY_HOSTILE = 1 << 2,
    FIGURE_CATEGORY_CRIMINAL = 1 << 3,
    FIGURE_CATEGORY_NATIVE = 1 << 4,
    FIGURE_CATEGORY_ANIMAL = 1 << 5,
    FIGURE_CATEGORY_AGGRESSIVE_ANIMAL = 1 << 6,
    FIGURE_CATEGORY_PROJECTILE = 1 << 7,
    FIGURE_CATEGORY_EFFECT = 1 << 8,
    FIGURE_CATEGORY_INDUSTRY = 1 << 9,
    FIGURE_CATEGORY_ENTERTAINMENT = 1 << 10,
    FIGURE_CATEGORY_EDUCATION = 1 << 11,
    FIGURE_CATEGORY_HEALTH = 1 << 12,
        
    FIGURE_CATEGORY_ALL = FIGURE_CATEGORY_INACTIVE | FIGURE_CATEGORY_CITIZEN | FIGURE_CATEGORY_HOSTILE | FIGURE_CATEGORY_ANIMAL
};

struct figure_properties {
    figure_category_mask category;
    int max_damage; //health points - 1 (this is maximum damage that can be taken without dying)
    int attack_value;
    int defense_value;
    int missile_defense_value;
    int missile_attack_value;
    int missile_delay;
};

const figure_properties *figure_properties_for_type(figure_type type);
int figure_damage_limit_for_type(figure_type type);
int figure_attack_value_for_type(figure_type type);
int figure_defense_value_for_type(figure_type type);
int figure_missile_defense_for_type(figure_type type);


constexpr figure_category_mask operator|(figure_category_mask lhs, figure_category_mask rhs)
{
    return static_cast<figure_category_mask>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

constexpr figure_category_mask operator&(figure_category_mask lhs, figure_category_mask rhs)
{
    return static_cast<figure_category_mask>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr figure_category_mask operator^(figure_category_mask lhs, figure_category_mask rhs)
{
    return static_cast<figure_category_mask>(static_cast<unsigned int>(lhs) ^ static_cast<unsigned int>(rhs));
}

constexpr bool figure_category_mask_has_any(figure_category_mask value, figure_category_mask mask)
{
    return static_cast<unsigned int>(value & mask) != 0;
}
