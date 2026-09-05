#pragma once

#include "game/mod_definition_loader.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace building_type_registry_impl {

class WaterAccessType {
public:
    WaterAccessType(std::string text_id, uint8_t number_id);

    const char *text_id() const;
    uint8_t number_id() const;
    uint8_t mask() const;

private:
    std::string text_id_;
    uint8_t number_id_ = 0;
    uint8_t mask_ = 0;
};

const WaterAccessType *find_water_access_type(const char *text_id);
const WaterAccessType *water_access_type_from_number_id(uint8_t number_id);
uint8_t water_access_mask_from_text(const char *text_id);
uint8_t water_access_defined_mask(void);
const std::vector<std::unique_ptr<WaterAccessType>> &water_access_types(void);

} // namespace building_type_registry_impl

int water_access_type_registry_load(void);
int water_access_type_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
const char *water_access_type_registry_get_failure_reason(void);
const char *water_access_type_definition_source_path(const char *text_id);
int water_access_type_definition_is_suppressed(const char *text_id);
const char *water_access_type_text_from_number_id(uint8_t number_id);
uint8_t water_access_type_mask_from_text_id(const char *text_id);
uint8_t water_access_type_defined_mask_c(void);

#ifdef STARTUP_PARSER_TEST
struct water_access_type_layer_test_input {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
    const char *definition_path;
};

struct water_access_type_layer_test_result {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_source_layer;
    int queried_number_id;
    int defined_mask;
};

int water_access_type_layered_definition_buffers_are_valid_for_test(
    const water_access_type_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    water_access_type_layer_test_result *result);

int water_access_type_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    water_access_type_layer_test_result *result,
    std::string *failure_reason = nullptr);
#endif

