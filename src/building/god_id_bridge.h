#pragma once

#include "city/constants.h"
#include "core/buffer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void god_id_bridge_reset_for_runtime(void);
void god_id_bridge_clear_save_table(void);
const char *god_id_bridge_text_from_runtime(int runtime_id);
const char *god_id_bridge_text_from_legacy(god_type legacy_type);
int god_id_bridge_runtime_from_text(const char *text_id);
int god_id_bridge_runtime_from_legacy(god_type legacy_type);

void god_id_bridge_prepare_new_save_table(void);
void god_id_bridge_save_table_save_state(buffer *buf);
void god_id_bridge_save_table_load_state(buffer *buf, int has_save_table);
uint16_t god_id_bridge_save_id_from_runtime(int runtime_id);
int god_id_bridge_runtime_from_save_id(uint16_t save_id);

#ifdef __cplusplus
}
#endif
