#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "building/type.h"

typedef struct building building;

const char *building_type_registry_get_building_type_path(void);

int building_type_registry_validate_mod(void);
int building_type_registry_load(void);
void building_type_registry_apply_model_overrides(void);

building_type building_type_registry_runtime_id_from_text(const char *text_id);
int building_type_registry_has_definition(building_type type);
const char *building_type_registry_get_name_key(building_type type);
int building_type_registry_get_model_size(building_type type);
const char *building_type_registry_get_foundation_policy(building_type type);
const char *building_type_registry_get_button_group(building_type type);
int building_type_registry_get_button_order(building_type type);
const char *building_type_registry_get_button_icon(building_type type);
const char *building_type_registry_get_button_text_key(building_type type);
int building_type_registry_get_sound_id(building_type type);
int building_type_registry_get_sound_mute_on_enemies(building_type type);
int building_type_registry_get_sound_always_play(building_type type);
int building_type_registry_get_sound_requires_water_access(building_type type);

#ifdef __cplusplus
}
#endif
