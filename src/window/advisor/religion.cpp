#include "religion.h"
#include "building/building.h"
#include "building/count.h"
#include "building/building_type_registry_internal.h"
#include "building/god_registry.h"
#include "city/god.h"
#include "city/festival.h"
#include "city/houses.h"
#include "graphics/declarative_window.h"
#include "graphics/lang_text.h"
#include "graphics/window.h"
#include "window/epithets.h"
#include "window/hold_festival.h"
#include "game/settings.h"
#include <memory>
#include <algorithm>
#include <vector>

using namespace building_type_registry_impl;

static int get_religion_advice(void)
{
    int least_happy = city_god_least_happy();
    const auto *least_happy_definition = find_god_definition_by_runtime_id(least_happy);
    const int legacy_advice = least_happy_definition && least_happy_definition->legacy_type() >= 0 && least_happy_definition->legacy_type() < GOD_ALL ? 6 + least_happy_definition->legacy_type() : 5;
    const house_demands *demands = city_houses_demands();
    if (least_happy >= 0 && city_god_wrath_bolts(least_happy) > 4) {
        return legacy_advice;
    } else if (demands->religion == 1) {
        return demands->requiring.religion ? 1 : 0;
    } else if (demands->religion == 2) {
        return 2;
    } else if (demands->religion == 3) {
        return 3;
    } else if (!demands->requiring.religion) {
        return 4;
    } else if (least_happy >= 0) {
        return legacy_advice;
    } else {
        return 5;
    }
}

static int get_festival_advice(void)
{
    int months_since_festival = city_festival_months_since_last();
    if (months_since_festival <= 1) {
        return 0;
    } else if (months_since_festival <= 6) {
        return 1;
    } else if (months_since_festival <= 12) {
        return 2;
    } else if (months_since_festival <= 18) {
        return 3;
    } else if (months_since_festival <= 24) {
        return 4;
    } else if (months_since_festival <= 30) {
        return 5;
    } else {
        return 6;
    }
}

namespace {
std::string translated(const char *key) { return reinterpret_cast<const char *>(translation_for_key(key)); }

int count_temples(const God *god, ReligionTier tier, bool active)
{
    int count = 0;
    for (const auto &type : g_building_types) {
        const auto *religion = type ? type->religion() : nullptr;
        if (!religion || religion->tier() != tier) continue;
        const bool matches = god ? !religion->has_all_gods() && religion->has_god_runtime_id(god->runtime_id()) : religion->has_all_gods();
        if (matches) count += active ? building_count_active(type->type()) : building_count_total(type->type());
    }
    return count;
}

class ReligionAdvisorController final : public DeclarativeWindowController {
public:
    std::vector<const God *> gods;
    const God *god_at(int index) const { return index >= 0 && index < static_cast<int>(gods.size()) ? gods[index] : nullptr; }
    int page = 0;
    int capacity = 6;
    int repeat_count(std::string_view source) const override { return source == "religion.gods" ? std::max(0, std::min(capacity, god_definition_count() + 1 - page * capacity)) : 0; }
    std::string text(std::string_view binding, int index) const override
    {
        if (binding == "religion.advice") return translated(current_string_key(59, 21 + get_religion_advice()).c_str());
        if (binding == "festival.advice") return translated(current_string_key(58, 18 + get_festival_advice()).c_str());
        if (binding == "festival.elapsed") return std::to_string(city_festival_months_since_last()) + " " + translated(current_string_amount_key(8, 4, city_festival_months_since_last()).c_str()) + " " + translated("main_strings.58.15");
        index += page * capacity;
        if (index < 0 || index > god_definition_count()) return {};
        const God *god = god_at(index);
        const int legacy = god ? god->legacy_type() : -1;
        const int id = god ? god->runtime_id() : -1;
        if (binding == "god.name") return god ? (legacy >= 0 && legacy < GOD_ALL ? translated(current_string_key(59, 11 + legacy).c_str()) : god->path()) : translated("main_strings.59.8");
        if (binding == "god.domain" && god && legacy >= 0 && legacy < GOD_ALL) return translated(current_string_key(59, 16 + legacy).c_str());
        if (binding == "god.altars") return std::to_string(count_temples(god, ReligionTier::Shrine, false));
        if (binding == "god.small") return std::to_string(count_temples(god, ReligionTier::Small, true) + (!god ? count_temples(nullptr, ReligionTier::Oracle, true) : 0));
        if (binding == "god.large") return std::to_string(count_temples(god, ReligionTier::Large, true) + count_temples(god, ReligionTier::Grand, true));
        if (binding == "god.small_total") return std::to_string(count_temples(god, ReligionTier::Small, false) + (!god ? count_temples(nullptr, ReligionTier::Oracle, false) : 0));
        if (binding == "god.large_total") return god ? std::to_string(count_temples(god, ReligionTier::Large, false)) : "";
        if (binding == "god.wrath" && god) { const int bolts = city_god_wrath_bolts(id) / 10; return bolts ? std::to_string(bolts) : ""; }
        if (binding == "god.festival" && god) return std::to_string(city_god_months_since_festival(id));
        if (binding == "god.mood" && god) return translated(current_string_key(59, 32 + city_god_happiness(id) / 10).c_str());
        if (binding == "god.bolts" && god) {
            const int wrath = city_god_wrath_bolts(id) / 10;
            const int happiness = city_god_happy_bolts(id);
            return wrath || happiness ? std::to_string(wrath ? wrath : happiness) : "";
        }
        return {};
    }
    ImageGroupEntryRef image(std::string_view binding, int index) const override
    {
        index += page * capacity;
        const auto *god = god_at(index);
        if ((binding != "god.bolt" && binding != "god.wrath_icon") || !god) return {};
        if (city_god_wrath_bolts(god->runtime_id()) / 10) return ImageGroupEntryRef::from_group("FX\\God_Bolt", "Image_0000");
        if (binding == "god.bolt" && city_god_happy_bolts(god->runtime_id())) return ImageGroupEntryRef::from_group("UI\\Happy_God_Icon", "Happy God Icon");
        return {};
    }
    int condition(std::string_view binding, int index) const override
    {
        if (binding == "gods.disabled") return !setting_gods_enabled();
        if (binding == "page.previous") return page > 0;
        if (binding == "page.next") return (page + 1) * capacity < god_definition_count() + 1;
        index += page * capacity;
        if (binding == "festival.available") return !city_festival_is_planned();
        if (binding == "festival.planned") return city_festival_is_planned();
        if (binding == "epithets.available") return window_epithets_available();
        if (binding == "god.grand") return index >= 0 && count_temples(god_at(index), ReligionTier::Grand, true) > 0;
        return 0;
    }
    void action(std::string_view action, int) override
    {
        if (action == "page.previous" && page) --page;
        if (action == "page.next" && condition(action, 0)) ++page;
        if (action == "festival.hold" && !city_festival_is_planned()) window_hold_festival_show();
        if (action == "epithets.show") window_epithets_show();
    }
};
ReligionAdvisorController controller;
std::unique_ptr<DeclarativeWindowRuntime> runtime;
const DeclarativeWindowDefinition *definition = nullptr;

int window_height()
{
    if (!definition) return 432;
    const auto *disabled = definition->widget("gods_disabled");
    return disabled && !setting_gods_enabled() ? std::max(definition->base_height(), disabled->y + disabled->height + 8) : definition->base_height();
}

int draw_background()
{
    city_gods_calculate_least_happy();
    if (runtime) runtime->draw(DeclarativeDrawPhase::Background, definition->base_width(), window_height());
    return (window_height() + 15) / 16;
}
void draw_foreground() { if (runtime) runtime->draw(DeclarativeDrawPhase::Foreground, definition->base_width(), window_height()); }
int handle_mouse(const mouse *m) { return runtime ? runtime->handle_mouse(*m, definition->base_width(), window_height()) : 0; }
void get_tooltip(advisor_tooltip_result *) {}
}

const advisor_window_type *window_advisor_religion(void)
{
    definition = declarative_window_definition("advisor_religion");
    controller.page = 0;
    controller.gods.clear();
    for (int i = 0; i < god_definition_count(); ++i) controller.gods.push_back(god_definition_at_runtime_index(i));
    std::stable_sort(controller.gods.begin(), controller.gods.end(), [](const God *a, const God *b) {
        const int left = a->legacy_type() >= 0 && a->legacy_type() < GOD_ALL ? a->legacy_type() : GOD_ALL;
        const int right = b->legacy_type() >= 0 && b->legacy_type() < GOD_ALL ? b->legacy_type() : GOD_ALL;
        return left < right;
    });
    if (definition && definition->widget("god_name") && definition->widget("gods_panel")) controller.capacity = std::max(1, (definition->widget("gods_panel")->height - 8) / std::max(1, definition->widget("god_name")->repeat_spacing_y));
    runtime = definition ? std::make_unique<DeclarativeWindowRuntime>(*definition, controller) : nullptr;
    static const advisor_window_type window = {draw_background, draw_foreground, handle_mouse, get_tooltip};
    return &window;
}
