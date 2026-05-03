#pragma once

#include "building/type.h"
#include "core/buffer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void building_type_id_bridge_reset_for_runtime(void);
const char *building_type_id_bridge_text_from_runtime(building_type runtime_id);
building_type building_type_id_bridge_runtime_from_text(const char *text_id);

void building_type_id_bridge_prepare_new_save_table(void);
void building_type_id_bridge_save_table_save_state(buffer *buf);
void building_type_id_bridge_save_table_load_state(buffer *buf, int has_save_table);
uint16_t building_type_id_bridge_save_id_from_runtime(building_type runtime_id);
building_type building_type_id_bridge_runtime_from_save_id(uint16_t save_id);
int building_type_id_bridge_save_id_is_missing(uint16_t save_id);

#ifdef __cplusplus
}
#endif
