#pragma once

#include "input/hotkey.h"
#include "input/mouse.h"

void menu_file_new_map(int param);
void widget_top_menu_editor_draw(void);
void widget_top_menu_editor_draw_panels(void);
int widget_top_menu_editor_handle_input(const mouse *m, const hotkeys *h);
