#pragma once

#include <cstdint>

struct HousingServiceState {
    uint8_t theater = 0;
    uint8_t amphitheater_actor = 0;
    uint8_t amphitheater_gladiator = 0;
    uint8_t colosseum_gladiator = 0;
    uint8_t colosseum_lion = 0;
    uint8_t hippodrome = 0;
    uint8_t school = 0;
    uint8_t library = 0;
    uint8_t academy = 0;
    uint8_t barber = 0;
    uint8_t clinic = 0;
    uint8_t bathhouse = 0;
    uint8_t hospital = 0;
    uint8_t temple_ceres = 0;
    uint8_t temple_neptune = 0;
    uint8_t temple_mercury = 0;
    uint8_t temple_mars = 0;
    uint8_t temple_venus = 0;
    uint8_t num_foods = 0;
    uint8_t entertainment = 0;
    uint8_t education = 0;
    uint8_t health = 0;
    uint8_t num_gods = 0;
};

struct HousingState {
    int16_t population = 0;
    int16_t population_room = 0;
    int16_t highest_population = 0;
    int16_t unreachable_ticks = 0;
    uint32_t immigrant_figure_id = 0;

    HousingServiceState services;
    uint8_t no_space_to_expand = 0;
    uint8_t devolve_delay = 0;
    uint8_t evolve_text_id = 0;
    uint8_t sentiment_message = 0;
    uint8_t criminal_active = 0;
    uint8_t figure_generation_delay = 0;
    uint8_t tax_coverage = 0;
    int8_t monthly_levy = 0;
    int32_t tax_income = 0;
    uint8_t pantheon_access = 0;
    uint8_t days_without_food = 0;
    int8_t happiness = 0;
    uint8_t tavern_wine_access = 0;
    uint8_t tavern_food_access = 0;
    uint8_t arena_gladiator = 0;
    uint8_t arena_lion = 0;
};
