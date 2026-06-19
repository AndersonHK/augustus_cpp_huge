#pragma once


namespace building_type_registry_impl {
class Religion;

const Religion *find_religion_definition(const char *path);
} // namespace building_type_registry_impl


const char *religion_registry_get_religion_path(void);
int religion_registry_load(void);

