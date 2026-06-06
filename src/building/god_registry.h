#pragma once

#include "city/constants.h"

#ifdef __cplusplus
class God;

namespace building_type_registry_impl {

const God *find_god_definition(const char *path);
const God *find_god_definition(god_type legacy_type);
int god_definition_count(void);
} // namespace building_type_registry_impl

extern "C" {
#endif

const char *god_registry_get_god_path(void);
int god_registry_load(void);

#ifdef __cplusplus
}
#endif
