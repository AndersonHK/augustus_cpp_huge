#include "building/count.h"
#include "city/buildings.h"
#include "graphics/arrow_button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "widget/minimap.h"
#include "widget/sidebar/city.h"
#include "widget/sidebar/common.h"
#include "widget/sidebar/extra.h"
#include "window/city.h"
#include "window/military_menu.h"

#include "military.h"

#include "widget/sidebar/slide.h"
#include "window/building/military.h"

#include "assets/assets.h"
#include "city/view.h"
#include "core/calc.h"
#include "figure/formation.h"
#include "figure/formation_legion.h"
#include "graphics/image_button.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "sound/speech.h"


#define LAYOUTS_PER_LEGION 5

#define MILITARY_PANEL_HEIGHT 474
#define Y_OFFSET_PANEL_START 176
#define Y_OFFSET_LAYOUT_BUTTONS 156
#define Y_OFFSET_BOTTOM_BUTTONS 257
#define MILITARY_PANEL_BLOCKS 18
#define CONTENT_PADDING 10
#define CONTENT_WIDTH (SIDEBAR_EXPANDED_WIDTH - 2 * CONTENT_PADDING)

static const char *IMAGE_OFFSETS_TO_FORMATION[7] = {
    "column",
    "tortoise",
    "double_line_1",
    "double_line_2",
    "mop_up",
    "single_line_1",
    "single_line_2"
};

static const int LAYOUT_IMAGE_OFFSETS_LEGIONARY[2][LAYOUTS_PER_LEGION] = {
    {1, 0, 2, 3, 4}, {1, 0, 3, 2, 4},
};

static const int LAYOUT_IMAGE_OFFSETS_AUXILIARY[2][LAYOUTS_PER_LEGION] = {
    {5, 6, 2, 3, 4}, {6, 5, 3, 2, 4},
};

static const char *LAYOUT_BUTTON_INDEXES_LEGIONARY[2][LAYOUTS_PER_LEGION] = {
    {
        "tortoise", "column", "double_line_1", "double_line_2", "mop_up"
    },
    {
        "tortoise", "column", "double_line_2", "double_line_1", "mop_up"
    }
};

static const char *LAYOUT_BUTTON_INDEXES_AUXILIARY[2][LAYOUTS_PER_LEGION] = {
    {
        "single_line_1", "single_line_2", "double_line_1", "double_line_2", "mop_up"
    },
    {
        "single_line_2", "single_line_1", "double_line_2", "double_line_1", "mop_up"
    }
};

static void button_military_menu(int param1, int param2);
static void button_close_military_sidebar(int param1, int param2);
static void button_cycle_legion(int cycle_forward, int param2);
static void button_select_formation_layout(const generic_button *button);
static void button_go_to_legion(const generic_button *button);
static void button_return_to_fort(const generic_button *button);
static void button_empire_service(const generic_button *button);

static image_button buttons_title_close[] = {
    {123, 4, 39, 26, IB_NORMAL, GROUP_OK_CANCEL_SCROLL_BUTTONS, 4,
        button_close_military_sidebar, button_none, 0, 0, 1},
    {4, 3, 117, 31, IB_NORMAL, 93, 0, button_military_menu, button_none, 0, 0, 1}
};

static arrow_button buttons_cycle_legion[] = {
    {10, 10, 19, 24, button_cycle_legion, 0, 0},
    {126, 10, 21, 24, button_cycle_legion, 1, 0},
};

static generic_button buttons_formation_layout[LAYOUTS_PER_LEGION - 2][LAYOUTS_PER_LEGION] = {
    {
        {8, 0, 46, 46, button_select_formation_layout},
        {58, 0, 46, 46, button_select_formation_layout, 0, 1},
        {108, 0, 46, 46, button_select_formation_layout, 0, 2}
    },
    {
        {33, 50, 46, 46, button_select_formation_layout},
        {33, 0, 46, 46, button_select_formation_layout, 0, 1},
        {83, 0, 46, 46, button_select_formation_layout, 0, 2},
        {83, 50, 46, 46, button_select_formation_layout, 0, 3}
    },
    {
        {33, 0, 46, 46, button_select_formation_layout},
        {83, 0, 46, 46, button_select_formation_layout, 0, 1},
        {8, 50, 46, 46, button_select_formation_layout, 0, 2},
        {58, 50, 46, 46, button_select_formation_layout, 0, 3},
        {108, 50, 46, 46, button_select_formation_layout, 0, 4}
    }
};

static generic_button buttons_bottom[] = {
    {16, 0, 30, 30, button_go_to_legion},
    {66, 0, 30, 30, button_return_to_fort},
    {116, 0, 30, 30, button_empire_service},
};

typedef struct {
    int formation_id;
    int standard_id; //figure id of the standard in the figure array
    int soldiers;
    int health;
    int morale;
    const FormationLayoutDef *layout_definition;
    int is_at_fort;
    int empire_service;
} legion_info;

static struct {
    legion_info active_legion;
    unsigned int top_buttons_focus_id;
    unsigned int inner_buttons_focus_id;
    unsigned int bottom_buttons_focus_id;
    int city_view_was_collapsed;
} data;

static void draw_layout_buttons(int x, int y, int background, const formation *m)
{
    int index = 0;
    if (city_view_orientation() == DIR_6_LEFT || city_view_orientation() == DIR_2_RIGHT) {
        index = 1;
    }
    const int *offsets = (m->figure_type == FIGURE_FORT_LEGIONARY || m->figure_type == FIGURE_FORT_INFANTRY) ?
        LAYOUT_IMAGE_OFFSETS_LEGIONARY[index] : LAYOUT_IMAGE_OFFSETS_AUXILIARY[index];
    int formation_types = m->available_layout_count();

    int start_formation = LAYOUTS_PER_LEGION - formation_types;
    const generic_button *button_offsets = buttons_formation_layout[formation_types - 3];

    for (unsigned int i = start_formation; i < LAYOUTS_PER_LEGION; i++) {
        const generic_button *btn = &button_offsets[i - start_formation];

        if (background) {
            Image::from_id(Image::group(GROUP_FORT_FORMATIONS) + offsets[i]).draw((x + btn->x + 3) * 2, (y + btn->y + 3) * 2, COLOR_MASK_NONE, 2.0f);
        } else {
            int is_selected_formation = m->uses_layout(IMAGE_OFFSETS_TO_FORMATION[offsets[i]]);
            int is_button_focused = i == data.inner_buttons_focus_id - 1 + start_formation;
            button_border_draw(x + btn->x, y + btn->y, 46, 46, is_button_focused || is_selected_formation);
        }
    }
}

static int get_health_text_id(int health)
{
    if (health <= 0) {
        return 26;
    } else if (health <= 20) {
        return 27;
    } else if (health <= 40) {
        return 28;
    } else if (health <= 55) {
        return 29;
    } else if (health <= 70) {
        return 30;
    } else if (health <= 90) {
        return 31;
    } else {
        return 32;
    }
}

int widget_sidebar_military_get_standard_image(int legion_id)
{
    switch (legion_id) {
        case 0: return 0; // No standard for non-legion formations;
        case 1:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS + 0);
        case 2:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 1;
        case 3:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 2;
        case 4:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 3;
        case 5:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 4;
        case 6:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 5;
        case 7:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 6;
        case 8:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 7;
        case 9:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 8;
        case 10:  return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 9;
        case 11: return assets_get_image_id("UI", "11Legion_Elephants");
        case 12: return assets_get_image_id("UI", "12Legion_Thunder_Bolts");
        case 13: return assets_get_image_id("UI", "13Legion_Bulls");
        case 14: return assets_get_image_id("UI", "14Legion_Centaurs");
        case 15: return assets_get_image_id("UI", "15Legion_Octopi");
        case 16: return assets_get_image_id("UI", "16Legion_Bears");
        case 17: return assets_get_image_id("UI", "17Legion_Scorpions");
        case 18: return assets_get_image_id("UI", "18Legion_Camels");
        case 19: return assets_get_image_id("UI", "19Legion_Dolphins");
        case 20: return assets_get_image_id("UI", "20Legion_Sea_Goats");
        default: return Image::group(GROUP_FIGURE_FORT_STANDARD_ICONS + 9);
    }
}

int widget_sidebar_military_get_legion_name_group(int legion_id)
{
    return legion_id <= 10 ? 138 : 10000;
}

int widget_sidebar_military_get_legion_name_id(int legion_id)
{
    if (legion_id <= 10) {
        return legion_id - 1; // old index was 0-based, now 1-based
    } else {
        return legion_id - 11;
    }
}

const uint8_t *widget_sidebar_military_get_legion_name_text(int group, int id)
{
    static const translation_key extra_legion_names[] = {
        "TR_BUILDING_FORT_STANDARD_ELEPHANTS",
        "TR_BUILDING_FORT_STANDARD_THUNDER_BOLTS",
        "TR_BUILDING_FORT_STANDARD_BULLS",
        "TR_BUILDING_FORT_STANDARD_CENTAURS",
        "TR_BUILDING_FORT_STANDARD_OCTOPI",
        "TR_BUILDING_FORT_STANDARD_BEARS",
        "TR_BUILDING_FORT_STANDARD_SCORPIONS",
        "TR_BUILDING_FORT_STANDARD_CAMELS",
        "TR_BUILDING_FORT_STANDARD_DOLPHINS",
        "TR_BUILDING_FORT_STANDARD_SEA_GOATS",
    };
    if (group == 10000 && id >= 0 && id < static_cast<int>(sizeof(extra_legion_names) / sizeof(extra_legion_names[0]))) {
        return translation_for(extra_legion_names[id]);
    }
    return lang_get_string(current_string_key(group, id));
}

static void clear_focus_buttons(void)
{
    data.top_buttons_focus_id = 0;
    data.inner_buttons_focus_id = 0;
    data.bottom_buttons_focus_id = 0;
}

static void clear_legion_info(legion_info *legion)
{
    legion->health = 0;
    legion->layout_definition = nullptr;
    legion->morale = 0;
    legion->soldiers = 0;
    legion->is_at_fort = 0;
    legion->empire_service = 0;
}

static void update_legion_info(legion_info *legion, const formation *m)
{
    legion->health = calc_percentage(m->total_damage, m->max_total_damage);
            legion->layout_definition = m->layout_definition;
    legion->morale = m->morale;
        legion->soldiers = m->count_alive_figures();
    legion->is_at_fort = m->is_at_fort;
    legion->empire_service = m->empire_service;
}

static void draw_military_info_text(int x_offset, int y_offset)
{
    legion_info *legion = &data.active_legion;
    const formation *m = formation_get(legion->formation_id);
    update_legion_info(legion, m);

    int formation_image_id = m->legion_flag_id;
    const image *formation_image = image_get(formation_image_id);

    // Legion name
    Image::from_id(formation_image_id).draw(x_offset + (CONTENT_WIDTH - formation_image->width - formation_image->x_offset) / 2, y_offset + 12);

    text_draw_centered(widget_sidebar_military_get_legion_name_text(m->legion_name_group, m->legion_name_id),
        x_offset, y_offset + 40, CONTENT_WIDTH, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    // Number of soldiers
    int width = text_draw_number(m->num_figures, '@', " ", x_offset, y_offset + 60, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    if (m->figure_type == FIGURE_FORT_INFANTRY) {
        text_draw(translation_for_key("TR_WINDOW_ADVISOR_MILITARY_INFANTRY"), x_offset + width, y_offset + 60, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    } else if (m->figure_type == FIGURE_FORT_ARCHER) {
        text_draw(translation_for_key("TR_WINDOW_ADVISOR_MILITARY_ARCHER"), x_offset + width, y_offset + 60, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    } else if (m->figure_type == FIGURE_FORT_LEGIONARY) {
        text_draw(translation_for_key("TR_WINDOW_ADVISOR_LEGIONARIES"), x_offset + width, y_offset + 60, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    } else if (m->figure_type == FIGURE_FORT_JAVELIN) {
        text_draw(translation_for_key("TR_WINDOW_ADVISOR_JAVELIN"), x_offset + width, y_offset + 60, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    } else if (m->figure_type == FIGURE_FORT_MOUNTED) {
        text_draw(translation_for_key("TR_WINDOW_ADVISOR_MOUNTED"), x_offset + width, y_offset + 60, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    }
    // No soldiers
    if (!m->num_figures) {
        int group_id, text_id;
        if (m->cursed_by_mars) {
            group_id = 89;
            text_id = 1;
        } else if (city_buildings_has_barracks()) {
            group_id = 138;
            text_id = 10;
        } else {
            group_id = 138;
            text_id = 11;
        }
        lang_text_draw_multiline(current_string_key(group_id, text_id), x_offset, y_offset + 80, CONTENT_WIDTH, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
        clear_legion_info(legion);
        return;
    }

    int ellipsized_width = CONTENT_WIDTH + CONTENT_PADDING / 2;
    // Morale
    lang_text_draw_ellipsized("main_strings.138.36", x_offset, y_offset + 80, ellipsized_width, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    lang_text_draw_ellipsized(current_string_key(138, 37 + m->morale / 5), x_offset + 4, y_offset + 98,
        ellipsized_width, m->morale < 13 ? FONT_NORMAL_RED : FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(m->morale < 13 ? FONT_NORMAL_RED : FONT_NORMAL_GREEN)->line_height));

    // Health
    lang_text_draw_ellipsized("main_strings.138.24", x_offset, y_offset + 120, ellipsized_width, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    lang_text_draw_ellipsized(current_string_key(138, get_health_text_id(legion->health)), x_offset + 4, y_offset + 138,
        ellipsized_width, legion->health < 55 ? FONT_NORMAL_GREEN : FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(legion->health < 55 ? FONT_NORMAL_GREEN : FONT_NORMAL_RED)->line_height));
}

static void draw_military_info_buttons(int x_offset, int y_offset)
{
    if (!data.active_legion.soldiers) {
        return;
    }
    const formation *m = formation_get(data.active_legion.formation_id);
    // Formation layout
    draw_layout_buttons(x_offset, y_offset + Y_OFFSET_LAYOUT_BUTTONS, 1, m);

    int formation_options_image = Image::group(GROUP_FORT_ICONS);

    // Go to legion button
    const generic_button *btn = buttons_bottom;
    Image::from_id(formation_options_image).draw(x_offset + btn->x + 3, y_offset + 260);

    // Return to fort button
    ++btn;
    Image::from_id(formation_options_image + 1 + m->is_at_fort).draw(x_offset + btn->x + 3, y_offset + 260);

    // Empire service button
    ++btn;
    Image::from_id(formation_options_image + 4 - m->empire_service).draw(x_offset + btn->x + 3, y_offset + 260);
}

static void draw_military_panel_background(int x_offset)
{
    graphics_draw_line(x_offset, x_offset, Y_OFFSET_PANEL_START,
        Y_OFFSET_PANEL_START + MILITARY_PANEL_BLOCKS * BLOCK_SIZE, COLOR_WHITE);
    graphics_draw_line(x_offset + SIDEBAR_EXPANDED_WIDTH - 1, x_offset + SIDEBAR_EXPANDED_WIDTH - 1,
        Y_OFFSET_PANEL_START, Y_OFFSET_PANEL_START + MILITARY_PANEL_BLOCKS * BLOCK_SIZE, COLOR_SIDEBAR);
    inner_panel_draw(x_offset + 1, Y_OFFSET_PANEL_START + 10,
        SIDEBAR_EXPANDED_WIDTH / BLOCK_SIZE, MILITARY_PANEL_BLOCKS);
    inner_panel_draw(x_offset + 1, Y_OFFSET_PANEL_START, SIDEBAR_EXPANDED_WIDTH / BLOCK_SIZE, 1);

    draw_military_info_text(x_offset + CONTENT_PADDING, Y_OFFSET_PANEL_START);
    draw_military_info_buttons(x_offset, Y_OFFSET_PANEL_START);
}

static void draw_legion_buttons(int x_offset, int y_offset)
{
    int num_legions = formation_get_num_legions();
    if (num_legions > 1) {
        arrow_buttons_draw(x_offset, y_offset, buttons_cycle_legion, 2);
    }
    const formation *m = formation_get(data.active_legion.formation_id);
    if (m->num_figures) {
        draw_layout_buttons(x_offset, y_offset + Y_OFFSET_LAYOUT_BUTTONS, 0, m);
        for (unsigned int i = 0; i < 3; i++) {
            button_border_draw(x_offset + buttons_bottom[i].x, y_offset + Y_OFFSET_BOTTOM_BUTTONS,
                30, 30, data.bottom_buttons_focus_id == i + 1);
        }
    }

}

static void draw_background(int x_offset)
{
    Image::from_id(Image::group(GROUP_SIDE_PANEL) + 1).draw(x_offset, 24);
    image_buttons_draw(x_offset, 24, buttons_title_close, 2);
    lang_text_draw_centered("main_strings.61.5", x_offset, 32, 117, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    widget_minimap_update(0);
    widget_minimap_draw_decorated(x_offset + 8, 59, MINIMAP_WIDTH, MINIMAP_HEIGHT);
    draw_military_panel_background(x_offset);
    draw_legion_buttons(x_offset, Y_OFFSET_PANEL_START);
    int extra_height = sidebar_extra_draw_background(x_offset, MILITARY_PANEL_HEIGHT,
        SIDEBAR_EXPANDED_WIDTH, sidebar_common_get_height() - MILITARY_PANEL_HEIGHT + TOP_MENU_HEIGHT,
        0, SIDEBAR_EXTRA_DISPLAY_ALL);
    sidebar_extra_draw_foreground();

    sidebar_common_draw_relief(x_offset, MILITARY_PANEL_HEIGHT + extra_height, GROUP_SIDE_PANEL, 0);
}

void widget_sidebar_military_draw_background(void)
{
    draw_background(sidebar_common_get_x_offset_expanded());
}

static int has_legion_changed(const legion_info *legion, const formation *m)
{
    return legion->health != calc_percentage(m->total_damage, m->max_total_damage) ||
                legion->layout_definition != m->layout_definition ||
        legion->morale != m->morale ||
        legion->soldiers != m->num_figures ||
        legion->is_at_fort != m->is_at_fort ||
        legion->empire_service != m->empire_service;
}

static void draw_military_panel_foreground(int x_offset)
{
    const formation *m = formation_get(data.active_legion.formation_id);
    if (has_legion_changed(&data.active_legion, m)) {
        draw_military_panel_background(x_offset);
    }
    draw_legion_buttons(x_offset, Y_OFFSET_PANEL_START);
}

static void draw_foreground(int x_offset)
{
    widget_minimap_draw_decorated(x_offset + 8, 59, MINIMAP_WIDTH, MINIMAP_HEIGHT);
    image_buttons_draw(x_offset, 24, buttons_title_close, 2);
    lang_text_draw_centered("main_strings.61.5", x_offset, 32, 117, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    draw_military_panel_foreground(x_offset);
    sidebar_extra_draw_foreground();
}

void widget_sidebar_military_draw_foreground(void)
{
    draw_foreground(sidebar_common_get_x_offset_expanded());
}

static void draw_sliding(int x_offset)
{
    draw_background(x_offset);
    draw_foreground(x_offset);
}

int widget_sidebar_military_handle_input(const mouse *m)
{
    int x_offset = sidebar_common_get_x_offset_expanded();
    if (image_buttons_handle_mouse(m, x_offset, 24, buttons_title_close, 2, &data.top_buttons_focus_id)) {
        return 1;
    }
    int num_legions = formation_get_num_legions();
    if (num_legions > 1 &&
        arrow_buttons_handle_mouse(m, x_offset, Y_OFFSET_PANEL_START, buttons_cycle_legion, 2, 0)) {
        return 1;
    }
    const formation *selected_legion = formation_get(data.active_legion.formation_id);
    if (data.active_legion.soldiers > 0) {
        generic_button *layout_buttons = buttons_formation_layout[selected_legion->available_layout_count() - 3];
        if (GenericButtonList(layout_buttons, selected_legion->available_layout_count()).handle_mouse(
            *m,
            x_offset,
            Y_OFFSET_PANEL_START + Y_OFFSET_LAYOUT_BUTTONS,
            &data.inner_buttons_focus_id
        )) {
            return 1;
        }
        if (GenericButtonList(buttons_bottom, 3).handle_mouse(
            *m,
            x_offset,
            Y_OFFSET_PANEL_START + Y_OFFSET_BOTTOM_BUTTONS,
            &data.bottom_buttons_focus_id
        )) {
            return 1;
        }
    }
    return sidebar_extra_handle_mouse(m);
}

static int get_layout_text_id(const char *layout_key)
{
    const FormationLayoutDef *layout = formation_layout_registry_impl::find_layout(layout_key);
    if (!layout || layout->matches_key("single_line_1") || layout->matches_key("single_line_2")) {
        return 16;
    }
    if (layout->matches_key("double_line_1") || layout->matches_key("double_line_2")) {
        return 14;
    }
    if (layout->matches_key("tortoise")) {
        return 12;
    }
    if (layout->matches_key("mop_up")) {
        return 15;
    }
    return layout->matches_key("column") ? 13 : 16;
}

int widget_sidebar_military_get_tooltip_text(tooltip_context *c)
{
    if (data.top_buttons_focus_id) {
        if (data.top_buttons_focus_id == 1) {
            c->text_group = 68;
            return 2;
        }
        return 0;
    }
    if (data.inner_buttons_focus_id) {
        int index = data.inner_buttons_focus_id - 1;
        const char *layout;
        const formation *m = formation_get(data.active_legion.formation_id);
        if (m->figure_type == FIGURE_FORT_LEGIONARY || m->figure_type == FIGURE_FORT_INFANTRY) {
            int index_increase = LAYOUTS_PER_LEGION - m->available_layout_count();
            if (index > 4 - index_increase) {
                return 0;
            }
            index += index_increase;
            layout = LAYOUT_BUTTON_INDEXES_LEGIONARY[0][index];
        } else {
            layout = LAYOUT_BUTTON_INDEXES_AUXILIARY[0][index];
        }
        c->text_group = 138;
        return get_layout_text_id(layout);
    }
    if (data.bottom_buttons_focus_id) {
        c->extra_text_type = TOOLTIP_EXTRA_TEXT_JOINED_BY_SPACE;
        c->num_extra_texts = 1;
        c->text_group = 51;
        c->extra_text_groups[0] = 51;
        int text_id = data.bottom_buttons_focus_id * 2;
        c->extra_text_ids[0] = text_id;
        return text_id - 1;
    }
    return 0;
}

static void set_formation_id(int formation_id)
{
    data.active_legion.formation_id = formation_id;
    clear_legion_info(&data.active_legion);
    widget_minimap_invalidate();
}

static void slide_in_finished(void)
{
    if (data.city_view_was_collapsed) {
        city_view_toggle_sidebar();
    }
    widget_minimap_invalidate();
    window_city_return();
}

static void slide_out_finished(void)
{
    data.active_legion.formation_id = 0;
    widget_minimap_invalidate();
    window_city_show();
}

int widget_sidebar_military_enter(int formation_id)
{
    clear_focus_buttons();
    int had_selected_legions = data.active_legion.formation_id;
    set_formation_id(formation_id);
    if (had_selected_legions) {
        return 0;
    }
    data.city_view_was_collapsed = city_view_is_sidebar_collapsed();
    if (data.city_view_was_collapsed) {
        city_view_start_sidebar_toggle();
        sidebar_slide(SLIDE_DIRECTION_IN, widget_sidebar_city_draw_background, draw_sliding, slide_in_finished);
    } else {
        slide_in_finished();
    }
    return 1;
}

int widget_sidebar_military_exit(void)
{
    clear_focus_buttons();
    if (!window_is(WINDOW_CITY_MILITARY)) {
        widget_minimap_invalidate();
        return 0;
    }
    if (data.city_view_was_collapsed) {
        city_view_toggle_sidebar();
        sidebar_slide(SLIDE_DIRECTION_OUT, widget_sidebar_city_draw_background, draw_sliding, slide_out_finished);
    } else {
        slide_out_finished();
    }
    return 1;
}

static void button_military_menu(int param1, int param2)
{
    (void)param1;
    (void)param2;

    window_military_menu_show();
}

static void button_close_military_sidebar(int param1, int param2)
{
    (void)param1;
    (void)param2;

    widget_sidebar_military_exit();
}

static void button_cycle_legion(int cycle_forward, int param2)
{
    (void)param2;

    legion_info *legion = &data.active_legion;
    int step = cycle_forward ? 1 : -1;
    for (int i = legion->formation_id + step; i != legion->formation_id; i += step) {
        if (i == 0) {
            i = formation_count();
        } else if (i > formation_count()) {
            i = 1;
        }
        const formation *m = formation_get(i);
        if (m->in_use && !m->is_herd && m->is_legion) {
            legion->formation_id = i;
            break;
        }
    }
    formation_set_selected(legion->formation_id);
    set_formation_id(legion->formation_id);
}

static void button_select_formation_layout(const generic_button *button)
{
    int index = button->parameter1;
    formation *m = formation_get(data.active_legion.formation_id);
    if (m->in_distant_battle) {
        return;
    }
    const char *const *layout_indexes;
    int swap_lines = city_view_orientation() == DIR_6_LEFT || city_view_orientation() == DIR_2_RIGHT;
    if (m->figure_type == FIGURE_FORT_LEGIONARY || m->figure_type == FIGURE_FORT_INFANTRY) {
        int index_increase = LAYOUTS_PER_LEGION - m->available_layout_count();
        if (index > 4 - index_increase) {
            return;
        }
        index += index_increase;
        layout_indexes = LAYOUT_BUTTON_INDEXES_LEGIONARY[swap_lines];
    } else {
        layout_indexes = LAYOUT_BUTTON_INDEXES_AUXILIARY[swap_lines];
    }
    formation_legion_change_layout(m, layout_indexes[index]);
    switch (index) {
        case 0: sound_speech_play_file("wavs/cohort1.wav"); break;
        case 1: sound_speech_play_file("wavs/cohort2.wav"); break;
        case 2: sound_speech_play_file("wavs/cohort3.wav"); break;
        case 3: sound_speech_play_file("wavs/cohort4.wav"); break;
        case 4: sound_speech_play_file("wavs/cohort5.wav"); break;
    }
}

static void button_go_to_legion(const generic_button *button)
{
    (void)button;

    const formation *m = formation_get(data.active_legion.formation_id);
    city_view_go_to_grid_offset(map_grid_offset(m->x_home, m->y_home));
}

static void button_return_to_fort(const generic_button *button)
{
    (void)button;

    formation *m = formation_get(data.active_legion.formation_id);
    if (!m->in_distant_battle) {
        formation_legion_return_home(m);
    }
}

static void button_empire_service(const generic_button *button)
{
    (void)button;

    formation_get(data.active_legion.formation_id)->toggle_empire_service();
    formation_calculate_figures();
}
