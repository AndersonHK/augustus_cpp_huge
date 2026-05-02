#pragma once

#include "core/buffer.h"

void scenario_earthquake_init(void);

void scenario_earthquake_process(void);

int scenario_earthquake_is_in_progress(void);

void scenario_earthquake_save_state(buffer *buf);

void scenario_earthquake_load_state(buffer *buf);
