#pragma once

#include "game/resource.h"
#include "game/mod_definition_loader.h"

#include "building/production_method.h"

#include <string>
#include <vector>
namespace building_type_registry_impl {

ProductionMethod *find_production_method_definition(const char *path);
int production_per_month_for_resource(resource_type resource);
int default_production_per_month_for_resource(resource_type resource);
int set_production_per_month_for_resource(resource_type resource, int production);
int adjust_production_per_month_for_resource(resource_type resource, int delta);
void reset_production_overrides();

}


int production_method_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
int production_method_registry_load(void);
const char *production_method_definition_source_path(const char *path);
int production_method_definition_is_suppressed(const char *path);
int production_method_registry_production_per_month_for_resource(resource_type resource);
int production_method_registry_default_production_per_month_for_resource(resource_type resource);
int production_method_registry_set_production_per_month_for_resource(resource_type resource, int production);
int production_method_registry_adjust_production_per_month_for_resource(resource_type resource, int delta);
void production_method_registry_reset_production_overrides(void);
void production_method_registry_save_overrides(buffer *buf);
int production_method_registry_load_overrides(buffer *buf);
int production_method_registry_supply_chain_for_good(resource_supply_chain *chain, resource_type good, int max_entries);
int production_method_registry_supply_chain_for_raw_material(
    resource_supply_chain *chain,
    resource_type raw_material,
    int max_entries);

#ifdef STARTUP_PARSER_TEST
struct production_method_layer_test_input {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
    const char *definition_path;
};

struct production_method_layer_test_result {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_source_layer;
    int queried_production_per_month;
    int queried_input_count;
};

int production_method_layered_definition_buffers_are_valid_for_test(
    const production_method_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    production_method_layer_test_result *result);

int production_method_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    production_method_layer_test_result *result,
    std::string *failure_reason = nullptr);
#endif

