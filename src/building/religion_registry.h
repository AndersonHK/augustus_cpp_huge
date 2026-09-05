#pragma once

#include "game/mod_definition_loader.h"

#include <string>
#include <vector>

namespace building_type_registry_impl {
class Religion;

const Religion *find_religion_definition(const char *path);
} // namespace building_type_registry_impl


int religion_registry_load(void);
int religion_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
const char *religion_registry_get_failure_reason(void);
const char *religion_definition_source_path(const char *path);
int religion_definition_is_suppressed(const char *path);

#ifdef STARTUP_PARSER_TEST
struct religion_layer_test_input {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
    const char *definition_path;
};

struct religion_layer_test_result {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_source_layer;
    int queried_tier;
    int queried_capacity;
    int queried_god_count;
    int queried_all_gods;
};

int religion_layered_definition_buffers_are_valid_for_test(
    const religion_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    religion_layer_test_result *result);

int religion_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    religion_layer_test_result *result,
    std::string *failure_reason = nullptr);
#endif

