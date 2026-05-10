#pragma once

#include "building/housing_type.h"

namespace building_type_registry_impl {

const HousingType *find_housing_type_definition(const char *path);
int housing_type_legacy_level_for_text_id(const char *text_id, int *out_level);
const char *housing_type_text_id_for_legacy_level(int level);

}

#ifdef __cplusplus
extern "C" {
#endif

const char *housing_type_registry_get_housing_type_path(void);
int housing_type_registry_load(void);
int housing_type_registry_text_id_has_legacy_house_level(const char *text_id);

#ifdef __cplusplus
}
#endif
