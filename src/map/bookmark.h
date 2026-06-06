#pragma once

#include "core/buffer.h"
#ifdef __cplusplus
extern "C" {
#endif


void map_bookmarks_clear(void);

void map_bookmark_save(int number);

int map_bookmark_go_to(int number);

void map_bookmark_save_state(buffer *buf);

void map_bookmark_load_state(buffer *buf);

#ifdef __cplusplus
}
#endif
