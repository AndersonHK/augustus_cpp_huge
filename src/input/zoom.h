#pragma once

#include "city/view.h"
#include "input/hotkey.h"
#include "input/mouse.h"
#include "input/touch.h"

#ifdef __cplusplus
extern "C" {
#endif

void zoom_map(const mouse *m, const hotkeys *h, int current_zoom);
void zoom_update_touch(const touch *first, const touch *last, int scale);
void zoom_end_touch(void);

int zoom_update_value(int *zoom, int max, pixel_offset *camera_position);

#ifdef __cplusplus
}
#endif
