#pragma once

#include "city/constants.h"

class God;

namespace building_type_registry_impl {

const God *find_god_definition(const char *path);
const God *find_god_definition(god_type legacy_type);
const God *find_god_definition_by_runtime_id(int runtime_id);
const God *god_definition_at_runtime_index(int index);
int god_definition_count(void);
} // namespace building_type_registry_impl


const char *god_registry_get_god_path(void);
int god_registry_load(void);

