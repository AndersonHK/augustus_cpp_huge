#include "building/HousingStateBridge.h"

HousingState housing_state_from_legacy_save(const building_save_bridge::LegacyBuildingSaveDto &legacy)
{
    HousingState state;
    state.population = legacy.housing_population;
    state.population_room = legacy.housing_population_room;
    state.highest_population = legacy.housing_highest_population;
    state.unreachable_ticks = legacy.housing_unreachable_ticks;
    state.immigrant_figure_id = legacy.housing_immigrant_figure_id;

    state.services.theater = legacy.housing.theater;
    state.services.amphitheater_actor = legacy.housing.amphitheater_actor;
    state.services.amphitheater_gladiator = legacy.housing.amphitheater_gladiator;
    state.services.colosseum_gladiator = legacy.housing.colosseum_gladiator;
    state.services.colosseum_lion = legacy.housing.colosseum_lion;
    state.services.hippodrome = legacy.housing.hippodrome;
    state.services.school = legacy.housing.school;
    state.services.library = legacy.housing.library;
    state.services.academy = legacy.housing.academy;
    state.services.barber = legacy.housing.barber;
    state.services.clinic = legacy.housing.clinic;
    state.services.bathhouse = legacy.housing.bathhouse;
    state.services.hospital = legacy.housing.hospital;
    state.services.temple_ceres = legacy.housing.temple_ceres;
    state.services.temple_neptune = legacy.housing.temple_neptune;
    state.services.temple_mercury = legacy.housing.temple_mercury;
    state.services.temple_mars = legacy.housing.temple_mars;
    state.services.temple_venus = legacy.housing.temple_venus;
    state.services.num_foods = legacy.housing.num_foods;
    state.services.entertainment = legacy.housing.entertainment;
    state.services.education = legacy.housing.education;
    state.services.health = legacy.housing.health;
    state.services.num_gods = legacy.housing.num_gods;

    state.no_space_to_expand = legacy.housing.no_space_to_expand;
    state.devolve_delay = legacy.housing.devolve_delay;
    state.evolve_text_id = legacy.housing.evolve_text_id;
    state.sentiment_message = legacy.housing_sentiment_message;
    state.criminal_active = legacy.housing_criminal_active;
    state.figure_generation_delay = legacy.housing_generation_delay_or_rubble_seed;
    state.tax_coverage = legacy.housing_tax_coverage;
    state.monthly_levy = legacy.housing_monthly_levy_or_building_levy;
    state.tax_income = legacy.housing_tax_income_or_collected_tax;
    state.pantheon_access = legacy.housing_pantheon_access;
    state.days_without_food = legacy.housing_days_without_food;
    state.happiness = legacy.housing_happiness_or_native_anger;
    state.tavern_wine_access = legacy.housing_tavern_wine_access;
    state.tavern_food_access = legacy.housing_tavern_food_access;
    state.arena_gladiator = legacy.housing_arena_gladiator;
    state.arena_lion = legacy.housing_arena_lion;
    return state;
}

void housing_state_to_legacy_save(
    const HousingState &state,
    building_save_bridge::LegacyBuildingSaveDto &legacy)
{
    legacy.housing_population = state.population;
    legacy.housing_population_room = state.population_room;
    legacy.housing_highest_population = state.highest_population;
    legacy.housing_unreachable_ticks = state.unreachable_ticks;
    legacy.housing_immigrant_figure_id = state.immigrant_figure_id;

    legacy.housing.theater = state.services.theater;
    legacy.housing.amphitheater_actor = state.services.amphitheater_actor;
    legacy.housing.amphitheater_gladiator = state.services.amphitheater_gladiator;
    legacy.housing.colosseum_gladiator = state.services.colosseum_gladiator;
    legacy.housing.colosseum_lion = state.services.colosseum_lion;
    legacy.housing.hippodrome = state.services.hippodrome;
    legacy.housing.school = state.services.school;
    legacy.housing.library = state.services.library;
    legacy.housing.academy = state.services.academy;
    legacy.housing.barber = state.services.barber;
    legacy.housing.clinic = state.services.clinic;
    legacy.housing.bathhouse = state.services.bathhouse;
    legacy.housing.hospital = state.services.hospital;
    legacy.housing.temple_ceres = state.services.temple_ceres;
    legacy.housing.temple_neptune = state.services.temple_neptune;
    legacy.housing.temple_mercury = state.services.temple_mercury;
    legacy.housing.temple_mars = state.services.temple_mars;
    legacy.housing.temple_venus = state.services.temple_venus;
    legacy.housing.num_foods = state.services.num_foods;
    legacy.housing.entertainment = state.services.entertainment;
    legacy.housing.education = state.services.education;
    legacy.housing.health = state.services.health;
    legacy.housing.num_gods = state.services.num_gods;

    legacy.housing.no_space_to_expand = state.no_space_to_expand;
    legacy.housing.devolve_delay = state.devolve_delay;
    legacy.housing.evolve_text_id = state.evolve_text_id;
    legacy.housing_sentiment_message = state.sentiment_message;
    legacy.housing_criminal_active = state.criminal_active;
    legacy.housing_generation_delay_or_rubble_seed = state.figure_generation_delay;
    legacy.housing_tax_coverage = state.tax_coverage;
    legacy.housing_monthly_levy_or_building_levy = state.monthly_levy;
    legacy.housing_tax_income_or_collected_tax = state.tax_income;
    legacy.housing_pantheon_access = state.pantheon_access;
    legacy.housing_days_without_food = state.days_without_food;
    legacy.housing_happiness_or_native_anger = state.happiness;
    legacy.housing_tavern_wine_access = state.tavern_wine_access;
    legacy.housing_tavern_food_access = state.tavern_food_access;
    legacy.housing_arena_gladiator = state.arena_gladiator;
    legacy.housing_arena_lion = state.arena_lion;
}
