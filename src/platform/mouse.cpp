#include "core/calc.h"
#include "core/config.h"
#include "game/system.h"
#include "input/mouse.h"
#include "graphics/screen.h"
#include "platform/screen.h"

#include <SDL_mouse.h>

static struct {
    int x;
    int y;
    int enabled;
} data;

void system_mouse_get_relative_state(int *x, int *y)
{
    int delta_x, delta_y;
    SDL_GetRelativeMouseState(&delta_x, &delta_y);
    platform_screen_window_to_pixels(&delta_x, &delta_y);
    if (x) *x = delta_x;
    if (y) *y = delta_y;
}

void system_mouse_set_relative_mode(int enabled)
{
    if (enabled == data.enabled) {
        return;
    }
    if (enabled) {
        SDL_GetMouseState(&data.x, &data.y);
        platform_screen_window_to_pixels(&data.x, &data.y);
        data.x = screen_pixel_to_ui(data.x);
        data.y = screen_pixel_to_ui(data.y);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        system_mouse_get_relative_state(NULL, NULL);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        system_set_mouse_position(&data.x, &data.y);
        mouse_set_logical_position(data.x, data.y);
    }
    data.enabled = enabled;
}


void system_move_mouse_cursor(int delta_x, int delta_y)
{
    int x = mouse_get()->x + delta_x;
    int y = mouse_get()->y + delta_y;
    system_set_mouse_position(&x, &y);
    mouse_set_logical_position(x, y);
}
