#pragma once

#include "building/production_method.h"

namespace building_type_registry_impl {

const ProductionMethod *find_production_method_definition(const char *path);

}

#ifdef __cplusplus
extern "C" {
#endif

const char *production_method_registry_get_production_method_path(void);
int production_method_registry_load(void);

#ifdef __cplusplus
}
#endif
