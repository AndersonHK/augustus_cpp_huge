#pragma once

#include "city/constants.h"
#include "game/mod_definition_loader.h"

#include <string>
#include <vector>

class God;

namespace building_type_registry_impl {

const God *find_god_definition(const char *path);
const God *find_god_definition(god_type legacy_type);
const God *find_god_definition_by_runtime_id(int runtime_id);
const God *god_definition_at_runtime_index(int index);
int god_definition_count(void);
} // namespace building_type_registry_impl


int god_registry_load(void);
int god_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
const char *god_registry_get_failure_reason(void);
const char *god_definition_source_path(const char *path);
int god_definition_is_suppressed(const char *path);

#ifdef STARTUP_PARSER_TEST
struct god_layer_test_input {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
    const char *definition_path;
};

struct god_layer_test_result {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_source_layer;
    int queried_legacy_type;
    int queried_runtime_id;
    int queried_neptune_blessing_months;
    int runtime_count;
};

int god_layered_definition_buffers_are_valid_for_test(
    const god_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    god_layer_test_result *result);

int god_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    god_layer_test_result *result,
    std::string *failure_reason = nullptr);
#endif

