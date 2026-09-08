#include "building/religion_effects.h"
#include "building/building_type_registry_internal.h"
#include "building/count.h"
#include "building/granary.h"
#include "building/industry.h"
#include "city/data_private.h"
#include "city/god.h"
#include "city/health.h"
#include "city/message.h"
#include "city/population.h"
#include "city/sentiment.h"
#include "city/trade.h"
#include "core/random.h"
#include "core/buffer.h"
#include "core/Logger.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include "figure/formation_legion.h"
#include "figuretype/water.h"
#include "game/campaign.h"
#include "game/time.h"
#include "scenario/invasion.h"
#include "scenario/property.h"

namespace religion {
namespace {
using Args = const Action &;
bool message(Context &, Args a) { city_message_post(1, a[0], 0, 0); return true; }
bool farm_harvest(Context &, Args a) { building_bless_farms(a[0]); return true; }
bool farm_drought(Context &, Args a) { building_curse_farms(a[0]); return true; }
bool workshop_inputs(Context &, Args a) { building_bless_industry(a[0]); return true; }
// These are effect channels, not deity identities. Keep their historical serialized
// fields so active effects in existing cities survive this definition migration.
bool trade_bonus(Context &, Args a) { city_data.religion.neptune_trade_bonus_active = a[0]; city_data.religion.trade_bonus_percent = a[1]; return true; }
bool military_protection(Context &, Args a) { city_data.religion.mars_spirit_power = a[0]; return true; }
bool sentiment(Context &, Args a) { city_data.sentiment.blessing_festival_boost = static_cast<int16_t>(std::clamp(city_data.sentiment.blessing_festival_boost + a[0], -32768, 32767)); return true; }
bool rejuvenate(Context &, Args a) { city_population_rejuvenate(a[0], a[1]); return true; }
bool employment(Context &, Args a) { city_data.religion.venus_blessing_months_left = a[0]; city_data.religion.employment_months_per_point = a[1]; city_data.religion.employment_base_bonus = a[2]; return true; }
bool sink_ships(Context &, Args a) { figure_sink_ships(a[0]); city_data.religion.neptune_sank_ships = 1; return true; }
bool trade_disruption(Context &, Args a) { city_trade_start_sea_trade_problems(a[0]); return true; }
bool storage_loss(Context &, Args a) { building_granary_warehouse_curse(0, a[0]); return true; }
bool storage_fire(Context &, Args) { building_granary_warehouse_curse(1, 0); return true; }
bool local_invasion(Context &, Args a) { return scenario_invasion_start_local(a[0] == a[1] ? a[0] : random_between_from_stdlib(a[0], a[1]), a[2]) != 0; }
bool legion_disband(Context &, Args a) { return formation_legion_curse(a[0]) != 0; }
bool health(Context &, Args a) { city_health_change(a[0]); return true; }
bool happiness_cap(Context &, Args a) { city_sentiment_set_max_happiness(a[0]); return true; }
bool happiness_change(Context &, Args a) { city_sentiment_change_happiness(a[0]); return true; }
bool disease(Context &, Args a) { city_data.religion.venus_curse_active = a[0]; return true; }
bool refresh_sentiment(Context &, Args) { city_sentiment_update(); return true; }
bool god_happiness(Context &c, Args a) { city_god_change_happiness(c.god, a[0]); return true; }
bool favor(Context &c, Args a) { city_data.religion.gods[c.god].happy_bolts = static_cast<int8_t>(a[0]); return true; }
bool wrath(Context &c, Args a) { city_data.religion.gods[c.god].wrath_bolts = static_cast<int8_t>(a[0]); return true; }
bool minor_curse_done(Context &c, Args a) { city_data.religion.gods[c.god].small_curse_done = static_cast<int8_t>(a[0]); return true; }
bool blessing_done(Context &c, Args a) { city_data.religion.gods[c.god].blessing_done = static_cast<int8_t>(a[0]); return true; }
bool granary_fill(Context &, Args a) { building_granary_fill_least_stocked(a[0], a.resources); return true; }
bool trade_bonus_year_end(Context &, Args a) { city_data.religion.neptune_trade_bonus_active = 12 - game_time_month(); city_data.religion.trade_bonus_percent = a[0]; return true; }
const std::array<EffectCallback, static_cast<size_t>(Callback::Count)> callbacks = {
    message, farm_harvest, farm_drought, workshop_inputs, trade_bonus, military_protection,
    sentiment, rejuvenate, employment, sink_ships, trade_disruption, storage_loss,
    storage_fire, local_invasion, legion_disband, health, happiness_cap, happiness_change,
    disease, refresh_sentiment, god_happiness, favor, wrath, minor_curse_done, blessing_done, granary_fill, trade_bonus_year_end
};
}
Context context_for_god(int legacy_slot)
{
    Context context;
    context.god = legacy_slot;
    const auto &god = city_data.religion.gods[legacy_slot];
    auto set = [&](Metric metric, int value) { context.values[static_cast<size_t>(metric)] = value; };
    set(Metric::Happiness, god.happiness); set(Metric::Favor, god.happy_bolts);
    set(Metric::Wrath, god.wrath_bolts); set(Metric::FestivalMonths, god.months_since_festival);
    set(Metric::BlessingDone, god.blessing_done);
    set(Metric::MinorCurseDone, god.small_curse_done); set(Metric::Health, city_data.health.value);
    bool sea_trade = city_data.trade.num_sea_routes > 0;
    for (const char *id : {"shipyard", "wharf"}) {
        const auto type = building_type_registry_impl::type_from_attr(id);
        sea_trade |= type > BUILDING_NONE && building_count_active(type) > 0;
    }
    set(Metric::SeaTrade, sea_trade);
    set(Metric::SeaRoutes, city_data.trade.num_sea_routes);
    set(Metric::CampaignRank, game_campaign_is_original() ? scenario_campaign_rank() : 10000);
    set(Metric::CampaignMission, game_campaign_is_original() ? scenario_campaign_mission() : -1);
    set(Metric::Invasions, scenario_invasion_count_total());
    return context;
}
void apply_effect(const Effect &effect, Context &context, bool automatic)
{
    if (automatic) execute(effect.on_trigger, context, callbacks);
    execute(effect.actions, context, callbacks);
}

void save_state(buffer *buf)
{
    buffer_write_i32(buf, city_data.religion.trade_bonus_percent);
    buffer_write_i32(buf, city_data.religion.employment_months_per_point);
    buffer_write_i32(buf, city_data.religion.employment_base_bonus);
}
bool load_state(buffer *buf)
{
    // Older archives have the timers, but their effect magnitudes were implicit.
    auto &state = city_data.religion;
    state.trade_bonus_percent = 50; state.employment_months_per_point = 12; state.employment_base_bonus = 1;
    if (!buf) return true;
    state.trade_bonus_percent = buffer_read_i32(buf);
    state.employment_months_per_point = buffer_read_i32(buf);
    state.employment_base_bonus = buffer_read_i32(buf);
    if (state.trade_bonus_percent < 0 || state.trade_bonus_percent > 1000) {
        Logger::warning("Repairing invalid active trade blessing percentage", nullptr, state.trade_bonus_percent);
        state.trade_bonus_percent = 50;
    }
    if (state.employment_months_per_point < 0 || state.employment_months_per_point > 1200 || (state.venus_blessing_months_left > 0 && !state.employment_months_per_point)) {
        Logger::warning("Repairing invalid active employment blessing divisor", nullptr, state.employment_months_per_point);
        state.employment_months_per_point = 12;
    }
    if (state.employment_base_bonus < 0 || state.employment_base_bonus > 100) {
        Logger::warning("Repairing invalid active employment blessing base", nullptr, state.employment_base_bonus);
        state.employment_base_bonus = 1;
    }
    return true;
}

void import_original_effects()
{
    // C3/Julius stored a year-end expiry flag, not a remaining-month count.
    if (city_data.religion.neptune_trade_bonus_active > 0) {
        city_data.religion.neptune_trade_bonus_active = 12 - game_time_month();
        city_data.religion.trade_bonus_percent = 100;
    }
}
} // namespace religion
