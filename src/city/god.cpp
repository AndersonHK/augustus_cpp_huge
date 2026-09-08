#include "god.h"

#include "building/count.h"
#include "building/granary.h"
#include "building/god_id_bridge.h"
#include "building/god_registry.h"
#include "building/industry.h"
#include "building/building_type_registry_internal.h"
#include "city/culture.h"
#include "city/data_private.h"
#include "city/health.h"
#include "city/message.h"
#include "city/population.h"
#include "city/sentiment.h"
#include "city/trade.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/random.h"
#include "figure/formation_legion.h"
#include "figuretype/water.h"
#include "game/campaign.h"
#include "game/settings.h"
#include "game/time.h"
#include "scenario/property.h"
#include "scenario/invasion.h"
#include <algorithm>

#define TIE 10

static constexpr int GOD_STATUS_CAPACITY = static_cast<int>(GOD_ALL);

int city_gods_count(void)
{
    int count = building_type_registry_impl::god_definition_count();
    if (count <= 0) {
        return GOD_STATUS_CAPACITY;
    }
    if (count > GOD_STATUS_CAPACITY) {
        return GOD_STATUS_CAPACITY;
    }
    return count;
}

static int temple_definition_serves_god(const building_type_registry_impl::BuildingType *definition, god_type god)
{
    return definition && definition->is_temple(god);
}

static int temple_count_for_god(god_type god)
{
    int total = 0;
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (temple_definition_serves_god(definition, god)) {
            total += building_count_active(type);
        }
    }
    return total;
}

void city_gods_reset(void)
{
    for (int i = 0; i < city_gods_count(); i++) {
        god_status *god = &city_data.religion.gods[i];
        god->target_happiness = 50;
        god->happiness = 50;
        god->wrath_bolts = 0;
        god->blessing_done = 0;
        god->small_curse_done = 0;
        god->happy_bolts = 0;
        god->months_since_festival = 0;
    }
    city_data.religion.angry_message_delay = 0;
}

void city_gods_reset_neptune_blessing(void)
{
    city_data.religion.neptune_trade_bonus_active = 0;
}

void city_gods_update_blessings(void)
{
    if (city_data.religion.neptune_trade_bonus_active > 0) {
        city_data.religion.neptune_trade_bonus_active--;
    }

    if (city_data.religion.venus_blessing_months_left > 0) {
        city_data.religion.venus_blessing_months_left--;
    }
}

bool city_gods_have_effects(religion::Trigger trigger)
{
    for (int i = 0; i < building_type_registry_impl::god_definition_count(); ++i) {
        const auto *definition = building_type_registry_impl::god_definition_at_runtime_index(i);
        for (const auto &effect : definition->effects) if (effect.trigger == trigger) return true;
    }
    return false;
}
bool city_gods_have_effects()
{
    return city_gods_have_effects(religion::Trigger::Blessing) || city_gods_have_effects(religion::Trigger::MinorCurse) || city_gods_have_effects(religion::Trigger::MajorCurse);
}
static void perform_effect(god_type god, religion::Trigger trigger)
{
    const auto *definition = building_type_registry_impl::find_god_definition(god);
    if (!definition) return;
    auto context = religion::context_for_god(god);
    for (const auto &effect : definition->effects) {
        if (effect.trigger == trigger && !effect.automatic_only) religion::apply_effect(effect, context, false);
    }
}

static void update_god_moods(void)
{
    for (int i = 0; i < city_gods_count(); i++) {
        god_status *god = &city_data.religion.gods[i];
        const auto *definition = building_type_registry_impl::find_god_definition(static_cast<god_type>(i));
        if (!definition) continue;
        const int neutral = definition->wrath.neutral;
        if (god->happiness < god->target_happiness) {
            god->happiness++;
        } else if (god->happiness > god->target_happiness) {
            god->happiness--;
        }
        if (scenario_is_tutorial_1()) {
            if (god->happiness < 50) {
                god->happiness = 50;
            }
        }
        if (god->happiness > neutral) {
            god->small_curse_done = 0;
        }
        if (god->happiness < neutral) {
            god->blessing_done = 0;
        }
    }

    int god_id = random_byte() & 7;
    if (god_id < city_gods_count()) {
        god_status *god = &city_data.religion.gods[god_id];
        const auto *definition = building_type_registry_impl::find_god_definition(static_cast<god_type>(god_id));
        if (!definition) return;
        const auto &rules = definition->favor;
        const auto &wrath = definition->wrath;
        if (god->happiness >= wrath.neutral) {
            god->wrath_bolts = 0;
        } else if (god->happiness < wrath.threshold) {
            if (wrath.favor_decay && god->happy_bolts > 0) {
                god->happy_bolts = static_cast<int8_t>(std::max(0, god->happy_bolts - wrath.favor_decay));
            } else {
                const int amount = god->happiness >= wrath.mild_threshold ? wrath.mild_amount : god->happiness >= wrath.severe_threshold ? wrath.moderate_amount : wrath.severe_amount;
                god->wrath_bolts = static_cast<int8_t>(std::min(wrath.maximum, god->wrath_bolts + amount));
            }
        }
        god->wrath_bolts = static_cast<int8_t>(std::min(wrath.maximum, static_cast<int>(god->wrath_bolts)));
        if (rules.enabled && god->happiness >= rules.neutral) {
            int chance_for_happy_bolt = std::max(0, god->happiness - rules.neutral) / rules.happiness_divisor + rules.base_chance;
            if (god->months_since_festival <= rules.festival_months) {
                chance_for_happy_bolt += (rules.festival_months - god->months_since_festival) / rules.festival_divisor + rules.base_chance;
            }
            random_generate_next();
            int roll = random_short_alt() % 100;
            if (roll < chance_for_happy_bolt) {
                god->happy_bolts++;
            }
            if (god->happy_bolts > rules.maximum) {
                god->happy_bolts = static_cast<int8_t>(rules.maximum);
            }
        }
    }
    if (game_time_day() != 0) {
        return;
    }

    // handle blessings, curses, etc every month
    for (int i = 0; i < city_gods_count(); i++) {
        city_data.religion.gods[i].months_since_festival++;
    }
    if (god_id >= city_gods_count()) {
        if (city_gods_calculate_least_happy()) {
            god_id = city_data.religion.least_happy_god - 1;
        }
    }
    if (god_id < city_gods_count()) {
        const auto *definition = building_type_registry_impl::find_god_definition(static_cast<god_type>(god_id));
        if (definition) {
            auto context = religion::context_for_god(god_id);
            for (const auto &effect : definition->effects) {
                if (!effect.matches(context)) continue;
                religion::apply_effect(effect, context, true);
                if (effect.stop_update) return;
                break;
            }
        }
    }
    if (!city_gods_have_effects(religion::Trigger::MinorCurse) && !city_gods_have_effects(religion::Trigger::MajorCurse)) return;

    int min_happiness = 100;
    for (int i = 0; i < city_gods_count(); i++) {
        if (city_data.religion.gods[i].happiness < min_happiness) {
            min_happiness = city_data.religion.gods[i].happiness;
        }
    }
    if (city_data.religion.angry_message_delay) {
        city_data.religion.angry_message_delay--;
    } else if (min_happiness < 30) {
        city_data.religion.angry_message_delay = 20;
        if (min_happiness < 10) {
            city_message_post(0, MESSAGE_GODS_WRATHFUL, 0, 0);
        } else {
            city_message_post(0, MESSAGE_GODS_UNHAPPY, 0, 0);
        }
    }
}

void city_gods_calculate_moods(int update_moods)
{
    // base happiness: percentage of houses covered
    for (int i = 0; i < city_gods_count(); i++) {
        city_data.religion.gods[i].target_happiness =
            static_cast<int8_t>(city_culture_coverage_religion(static_cast<god_type>(i)));
    }

    int max_temples = 0;
    int max_god = TIE;
    int min_temples = 100000;
    int min_god = TIE;
    for (int i = 0; i < city_gods_count(); i++) {
        int num_temples = temple_count_for_god(static_cast<god_type>(i));
        if (num_temples == max_temples) {
            max_god = TIE;
        } else if (num_temples > max_temples) {
            max_temples = num_temples;
            max_god = i;
        }
        if (num_temples == min_temples) {
            min_god = TIE;
        } else if (num_temples < min_temples) {
            min_temples = num_temples;
            min_god = i;
        }
    }
    // happiness factor based on months since festival (max 40)
    for (int i = 0; i < city_gods_count(); i++) {
        int festival_penalty = city_data.religion.gods[i].months_since_festival;
        if (festival_penalty > 40) {
            festival_penalty = 40;
        }
        city_data.religion.gods[i].target_happiness =
            static_cast<int8_t>(city_data.religion.gods[i].target_happiness + 12 - festival_penalty);
    }

    if (!(config_get(CONFIG_GP_CH_JEALOUS_GODS))) {
        if (max_god < city_gods_count()) {
            if (city_data.religion.gods[max_god].target_happiness >= 50) {
                city_data.religion.gods[max_god].target_happiness = 100;
            } else {
                city_data.religion.gods[max_god].target_happiness += 50;
            }
        }
        if (min_god < city_gods_count()) {
            city_data.religion.gods[min_god].target_happiness -= 25;
        }
    }
    int min_happiness;
    if (city_data.population.population < 100) {
        min_happiness = 50;
    } else if (city_data.population.population < 200) {
        min_happiness = 40;
    } else if (city_data.population.population < 300) {
        min_happiness = 30;
    } else if (city_data.population.population < 400) {
        min_happiness = 20;
    } else if (city_data.population.population < 500) {
        min_happiness = 10;
    } else {
        min_happiness = 0;
    }
    for (int i = 0; i < city_gods_count(); i++) {
        city_data.religion.gods[i].target_happiness =
            static_cast<int8_t>(calc_bound(city_data.religion.gods[i].target_happiness, min_happiness, 100));
    }
    if (update_moods) {
        update_god_moods();
    }
}

int city_gods_calculate_least_happy(void)
{
    int max_god = 0;
    int max_wrath = 0;
    for (int i = 0; i < city_gods_count(); i++) {
        if (city_data.religion.gods[i].wrath_bolts > max_wrath) {
            max_god = i + 1;
            max_wrath = city_data.religion.gods[i].wrath_bolts;
        }
    }
    if (max_god > 0) {
        city_data.religion.least_happy_god = max_god;
        return 1;
    }
    int min_happiness = 40;
    for (int i = 0; i < city_gods_count(); i++) {
        if (city_data.religion.gods[i].happiness < min_happiness) {
            max_god = i + 1;
            min_happiness = city_data.religion.gods[i].happiness;
        }
    }
    city_data.religion.least_happy_god = max_god;
    return max_god > 0;
}

void city_god_change_happiness(int god_id, int amount)
{
    if (god_id < 0) {
        return;
    } else if (god_id == GOD_ALL) {
        for (int i = 0; i < city_gods_count(); i++) {
            city_data.religion.gods[i].happiness =
                static_cast<int8_t>(calc_bound(amount + city_data.religion.gods[i].happiness, 0, 100));
        }
    } else {
        city_data.religion.gods[god_id].happiness =
            static_cast<int8_t>(calc_bound(amount + city_data.religion.gods[god_id].happiness, 0, 100));
    }
}

void city_god_set_happiness(int god_id, int amount_set)
{
    if (god_id < 0) {
        return;
    } else if (god_id == GOD_ALL) {
        for (int i = 0; i < city_gods_count(); i++) {
            city_data.religion.gods[i].happiness =
                static_cast<int8_t>(calc_bound(amount_set, 0, 100));
        }
    } else {
        city_data.religion.gods[god_id].happiness =
            static_cast<int8_t>(calc_bound(amount_set, 0, 100));
    }
}

int city_god_happiness(int god_id)
{
    return city_data.religion.gods[god_id].happiness;
}

int city_god_wrath_bolts(int god_id)
{
    return city_data.religion.gods[god_id].wrath_bolts;
}

int city_god_happy_bolts(int god_id)
{
    return city_data.religion.gods[god_id].happy_bolts;
}

int city_god_months_since_festival(int god_id)
{
    return city_data.religion.gods[god_id].months_since_festival;
}

int city_god_least_happy(void)
{
    return city_data.religion.least_happy_god - 1;
}

int city_god_spirit_of_mars_power(void)
{
    return city_data.religion.mars_spirit_power;
}

void city_god_spirit_of_mars_mark_used(void)
{
    city_data.religion.mars_spirit_power = 0;
}

int city_god_neptune_create_shipwreck_flotsam(void)
{
    if (city_data.religion.neptune_sank_ships) {
        city_data.religion.neptune_sank_ships = 0;
        return 1;
    } else {
        return 0;
    }
}

int city_god_venus_bonus_employment(void)
{
    if (city_data.religion.venus_blessing_months_left > 0 && city_data.religion.employment_months_per_point > 0) {
        return city_data.religion.venus_blessing_months_left / city_data.religion.employment_months_per_point + city_data.religion.employment_base_bonus;
    } else {
        return 0;
    }
}

void city_god_blessing(int god_id)
{
    if (god_id == GOD_ALL) {
        for (int i = 0; i < city_gods_count(); i++) {
            perform_effect(static_cast<god_type>(i), religion::Trigger::Blessing);
        }
    } else {
        perform_effect(static_cast<god_type>(god_id), religion::Trigger::Blessing);
    }

}

void city_god_curse(int god_id, int is_major)
{
    if (god_id == GOD_ALL) {
        for (int i = 0; i < city_gods_count(); i++) {
            if (is_major) {
                perform_effect(static_cast<god_type>(i), religion::Trigger::MajorCurse);
            } else {
                perform_effect(static_cast<god_type>(i), religion::Trigger::MinorCurse);
            }
        }
    } else {
        if (is_major) {
            perform_effect(static_cast<god_type>(god_id), religion::Trigger::MajorCurse);
        } else {
            perform_effect(static_cast<god_type>(god_id), religion::Trigger::MinorCurse);
        }
    }
}
