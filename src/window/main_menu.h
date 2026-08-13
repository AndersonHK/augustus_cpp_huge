#pragma once

#include <string_view>

int window_main_menu_action_is_supported(std::string_view action);
void window_main_menu_draw_background(void);
void window_main_menu_show(int restart_music);
