#pragma once

#include "graphics/tooltip.h"
#include "input/mouse.h"

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif


int widget_sidebar_military_enter(int formation_id);
int widget_sidebar_military_exit(void);

int widget_sidebar_military_get_standard_image(int legion_id);
int widget_sidebar_military_get_legion_name_id(int legion_id);
int widget_sidebar_military_get_legion_name_group(int legion_id);
const uint8_t *widget_sidebar_military_get_legion_name_text(int group, int id);

void widget_sidebar_military_draw_background(void);
void widget_sidebar_military_draw_foreground(void);

int widget_sidebar_military_handle_input(const mouse *m);

int widget_sidebar_military_get_tooltip_text(tooltip_context *c);

#ifdef __cplusplus
}
#endif
