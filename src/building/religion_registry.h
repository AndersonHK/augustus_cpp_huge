#pragma once

#ifdef __cplusplus

namespace building_type_registry_impl {
class Religion;

const Religion *find_religion_definition(const char *path);
} // namespace building_type_registry_impl

extern "C" {
#endif

const char *religion_registry_get_religion_path(void);
int religion_registry_load(void);

#ifdef __cplusplus
}
#endif
