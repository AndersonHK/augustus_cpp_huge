#pragma once

#include "building/culture_module.h"
#include "game/mod_definition_loader.h"

#include <string>
#include <vector>

namespace building_type_registry_impl {

const CultureModule *find_culture_module_definition(const char *path);

} // namespace building_type_registry_impl


int culture_module_registry_load(void);
int culture_module_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
const char *culture_module_registry_get_failure_reason(void);
const char *culture_module_definition_source_path(const char *path);
int culture_module_definition_is_suppressed(const char *path);

#ifdef STARTUP_PARSER_TEST
struct culture_module_layer_test_input {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
    const char *definition_path;
};

struct culture_module_layer_test_result {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_source_layer;
    int queried_type;
};

int culture_module_layered_definition_buffers_are_valid_for_test(
    const culture_module_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    culture_module_layer_test_result *result);

int culture_module_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    culture_module_layer_test_result *result,
    std::string *failure_reason = nullptr);
#endif

