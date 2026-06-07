#include "graphics/graphics.h"
#include "translation/translation.h"
#include "graphics/image.h"
#include "graphics/image_border.h"
#include "graphics/lang_text.h"
#include "input/input.h"
#include "window/mission_briefing.h"
#include "window/mission_list.h"

#include "mission_selection.h"

#include "window/video.h"
#include <cstdlib>

extern "C" {

#include "assets/assets.h"
#include "core/image_group.h"
#include "core/log.h"
#include "game/campaign.h"
#include "game/mission.h"
#include "game/settings.h"
#include "graphics/image_button.h"
#include "graphics/text.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "sound/device.h"
#include "sound/music.h"
#include "sound/speech.h"
}

#define BACKGROUND_WIDTH 1024
#define BACKGROUND_HEIGHT 768

namespace {

constexpr int kMissionChoiceButtonSize = 44;
constexpr int kActionButtonsX = 530;
constexpr int kActionButtonsY = 410;

void button_start(int param1, int param2);
void button_back(int param1, int param2);

static image_button image_buttons_start_mission[] = {
    { 0, 0, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 120, button_back, button_none, 1, 0, 1 },
    { 50, 0, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 116, button_start, button_none, 1, 0, 1 }
};

struct MissionSelectionData {
    int choice;
    int focus_button;
    struct {
        const char *intro_video;
        int total_scenarios;
        int background_image_id;
        const uint8_t *title;
        const campaign_scenario **scenarios;
    } mission;
};

MissionSelectionData data;

class MissionChoiceButtonWidget {
public:
    MissionChoiceButtonWidget(const campaign_scenario *scenario, int index)
        : scenario_(scenario)
        , index_(index)
    {
    }

    int hit_test(const mouse *m) const
    {
        return scenario_ &&
            scenario_->x <= m->x &&
            m->x < scenario_->x + kMissionChoiceButtonSize &&
            scenario_->y <= m->y &&
            m->y < scenario_->y + kMissionChoiceButtonSize;
    }

    void draw(int selected_choice, int focused_choice) const
    {
        if (!scenario_) {
            return;
        }

        int offset = selected_choice == index_ + 1 ? 2 : 0;
        offset = focused_choice == index_ + 1 ? 1 : offset;
        Image::from_id(Image::group(GROUP_SELECT_MISSION_BUTTON) + offset).draw(scenario_->x, scenario_->y);

        // Scenario choices are image-backed buttons; keep the legacy art and add the missing numeric label.
        text_draw_number_centered(
            index_ + 1,
            scenario_->x,
            scenario_->y + 14,
            kMissionChoiceButtonSize,
            FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }

private:
    const campaign_scenario *scenario_;
    int index_;
};

class MissionActionButtonRow {
public:
    explicit MissionActionButtonRow(image_button *buttons)
        : buttons_(buttons)
    {
    }

    void draw(int has_choice) const
    {
        image_buttons_draw(kActionButtonsX, kActionButtonsY, buttons_, visible_count(has_choice));
    }

    int handle_mouse(const mouse *m, int has_choice) const
    {
        return image_buttons_handle_mouse(m, kActionButtonsX, kActionButtonsY, buttons_, visible_count(has_choice), 0);
    }

private:
    static int visible_count(int has_choice)
    {
        return has_choice ? 2 : 1;
    }

    image_button *buttons_;
};

static void clear_loaded_mission(void)
{
    free(data.mission.scenarios);
    data.mission.scenarios = 0;
    data.mission.title = 0;
    data.mission.background_image_id = 0;
    data.mission.total_scenarios = 0;
    data.choice = 0;
}

static void load_scenarios(void)
{
    const campaign_mission_info *mission = game_campaign_get_current_mission(scenario_campaign_mission());
    data.mission.title = mission->title;
    data.mission.total_scenarios = mission->total_scenarios;
    data.mission.intro_video = mission->intro_video;
    if (mission->background_image.path) {
        data.mission.background_image_id = assets_get_external_image(mission->background_image.path, 0);
    } else {
        data.mission.background_image_id = mission->background_image.id;
    }
    data.mission.scenarios = static_cast<const campaign_scenario **>(
        malloc(sizeof(campaign_scenario *) * data.mission.total_scenarios));
    if (!data.mission.scenarios) {
        log_error("Failed to allocate memory for scenarios. The game will now crash.", 0, 0);
        return;
    }
    for (int i = 0; i < mission->total_scenarios; i++) {
        data.mission.scenarios[i] = game_campaign_get_scenario(i + mission->first_scenario);
    }
    scenario_set_custom(game_campaign_is_original() ? 0 : 2);
}

static void init(void)
{
    clear_loaded_mission();
    sound_music_stop();
    sound_speech_stop();
    if (!game_campaign_is_active()) {
        return;
    }
    load_scenarios();
}

static void draw_background_images(void)
{
    int s_width = screen_width();
    int s_height = screen_height();
    int image_offset_x = (s_width - BACKGROUND_WIDTH) / 2;
    int image_offset_y = (s_height - BACKGROUND_HEIGHT) / 2;

    if (s_width > BACKGROUND_WIDTH || s_height > BACKGROUND_HEIGHT) {
        Image::from_id(Image::group(GROUP_EMPIRE_MAP)).draw_fullscreen_background();
        Image::from_id(Image::group(GROUP_SELECT_MISSION_BACKGROUND)).draw(image_offset_x, image_offset_y);
        ImageBorder::mission_selection().draw(image_offset_x, image_offset_y);
    } else {
        Image::from_id(Image::group(GROUP_SELECT_MISSION_BACKGROUND)).draw(image_offset_x, image_offset_y);
    }
}

static void draw_background(void)
{
    draw_background_images();
    graphics_in_dialog();
    graphics_set_clip_rectangle(0, 0, 640, 400);
    if (data.mission.background_image_id) {
        Image::from_id(data.mission.background_image_id).draw(0, 0);
    } else {
        Image::from_id(Image::group(GROUP_EMPIRE_MAP)).draw(0, 0, COLOR_MASK_NONE, 2.5f);
    }
    graphics_reset_clip_rectangle();
    if (data.mission.title) {
        text_draw(data.mission.title, 20, 410, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    }
    if (data.choice) {
        const campaign_scenario *camp_scenario = data.mission.scenarios[data.choice - 1];
        if (camp_scenario->name) {
            text_draw_multiline(camp_scenario->name, 20, 440, 560, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        if (camp_scenario->description) {
            text_draw_multiline(camp_scenario->description, 20, 456, 560, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
    } else {
        lang_text_draw_multiline(144, 0, 20, 440, 560, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }
    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    MissionActionButtonRow(image_buttons_start_mission).draw(data.choice);

    for (int i = 0; i < data.mission.total_scenarios; i++) {
        MissionChoiceButtonWidget(data.mission.scenarios[i], i).draw(data.choice, data.focus_button);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    data.focus_button = 0;
    for (int i = 0; i < data.mission.total_scenarios; i++) {
        if (MissionChoiceButtonWidget(data.mission.scenarios[i], i).hit_test(m_dialog)) {
            data.focus_button = i + 1;
        }
    }

    if (MissionActionButtonRow(image_buttons_start_mission).handle_mouse(m_dialog, data.choice)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        if (data.choice > 0) {
            data.choice = 0;
            window_invalidate();
        } else {
            button_back(0, 0);
        }
        return;
    }

    if (m_dialog->left.went_up) {
        for (int i = 0; i < data.mission.total_scenarios; i++) {
            const campaign_scenario *camp_scenario = data.mission.scenarios[i];
            if (MissionChoiceButtonWidget(camp_scenario, i).hit_test(m_dialog)) {
                scenario_set_campaign_mission(camp_scenario->id);
                data.choice = i + 1;
                if (m_dialog->left.double_click) {
                    button_start(0, 0);
                    return;
                }
                window_invalidate();
                if (camp_scenario->fanfare) {
                    sound_device_play_file_on_channel(camp_scenario->fanfare, SOUND_TYPE_SPEECH,
                        setting_sound(SOUND_TYPE_SPEECH)->volume);
                } else {
                    sound_speech_stop();
                }
                break;
            }
        }
    }
}

void button_start(int param1, int param2)
{
    clear_loaded_mission();
    window_mission_briefing_show();
}

void button_back(int param1, int param2)
{
    clear_loaded_mission();
    window_mission_list_show();
    sound_music_play_intro();
}

static void show(void)
{
    if (!game_mission_has_choice()) {
        window_mission_briefing_show();
        return;
    }
    window_type window = {
        WINDOW_MISSION_SELECTION,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}

} // namespace

extern "C" void window_mission_selection_show(void)
{
    init();
    data.mission.intro_video ? window_video_show(data.mission.intro_video, show) : show();
}

extern "C" void window_mission_selection_show_again(void)
{
    init();
    show();
}
