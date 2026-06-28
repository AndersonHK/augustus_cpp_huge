#include "figure/phrase.h"
#include "translation/translation.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/rich_text.h"
#include "widget/city.h"
#include "window/city.h"

#include "figures.h"
#include "figure/figure.h"
#include "window/building/utility.h"

#include "city/view.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/window.h"

static void select_figure(const generic_button *button);
static void figure_action(const generic_button *button);

static generic_button figure_buttons[] = {
    {26, 46, 50, 50, select_figure},
    {86, 46, 50, 50, select_figure, 0, 1},
    {146, 46, 50, 50, select_figure, 0, 2},
    {206, 46, 50, 50, select_figure, 0, 3},
    {266, 46, 50, 50, select_figure, 0, 4},
    {326, 46, 50, 50, select_figure, 0, 5},
    {386, 46, 50, 50, select_figure, 0, 6},
};

static generic_button figure_action_buttons[] = {
    {90, 160, 100, 22, figure_action},
};

static struct {
    int figure_images[7];
    unsigned int focus_button_id;
    unsigned int action_focus_button_id;
    building_info_context *context_for_callback;
} data;

unsigned int window_building_figure_action_focus_button_id()
{
    return data.action_focus_button_id;
}

void window_building_draw_figure_list(building_info_context *c)
{
    Figure *f = Figure::get(c->figure.figure_ids[c->figure.selected_index]);
    if (f && f->uses_tall_info_panel()) {
        inner_panel_draw(c->x_offset + 16, c->y_offset + 11, c->width_blocks - 2, 15);
        button_border_draw(c->x_offset + 24, c->y_offset + 73, BLOCK_SIZE * (c->width_blocks - 3), 170, 0); //white border
        if (c->figure.count <= 0) {
            lang_text_draw_centered("main_strings.70.0", c->x_offset, c->y_offset + 120, BLOCK_SIZE * c->width_blocks, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        } else {
            for (int i = 0; i < c->figure.count; i++) {
                button_border_draw(c->x_offset + 60 * i + 25, c->y_offset + 16, 52, 52, i == c->figure.selected_index);
                graphics_draw_from_image(data.figure_images[i], c->x_offset + 27 + 60 * i, c->y_offset + 18);
            }
            f->draw_figure_info(c);
            c->figure.drawn = 1;
            return;
        }
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 40, c->width_blocks - 2, 13);
    if (c->figure.count <= 0) {
        lang_text_draw_centered("main_strings.70.0", c->x_offset, c->y_offset + 120, BLOCK_SIZE * c->width_blocks, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        for (int i = 0; i < c->figure.count; i++) {
            button_border_draw(c->x_offset + 60 * i + 25, c->y_offset + 45, 52, 52, i == c->figure.selected_index);
            button_border_draw(c->x_offset + 24, c->y_offset + 102, BLOCK_SIZE * (c->width_blocks - 3), 138, 0);
            graphics_draw_from_image(data.figure_images[i], c->x_offset + 27 + 60 * i, c->y_offset + 47);
        }
        Figure::get(c->figure.figure_ids[c->figure.selected_index])->draw_figure_info(c);
    }
    c->figure.drawn = 1;
}

static void draw_figure_in_city(int figure_id, pixel_coordinate *coord)
{
    int x_cam, y_cam;
    city_view_get_camera_in_pixels(&x_cam, &y_cam);
    int scale = city_view_get_scale();

    int grid_offset = Figure::get(figure_id)->grid_offset;
    int x, y;
    city_view_grid_offset_to_xy_view(grid_offset, &x, &y);

    city_view_set_scale(100);
    city_view_set_camera(x - 2, y - 6);

    widget_city_draw_for_figure(figure_id, coord);

    city_view_set_scale(scale);
    city_view_set_camera_from_pixel_position(x_cam, y_cam);
}

void window_building_prepare_figure_list(building_info_context *c)
{
    if (c->figure.count > 0) {
        pixel_coordinate coord = { 0, 0 };
        screen_set_pixel_render_scale();
        for (int i = 0; i < c->figure.count; i++) {
            draw_figure_in_city(c->figure.figure_ids[i], &coord);
            data.figure_images[i] = graphics_save_to_image(data.figure_images[i], coord.x, coord.y, 48, 48);
        }
        widget_city_draw();
        screen_set_ui_render_scale();
    }
}

int window_building_handle_mouse_figure_list(const mouse *m, building_info_context *c)
{
    data.context_for_callback = c;
    Figure *f = Figure::get(c->figure.figure_ids[c->figure.selected_index]);
    const int button_y = f && f->uses_tall_info_panel() ? 17 : 46;
    for (int i = 0; i < c->figure.count; i++) {
        figure_buttons[i].y = static_cast<short>(button_y);
    }
    generic_button *buttons = figure_buttons;
    int button_count = c->figure.count;

    int handled = GenericButtonList(buttons, button_count).handle_mouse(
        *m,
        c->x_offset,
        c->y_offset,
        &data.focus_button_id
    );
    data.context_for_callback = 0;

    if (f && f->has_info_action_button()) {
        figure_action_buttons[0].parameter1 = f->id();
        unsigned int focus_id = data.action_focus_button_id;
        GenericButtonList(figure_action_buttons, 1).handle_mouse(
            *m,
            c->x_offset,
            c->y_offset,
            &data.action_focus_button_id
        );
        if (focus_id != data.action_focus_button_id) {
            window_request_refresh();
        }
    }

    if (c->terrain_type == TERRAIN_INFO_BRIDGE) {
        if (c->show_special_orders) {
            return window_building_handle_mouse_roadblock_orders(m, c);
        } else {
            return window_building_handle_mouse_roadblock_button(m, c);
        }
    }
    return handled;
}

static void select_figure(const generic_button *button)
{
    int index = button->parameter1;
    data.context_for_callback->figure.selected_index = index;
    window_building_play_figure_phrase(data.context_for_callback);
    window_invalidate();
}

void window_building_play_figure_phrase(building_info_context *c)
{
    int figure_id = c->figure.figure_ids[c->figure.selected_index];
    Figure *f = Figure::get(figure_id);
    c->figure.sound_id = figure_phrase_play(f);
    c->figure.phrase_id = f->phrase_id;
}

static void figure_action(const generic_button *button)
{
    int figure_id = button->parameter1;
    if (Figure *figure = Figure::get(figure_id)) {
        figure->handle_info_action_button();
    }
    window_city_show();
}
