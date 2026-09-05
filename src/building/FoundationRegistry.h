#pragma once

#include "building/FoundationDef.h"
#include "game/mod_definition_loader.h"

#include <string>
#include <vector>

namespace building_type_registry_impl {

const FoundationDef *find_foundation_definition(const char *path);
const std::vector<const FoundationDef *> &foundation_definitions();
const mod_definition::DefinitionOverlayEntry *find_foundation_definition_overlay(const char *path);

} // namespace building_type_registry_impl

int foundation_registry_load(void);
int foundation_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);

#ifdef STARTUP_PARSER_TEST
int foundation_definition_buffer_is_valid_for_test(const char *xml, const char *definition_path);

typedef struct {
    const char *xml;
    int layer;
    const char *mod_name;
    const char *definition_path;
    const char *source_path;
} foundation_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_width;
    int queried_source_layer;
} foundation_layer_test_result;

int foundation_layered_definition_buffers_are_valid_for_test(
    const foundation_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    foundation_layer_test_result *result = nullptr);
#endif
