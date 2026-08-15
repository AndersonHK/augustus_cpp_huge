#pragma once

#include "building/building_type.h"
#include "building/storage.h"
#include "game/mod_definition_loader.h"
#include "game/resource.h"
#include "figure/route_policy.h"
#include "map/point.h"

#define BASELINE_STOCK 50

#include <string>
#include <vector>

class Building;
class Figure;

namespace building_type_registry_impl {
class StorageType;

struct DistributionResourceRule {
    resource_type resource = RESOURCE_NONE;
    int priority = 0;
    int baseline_stock = 0;
    int max_stock = 0;
};

class Distribution {
public:
    explicit Distribution(std::string path);

    const char *path() const;
    void add_resource(DistributionResourceRule rule);
    void add_storage_type(const StorageType *storage_type, int priority, int baseline_stock, int max_stock);
    const std::vector<DistributionResourceRule> &resources() const;
    const DistributionResourceRule *rule_for(resource_type resource) const;
    int handles_resource(resource_type resource) const;
    int priority_for(resource_type resource) const;
    int stock_target_for(resource_type resource, int fallback_stock) const;
    int needed_resources_for(const Building &building, resource_storage_info info[RESOURCE_SLOT_COUNT]) const;
    void set_acceptance(Building &building, bool accepted) const;
    int accepts_nothing(const Building &building) const;
    void update_demands(Building &building) const;
    int find_sources_for_building(
        resource_storage_info info[RESOURCE_SLOT_COUNT],
        const Building &start,
        int max_distance) const;
    int find_sources_for_building_by_road(
        resource_storage_info info[RESOURCE_SLOT_COUNT],
        const Building &start,
        const map_point &source_road,
        const RoutePolicy &route_policy,
        int max_distance) const;
    int find_sources_for_figure(
        resource_storage_info info[RESOURCE_SLOT_COUNT],
        building_type type,
        int road_network,
        Figure *start,
        int max_distance) const;
    resource_type fetch_resource(
        const Building &building,
        const resource_storage_info info[RESOURCE_SLOT_COUNT],
        int default_stock,
        int priority_only,
        int pick_first) const;

private:
    std::string path_;
    std::vector<DistributionResourceRule> resources_;
};

const Distribution *find_distribution_definition(const char *path);
int find_distribution_sources_for_building(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    const Building &start,
    int max_distance);
int find_distribution_sources_for_building_by_road(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    const Building &start,
    const map_point &source_road,
    const RoutePolicy &route_policy,
    int max_distance);
int find_distribution_sources_for_figure(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    building_type type,
    int road_network,
    Figure *start,
    int max_distance);
}

int distribution_registry_load(void);
int distribution_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
const char *distribution_registry_get_failure_reason(void);
const char *distribution_definition_source_path(const char *path);
int distribution_definition_is_suppressed(const char *path);

#ifdef STARTUP_PARSER_TEST
struct distribution_layer_test_input {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *definition_path;
    const char *source_path;
};

struct distribution_layer_test_result {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_source_layer;
    int queried_resource_count;
    int queried_wheat_priority;
};

int distribution_layered_definition_buffers_are_valid_for_test(
    const distribution_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    distribution_layer_test_result *result);

int distribution_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    distribution_layer_test_result *result,
    std::string *failure_reason = nullptr);
#endif

