#pragma once

#include "core/buffer.h"

typedef struct building building;

#ifdef __cplusplus
extern "C" {
#endif

void building_save_heap_clear(void);
void building_save_heap_capture_building(const building *b);
void building_save_heap_write_current(buffer *buf);
void building_save_heap_load_current(buffer *buf);
int building_save_heap_building_count(void);
void building_save_heap_materialize_building(int id, building *b);

#ifdef __cplusplus
}
#endif
