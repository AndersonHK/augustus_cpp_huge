#pragma once

#include "building/storage_type.h"

namespace building_type_registry_impl {

const StorageType *find_storage_type_definition(const char *path);

}


const char *storage_type_registry_get_failure_reason(void);
const char *storage_type_definition_source_path(const char *path);
int storage_type_definition_is_suppressed(const char *path);
int storage_type_registry_load(void);

#ifdef STARTUP_PARSER_TEST
typedef struct {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *definition_path;
    const char *source_path;
} storage_type_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_capacity;
    int queried_resource_count;
    int queried_source_layer;
} storage_type_layer_test_result;

int storage_type_layered_definition_buffers_are_valid_for_test(
    const storage_type_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    storage_type_layer_test_result *result);
#endif

