#pragma once

#include "game/mod_content.h"
#include "core/buffer.h"
#include <array>
#include <limits>
#include <vector>

namespace religion {

enum class Trigger { Blessing, MinorCurse, MajorCurse };
enum class Metric { Happiness, Favor, Wrath, FestivalMonths, BlessingDone, MinorCurseDone, Health, SeaTrade, SeaRoutes, CampaignRank, CampaignMission, Invasions, Result, Count };
enum class Callback {
    Message, FarmHarvest, FarmDrought, WorkshopInputs, TradeBonus, MilitaryProtection,
    Sentiment, Rejuvenate, Employment, SinkShips, TradeDisruption, StorageLoss,
    StorageFire, LocalInvasion, LegionDisband, Health, HappinessCap, HappinessChange,
    Disease, RefreshSentiment, GodHappiness, Favor, Wrath, MinorCurseDone, BlessingDone, GranaryFill, TradeBonusYearEnd, Count
};

struct Context {
    std::array<int, static_cast<size_t>(Metric::Count)> values{};
    int god = -1;
};

struct Condition {
    Metric metric;
    int minimum = std::numeric_limits<int>::min();
    int maximum = std::numeric_limits<int>::max();
    bool matches(const Context &context) const;
};

struct Action {
    Callback callback;
    std::array<int, 3> arguments{};
    std::vector<Condition> conditions;
    bool record_result = false;
    std::vector<int> resources;
    int operator[](size_t index) const { return arguments[index]; }
};

struct Effect {
    Trigger trigger;
    bool automatic_only = false;
    bool stop_update = false;
    std::vector<Condition> conditions;
    std::vector<Action> on_trigger;
    std::vector<Action> actions;
    bool matches(const Context &context) const;
};

// Bound callback identities and numeric arguments only; no XML or string lookup during execution.
using EffectCallback = bool (*)(Context &, const Action &);
void execute(const std::vector<Action> &actions, Context &context, const std::array<EffectCallback, static_cast<size_t>(Callback::Count)> &callbacks);
Effect parse_effect(const mod_content::Node &node, Trigger trigger);

struct WrathRules {
    int neutral = 0, threshold = 0, mild_threshold = 0, severe_threshold = 0;
    int mild_amount = 0, moderate_amount = 0, severe_amount = 0, maximum = 0, favor_decay = 0;
};

struct FavorRules {
    bool enabled = false;
    int neutral = 0, happiness_divisor = 1, base_chance = 0;
    int festival_months = 0, festival_divisor = 1, maximum = 0;
};

Context context_for_god(int legacy_slot);
void apply_effect(const Effect &effect, Context &context, bool automatic);
void save_state(buffer *buf);
bool load_state(buffer *buf);
void import_original_effects();

} // namespace religion
