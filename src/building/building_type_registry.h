#pragma once

#include "building/building_type.h"
#include "game/mod_definition_loader.h"

#include <string>
#include <vector>

int building_type_registry_load(void);
const char *building_type_registry_get_failure_reason(void);
int building_type_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);

namespace building_type_registry_impl {

const mod_definition::DefinitionOverlayEntry *find_building_type_definition_overlay(const char *identity);

} // namespace building_type_registry_impl

#ifdef STARTUP_PARSER_TEST
typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_cost;
    int queried_runtime_type;
    int queried_source_layer;
} building_type_layer_test_result;

int building_type_registry_layers_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_identity,
    building_type_layer_test_result *result = nullptr,
    std::string *failure_reason = nullptr);
#endif
