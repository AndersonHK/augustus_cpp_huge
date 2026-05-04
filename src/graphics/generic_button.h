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
    static int handle_mouse(const mouse &m, int x, int y, GenericButton *buttons, unsigned int num_buttons,
        unsigned int *focus_button_id);
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

using generic_button = GenericButton;
using generic_button_click_handler = GenericButton::ClickHandler;
