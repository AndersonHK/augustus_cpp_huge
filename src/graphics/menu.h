#pragma once

#include "input/mouse.h"
#include "translation/translation.h"

#define TOP_MENU_HEIGHT 24

struct menu_item {
    int text_group = 0;
    int text_number = 0;
    translation_key text_key;
    void (*left_click_handler)(int param);
    int parameter;

    constexpr menu_item(int group, int number, void (*handler)(int), int param)
        : text_group(group)
        , text_number(number)
        , left_click_handler(handler)
        , parameter(param)
    {
    }

    constexpr menu_item(translation_key key, void (*handler)(int), int param)
        : text_key(key)
        , left_click_handler(handler)
        , parameter(param)
    {
    }
};

struct menu_bar_item {
    int text_group = 0;
    translation_key text_key;
    menu_item *items;
    int num_items;
    short x_start = 0;
    short x_end = 0;
    int calculated_width_blocks = 0;
    int calculated_height_blocks = 0;

    constexpr menu_bar_item(int group, menu_item *menu_items, int item_count)
        : text_group(group)
        , items(menu_items)
        , num_items(item_count)
    {
    }

    constexpr menu_bar_item(translation_key key, menu_item *menu_items, int item_count)
        : text_key(key)
        , items(menu_items)
        , num_items(item_count)
    {
    }
};

int menu_bar_draw(menu_bar_item *items, int num_items, int max_width);
int menu_bar_handle_mouse(const mouse *m, menu_bar_item *items, int num_items, int *focus_menu_id);

void menu_draw(menu_bar_item *menu, int focus_item_id);
int menu_handle_mouse(const mouse *m, menu_bar_item *menu, int *focus_item_id);
void menu_update_text(menu_bar_item *menu, int index, int text_number);
void menu_update_text(menu_bar_item *menu, int index, translation_key key);
