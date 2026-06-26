#pragma once

#include "building/building_type.h"


const char *building_type_startup_bridge_get_building_type_path(void);
int building_type_startup_bridge_validate_mod(void);
void building_type_startup_bridge_apply_model_overrides(void);
building_type building_type_startup_bridge_runtime_id_from_text(const char *text_id);

