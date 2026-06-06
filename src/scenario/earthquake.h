#pragma once

#include "core/buffer.h"
#ifdef __cplusplus
extern "C" {
#endif


void scenario_earthquake_init(void);

void scenario_earthquake_process(void);

int scenario_earthquake_is_in_progress(void);

void scenario_earthquake_save_state(buffer *buf);

void scenario_earthquake_load_state(buffer *buf);

#ifdef __cplusplus
}
#endif
