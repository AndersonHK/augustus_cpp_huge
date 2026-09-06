#pragma once
#include "core/buffer.h"


void map_natives_init(void);
void map_natives_init_editor(void);

void map_natives_check_land(int update_behavior);
// Scenario-only identity tokens replace renderer image IDs in the legacy grid.
void map_natives_prepare_scenario_tokens(void);
void map_natives_save_scenario_image_grid(buffer *buf);

