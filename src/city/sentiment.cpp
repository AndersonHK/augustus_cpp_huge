#include "figure/figure.h"
#include "building/building_record.h"
#include "sentiment.h"

#include "building/building.h"
#include "building/HousingProfileDef.h"
#include "city/constants.h"
#include "city/data_private.h"
#include "city/figures.h"
#include "city/games.h"
#include "city/god.h"
#include "city/message.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/random.h"
#include "game/difficulty.h"
#include "game/tutorial.h"

#include <math.h>
#include <stdlib.h>

#define MAX_SENTIMENT_FROM_EXTRA_ENTERTAINMENT 24
#define MAX_SENTIMENT_FROM_EXTRA_FOOD 24
#define FESTIVAL_BOOST_DECREMENT_RATE 0.84
#define UNEMPLOYMENT_THRESHHOLD 5
#define MAX_TAX_MULTIPLIER 12
#define WAGE_NEGATIVE_MODIFIER 3
#define WAGE_POSITIVE_MODIFIER 2
#define SQUALOR_MULTIPLIER 3
#define SENTIMENT_PER_ENTERTAINMENT 1
#define SENTIMENT_PER_EXTRA_FOOD 12
#define MAX_SENTIMENT_CHANGE 2
#define DESIRABILITY_TO_SENTIMENT_RATIO 2
#define COOLDOWN_AFTER_CRIME_DAYS 10
#define EXECUTIONS_GAMES_SENTIMENT_BONUS 20
#define IMPERIAL_GAMES_SENTIMENT_BONUS 15
#define POP_STEP_FOR_BASE_AVERAGE_HOUSE_LEVEL 625

int city_sentiment(void)
{
    return city_data.sentiment.value;
}

int city_sentiment_low_mood_cause(void)
{
    return city_data.sentiment.low_mood_cause;
}

void city_sentiment_change_happiness(int amount)
{
    Building::for_each(BuildingRuntimeList::Housing, [amount](Building *building) {
        if (building->is_in_use()) {
            HousingState &state = building->Housing->state();
            state.happiness = static_cast<int8_t>(calc_bound(state.happiness + amount, 0, 100));
        }
    });
}

void city_sentiment_set_happiness(int amount_set)
{
    Building::for_each(BuildingRuntimeList::Housing, [amount_set](Building *building) {
        if (building->is_in_use()) {
            building->Housing->state().happiness =
                static_cast<int8_t>(calc_bound(amount_set, 0, 100));
        }
    });
}

void city_sentiment_set_max_happiness(int max)
{
    max = calc_bound(max, 0, 100);
    Building::for_each(BuildingRuntimeList::Housing, [max](Building *building) {
        if (building->is_in_use()) {
            HousingState &state = building->Housing->state();
            if (state.happiness > max) {
                state.happiness = static_cast<int8_t>(max);
            }
        }
    });
}

void city_sentiment_set_min_happiness(int min)
{
    min = calc_bound(min, 0, 100);
    Building::for_each(BuildingRuntimeList::Housing, [min](Building *building) {
        if (building->is_in_use()) {
            HousingState &state = building->Housing->state();
            if (state.happiness < min) {
                state.happiness = static_cast<int8_t>(min);
            }
        }
    });
}

int city_sentiment_get_population_below_happiness(int happiness)
{
    int population = 0;
    Building::for_each(BuildingRuntimeList::Housing, [happiness, &population](Building *building) {
        if (building->is_in_use()) {
            const HousingState &state = building->Housing->state();
            if (state.happiness < happiness) {
                population += state.population;
            }
        }
    });
    return population;
}

void city_sentiment_reset_protesters_criminals(void)
{
    city_data.sentiment.protesters = 0;
    city_data.sentiment.criminals = 0;
}

void city_sentiment_add_protester(void)
{
    city_data.sentiment.protesters++;
}

void city_sentiment_add_criminal(void)
{
    city_data.sentiment.criminals++;
}

int city_sentiment_protesters(void)
{
    return city_data.sentiment.protesters;
}

int city_sentiment_criminals(void)
{
    return city_data.sentiment.criminals;
}

int city_sentiment_crime_cooldown(void)
{
    return city_data.sentiment.crime_cooldown;
}

void city_sentiment_set_crime_cooldown(void)
{
    city_data.sentiment.crime_cooldown = COOLDOWN_AFTER_CRIME_DAYS;
}

void city_sentiment_reduce_crime_cooldown(void)
{
    if (city_sentiment_crime_cooldown() > 0) {
        city_data.sentiment.crime_cooldown -= 1;
    }
}


int city_sentiment_get_blessing_festival_boost(void)
{
    return city_data.sentiment.blessing_festival_boost;
}

void city_sentiment_decrement_blessing_boost(void)
{
    short new_sentiment = (short) (city_data.sentiment.blessing_festival_boost * FESTIVAL_BOOST_DECREMENT_RATE);
    city_data.sentiment.blessing_festival_boost = new_sentiment;
}

static int get_games_bonus(void)
{
    int bonus = 0;
    if (city_games_executions_active()) {
        bonus += EXECUTIONS_GAMES_SENTIMENT_BONUS;
    }
    if (city_games_imperial_festival_active()) {
        bonus += IMPERIAL_GAMES_SENTIMENT_BONUS;
    }
    return bonus;
}

static int get_wage_sentiment_modifier(void)
{
    int wage_differential = city_data.labor.wages - city_data.labor.wages_rome;
    return wage_differential * (wage_differential > 0 ? WAGE_POSITIVE_MODIFIER : WAGE_NEGATIVE_MODIFIER);
}

static int get_unemployment_sentiment_modifier(void)
{
    int unhappiness_treshhold = UNEMPLOYMENT_THRESHHOLD + city_god_venus_bonus_employment();
    if (city_data.labor.unemployment_percentage > unhappiness_treshhold) {
        return (city_data.labor.unemployment_percentage - unhappiness_treshhold);
    }
    return 0;
}

static int get_sentiment_modifier_for_tax_rate(int tax)
{
    int base_tax = difficulty_base_tax_rate();
    int tax_differential = base_tax - tax;
    tax_differential *= tax_differential < 0 ? (MAX_TAX_MULTIPLIER - base_tax) : (base_tax / 2);
    return tax_differential;
}

static int house_legacy_level_or_min(const Building &house)
{
    const auto *profile = house.Housing ? house.Housing->definition().profile : nullptr;
    int level = profile ? profile->compatibility_level : -1;
    return level >= HOUSE_MIN ? level : HOUSE_MIN;
}

static int house_average_level_multiplier(int level)
{
    if (level >= HOUSE_SMALL_PALACE) {
        return 30;
    } else if (level >= HOUSE_SMALL_VILLA) {
        return 20;
    } else if (level >= HOUSE_LARGE_CASA) {
        return 10;
    }
    return 1;
}

static int house_level_sentiment_multiplier(int level)
{
    if (level >= HOUSE_SMALL_VILLA) {
        return 0;
    } else if (level >= HOUSE_LARGE_CASA) {
        return 1;
    } else if (level >= HOUSE_SMALL_SHACK) {
        return 2;
    }
    return 3;
}

// Average housing Level
// Determines the level under which houses will suffer squalor (inequality) sentiment penalty
static int get_average_housing_level(void)
{
    int avg = 0;
    int population = 0;

    Building::for_each(BuildingRuntimeList::Housing, [&avg, &population](Building *building) {
        if (!building->is_in_use()) {
            return;
        }
        int level = house_legacy_level_or_min(*building);
        int multiplier = house_average_level_multiplier(level);
        const int house_population = building->Housing->state().population;
        avg += level * house_population * multiplier;
        population += house_population * multiplier;
    });
    if (population) {
        avg = avg / population;
    }
    int base_average_level = population / POP_STEP_FOR_BASE_AVERAGE_HOUSE_LEVEL;
    base_average_level = calc_bound(base_average_level, HOUSE_SMALL_TENT, HOUSE_SMALL_INSULA);
    return avg < base_average_level ? base_average_level : avg;
}

static int house_level_sentiment_modifier(int level, int average)
{
    int diff_from_average = level - average;
    if (diff_from_average < 0) {
        diff_from_average *= SQUALOR_MULTIPLIER;
    }
    return diff_from_average;
}

static int extra_desirability_bonus(int desirability, int max)
{
    int extra = (desirability - 50) / DESIRABILITY_TO_SENTIMENT_RATIO;
    return calc_bound(extra, 0, max);
}

static int extra_entertainment_bonus(int entertainment, int required)
{
    int extra = (entertainment - required) * SENTIMENT_PER_ENTERTAINMENT;
    return calc_bound(extra, 0, MAX_SENTIMENT_FROM_EXTRA_ENTERTAINMENT);
}

static int extra_food_bonus(int types, int required)
{
    // Tents have no food bonus because they always scavenge for food
    if (required == 0) {
        return 0;
    }
    int extra = ((types - required) * SENTIMENT_PER_EXTRA_FOOD);
    return calc_bound(extra, 0, MAX_SENTIMENT_FROM_EXTRA_FOOD);
}

// Updates city sentiment (daily)
void city_sentiment_update(void)
{
    city_population_check_consistency();

    int default_sentiment = difficulty_sentiment();
    int houses_calculated = 0;
    int sentiment_contribution_taxes = get_sentiment_modifier_for_tax_rate(city_data.finance.tax_percentage);
    int sentiment_contribution_no_tax = get_sentiment_modifier_for_tax_rate(0) / 2;
    int sentiment_contribution_wages = get_wage_sentiment_modifier();
    int sentiment_contribution_unemployment = get_unemployment_sentiment_modifier();
    int average_housing_level = get_average_housing_level();
    int blessing_festival_boost = city_data.sentiment.blessing_festival_boost;
    int average_squalor_penalty = 0;
    int games_bonus = get_games_bonus();

    int total_sentiment = 0;
    int total_pop = 0;
    int total_houses = 0;

    Building::for_each(BuildingRuntimeList::Housing, [&](Building *building) {
        if (!building->is_in_use()) {
            return;
        }
        ::building *record = const_cast<::building *>(building->record());
        HousingState &state = building->Housing->state();
        const HousingServiceState &services = state.services;
        if (!state.population) {
            state.happiness = static_cast<int8_t>(
                calc_bound(city_data.sentiment.value + 10, 0, 100));
            return;
        }

        int level = house_legacy_level_or_min(*building);
        int house_level_multiplier = house_level_sentiment_multiplier(level);
        const auto *profile = building->Housing->definition().profile;
        int required_entertainment = profile ? profile->requirements.entertainment : 0;
        int required_food_types = profile ? profile->requirements.food_types : 0;
        int sentiment = default_sentiment;

        // Taxes
        if (state.tax_coverage) {
            sentiment += sentiment_contribution_taxes;
        } else {
            sentiment += sentiment_contribution_no_tax;
        }

        // Wages and Unemployment
        if (building->Housing->has_plebeian_residents()) {
            sentiment += sentiment_contribution_wages;
            sentiment -= sentiment_contribution_unemployment;
        }

        // Squalor
        int house_level_sentiment = house_level_sentiment_modifier(level, average_housing_level);
        if (house_level_sentiment < 0) {
            house_level_sentiment *= house_level_multiplier;
            average_squalor_penalty += house_level_sentiment;
        }
        sentiment += house_level_sentiment;

        // Desirability
        int max_desirability = level + 1;
        int desirability_bonus = extra_desirability_bonus(record->desirability, max_desirability);
        sentiment += desirability_bonus;

        // Entertainment
        int entertainment_bonus = extra_entertainment_bonus(services.entertainment, required_entertainment);
        sentiment += entertainment_bonus;

        // Food Variety
        int food_bonus = extra_food_bonus(services.num_foods, required_food_types);
        sentiment += food_bonus;

        // Games and Festivals (calculated earlier)
        sentiment += games_bonus;
        sentiment += blessing_festival_boost;

        // Change sentiment gradually to the new value
        int sentiment_delta = sentiment - state.happiness;
        sentiment_delta = calc_bound(sentiment_delta, -MAX_SENTIMENT_CHANGE, MAX_SENTIMENT_CHANGE);
        state.happiness = static_cast<int8_t>(
            calc_bound(state.happiness + sentiment_delta, 0, 100));
        houses_calculated++;

        total_pop += state.population;
        total_houses++;
        total_sentiment += state.happiness * state.population;

        int worst_sentiment = 0;
        state.sentiment_message = LOW_MOOD_CAUSE_NONE;

        // If the house is under 80 happiness, its worst sentiment is used to show the player why
        if (state.happiness < 80) {
            // Taxes
            if (state.tax_coverage) {
                worst_sentiment = sentiment_contribution_taxes;
                state.sentiment_message = LOW_MOOD_CAUSE_HIGH_TAXES;
            }
            // Unemployment, low Wages and Squalor
            if (building->Housing->has_plebeian_residents()) {
                if (-sentiment_contribution_unemployment < worst_sentiment) {
                    worst_sentiment = -sentiment_contribution_unemployment;
                    state.sentiment_message = LOW_MOOD_CAUSE_NO_JOBS;
                }
                if (sentiment_contribution_wages < worst_sentiment) {
                    worst_sentiment = sentiment_contribution_wages;
                    state.sentiment_message = LOW_MOOD_CAUSE_LOW_WAGES;
                }
                if (house_level_sentiment < worst_sentiment) {
                    worst_sentiment = house_level_sentiment;
                    state.sentiment_message = LOW_MOOD_CAUSE_SQUALOR;
                }
            }
            // If the worst sentiment isn't that bad, suggest a way to improve it directly instead
            if (worst_sentiment > -15) {
                // Suggest more entertainment
                if (entertainment_bonus < SENTIMENT_PER_EXTRA_FOOD ||
                    (entertainment_bonus < food_bonus && entertainment_bonus < desirability_bonus)) {
                    state.sentiment_message = SUGGEST_MORE_ENT;
                // Suggest more desirability
                } else if (desirability_bonus < max_desirability && desirability_bonus < food_bonus) {
                    state.sentiment_message = SUGGEST_MORE_DESIRABILITY;
                // Suggest more food types
                } else if (required_food_types > 0 &&
                    food_bonus < MAX_SENTIMENT_FROM_EXTRA_FOOD && services.num_foods < 3) {
                    state.sentiment_message = SUGGEST_MORE_FOOD;
                // Suggest... nothing?
                } else {
                    state.sentiment_message = LOW_MOOD_CAUSE_NONE;
                }
            }
        }
    });

    if (total_pop) {
        city_data.sentiment.value = calc_bound(total_sentiment / total_pop, 0, 100);
        average_squalor_penalty = average_squalor_penalty / total_houses;
    } else {
        city_data.sentiment.value = default_sentiment;
    }

    int worst_sentiment = 0;
    city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_NONE;

    if (-sentiment_contribution_unemployment < worst_sentiment) {
        worst_sentiment = -sentiment_contribution_unemployment;
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_NO_JOBS;
    }
    if (sentiment_contribution_taxes < worst_sentiment) {
        worst_sentiment = sentiment_contribution_taxes;
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_HIGH_TAXES;
    }
    if (sentiment_contribution_wages < worst_sentiment) {
        worst_sentiment = sentiment_contribution_wages;
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_LOW_WAGES;
    }
    if (average_squalor_penalty < worst_sentiment) {
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_SQUALOR;
    }

    if (city_data.sentiment.message_delay) {
        city_data.sentiment.message_delay--;
    }

    if (city_data.sentiment.value < 48 && city_data.sentiment.value < city_data.sentiment.previous_value) {
        if (city_data.sentiment.message_delay <= 0) {
            city_data.sentiment.message_delay = 3;
            int cause = city_data.sentiment.low_mood_cause;
            if (!cause) {
                cause = -1;
            }
            if (city_data.sentiment.value < 35) {
                city_message_post(0, MESSAGE_PEOPLE_ANGRY, cause, 0);
            } else if (city_data.sentiment.value < 40) {
                city_message_post(0, MESSAGE_PEOPLE_UNHAPPY, cause, 0);
            } else {
                city_message_post(0, MESSAGE_PEOPLE_DISGRUNTLED, cause, 0);
            }
        }
    }

    city_data.sentiment.previous_value = city_data.sentiment.value;
}
