#pragma once

#include "building/housing_type.h"

namespace building_type_registry_impl {

const HousingType *find_housing_type_definition(const char *path);
const HousingType *find_housing_type_definition_for_building_path(const char *path);
const HousingType *find_housing_type_definition_for_level(int level);
int housing_type_level_count();
int housing_type_level_at(int index);

}

#ifdef __cplusplus
extern "C" {
#endif

const char *housing_type_registry_get_housing_type_path(void);
int housing_type_registry_load(void);

#ifdef __cplusplus
}
#endif
