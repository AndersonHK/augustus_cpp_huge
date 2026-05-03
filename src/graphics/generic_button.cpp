#include "generic_button.h"

bool GenericButton::contains(const mouse &m, int origin_x, int origin_y) const
{
    return origin_x + x <= m.x &&
        origin_x + x + width > m.x &&
        origin_y + y <= m.y &&
        origin_y + y + height > m.y;
}

int GenericButton::handle_mouse(const mouse &m) const
{
    if (m.left.went_up) {
        if (left_click_handler) {
            left_click_handler(this);
            return 1;
        }
        return 0;
    }
    if (m.right.went_up) {
        if (right_click_handler) {
            right_click_handler(this);
            return 1;
        }
        return 0;
    }
    return 0;
}

int GenericButton::primary_parameter() const
{
    return parameter1;
}

int GenericButton::secondary_parameter() const
{
    return parameter2;
}

void GenericButton::set_bounds(short button_x, short button_y, short button_width, short button_height)
{
    x = button_x;
    y = button_y;
    width = button_width;
    height = button_height;
}

void GenericButton::set_handlers(generic_button_click_handler left_click, generic_button_click_handler right_click)
{
    left_click_handler = left_click;
    right_click_handler = right_click;
}

void GenericButton::set_parameters(int primary, int secondary)
{
    parameter1 = primary;
    parameter2 = secondary;
}

void GenericButton::set_context(void *context)
{
    context_data = context;
}

void *GenericButton::context() const
{
    return context_data;
}

const char *GenericButton::name() const
{
    return debug_name ? debug_name : "";
}

void GenericButton::reset()
{
    x = 0;
    y = 0;
    width = 0;
    height = 0;
    left_click_handler = nullptr;
    right_click_handler = nullptr;
    parameter1 = 0;
    parameter2 = 0;
    context_data = nullptr;
    debug_name = nullptr;
}

static unsigned int get_button(const mouse *m, int x, int y, generic_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (buttons[i].contains(*m, x, y)) {
            return i + 1;
        }
    }
    return 0;
}

extern "C" int generic_buttons_handle_mouse(const mouse *m, int x, int y, generic_button *buttons,
    unsigned int num_buttons, unsigned int *focus_button_id)
{
    unsigned int button_id = get_button(m, x, y, buttons, num_buttons);
    if (focus_button_id) {
        *focus_button_id = button_id;
    }
    if (!button_id) {
        return 0;
    }
    return buttons[button_id - 1].handle_mouse(*m);
}
