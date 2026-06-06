#pragma once

#include "core/buffer.h"
#include "game/resource.h"

#include <stdint.h>

typedef int resource_version_t;

#ifdef __cplusplus
extern "C" {
#endif

void resource_id_bridge_reset_for_runtime(void);

void resource_id_bridge_save_table_save_state(buffer *buf);
void resource_id_bridge_save_table_load_state(buffer *buf, int has_save_table);

resource_version_t resource_id_bridge_original_version(void);
resource_version_t resource_id_bridge_current_version(void);
int resource_id_bridge_legacy_resource_count(void);
int resource_id_bridge_legacy_food_count(void);
int resource_id_bridge_legacy_inventory_count(void);
int resource_id_bridge_mapping_joins_meat_and_fish(void);

void resource_set_mapping(resource_version_t version);
resource_version_t resource_mapping_get_version(void);
resource_type resource_map_legacy_inventory(int id);
resource_type resource_remap(int id);
int resource_total_mapped(void);
int resource_total_food_mapped(void);

#ifdef __cplusplus
}
#endif
