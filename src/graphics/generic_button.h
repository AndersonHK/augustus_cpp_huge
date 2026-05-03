#pragma once

#include "input/mouse.h"

#ifdef __cplusplus
extern "C++" {

class GenericButton;
using generic_button_click_handler = void (*)(const GenericButton *button);

class GenericButton {
public:
    short x;
    short y;
    short width;
    short height;
    generic_button_click_handler left_click_handler;
    generic_button_click_handler right_click_handler;
    int parameter1;
    int parameter2;
    void *context_data;
    const char *debug_name;

    bool contains(const mouse &m, int origin_x, int origin_y) const;
    int handle_mouse(const mouse &m) const;
    int primary_parameter() const;
    int secondary_parameter() const;
    void set_bounds(short button_x, short button_y, short button_width, short button_height);
    void set_handlers(generic_button_click_handler left_click, generic_button_click_handler right_click);
    void set_parameters(int primary, int secondary);
    void set_context(void *context);
    void *context() const;
    const char *name() const;
    void reset();
};

using generic_button = GenericButton;

}

extern "C" {
#else
typedef struct generic_button generic_button;
typedef void (*generic_button_click_handler)(const generic_button *button);

struct generic_button {
    short x;
    short y;
    short width;
    short height;
    generic_button_click_handler left_click_handler;
    generic_button_click_handler right_click_handler;
    int parameter1;
    int parameter2;
    void *context_data;
    const char *debug_name;
};
#endif

int generic_buttons_handle_mouse(const mouse *m, int x, int y, generic_button *buttons, unsigned int num_buttons,
    unsigned int *focus_button_id);

#ifdef __cplusplus
}
#endif
