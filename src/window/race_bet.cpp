#include "race_bet.h"

#include "building/building.h"
#include "city/data_private.h"
#include "city/race_bet.h"
#include "core/calc.h"
#include "graphics/declarative_window.h"
#include "graphics/graphics.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"

#include <memory>
#include <string>

namespace {

std::string translated(const char *key)
{
    return reinterpret_cast<const char *>(translation_for_key(key));
}

class RaceBetController final : public DeclarativeWindowController {
public:
    int initialize(unsigned int building_id)
    {
        Building *building = Building::get(building_id);
        if (!building || !building->type || !building->type->has_race() ||
            !building->type->race().betting.enabled || building->type->race().teams.empty()) return 0;
        building_id_ = building_id;
        race_ = &building->type->race();
        selected_team_ = city_data.games.chosen_horse;
        wager_ = static_cast<unsigned int>(calc_bound(city_data.games.bet_amount, 0, city_data.emperor.personal_savings));
        locked_ = has_bet_in_progress();
        return 1;
    }

    const building_type_registry_impl::RaceDefinition *race() const { return race_; }

    int repeat_count(std::string_view source) const override
    {
        return source == "race.teams" && race_ ? static_cast<int>(race_->teams.size()) : 0;
    }

    std::string text(std::string_view binding, int item_index) const override
    {
        (void)item_index;
        if (binding == "race.selected_description" && selected_team_ > 0 && selected_team_ <= race_->teams.size()) {
            return translated(race_->teams[selected_team_ - 1].description_key.c_str());
        }
        if (binding == "race.wager") return std::to_string(wager_) + " dn";
        if (binding == "race.savings") {
            return translated("TR_PERSONAL_SAVINGS") + " " +
                std::to_string(city_data.emperor.personal_savings) + " dn";
        }
        if (binding == "race.confirm_label") {
            return translated(locked_ ? "TR_WINDOW_IN_PROGRESS_BET_BUTTON" : "TR_WINDOW_RACE_BET_BUTTON");
        }
        return {};
    }

    ImageGroupEntryRef image(std::string_view binding, int item_index) const override
    {
        if (binding != "race.team.portrait" || !race_ || item_index < 0 || item_index >= race_->teams.size()) return {};
        const building_type_registry_impl::RaceTeamDefinition &team = race_->teams[item_index];
        return ImageGroupEntryRef::from_group(team.portrait_path, team.portrait_image);
    }

    int condition(std::string_view binding, int item_index) const override
    {
        if (binding == "race.can_edit") return !locked_ && race_bet_can_place_for(building_id_);
        if (binding == "race.can_confirm") return !locked_ && selected_team_ > 0 && wager_ > 0 &&
            race_bet_can_place_for(building_id_);
        if (binding == "race.team.selected") return item_index >= 0 && selected_team_ == static_cast<unsigned int>(item_index + 1);
        return 0;
    }

    void action(std::string_view action_id, int item_index) override
    {
        if (action_id == "window.close") {
            window_go_back();
        } else if (action_id == "race.team.select" && condition("race.can_edit", item_index) && item_index >= 0) {
            selected_team_ = static_cast<unsigned int>(item_index + 1);
        } else if (action_id == "race.wager.decrease" && condition("race.can_edit", -1)) {
            wager_ = static_cast<unsigned int>(calc_bound(static_cast<int>(wager_) - race_->betting.wager_step,
                0, city_data.emperor.personal_savings));
        } else if (action_id == "race.wager.increase" && condition("race.can_edit", -1)) {
            wager_ = static_cast<unsigned int>(calc_bound(static_cast<int>(wager_) + race_->betting.wager_step,
                0, city_data.emperor.personal_savings));
        } else if (action_id == "race.confirm" && condition("race.can_confirm", -1)) {
            city_data.games.chosen_horse = static_cast<uint8_t>(selected_team_);
            city_data.games.bet_amount = static_cast<int32_t>(wager_);
            race_bet_select_building(building_id_);
            window_go_back();
        }
        window_request_refresh();
    }

    const char *tooltip(std::string_view binding, int item_index) const override
    {
        if (binding == "race.team.tooltip" && race_ && item_index >= 0 && item_index < race_->teams.size()) {
            return race_->teams[item_index].tooltip_key.c_str();
        }
        return nullptr;
    }

private:
    unsigned int building_id_ = 0;
    const building_type_registry_impl::RaceDefinition *race_ = nullptr;
    unsigned int selected_team_ = 0;
    unsigned int wager_ = 0;
    int locked_ = 0;
};

RaceBetController controller;
std::unique_ptr<DeclarativeWindowRuntime> runtime;
const DeclarativeWindowDefinition *definition = nullptr;

int init()
{
    if (window_is(WINDOW_RACE_BET) || !controller.initialize(race_bet_selected_building())) return 0;
    definition = declarative_window_definition(controller.race()->betting.window);
    if (!definition) return 0;
    runtime = std::make_unique<DeclarativeWindowRuntime>(*definition, controller);
    return 1;
}

void draw_background()
{
    window_draw_underlying_window();
    graphics_in_dialog_with_size(definition->base_width(), definition->base_height());
    runtime->draw(DeclarativeDrawPhase::Background, definition->base_width(), definition->base_height());
    graphics_reset_dialog();
}

void draw_foreground()
{
    graphics_in_dialog_with_size(definition->base_width(), definition->base_height());
    runtime->draw(DeclarativeDrawPhase::Foreground, definition->base_width(), definition->base_height());
    graphics_reset_dialog();
    window_request_refresh();
}

void handle_input(const mouse *m, const hotkeys *hotkeys)
{
    const mouse *dialog_mouse = mouse_in_dialog_with_size(m, definition->base_width(), definition->base_height());
    if (runtime->handle_mouse(*dialog_mouse, definition->base_width(), definition->base_height())) return;
    if (input_go_back_requested(m, hotkeys)) window_go_back();
}

void handle_tooltip(tooltip_context *context)
{
    runtime->tooltip(*context);
}

} // namespace

void window_race_bet_show(void)
{
    if (!init()) return;
    window_type window = { WINDOW_RACE_BET, draw_background, draw_foreground, handle_input, handle_tooltip };
    window_show(&window);
}
