#pragma once

#include "input/mouse.h"

class GenericButton {
public:
    using ClickHandler = void (*)(const GenericButton *button);

    short x;
    short y;
    short width;
    short height;
    ClickHandler left_click_handler;
    ClickHandler right_click_handler;
    int parameter1;
    int parameter2;
    void *context_data;
    const char *debug_name;

    bool contains(const mouse &m, int origin_x, int origin_y) const;
    int handle_mouse(const mouse &m) const;
    int primary_parameter() const;
    int secondary_parameter() const;
    void set_bounds(short button_x, short button_y, short button_width, short button_height);
    void set_handlers(ClickHandler left_click, ClickHandler right_click);
    void set_parameters(int primary, int secondary);
    void set_context(void *context);
    void *context() const;
    const char *name() const;
    void reset();
};

class GenericButtonList {
public:
    GenericButtonList(const GenericButton *items, unsigned int count)
        : button_items(items), button_count(count)
    {
    }

    int handle_mouse(const mouse &m, int origin_x, int origin_y, unsigned int *focus_button_id) const
    {
        unsigned int button_id = focused_button(m, origin_x, origin_y);
        if (focus_button_id) {
            *focus_button_id = button_id;
        }
        if (!button_id) {
            return 0;
        }
        return button_items[button_id - 1].handle_mouse(m);
    }

private:
    unsigned int focused_button(const mouse &m, int origin_x, int origin_y) const
    {
        for (unsigned int i = 0; i < button_count; i++) {
            if (button_items[i].contains(m, origin_x, origin_y)) {
                return i + 1;
            }
        }
        return 0;
    }

    const GenericButton *button_items;
    unsigned int button_count;
};

using generic_button = GenericButton;
using generic_button_click_handler = GenericButton::ClickHandler;
