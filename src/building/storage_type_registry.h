#pragma once

#include "building/storage_type.h"

namespace building_type_registry_impl {

const StorageType *find_storage_type_definition(const char *path);

}


const char *storage_type_registry_get_storage_type_path(void);
int storage_type_registry_load(void);

