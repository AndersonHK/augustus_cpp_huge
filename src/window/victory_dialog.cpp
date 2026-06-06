extern "C" {
#include "victory_dialog.h"

#include "assets/assets.h"
#include "city/victory.h"
#include "game/campaign.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "sound/music.h"
#include "window/city.h"
}
#include "graphics/image.h"

#define MAX_RANK 10

static void button_accept(const generic_button *button);
static void button_continue_governing(const generic_button *button);

static generic_button victory_buttons[] = {
    {32, 112, 480, 20, button_accept},
    {32, 144, 480, 20, button_continue_governing, 0, 24, 0},
    {32, 176, 480, 20, button_continue_governing, 0, 60, 0},
};

static unsigned int focus_button_id = 0;

static void draw_background(void)
{
    window_draw_underlying_window();    	
    graphics_in_dialog();

    outer_panel_draw(48, 128, 34, 15);
    const campaign_mission_info *mission = game_campaign_get_current_mission(scenario_campaign_mission());

    if (!mission || mission->next_rank <= CAMPAIGN_NO_RANK || mission->next_rank == scenario_campaign_rank()) {
        lang_text_draw_centered(62, 0, 48, 159, 544, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    } else {
        if (mission->next_rank < 10) {
            lang_text_draw_centered(62, 0, 48, 144, 544, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
            lang_text_draw_centered(62, 2, 48, 175, 544, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
            lang_text_draw_centered(32, mission->next_rank, 48, 194, 544, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        } else {
            text_draw_centered(scenario_player_name(), 48, 144, 512, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
            lang_text_draw_multiline(62, 26, 140, 175, 360, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        }
    }

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    if (city_victory_state() == VICTORY_STATE_WON) {
        int image_id = assets_get_image_id("UI", "Victory_Banner");
        Image::from_id(image_id).draw(88, 137);
        Image::from_id(image_id).draw(512, 137);
        large_label_draw(80, 240, 30, focus_button_id == 1);
        const campaign_mission_info *mission = game_campaign_get_current_mission(scenario_campaign_mission());
        if (!mission || mission->next_rank <= CAMPAIGN_NO_RANK || mission->next_rank == scenario_campaign_rank()) {
            lang_text_draw_centered(44, 16, 80, 246, 480, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        } else {
            if (mission->next_rank < 10) {
                lang_text_draw_centered(62, 3, 80, 246, 480, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
            } else {
                lang_text_draw_centered(62, 27, 80, 246, 480, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
            }
        }
        if (!game_campaign_is_original() || scenario_campaign_rank() >= 2) {
            // Continue for 2/5 years
            large_label_draw(80, 272, 30, focus_button_id == 2);
            lang_text_draw_centered(62, 4, 80, 278, 480, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));

            large_label_draw(80, 304, 30, focus_button_id == 3);
            lang_text_draw_centered(62, 5, 80, 310, 480, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        }
    } else {
        // lost
        large_label_draw(80, 224, 30, focus_button_id == 1);
        lang_text_draw_centered(62, 6, 80, 230, 480, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    int num_buttons;
    if (scenario_campaign_rank() >= 2 || !game_campaign_is_original()) {
        num_buttons = 3;
    } else {
        num_buttons = 1;
    }
    GenericButtonList(victory_buttons, num_buttons).handle_mouse(
        *mouse_in_dialog(m),
        48,
        128,
        &focus_button_id
    );
}

static void button_accept(const generic_button *button)
{
    window_city_show();
}

static void button_continue_governing(const generic_button *button)
{
    int months = button->parameter1;
    city_victory_continue_governing(months);
    window_city_show();
    city_victory_reset();
    sound_music_update(1);
}

void window_victory_dialog_show(void)
{
    window_type window = {
        WINDOW_VICTORY_DIALOG,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
