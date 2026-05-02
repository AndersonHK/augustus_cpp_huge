#pragma once

#include "SDL.h"

void platform_touch_start(SDL_TouchFingerEvent *event);
void platform_touch_move(SDL_TouchFingerEvent *event);
void platform_touch_end(SDL_TouchFingerEvent *event);
