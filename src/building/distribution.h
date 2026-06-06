#pragma once

#include "building/building_type.h"
#include "building/storage.h"
#include "game/resource.h"

#define BASELINE_STOCK 50

#ifdef __cplusplus
#include <string>
#include <vector>

extern "C++" {
class Building;

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
    void set_acceptance(Building &building, int value) const;
    int accepts_nothing(const Building &building) const;
    void update_demands(Building &building) const;
    int find_sources_for_building(
        resource_storage_info info[RESOURCE_SLOT_COUNT],
        const Building &start,
        int max_distance) const;
    int find_sources_for_figure(
        resource_storage_info info[RESOURCE_SLOT_COUNT],
        building_type type,
        int road_network,
        figure *start,
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
int find_distribution_sources_for_figure(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    building_type type,
    int road_network,
    figure *start,
    int max_distance);
}
}
extern "C" {
#endif

const char *distribution_registry_get_distribution_path(void);
int distribution_registry_load(void);

#ifdef __cplusplus
}
#endif
