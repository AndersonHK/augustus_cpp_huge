#pragma once

#include "core/buffer.h"
#ifdef __cplusplus
extern "C" {
#endif


void scenario_emperor_change_init(void);

void scenario_emperor_change_process(void);

void scenario_emperor_change_save_state(buffer *time, buffer *state);

void scenario_emperor_change_load_state(buffer *time, buffer *state);

#ifdef __cplusplus
}
#endif
