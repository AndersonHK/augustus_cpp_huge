#pragma once

#include "core/buffer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void water_access_type_id_bridge_reset_for_runtime(void);
const char *water_access_type_id_bridge_text_from_runtime(int runtime_id);
int water_access_type_id_bridge_runtime_from_text(const char *text_id);

void water_access_type_id_bridge_prepare_new_save_table(void);
void water_access_type_id_bridge_save_table_save_state(buffer *buf);
void water_access_type_id_bridge_save_table_load_state(buffer *buf, int has_save_table);
uint8_t water_access_type_id_bridge_save_id_from_runtime(int runtime_id);
int water_access_type_id_bridge_runtime_from_save_id(uint8_t save_id);

#ifdef __cplusplus
}
#endif
