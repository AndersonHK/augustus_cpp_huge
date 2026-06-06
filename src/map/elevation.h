#pragma once

#include "core/buffer.h"
#ifdef __cplusplus
extern "C" {
#endif


int map_elevation_at(int grid_offset);

void map_elevation_set(int grid_offset, int value);

void map_elevation_clear(void);

void map_elevation_remove_cliffs(void);

void map_elevation_save_state(buffer *buf);

void map_elevation_load_state(buffer *buf);

#ifdef __cplusplus
}
#endif
