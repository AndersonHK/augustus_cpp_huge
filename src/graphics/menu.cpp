#include "menu.h"

#include "core/calc.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/ui_runtime_api.h"

#define TOP_MENU_BASE_X_OFFSET 10
#define MENU_BASE_TEXT_Y_OFFSET 6
#define MENU_ITEM_HEIGHT 20

static int menu_bar_item_width(const menu_bar_item &item, font_t font, int pixel_size)
{
    return item.text_key ? lang_text_get_width(item.text_key, font, pixel_size) :
        lang_text_get_width(current_string_key(item.text_group, 0), font, pixel_size);
}

static int menu_bar_item_draw(const menu_bar_item &item, int x, int y, font_t font, int pixel_size)
{
    return item.text_key ? lang_text_draw(item.text_key, x, y, font, pixel_size) :
        lang_text_draw(current_string_key(item.text_group, 0), x, y, font, pixel_size);
}

static int menu_item_width(const menu_item &item, font_t font, int pixel_size)
{
    return item.text_key ? lang_text_get_width(item.text_key, font, pixel_size) :
        lang_text_get_width(current_string_key(item.text_group, item.text_number), font, pixel_size);
}

static int menu_item_draw(const menu_item &item, int x, int y, font_t font, int pixel_size, color_t color)
{
    if (item.text_key) {
        return color == COLOR_MASK_NONE ?
            lang_text_draw(item.text_key, x, y, font, pixel_size) :
            lang_text_draw_colored(item.text_key, x, y, font, pixel_size, color);
    }
    return color == COLOR_MASK_NONE ?
        lang_text_draw(current_string_key(item.text_group, item.text_number), x, y, font, pixel_size) :
        lang_text_draw_colored(current_string_key(item.text_group, item.text_number), x, y, font, pixel_size, color);
}

int menu_bar_draw(menu_bar_item *items, int num_items, int max_width)
{
    int total_text_width = 0;
    for (int i = 0; i < num_items; i++) {
        total_text_width += menu_bar_item_width(
            items[i], FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }
    int spacing_width = (max_width - total_text_width - TOP_MENU_BASE_X_OFFSET) / (num_items - 1);
    spacing_width = calc_bound(spacing_width, 0, 32);

    short x_offset = TOP_MENU_BASE_X_OFFSET;
    for (int i = 0; i < num_items; i++) {
        items[i].x_start = x_offset;
        x_offset += menu_bar_item_draw(
            items[i], x_offset, MENU_BASE_TEXT_Y_OFFSET, FONT_NORMAL_GREEN,
            screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        items[i].x_end = x_offset;
        x_offset += spacing_width;
    }

    return x_offset;
}

static int get_menu_bar_item(const mouse *m, menu_bar_item *items, int num_items)
{
    for (int i = 0; i < num_items; i++) {
        if (items[i].x_start <= m->x &&
            items[i].x_end > m->x &&
            MENU_BASE_TEXT_Y_OFFSET <= m->y &&
            MENU_BASE_TEXT_Y_OFFSET + 12 > m->y) {
            return i + 1;
        }
    }
    return 0;
}

int menu_bar_handle_mouse(const mouse *m, menu_bar_item *items, int num_items, int *focus_menu_id)
{
    int menu_id = get_menu_bar_item(m, items, num_items);
    if (focus_menu_id) {
        *focus_menu_id = menu_id;
    }
    return menu_id;
}

static void calculate_menu_dimensions(menu_bar_item *menu)
{
    int max_width = 0;
    int height_pixels = MENU_ITEM_HEIGHT;
    for (int i = 0; i < menu->num_items; i++) {
        menu_item *sub = &menu->items[i];
        int width_pixels = menu_item_width(
            *sub, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        if (width_pixels > max_width) {
            max_width = width_pixels;
        }
        height_pixels += MENU_ITEM_HEIGHT;
    }
    int blocks = (max_width + 8) / BLOCK_SIZE + 1; // 1 block padding
    menu->calculated_width_blocks = blocks < 10 ? 10 : blocks;
    menu->calculated_height_blocks = height_pixels / BLOCK_SIZE;
}

void menu_draw(menu_bar_item *menu, int focus_item_id)
{
    if (menu->calculated_width_blocks == 0 || menu->calculated_height_blocks == 0) {
        calculate_menu_dimensions(menu);
    }
    unbordered_panel_draw(menu->x_start, TOP_MENU_HEIGHT,
        menu->calculated_width_blocks, menu->calculated_height_blocks);
    int y_offset = TOP_MENU_HEIGHT + MENU_BASE_TEXT_Y_OFFSET * 2;
    for (int i = 0; i < menu->num_items; i++) {
        menu_item *sub = &menu->items[i];
        if (i == focus_item_id - 1) {
            graphics_fill_rect(menu->x_start, y_offset - 4,
                BLOCK_SIZE * menu->calculated_width_blocks, 20, COLOR_BLACK);
            menu_item_draw(*sub, menu->x_start + 8, y_offset, FONT_NORMAL_PLAIN,
                screen_ui_to_pixel(font_definition_for(FONT_NORMAL_PLAIN)->line_height), COLOR_FONT_ORANGE);
        } else {
            menu_item_draw(*sub, menu->x_start + 8, y_offset, FONT_NORMAL_BLACK,
                screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), COLOR_MASK_NONE);
        }
        y_offset += MENU_ITEM_HEIGHT;
    }
}

static int get_menu_item(const mouse *m, menu_bar_item *menu)
{
    int y_offset = TOP_MENU_HEIGHT + MENU_BASE_TEXT_Y_OFFSET * 2;
    for (int i = 0; i < menu->num_items; i++) {
        if (menu->x_start <= m->x &&
            menu->x_start + BLOCK_SIZE * menu->calculated_width_blocks > m->x &&
            y_offset - 2 <= m->y &&
            y_offset + 19 > m->y) {
            return i + 1;
        }
        y_offset += MENU_ITEM_HEIGHT;
    }
    return 0;
}

int menu_handle_mouse(const mouse *m, menu_bar_item *menu, int *focus_item_id)
{
    int item_id = get_menu_item(m, menu);
    if (focus_item_id) {
        *focus_item_id = item_id;
    }
    if (!item_id) {
        return 0;
    }
    if (m->left.went_up) {
        menu_item *item = &menu->items[item_id - 1];
        item->left_click_handler(item->parameter);
    }
    return item_id;
}

void menu_update_text(menu_bar_item *menu, int index, int text_number)
{
    menu->items[index].text_number = text_number;
    menu->items[index].text_key = {};
    if (menu->calculated_width_blocks > 0) {
        int item_width = menu_item_width(
            menu->items[index], FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        int blocks = (item_width + 8) / BLOCK_SIZE + 1;
        if (blocks > menu->calculated_width_blocks) {
            menu->calculated_width_blocks = blocks;
        }
    }
}

void menu_update_text(menu_bar_item *menu, int index, translation_key key)
{
    menu->items[index].text_number = 0;
    menu->items[index].text_key = key;
    if (menu->calculated_width_blocks > 0) {
        int item_width = menu_item_width(
            menu->items[index], FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        int blocks = (item_width + 8) / BLOCK_SIZE + 1;
        if (blocks > menu->calculated_width_blocks) {
            menu->calculated_width_blocks = blocks;
        }
    }
}
