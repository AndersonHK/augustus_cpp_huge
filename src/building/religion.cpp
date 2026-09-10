#include "building/religion.h"

#include "building/god_id_bridge.h"
#include "city/god.h"

#include <utility>
#include "building/religion_effects.h"
#include "core/xml_value.h"
#include "game/resource.h"
#include <algorithm>
#include <stdexcept>

namespace building_type_registry_impl {

void ReligionPresentation::set_sound(std::string value)
{
    sound_ = std::move(value);
    fields_ |= FieldSound;
}

void ReligionPresentation::set_name_key(translation_key value)
{
    name_key_ = std::move(value);
    fields_ |= FieldName;
}

void ReligionPresentation::set_bonus_key(translation_key value)
{
    bonus_key_ = std::move(value);
    fields_ |= FieldBonus;
}

void ReligionPresentation::set_quote_key(translation_key value)
{
    quote_key_ = std::move(value);
    fields_ |= FieldQuote;
}

void ReligionPresentation::set_banner_group(std::string value)
{
    banner_group_ = std::move(value);
    fields_ |= FieldBannerGroup;
}

void ReligionPresentation::set_banner_image(std::string value)
{
    banner_image_ = std::move(value);
    fields_ |= FieldBannerImage;
}

void ReligionPresentation::set_content_y_offset(int value)
{
    content_y_offset_ = value;
    fields_ |= FieldContentYOffset;
}

void ReligionPresentation::set_height_blocks(int value)
{
    height_blocks_ = value;
    fields_ |= FieldHeightBlocks;
}

void ReligionPresentation::set_module_index(int value)
{
    module_index_ = value;
    fields_ |= FieldModuleIndex;
}

const char *ReligionPresentation::sound() const
{
    return sound_.c_str();
}

translation_key ReligionPresentation::name_key() const
{
    return name_key_;
}

translation_key ReligionPresentation::bonus_key() const
{
    return bonus_key_;
}

translation_key ReligionPresentation::quote_key() const
{
    return quote_key_;
}

const char *ReligionPresentation::banner_group() const
{
    return banner_group_.c_str();
}

const char *ReligionPresentation::banner_image() const
{
    return banner_image_.c_str();
}

int ReligionPresentation::content_y_offset() const
{
    return content_y_offset_;
}

int ReligionPresentation::height_blocks() const
{
    return height_blocks_;
}

int ReligionPresentation::module_index() const
{
    return module_index_;
}

int ReligionPresentation::has_any() const
{
    return fields_ != 0;
}

int ReligionPresentation::is_complete() const
{
    return (fields_ & FieldComplete) == FieldComplete &&
        !sound_.empty() && name_key_ && bonus_key_ && quote_key_ &&
        !banner_group_.empty() && !banner_image_.empty() &&
        height_blocks_ > 0 && module_index_ >= 0;
}

Religion::Religion(std::string path)
    : path_(std::move(path))
{
}

const char *Religion::path() const
{
    return path_.c_str();
}

void Religion::add_god(const ::God *god)
{
    if (god) {
        gods_.push_back(god);
    }
}

void Religion::set_all_gods()
{
    all_gods_ = 1;
}

int Religion::has_all_gods() const
{
    return all_gods_;
}

const std::vector<const ::God *> &Religion::gods() const
{
    return gods_;
}

int Religion::has_god(god_type god) const
{
    return has_god_runtime_id(god_id_bridge_runtime_from_legacy(god));
}

int Religion::has_god_runtime_id(int runtime_id) const
{
    if (all_gods_) {
        return 1;
    }
    if (runtime_id < 0) {
        return 0;
    }
    for (const ::God *current : gods_) {
        if (current && current->runtime_id() == runtime_id) {
            return 1;
        }
    }
    return 0;
}

void Religion::set_tier(ReligionTier tier)
{
    tier_ = tier;
}

ReligionTier Religion::tier() const
{
    return tier_;
}

int Religion::is_tier(ReligionTier tier) const
{
    return tier_ == tier;
}

void Religion::set_capacity(int capacity)
{
    capacity_ = capacity;
}

int Religion::capacity() const
{
    return capacity_;
}

ReligionPresentation &Religion::presentation()
{
    return presentation_;
}

const ReligionPresentation &Religion::presentation() const
{
    return presentation_;
}

} // namespace building_type_registry_impl


namespace religion {
namespace {
struct Parameter { const char *name; int minimum; int maximum; };
struct Schema { const char *name; Callback callback; std::vector<Parameter> parameters; };
const Schema schemas[] = {
    {"message", Callback::Message, {{"id", 1, 1000}}},
    {"farm_harvest", Callback::FarmHarvest, {{"days", 0, 255}}},
    {"farm_drought", Callback::FarmDrought, {{"days", 0, 255}}},
    {"workshop_inputs", Callback::WorkshopInputs, {{"batches", 0, 100}}},
    {"trade_bonus", Callback::TradeBonus, {{"months", 0, 1200}, {"percent", 0, 1000}}},
    {"military_protection", Callback::MilitaryProtection, {{"power", 0, 1000}}},
    {"sentiment", Callback::Sentiment, {{"amount", -100, 100}}},
    {"rejuvenate", Callback::Rejuvenate, {{"years", 1, 25}, {"minimum_age", 25, 74}}},
    {"employment", Callback::Employment, {{"months", 0, 1200}, {"months_per_point", 1, 1200}, {"base_bonus", 0, 100}}},
    {"sink_ships", Callback::SinkShips, {{"percent", 0, 100}}},
    {"trade_disruption", Callback::TradeDisruption, {{"days", 0, 10000}}},
    {"storage_loss", Callback::StorageLoss, {{"loads", 0, 10000}}},
    {"storage_fire", Callback::StorageFire, {}},
    {"local_invasion", Callback::LocalInvasion, {{"minimum", 0, 1000}, {"maximum", 0, 1000}, {"message", 1, 1000}}},
    {"legion_disband", Callback::LegionDisband, {{"months", 0, 255}}},
    {"health", Callback::Health, {{"amount", -100, 100}}},
    {"happiness_cap", Callback::HappinessCap, {{"amount", 0, 100}}},
    {"happiness_change", Callback::HappinessChange, {{"amount", -100, 100}}},
    {"disease", Callback::Disease, {{"active", 0, 1}}},
    {"refresh_sentiment", Callback::RefreshSentiment, {}},
    {"god_happiness", Callback::GodHappiness, {{"amount", -100, 100}}},
    {"favor", Callback::Favor, {{"amount", 0, 100}}},
    {"wrath", Callback::Wrath, {{"amount", 0, 100}}},
    {"minor_curse_done", Callback::MinorCurseDone, {{"value", 0, 1}}},
    {"blessing_done", Callback::BlessingDone, {{"value", 0, 1}}},
    {"granary_fill", Callback::GranaryFill, {{"loads", 0, 1000}}},
    {"trade_bonus_until_year_end", Callback::TradeBonusYearEnd, {{"percent", 0, 1000}}}
};
int required_integer(const mod_content::Node &node, const char *name, int minimum, int maximum)
{
    int value = 0;
    if (!xml_value::parse_int_strict(node.attribute(name).c_str(), &value) || value < minimum || value > maximum) {
        throw std::runtime_error("Invalid religion parameter " + node.name + "." + name);
    }
    return value;
}
void attributes(const mod_content::Node &node, const std::vector<std::string> &allowed)
{
    for (const auto &entry : node.attributes) {
        if (std::find(allowed.begin(), allowed.end(), entry.first) == allowed.end()) throw std::runtime_error("Unknown religion attribute " + entry.first);
    }
    if (node.text.find_first_not_of(" \n\r\t") != std::string::npos) throw std::runtime_error("Unexpected religion text");
}
Condition parse_condition(const mod_content::Node &node)
{
    static const char *names[] = {"happiness", "favor", "wrath", "festival_months", "blessing_done", "minor_curse_done", "health", "sea_trade", "sea_routes", "campaign_rank", "campaign_mission", "invasions", "result"};
    attributes(node, {"metric", "min", "max"});
    if (!node.children.empty()) throw std::runtime_error("Condition cannot contain children");
    const auto metric = node.attribute("metric");
    for (size_t i = 0; i < std::size(names); ++i) {
        if (metric != names[i]) continue;
        Condition result{static_cast<Metric>(i)};
        if (node.attributes.count("min")) result.minimum = required_integer(node, "min", -10000, 10000);
        if (node.attributes.count("max")) result.maximum = required_integer(node, "max", -10000, 10000);
        if (result.minimum > result.maximum || (!node.attributes.count("min") && !node.attributes.count("max"))) throw std::runtime_error("Empty religion condition range");
        return result;
    }
    throw std::runtime_error("Unknown religion condition " + metric);
}
Action parse_action(const mod_content::Node &node)
{
    const auto name = node.attribute("callback");
    for (const auto &schema : schemas) {
        if (name != schema.name) continue;
        Action action{};
        action.callback = schema.callback;
        std::vector<std::string> allowed{"callback", "record_result"};
        for (size_t i = 0; i < schema.parameters.size(); ++i) {
            const auto &parameter = schema.parameters[i];
            allowed.push_back(parameter.name);
            action.arguments[i] = required_integer(node, parameter.name, parameter.minimum, parameter.maximum);
        }
        if (schema.callback == Callback::Rejuvenate && action.arguments[1] + 2 * action.arguments[0] > 100) throw std::runtime_error("Rejuvenation range exceeds the population census");
        if (schema.callback == Callback::LocalInvasion && action.arguments[0] > action.arguments[1]) throw std::runtime_error("Invasion range is reversed");
        attributes(node, allowed);
        if (node.attributes.count("record_result")) {
            int value = 0;
            if (!xml_value::parse_bool(node.attribute("record_result").c_str(), &value)) throw std::runtime_error("Invalid record_result");
            action.record_result = value != 0;
        }
        for (const auto &child : node.children) {
            if (child.name == "condition") action.conditions.push_back(parse_condition(child));
            else if (child.name == "resource" && schema.callback == Callback::GranaryFill) {
                attributes(child, {"type"});
                const auto resource = resource_type_from_text_id(child.attribute("type").c_str());
                if (!child.children.empty() || resource <= RESOURCE_NONE || !resource_is_food(resource)) throw std::runtime_error("Unknown or non-food granary blessing resource");
                if (std::find(action.resources.begin(), action.resources.end(), resource) != action.resources.end()) throw std::runtime_error("Duplicate granary blessing resource");
                action.resources.push_back(resource);
            } else throw std::runtime_error("Unknown religion action child");
        }
        if (schema.callback == Callback::GranaryFill && action.resources.empty()) throw std::runtime_error("Granary fill requires explicit resources");
        return action;
    }
    throw std::runtime_error("Unknown religion callback " + name);
}
}
bool Condition::matches(const Context &context) const
{
    const int value = context.values[static_cast<size_t>(metric)];
    return value >= minimum && value <= maximum;
}
bool Effect::matches(const Context &context) const
{
    return std::all_of(conditions.begin(), conditions.end(), [&](const Condition &condition) { return condition.matches(context); });
}
void execute(const std::vector<Action> &actions, Context &context, const std::array<EffectCallback, static_cast<size_t>(Callback::Count)> &callbacks)
{
    for (const auto &action : actions) {
        if (!std::all_of(action.conditions.begin(), action.conditions.end(), [&](const Condition &condition) { return condition.matches(context); })) continue;
        const auto callback = callbacks.at(static_cast<size_t>(action.callback));
        if (!callback) throw std::runtime_error("Unbound religion callback");
        const bool result = callback(context, action);
        if (action.record_result) context.values[static_cast<size_t>(Metric::Result)] = result;
    }
}
Effect parse_effect(const mod_content::Node &node, Trigger trigger)
{
    attributes(node, {"automatic_only", "stop_update"});
    Effect effect{}; effect.trigger = trigger;
    for (const auto &entry : node.attributes) {
        int flag = 0;
        if (!xml_value::parse_bool(entry.second.c_str(), &flag)) throw std::runtime_error("Invalid religion effect flag");
        if (entry.first == "automatic_only") effect.automatic_only = flag != 0;
        if (entry.first == "stop_update") effect.stop_update = flag != 0;
    }
    bool saw_trigger = false;
    for (const auto &child : node.children) {
        if (child.name == "condition") effect.conditions.push_back(parse_condition(child));
        else if (child.name == "action") effect.actions.push_back(parse_action(child));
        else if (child.name == "on_trigger" && !saw_trigger) {
            saw_trigger = true; attributes(child, {});
            for (const auto &action : child.children) {
                if (action.name != "action") throw std::runtime_error("on_trigger only accepts actions");
                effect.on_trigger.push_back(parse_action(action));
            }
        } else throw std::runtime_error("Unknown religion effect element " + child.name);
    }
    if (effect.actions.empty()) throw std::runtime_error("Religion effect has no actions");
    return effect;
}
} // namespace religion
