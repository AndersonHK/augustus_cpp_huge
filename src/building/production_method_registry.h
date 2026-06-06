#pragma once

#include "game/resource.h"

#ifdef __cplusplus
#include "building/production_method.h"
namespace building_type_registry_impl {

ProductionMethod *find_production_method_definition(const char *path);
int production_per_month_for_resource(resource_type resource);
int default_production_per_month_for_resource(resource_type resource);
int set_production_per_month_for_resource(resource_type resource, int production);
int adjust_production_per_month_for_resource(resource_type resource, int delta);
void reset_production_overrides();

}
#endif

#ifdef __cplusplus
extern "C" {
#endif

const char *production_method_registry_get_production_method_path(void);
int production_method_registry_load(void);
int production_method_registry_production_per_month_for_resource(resource_type resource);
int production_method_registry_default_production_per_month_for_resource(resource_type resource);
int production_method_registry_set_production_per_month_for_resource(resource_type resource, int production);
int production_method_registry_adjust_production_per_month_for_resource(resource_type resource, int delta);
void production_method_registry_reset_production_overrides(void);
int production_method_registry_supply_chain_for_good(resource_supply_chain *chain, resource_type good, int max_entries);
int production_method_registry_supply_chain_for_raw_material(
    resource_supply_chain *chain,
    resource_type raw_material,
    int max_entries);

#ifdef __cplusplus
}
#endif
