#include "window/new_career.h"

#include "core/string.h"
#include "game/settings.h"
#include "graphics/graphics.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/scenario.h"
#include "translation/translation.h"
#include "widget/input_box.h"
#include "window/main_menu.h"
#include "window/mission_selection.h"

namespace {

constexpr int kPlayerNameLength = 32;

void start_mission(int param1, int param2);
void button_back(int param1, int param2);

image_button image_buttons[] = {
    { 0, 2, 31, 20, IB_NORMAL, GROUP_MESSAGE_ICON, 8, button_back, button_none, 0, 0, 1 },
    { 305, 0, 27, 27, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 56, start_mission, button_none, 1, 0, 1 }
};

uint8_t player_name[kPlayerNameLength];
input_box player_name_input = { 160, 208, 20, 2, FONT_NORMAL_WHITE, 1, player_name, kPlayerNameLength };

int font_pixel_size(font_t font)
{
    return screen_ui_to_pixel(font_definition_for(font)->line_height);
}

void init()
{
    setting_clear_personal_savings();
    scenario_settings_init();
    string_copy(lang_get_string("main_strings.9.5"), player_name, kPlayerNameLength);
    input_box_start(&player_name_input);
}

void draw_foreground()
{
    graphics_in_dialog();
    outer_panel_draw(128, 160, 24, 8);
    lang_text_draw_centered("main_strings.31.0", 128, 172, 384, FONT_LARGE_BLACK,
        font_pixel_size(FONT_LARGE_BLACK));
    lang_text_draw("main_strings.13.5", 352, 256, FONT_NORMAL_BLACK,
        font_pixel_size(FONT_NORMAL_BLACK));
    lang_text_draw("main_strings.12.0", 200, 256, FONT_NORMAL_BLACK,
        font_pixel_size(FONT_NORMAL_BLACK));
    input_box_draw(&player_name_input);
    image_buttons_draw(159, 249, image_buttons, 2);
    graphics_reset_dialog();
}

void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *dialog_mouse = mouse_in_dialog(m);
    if (input_box_handle_mouse(dialog_mouse, &player_name_input) ||
        image_buttons_handle_mouse(dialog_mouse, 159, 249, image_buttons, 2, nullptr)) {
        return;
    }
    if (input_box_is_accepted()) {
        start_mission(0, 0);
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_back(0, 0);
    }
}

void button_back(int param1, int param2)
{
    (void)param1;
    (void)param2;
    input_box_stop(&player_name_input);
    window_go_back();
}

void start_mission(int param1, int param2)
{
    (void)param1;
    (void)param2;
    input_box_stop(&player_name_input);
    setting_set_player_name(player_name);
    window_mission_selection_show();
}

} // namespace

void window_new_career_show(void)
{
    window_type window = {
        WINDOW_NEW_CAREER,
        window_main_menu_draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}
