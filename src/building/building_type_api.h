#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;

const char *building_type_registry_get_building_type_path(void);

int building_type_registry_validate_mod(void);
int building_type_registry_load(void);
void building_type_registry_apply_model_overrides(void);

#ifdef __cplusplus
}
#endif
