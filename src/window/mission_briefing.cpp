#include "game/file.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/rich_text.h"
#include "window/city.h"
#include "window/intermezzo.h"
#include "window/main_menu.h"
#include "window/mission_list.h"
#include "window/mission_selection.h"
#include "window/plain_message_dialog.h"

#include "window/mission_briefing.h"

#include "window/video.h"
#include "translation/translation.h"
#include "graphics/declarative_window.h"
#include "graphics/ui_runtime.h"

#include <array>
#include <cstdio>
#include <memory>

extern "C" {

#include "city/mission.h"
#include "core/config.h"
#include "core/encoding.h"
#include "core/image_group.h"
#include "core/lang.h"
#include "game/campaign.h"
#include "game/mission.h"
#include "game/settings.h"
#include "game/tutorial.h"
#include "graphics/image_button.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/window.h"
#include "scenario/custom_messages.h"
#include "scenario/criteria.h"
#include "scenario/event/controller.h"
#include "scenario/property.h"
#include "scenario/scenario.h"
#include "sound/device.h"
#include "sound/music.h"
#include "sound/speech.h"
}

namespace {

enum button_go_back_action {
    BUTTON_GO_BACK_NONE = 0,
    BUTTON_GO_BACK_MISSION_SELECTION = 1,
    BUTTON_GO_BACK_SCENARIO_SELECTION
};

struct MissionObjective {
    int label_text_id = 0;
    int value = 0;
};

static void show(void);
static int can_go_back(void);
static void button_back(int param1, int param2);
static void button_start_mission(int param1, int param2);

static image_button image_button_back = {
    0, 0, 31, 20, IB_NORMAL, GROUP_MESSAGE_ICON, 8, button_back, button_none, 0, 0, 1
};
static image_button image_button_start_mission = {
    0, 0, 27, 27, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 56, button_start_mission, button_none, 1, 0, 1
};

static struct {
    button_go_back_action back_action;
    int video_played;
    int audio_played;
    int focus_button;
    int background_image_id;
    struct {
        char audio[FILE_NAME_MAX];
        char speech[FILE_NAME_MAX];
        char background_music[FILE_NAME_MAX];
    } paths;
    int file_loaded;
} data;

class MissionBriefingWindow {
public:
    explicit MissionBriefingWindow(const DeclarativeWindow &window)
        : window_(window)
    {
    }

    int width_blocks() const
    {
        const int current_screen_width = screen_width();
        const int width = current_screen_width / BLOCK_SIZE - definition().margin_x_blocks();
        return width >= definition().min_blocks_width() ? width : definition().min_blocks_width();
    }

    int height_blocks() const
    {
        const int current_screen_height = screen_height();
        const int height = current_screen_height / BLOCK_SIZE - definition().margin_y_blocks();
        return height >= definition().min_blocks_height() ? height : definition().min_blocks_height();
    }

    int dialog_width() const
    {
        return width_blocks() * BLOCK_SIZE;
    }

    int dialog_height() const
    {
        return height_blocks() * BLOCK_SIZE;
    }

    void draw_shell() const
    {
        draw_panel("outer_panel", PanelKind::Outer);
        draw_panel("objectives_panel", PanelKind::Inner);
        draw_panel("body_panel", PanelKind::Inner);
    }

    void draw_title_text(const uint8_t *title, const uint8_t *subtitle) const
    {
        if (title) {
            draw_bound_text("title", title);
        }
        if (subtitle) {
            draw_bound_text("subtitle", subtitle);
        }
        draw_language_text("objectives_header", 62, 10);
        draw_language_text("button_to_city_text", 62, 7);
        if (can_go_back()) {
            draw_language_text("button_cancel_text", 13, 4);
        }
    }

    void draw_objectives() const
    {
        std::array<MissionObjective, 5> objectives = {};
        int count = 0;
        if (scenario_criteria_population_enabled()) {
            objectives[count++] = { 11, scenario_criteria_population() };
        }
        if (scenario_criteria_culture_enabled()) {
            objectives[count++] = { 12, scenario_criteria_culture() };
        }
        if (scenario_criteria_prosperity_enabled()) {
            objectives[count++] = { 13, scenario_criteria_prosperity() };
        }
        if (scenario_criteria_peace_enabled()) {
            objectives[count++] = { 14, scenario_criteria_peace() };
        }
        if (scenario_criteria_favor_enabled()) {
            objectives[count++] = { 15, scenario_criteria_favor() };
        }

        for (int i = 0; i < count; i++) {
            char widget_id[32];
            snprintf(widget_id, sizeof(widget_id), "objective_%d", i);
            draw_objective(widget_id, objectives[i]);
        }

        const int immediate_goal_text = tutorial_get_immediate_goal_text();
        if (immediate_goal_text) {
            draw_immediate_goal(immediate_goal_text);
        }
    }

    void draw_body(const uint8_t *content) const
    {
        if (!content) {
            return;
        }

        const DeclarativeWidgetDefinition *body = widget("body");
        if (!body) {
            return;
        }

        const int x = body->resolved_x(dialog_width(), definition().base_width());
        const int y = body->resolved_y(dialog_height(), definition().base_height());
        const int width = body->resolved_width(dialog_width(), definition().base_width());
        const int height_blocks = resolved_height_blocks(*body);
        const int draw_x = x + body->draw_offset_x;
        const int draw_y = y + body->draw_offset_y;

        rich_text_set_fonts(FONT_NORMAL_WHITE, FONT_NORMAL_GREEN, FONT_NORMAL_RED, body->line_spacing);
        rich_text_set_font_size_delta(body->font_size_delta);
        rich_text_set_paragraph_spacing(body->paragraph_spacing);
        rich_text_init(content, draw_x, y, (width + BLOCK_SIZE - 1) / BLOCK_SIZE + 1, height_blocks, 0);
        const DeclarativeWidgetDefinition *scrollbar = widget("scrollbar");
        if (scrollbar) {
            const int scrollbar_x = scrollbar->resolved_x(dialog_width(), definition().base_width());
            const int scrollbar_y = scrollbar->resolved_y(dialog_height(), definition().base_height());
            const int scrollbar_height = resolved_height(*scrollbar);
            rich_text_set_scrollbar_bounds(scrollbar_x, scrollbar_y, scrollbar_height, scrollbar_x - draw_x);
        }
        const int height_lines = (height_blocks - 1) * BLOCK_SIZE / rich_text_get_line_height();
        rich_text_draw(
            content,
            draw_x,
            draw_y,
            width,
            height_lines,
            0);
    }

    void draw_scrollbar() const
    {
        rich_text_draw_scrollbar();
    }

    void draw_buttons() const
    {
        draw_image_button("button_to_city", image_button_start_mission);
        if (can_go_back()) {
            draw_image_button("button_cancel", image_button_back);
        }
    }

    int handle_input(const mouse *m_dialog) const
    {
        if (rich_text_handle_mouse(m_dialog)) {
            return 1;
        }
        if (handle_image_button(m_dialog, "button_to_city", image_button_start_mission)) {
            return 1;
        }
        if (can_go_back() && handle_image_button(m_dialog, "button_cancel", image_button_back)) {
            return 1;
        }
        return 0;
    }

private:
    enum class PanelKind {
        Outer,
        Inner,
    };

    const DeclarativeWindowDefinition &definition() const
    {
        return window_.definition();
    }

    const DeclarativeWidgetDefinition *widget(const char *id) const
    {
        return window_.widget(id);
    }

    void draw_panel(const char *id, PanelKind kind) const
    {
        const DeclarativeWidgetDefinition *panel = widget(id);
        if (!panel) {
            return;
        }
        const int x = panel->resolved_x(dialog_width(), definition().base_width());
        const int y = panel->resolved_y(dialog_height(), definition().base_height());
        const int width_blocks = panel->resolved_width_blocks(dialog_width(), definition().base_width());
        const int height_blocks = resolved_height_blocks(*panel);
        if (kind == PanelKind::Outer) {
            outer_panel_draw(x, y, width_blocks, height_blocks);
        } else {
            inner_panel_draw(x, y, width_blocks, height_blocks);
        }
    }

    void draw_bound_text(const char *id, const uint8_t *text) const
    {
        const DeclarativeWidgetDefinition *label = widget(id);
        if (!label || !text) {
            return;
        }
        const int x = label->resolved_x(dialog_width(), definition().base_width());
        const int y = label->resolved_y(dialog_height(), definition().base_height());
        const int pixel_size =
            screen_ui_to_pixel(font_definition_for(label->font)->line_height + label->font_size_delta);
        text_draw(text, x, y, label->font, pixel_size, label->color);
    }

    void draw_language_text(const char *id, int group, int text_id) const
    {
        const DeclarativeWidgetDefinition *label = widget(id);
        if (!label) {
            return;
        }
        const int x = label->resolved_x(dialog_width(), definition().base_width());
        const int y = label->resolved_y(dialog_height(), definition().base_height());
        const int pixel_size =
            screen_ui_to_pixel(font_definition_for(label->font)->line_height + label->font_size_delta);
        text_draw(lang_get_string(group, text_id), x, y, label->font, pixel_size, label->color);
    }

    void draw_objective(const char *id, const MissionObjective &objective) const
    {
        const DeclarativeWidgetDefinition *box = widget(id);
        if (!box) {
            return;
        }
        const int x = box->resolved_x(dialog_width(), definition().base_width());
        const int y = box->resolved_y(dialog_height(), definition().base_height());
        draw_objective_box(*box, x, y);

        const int text_x = x + box->padding_x;
        const int text_y = y + box->padding_y;
        const int pixel_size =
            screen_ui_to_pixel(font_definition_for(box->font)->line_height + box->font_size_delta);
        const int width = text_draw(
            lang_get_string(62, objective.label_text_id), text_x, text_y, box->font, pixel_size, box->color);
        text_draw_number(objective.value, '@', " ", text_x + width, text_y, box->font, pixel_size, box->color);
    }

    void draw_immediate_goal(int text_id) const
    {
        const DeclarativeWidgetDefinition *box = widget("immediate_goal");
        if (!box) {
            return;
        }
        const int x = box->resolved_x(dialog_width(), definition().base_width());
        const int y = box->resolved_y(dialog_height(), definition().base_height());
        draw_objective_box(*box, x, y);
        const int pixel_size =
            screen_ui_to_pixel(font_definition_for(box->font)->line_height + box->font_size_delta);
        text_draw(
            lang_get_string(62, text_id), x + box->padding_x, y + box->padding_y, box->font, pixel_size, box->color);
    }

    void draw_objective_box(const DeclarativeWidgetDefinition &box, int x, int y) const
    {
        const int width_blocks = box.resolved_width_blocks(dialog_width(), definition().base_width());
        label_draw(x, y, width_blocks, box.label_type);
    }

    int resolved_height(const DeclarativeWidgetDefinition &definition) const
    {
        if (!definition.stretch_to_widget.empty()) {
            const DeclarativeWidgetDefinition *target = widget(definition.stretch_to_widget.c_str());
            if (target) {
                const int target_y = target->resolved_y(dialog_height(), this->definition().base_height());
                const int height = target_y - definition.stretch_margin_y -
                    definition.resolved_y(dialog_height(), this->definition().base_height());
                if (height > 0) {
                    return height;
                }
            }
        }
        return definition.resolved_height(dialog_height(), this->definition().base_height());
    }

    int resolved_height_blocks(const DeclarativeWidgetDefinition &definition) const
    {
        if (!definition.stretch_to_widget.empty()) {
            const int height_blocks = resolved_height(definition) / BLOCK_SIZE;
            return height_blocks > 0 ? height_blocks : 1;
        }
        return (resolved_height(definition) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    }

    void draw_image_button(const char *id, image_button &button) const
    {
        const DeclarativeWidgetDefinition *definition = widget(id);
        if (!definition) {
            return;
        }
        if (definition->image_collection) {
            button.image_collection = definition->image_collection;
            button.image_offset = definition->image_offset;
        }
        shared_ui_runtime().draw_image_button(
            definition->resolved_x(dialog_width(), this->definition().base_width()),
            definition->resolved_y(dialog_height(), this->definition().base_height()),
            button);
    }

    int handle_image_button(const mouse *m_dialog, const char *id, image_button &button) const
    {
        const DeclarativeWidgetDefinition *definition = widget(id);
        if (!definition) {
            return 0;
        }
        if (definition->image_collection) {
            button.image_collection = definition->image_collection;
            button.image_offset = definition->image_offset;
        }
        return image_buttons_handle_mouse(
            m_dialog,
            definition->resolved_x(dialog_width(), this->definition().base_width()),
            definition->resolved_y(dialog_height(), this->definition().base_height()),
            &button,
            1,
            0);
    }

    const DeclarativeWindow &window_;
};

static const MissionBriefingWindow *mission_window()
{
    const DeclarativeWindow *window = declarative_window("mission_briefing");
    static std::unique_ptr<MissionBriefingWindow> mission;
    if (!window) {
        return nullptr;
    }
    if (!mission) {
        mission = std::make_unique<MissionBriefingWindow>(*window);
    }
    return mission.get();
}

static void init(void)
{
    data.focus_button = 0;
    data.audio_played = 0;
    data.paths.audio[0] = 0;
    data.paths.speech[0] = 0;
    data.paths.background_music[0] = 0;
    rich_text_reset(0);
}

static int load_scenario_file(void)
{
    if (!game_campaign_is_active()) {
        return game_file_start_scenario_by_name(scenario_name());
    } else {
        return game_campaign_load_scenario(scenario_campaign_mission());
    }
}

static int has_briefing_message(void)
{
    if (game_campaign_is_original()) {
        return 1;
    }
    if (!scenario_intro_message()) {
        return 0;
    }
    custom_message_t *message = custom_messages_get(scenario_intro_message());
    if (!message) {
        return 0;
    }
    return custom_messages_get_video(message) || custom_messages_get_text(message) ||
        custom_messages_get_title(message) || custom_messages_get_subtitle(message);
}

static int play_video(void)
{
    if (game_campaign_is_original() || data.back_action == BUTTON_GO_BACK_NONE || data.video_played) {
        return 0;
    }
    data.video_played = 1;
    uint8_t *video_file = custom_messages_get_video(custom_messages_get(scenario_intro_message()));
    if (video_file) {
        char utf8_filename[FILE_NAME_MAX];
        encoding_to_utf8(video_file, utf8_filename, FILE_NAME_MAX, encoding_system_uses_decomposed());
        window_video_show(utf8_filename, show);
        return 1;
    }
    return 0;
}

static void fadeout_music(sound_type unused)
{
    sound_device_fadeout_music(5000);
    sound_device_on_audio_finished(0);
}

static void init_speech(sound_type type)
{
    if (type != SOUND_TYPE_SPEECH) {
        return;
    }

    int has_speech = *data.paths.speech && *data.paths.background_music;
    if (*data.paths.speech) {
        has_speech &= sound_device_play_file_on_channel(data.paths.speech,
            SOUND_TYPE_SPEECH, setting_sound(SOUND_TYPE_SPEECH)->volume);
    }
    if (*data.paths.background_music) {
        int volume = 100;
        if (has_speech) {
            volume = setting_sound(SOUND_TYPE_SPEECH)->volume / 3;
        }
        if (volume > setting_sound(SOUND_TYPE_MUSIC)->volume) {
            volume = setting_sound(SOUND_TYPE_MUSIC)->volume;
        }
        has_speech &= sound_device_play_music(data.paths.background_music, volume, 0);
    }
    sound_device_on_audio_finished(has_speech ? fadeout_music : 0);
}

static void play_audio(void)
{
    if (game_campaign_is_original() || data.audio_played) {
        data.audio_played = 1;
        return;
    }
    data.audio_played = 1;

    custom_message_t *custom_message = custom_messages_get(scenario_intro_message());

    const char *audio_file = custom_messages_get_audio(custom_message);
    if (audio_file) {
        snprintf(data.paths.audio, FILE_NAME_MAX, "%s", audio_file);
    }
    const char *speech_file = custom_messages_get_speech(custom_message);
    if (speech_file) {
        snprintf(data.paths.speech, FILE_NAME_MAX, "%s", speech_file);
    }
    const char *background_music = custom_messages_get_background_music(custom_message);
    if (background_music) {
        snprintf(data.paths.background_music, FILE_NAME_MAX, "%s", background_music);
    }

    if (!audio_file && !speech_file && !background_music) {
        data.audio_played = 0;
        return;
    }

    int playing_audio = 0;

    sound_music_stop();

    if (audio_file) {
        playing_audio = sound_device_play_file_on_channel(data.paths.audio, SOUND_TYPE_SPEECH,
            setting_sound(SOUND_TYPE_SPEECH)->volume);
    }
    if (speech_file) {
        if (!playing_audio) {
            init_speech(SOUND_TYPE_SPEECH);
        } else {
            sound_device_on_audio_finished(init_speech);
        }
    } else if (background_music) {
        sound_device_play_music(data.paths.background_music, setting_sound(SOUND_TYPE_MUSIC)->volume, 0);
    }
}

static void draw_background_image(void)
{
    if (game_campaign_is_original()) {
        window_draw_underlying_window();
        return;
    }
    if (!data.background_image_id) {
        int image_id = 0;
        if (has_briefing_message()) {
            custom_message_t *intro_message = custom_messages_get(scenario_intro_message());
            const uint8_t *background_image = custom_messages_get_background_image(intro_message);
            if (background_image) {
                image_id = rich_text_parse_image_id(&background_image, GROUP_INTERMEZZO_BACKGROUND, 1);
            }
        }
        if (!image_id) {
            image_id = Image::group(GROUP_INTERMEZZO_BACKGROUND) + 2 * (scenario_campaign_mission() % 11) + 2;
        }
        data.background_image_id = image_id;
    }

    Image::from_id(data.background_image_id).draw_fullscreen_background();
}

static void get_briefing_texts(const uint8_t **title, const uint8_t **subtitle, const uint8_t **content)
{
    if (game_campaign_is_original()) {
        int text_id = 200 + scenario_campaign_mission();
        const lang_message *msg = lang_get_message(text_id);
        *title = msg->title.text;
        *subtitle = msg->subtitle.text;
        *content = msg->content.text;
    } else {
        custom_message_t *custom_message = custom_messages_get(scenario_intro_message());
        *title = custom_messages_get_title(custom_message);
        *subtitle = custom_messages_get_subtitle(custom_message);
        *content = custom_messages_get_text(custom_message);
    }
}

static int can_go_back(void)
{
    return data.back_action == BUTTON_GO_BACK_SCENARIO_SELECTION ||
        (data.back_action == BUTTON_GO_BACK_MISSION_SELECTION && game_mission_has_choice());
}

static void draw_background(void)
{
    const MissionBriefingWindow *window = mission_window();
    if (!window) {
        button_start_mission(0, 0);
        return;
    }

    if (!data.file_loaded) {
        if (!load_scenario_file()) {
            window_main_menu_show(1);
            setting_clear_personal_savings();
            scenario_settings_init();
            scenario_set_campaign_rank(2);
            window_plain_message_dialog_show(TR_WINDOW_CAMPAIGN_MISSION_FAILED_TO_LOAD_TITLE,
                TR_WINDOW_CAMPAIGN_MISSION_FAILED_TO_LOAD_TEXT, 0);
            return;
        }
        data.file_loaded = 1;
    }

    if (!has_briefing_message()) {
        button_start_mission(0, 0);
        return;
    }

    if (play_video()) {
        return;
    }

    const uint8_t *title;
    const uint8_t *subtitle;
    const uint8_t *content;

    get_briefing_texts(&title, &subtitle, &content);

    if (!title && !subtitle && !content) {
        button_start_mission(0, 0);
        return;
    }

    play_audio();
    draw_background_image();

    graphics_in_dialog_with_size(window->dialog_width(), window->dialog_height());
    window->draw_shell();
    window->draw_title_text(title, subtitle);
    window->draw_objectives();
    window->draw_body(content);
    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    const MissionBriefingWindow *window = mission_window();
    if (!window) {
        return;
    }

    graphics_in_dialog_with_size(window->dialog_width(), window->dialog_height());
    rich_text_reset(rich_text_scroll_position());
    window->draw_scrollbar();
    window->draw_buttons();
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const MissionBriefingWindow *window = mission_window();
    if (!window) {
        return;
    }

    const mouse *m_dialog = mouse_in_dialog(m);
    window->handle_input(m_dialog);
}

static void button_back(int param1, int param2)
{
    if (data.back_action == BUTTON_GO_BACK_NONE) {
        return;
    }
    sound_music_stop();
    sound_speech_stop();
    if (data.back_action == BUTTON_GO_BACK_MISSION_SELECTION) {
        window_mission_selection_show_again();
    } else {
        window_mission_list_show_again();
        sound_music_play_intro();
    }
}

static void button_start_mission(int param1, int param2)
{
    if (data.back_action != BUTTON_GO_BACK_NONE || data.audio_played) {
        sound_music_stop();
        sound_speech_stop();
    }
    sound_music_update(1);
    window_city_show();
    if (data.back_action != BUTTON_GO_BACK_NONE) {
        city_mission_reset_save_start();
    }
    scenario_events_process_all();
}

static void show(void)
{
    window_type window = {
        WINDOW_MISSION_BRIEFING,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}

} // namespace

extern "C" {

void window_mission_briefing_show(void)
{
    data.back_action = BUTTON_GO_BACK_MISSION_SELECTION;
    data.video_played = 0;
    data.file_loaded = 0;
    data.background_image_id = 0;
    game_campaign_is_original() ? window_intermezzo_show(INTERMEZZO_MISSION_BRIEFING, show) : show();
}

void window_mission_briefing_show_review(void)
{
    data.back_action = BUTTON_GO_BACK_NONE;
    data.file_loaded = 1;
    data.background_image_id = 0;
    game_campaign_is_original() ? window_intermezzo_show(INTERMEZZO_MISSION_BRIEFING, show) : show();
}

void window_mission_briefing_show_from_scenario_selection(void)
{
    data.back_action = BUTTON_GO_BACK_SCENARIO_SELECTION;
    data.video_played = 0;
    data.file_loaded = 0;
    data.background_image_id = 0;
    game_campaign_is_original() ? window_intermezzo_show(INTERMEZZO_MISSION_BRIEFING, show) : show();
}

} // extern "C"
