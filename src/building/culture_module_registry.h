#pragma once


#include "building/culture_module.h"

namespace building_type_registry_impl {

const CultureModule *find_culture_module_definition(const char *path);

} // namespace building_type_registry_impl


const char *culture_module_registry_get_culture_module_path(void);
int culture_module_registry_load(void);

