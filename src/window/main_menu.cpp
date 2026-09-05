#include "window/main_menu.h"
#include "window/new_career.h"

#include "core/config.h"
#include "core/image_group.h"
#include "core/string.h"
#include "editor/editor.h"
#include "game/campaign.h"
#include "game/game.h"
#include "game/system.h"
#include "graphics/declarative_window.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/ui_runtime.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/weather.h"
#include "graphics/window.h"
#include "sound/music.h"
#include "translation/translation.h"
#include "window/cck_selection.h"
#include "window/config.h"
#include "window/editor/map.h"
#include "window/file_dialog.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"
#include "window/select_campaign.h"
#include "window/video.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>

namespace {

struct MainMenuState {
    std::string focused_widget;
};

MainMenuState data;

void confirm_exit(int accepted, int checked)
{
    (void)checked;
    if (accepted) {
        system_exit();
    }
}

void action_select_campaign()
{
    window_select_campaign_show();
}

void action_new_career()
{
    window_new_career_show();
}

void action_load_game()
{
    window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
}

void action_construction_kit()
{
    window_cck_selection_show();
}

void action_assignment_editor()
{
    if (!editor_is_present() || !game_init_editor()) {
        window_plain_message_dialog_show("TR_NO_EDITOR_TITLE", "TR_NO_EDITOR_MESSAGE", 1);
        return;
    }
    if (config_get(CONFIG_UI_SHOW_INTRO_VIDEO)) {
        window_video_show("map_intro.smk", window_editor_map_show);
    }
    sound_music_play_editor();
}

void action_options()
{
    window_config_show(CONFIG_FIRST_PAGE, 0, 1);
}

void action_exit()
{
    window_popup_dialog_show(POPUP_DIALOG_QUIT, confirm_exit, 1);
}

void action_escape()
{
    hotkey_handle_escape();
}

struct MainMenuAction {
    std::string_view id;
    void (*handler)();
};

// XML owns the menu composition. This table is only the interpreter's named
// capability boundary and is also used to reject unknown actions at startup.
constexpr MainMenuAction kActions[] = {
    { "career.new", action_new_career },
    { "campaign.select", action_select_campaign },
    { "game.load", action_load_game },
    { "construction_kit.open", action_construction_kit },
    { "assignment_editor.open", action_assignment_editor },
    { "options.open", action_options },
    { "application.exit", action_exit },
    { "hotkey.escape", action_escape },
};

const MainMenuAction *find_action(std::string_view action)
{
    for (const MainMenuAction &candidate : kActions) {
        if (candidate.id == action) {
            return &candidate;
        }
    }
    return nullptr;
}

void execute_action(std::string_view action)
{
    if (const MainMenuAction *resolved = find_action(action)) {
        resolved->handler();
    }
}

class MainMenuWindow {
public:
    explicit MainMenuWindow(const DeclarativeWindow &window)
        : window_(window)
    {
    }

    void draw(DeclarativeDrawPhase phase) const
    {
        if (phase == DeclarativeDrawPhase::Background) {
            graphics_reset_dialog();
            graphics_reset_clip_rectangle();
        }

        for (const DeclarativeWidgetDefinition &widget : definition().widgets()) {
            if (widget.draw_phase != phase || !is_visible(widget)) {
                continue;
            }
            select_coordinate_space(widget);
            draw_widget(widget);
        }
        graphics_reset_dialog();
    }

    int handle_input(const mouse *screen_mouse)
    {
        data.focused_widget.clear();
        const mouse *dialog_mouse = mouse_in_dialog(screen_mouse);
        for (const DeclarativeWidgetDefinition &widget : definition().widgets()) {
            if ((widget.type != DeclarativeWidgetType::TextButton &&
                    widget.type != DeclarativeWidgetType::ImageButton) ||
                !is_visible(widget)) {
                continue;
            }

            const mouse *active_mouse = widget.coordinate_space == DeclarativeCoordinateSpace::Screen ?
                screen_mouse : dialog_mouse;
            const Bounds bounds = resolve_bounds(widget);
            if (active_mouse->x < bounds.x || active_mouse->x >= bounds.x + bounds.width ||
                active_mouse->y < bounds.y || active_mouse->y >= bounds.y + bounds.height) {
                continue;
            }

            data.focused_widget = widget.id;
            if (active_mouse->left.went_up) {
                execute_action(widget.action);
                return 1;
            }
            return 0;
        }
        return 0;
    }

    const DeclarativeWindowDefinition &definition() const
    {
        return window_.definition();
    }

private:
    struct Bounds {
        int x;
        int y;
        int width;
        int height;
    };

    Bounds resolve_bounds(const DeclarativeWidgetDefinition &widget) const
    {
        const int width = widget.coordinate_space == DeclarativeCoordinateSpace::Screen ?
            screen_width() : definition().base_width();
        const int height = widget.coordinate_space == DeclarativeCoordinateSpace::Screen ?
            screen_height() : definition().base_height();
        return {
            widget.resolved_x(width, definition().base_width()),
            widget.resolved_y(height, definition().base_height()),
            resolved_widget_width(widget, width),
            widget.resolved_height(height, definition().base_height()),
        };
    }

    int resolved_text_width(const DeclarativeWidgetDefinition &widget) const
    {
        std::string storage;
        const uint8_t *text = resolved_text(widget, storage);
        if (!text) {
            return 0;
        }
        const int pixel_size = screen_ui_to_pixel(
            font_definition_for(widget.font)->line_height + widget.font_size_delta);
        return text_get_width(text, widget.font, pixel_size);
    }

    int resolved_widget_width(const DeclarativeWidgetDefinition &widget, int coordinate_width) const
    {
        if (!widget.width_from_text.empty()) {
            const DeclarativeWidgetDefinition *text_widget = definition().widget(widget.width_from_text);
            int resolved = text_widget ? resolved_text_width(*text_widget) + widget.width_adjust : 0;
            if (widget.width_round_up_to > 0) {
                resolved = ((resolved + widget.width_round_up_to - 1) / widget.width_round_up_to) *
                    widget.width_round_up_to;
            }
            return resolved;
        }
        return widget.resolved_width(coordinate_width, definition().base_width());
    }

    void select_coordinate_space(const DeclarativeWidgetDefinition &widget) const
    {
        if (widget.coordinate_space == DeclarativeCoordinateSpace::Dialog) {
            graphics_in_dialog();
        } else {
            graphics_reset_dialog();
        }
    }

    int is_visible(const DeclarativeWidgetDefinition &widget) const
    {
        int base_visibility = 1;
        switch (widget.visibility) {
        case DeclarativeVisibility::MainMenu:
            base_visibility = window_get_draw_id() == WINDOW_MAIN_MENU;
            break;
        case DeclarativeVisibility::NotFileDialog:
            base_visibility = window_get_draw_id() != WINDOW_FILE_DIALOG;
            break;
        case DeclarativeVisibility::Always:
        default:
            break;
        }
        if (!base_visibility) {
            return 0;
        }

        int has_condition = 0;
        int condition = 1;
        if (widget.max_screen_height >= 0) {
            has_condition = 1;
            condition = condition && screen_height() <= widget.max_screen_height;
        }
        if (!widget.visible_if_side_margin_lt_text.empty()) {
            has_condition = 1;
            const DeclarativeWidgetDefinition *text_widget =
                definition().widget(widget.visible_if_side_margin_lt_text);
            const int text_width = text_widget ? resolved_text_width(*text_widget) : 0;
            const int side_margin = (screen_width() - definition().base_width()) / 2;
            condition = condition && side_margin < text_width + widget.side_margin_text_padding;
        }
        return has_condition && widget.invert_visibility_condition ? !condition : condition;
    }

    void draw_widget(const DeclarativeWidgetDefinition &widget) const
    {
        switch (widget.type) {
        case DeclarativeWidgetType::Image:
            draw_image(widget);
            break;
        case DeclarativeWidgetType::ImageButton:
            draw_image(widget);
            break;
        case DeclarativeWidgetType::Panel:
            draw_panel(widget);
            break;
        case DeclarativeWidgetType::Label:
            draw_text(widget);
            break;
        case DeclarativeWidgetType::TextButton:
            draw_button(widget);
            break;
        default:
            break;
        }
    }

    void draw_image(const DeclarativeWidgetDefinition &widget) const
    {
        const Bounds bounds = resolve_bounds(widget);
        if (widget.image_collection > 0) {
            Image &image = Image::from_id(Image::group(widget.image_collection) + widget.image_offset);
            if (widget.fullscreen) {
                image.draw_fullscreen_background();
            } else if (bounds.width > 0 && bounds.height > 0) {
                shared_ui_runtime().primitives().draw_image(
                    &image.legacy(), static_cast<float>(bounds.x), static_cast<float>(bounds.y),
                    bounds.width, bounds.height,
                    widget.color, RENDER_SCALING_POLICY_HIGH_QUALITY);
            } else {
                image.draw(bounds.x, bounds.y, widget.color);
            }
            return;
        }

        const ImageGroupEntryRef image = ImageGroupEntryRef::from_group(widget.assetlist_name, widget.image_name);
        const RuntimeDrawSlice slice = image.runtime_slice();
        if (!slice.is_valid()) {
            return;
        }
        if (widget.fullscreen) {
            graphics_renderer()->clear_screen();
            // Authored backgrounds cover the current screen without changing
            // aspect ratio; overflow is centered and cropped by the viewport.
            const double scale = (std::max)(
                static_cast<double>(screen_width()) / slice.width,
                static_cast<double>(screen_height()) / slice.height);
            const int draw_width = static_cast<int>(std::ceil(slice.width * scale));
            const int draw_height = static_cast<int>(std::ceil(slice.height * scale));
            shared_ui_runtime().primitives().draw_runtime_slice(
                slice,
                static_cast<float>((screen_width() - draw_width) / 2),
                static_cast<float>((screen_height() - draw_height) / 2),
                draw_width,
                draw_height,
                widget.color,
                RENDER_SCALING_POLICY_HIGH_QUALITY);
        } else if (bounds.width > 0 && bounds.height > 0) {
            shared_ui_runtime().primitives().draw_runtime_slice(
                slice, static_cast<float>(bounds.x), static_cast<float>(bounds.y),
                bounds.width, bounds.height,
                widget.color, RENDER_SCALING_POLICY_HIGH_QUALITY);
        } else {
            image.draw(bounds.x, bounds.y, widget.color);
        }
    }

    void draw_panel(const DeclarativeWidgetDefinition &widget) const
    {
        const Bounds bounds = resolve_bounds(widget);
        const int width_blocks = widget.width_from_text.empty() ?
            widget.resolved_width_blocks(
                widget.coordinate_space == DeclarativeCoordinateSpace::Screen ? screen_width() : definition().base_width(),
                definition().base_width()) :
            (bounds.width + BLOCK_SIZE - 1) / BLOCK_SIZE;
        const int height_blocks = widget.resolved_height_blocks(
            widget.coordinate_space == DeclarativeCoordinateSpace::Screen ? screen_height() : definition().base_height(),
            definition().base_height());
        switch (widget.style) {
        case DeclarativeWidgetStyle::OuterPanel:
            outer_panel_draw(bounds.x, bounds.y, width_blocks, height_blocks);
            break;
        case DeclarativeWidgetStyle::InnerPanel:
            inner_panel_draw(bounds.x, bounds.y, width_blocks, height_blocks);
            break;
        case DeclarativeWidgetStyle::Solid:
            graphics_fill_rect(bounds.x, bounds.y, bounds.width, bounds.height, widget.color);
            break;
        case DeclarativeWidgetStyle::Label:
            label_draw(bounds.x, bounds.y, width_blocks, widget.label_type);
            break;
        case DeclarativeWidgetStyle::LargeLabel:
            large_label_draw(bounds.x, bounds.y, width_blocks, widget.label_type);
            break;
        case DeclarativeWidgetStyle::None:
        default:
            break;
        }
    }

    const uint8_t *resolved_text(const DeclarativeWidgetDefinition &widget, std::string &storage) const
    {
        if (widget.binding == "system.version") {
            storage = widget.text;
            storage += system_version();
            return string_from_ascii(storage.c_str());
        }
        if (!widget.translation.empty()) {
            return lang_get_string(widget.translation.c_str());
        }
        return string_from_ascii(widget.text.c_str());
    }

    void draw_text(const DeclarativeWidgetDefinition &widget) const
    {
        const Bounds bounds = resolve_bounds(widget);
        std::string storage;
        const uint8_t *text = resolved_text(widget, storage);
        if (!text) {
            return;
        }
        const int x = bounds.x + widget.text_offset_x;
        const int y = bounds.y + widget.text_offset_y;
        const int pixel_size = screen_ui_to_pixel(
            font_definition_for(widget.font)->line_height + widget.font_size_delta);
        switch (widget.text_alignment) {
        case DeclarativeTextAlignment::Center:
            text_draw_centered(text, x, y, bounds.width, widget.font, pixel_size, widget.color);
            break;
        case DeclarativeTextAlignment::Right:
            text_draw_right_aligned(text, x, y, bounds.width, widget.font, pixel_size, widget.color);
            break;
        case DeclarativeTextAlignment::Left:
        default:
            text_draw(text, x, y, widget.font, pixel_size, widget.color);
            break;
        }
    }

    void draw_button(const DeclarativeWidgetDefinition &widget) const
    {
        const Bounds bounds = resolve_bounds(widget);
        const int focused = data.focused_widget == widget.id;
        const int width_blocks = widget.resolved_width_blocks(
            widget.coordinate_space == DeclarativeCoordinateSpace::Screen ? screen_width() : definition().base_width(),
            definition().base_width());
        if (widget.style == DeclarativeWidgetStyle::LargeLabel) {
            large_label_draw(bounds.x, bounds.y, width_blocks, focused ? 1 : widget.label_type);
        } else if (widget.style == DeclarativeWidgetStyle::Label) {
            label_draw(bounds.x, bounds.y, width_blocks, focused ? 1 : widget.label_type);
        } else if (widget.style == DeclarativeWidgetStyle::Solid) {
            graphics_fill_rect(bounds.x, bounds.y, bounds.width, bounds.height, widget.color);
        }
        draw_text(widget);
    }

    const DeclarativeWindow &window_;
};

MainMenuWindow *main_menu_window()
{
    const DeclarativeWindow *window = declarative_window("main_menu");
    if (!window) {
        return nullptr;
    }
    static const DeclarativeWindow *source = nullptr;
    static std::unique_ptr<MainMenuWindow> menu;
    if (source != window) {
        source = window;
        menu = std::make_unique<MainMenuWindow>(*window);
    }
    return menu.get();
}

void draw_foreground()
{
    if (MainMenuWindow *menu = main_menu_window()) {
        menu->draw(DeclarativeDrawPhase::Foreground);
    }
}

void handle_input(const mouse *m, const hotkeys *h)
{
    MainMenuWindow *menu = main_menu_window();
    if (!menu || menu->handle_input(m)) {
        return;
    }
    if (h->escape_pressed) {
        execute_action(menu->definition().escape_action());
    }
    if (h->load_file) {
        execute_action(menu->definition().load_file_action());
    }
}

} // namespace

int window_main_menu_action_is_supported(std::string_view action)
{
    return find_action(action) != nullptr;
}

void window_main_menu_draw_background(void)
{
    if (MainMenuWindow *menu = main_menu_window()) {
        menu->draw(DeclarativeDrawPhase::Background);
    }
}

void window_main_menu_show(int restart_music)
{
    if (restart_music) {
        sound_music_play_intro();
    }
    weather_reset();
    game_campaign_clear();
    data.focused_widget.clear();
    window_type window = {
        WINDOW_MAIN_MENU,
        window_main_menu_draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
