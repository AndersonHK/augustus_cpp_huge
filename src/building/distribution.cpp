#include "distribution.h"

#include "building/storage_type.h"

#include <utility>

namespace building_type_registry_impl {

namespace {

int valid_resource(resource_type resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT && resource_is_declared(resource);
}

} // namespace

Distribution::Distribution(std::string path)
    : path_(std::move(path))
{
}

const char *Distribution::path() const
{
    return path_.c_str();
}

void Distribution::add_resource(DistributionResourceRule rule)
{
    if (!valid_resource(rule.resource)) {
        return;
    }
    for (DistributionResourceRule &existing : resources_) {
        if (existing.resource == rule.resource) {
            existing.priority = rule.priority > existing.priority ? rule.priority : existing.priority;
            existing.baseline_stock = rule.baseline_stock > 0 ? rule.baseline_stock : existing.baseline_stock;
            existing.max_stock = rule.max_stock > 0 ? rule.max_stock : existing.max_stock;
            return;
        }
    }
    resources_.push_back(rule);
}

void Distribution::add_storage_type(const StorageType *storage_type, int priority, int baseline_stock, int max_stock)
{
    if (!storage_type) {
        return;
    }
    for (resource_type resource : storage_type->resources()) {
        add_resource({ resource, priority, baseline_stock, max_stock });
    }
}

const std::vector<DistributionResourceRule> &Distribution::resources() const
{
    return resources_;
}

const DistributionResourceRule *Distribution::rule_for(resource_type resource) const
{
    for (const DistributionResourceRule &rule : resources_) {
        if (rule.resource == resource) {
            return &rule;
        }
    }
    return nullptr;
}

int Distribution::handles_resource(resource_type resource) const
{
    return rule_for(resource) ? 1 : 0;
}

int Distribution::priority_for(resource_type resource) const
{
    const DistributionResourceRule *rule = rule_for(resource);
    return rule ? rule->priority : 0;
}

int Distribution::stock_target_for(resource_type resource, int fallback_stock) const
{
    const DistributionResourceRule *rule = rule_for(resource);
    if (!rule) {
        return fallback_stock;
    }
    if (rule->max_stock > 0) {
        return rule->max_stock;
    }
    if (rule->baseline_stock > 0) {
        return rule->baseline_stock;
    }
    return fallback_stock;
}

} // namespace building_type_registry_impl
