#include "building/building_record.h"
#include "distribution.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/properties.h"
#include "building/storage_type.h"
#include "building/warehouse.h"
#include "city/resource.h"
#include "core/calc.h"

#include <climits>
#include <utility>

namespace building_type_registry_impl {

namespace {

int valid_resource(resource_type resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT && resource_is_declared(resource);
}

void update_food_source(resource_storage_info *info, resource_type resource, const Building &source, int distance)
{
    if (distance < info[resource].min_distance && source.resource_amount(resource)) {
        info[resource].min_distance = distance;
        info[resource].building_id = source.id();
    }
}

void update_warehouse_source(resource_storage_info *info, resource_type resource, Building &source, int distance)
{
    if (distance < info[resource].min_distance &&
        !city_resource_is_stockpiled(resource) &&
        building_warehouse_get_available_amount(source, resource)) {
        info[resource].min_distance = distance;
        info[resource].building_id = source.id();
    }
}

int invalid_distribution_source(Building &source, building_storage_permission_states permission, int road_network)
{
    return !source.is_in_use() ||
        !source.has_cached_road_access() ||
        source.distance_from_entry() <= 0 ||
        source.road_network_id() != road_network ||
        !building_storage_get_permission(permission, source);
}

int has_needed_food(const resource_storage_info info[RESOURCE_SLOT_COUNT])
{
    for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT;
        resource = static_cast<resource_type>(resource + 1)) {
        if (info[resource].needed && resource_is_food(resource)) {
            return 1;
        }
    }
    return 0;
}

int distance_to_building_box(int x, int y, int width, int height, const Building &source)
{
    int size = building_properties_for_type(source.type_id())->size;
    return calc_box_distance(x, y, width, height, source.x(), source.y(), size, size);
}

int find_distribution_sources(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    building_type type,
    int road_network,
    int x,
    int y,
    int width,
    int height,
    int max_distance)
{
    for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT;
        resource = static_cast<resource_type>(resource + 1)) {
        info[resource].min_distance = max_distance;
        info[resource].building_id = 0;
    }

    building_storage_permission_states permission = building_storage_get_permission_from_building_type(type);
    if (has_needed_food(info)) {
        for (Building source = Building::first_of_type(runtime_id_from_text("granary")); source.id(); source = source.next_of_type()) {
            if (type && invalid_distribution_source(source, permission, road_network)) {
                continue;
            }
            int distance = distance_to_building_box(x, y, width, height, source);
            for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT;
                resource = static_cast<resource_type>(resource + 1)) {
                if (info[resource].needed && resource_is_food(resource)) {
                    update_food_source(info, resource, source, distance);
                }
            }
        }
    }

    for (Building source = Building::first_of_type(runtime_id_from_text("warehouse")); source.id(); source = source.next_of_type()) {
        if (type && invalid_distribution_source(source, permission, road_network)) {
            continue;
        }
        int distance = distance_to_building_box(x, y, width, height, source);
        for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT;
            resource = static_cast<resource_type>(resource + 1)) {
            if (info[resource].needed && resource_is_storable(resource)) {
                update_warehouse_source(info, resource, source, distance);
            }
        }
    }

    for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT;
        resource = static_cast<resource_type>(resource + 1)) {
        if (info[resource].building_id) {
            return 1;
        }
    }
    return 0;
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

int Distribution::needed_resources_for(const Building &building, resource_storage_info info[RESOURCE_SLOT_COUNT]) const
{
    int needed = 0;
    for (const DistributionResourceRule &rule : resources_) {
        info[rule.resource].needed = building.accepts_good(rule.resource) > 0;
        if (info[rule.resource].needed) {
            needed = 1;
        }
    }
    return needed;
}

void Distribution::set_acceptance(Building &building, int value) const
{
    for (const DistributionResourceRule &rule : resources_) {
        building.set_accepted_good(rule.resource, value);
    }
}

int Distribution::accepts_nothing(const Building &building) const
{
    for (const DistributionResourceRule &rule : resources_) {
        if (building.accepts_good(rule.resource)) {
            return 0;
        }
    }
    return 1;
}

void Distribution::update_demands(Building &building) const
{
    for (const DistributionResourceRule &rule : resources_) {
        int accepted = building.accepts_good(rule.resource);
        if (resource_is_inventory(rule.resource) && accepted > 1) {
            building.set_accepted_good(rule.resource, accepted - 1);
        }
    }
}

int Distribution::find_sources_for_building(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    const Building &start,
    int max_distance) const
{
    return find_distribution_sources_for_building(info, start, max_distance);
}

int Distribution::find_sources_for_figure(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    building_type type,
    int road_network,
    figure *start,
    int max_distance) const
{
    return find_distribution_sources_for_figure(info, type, road_network, start, max_distance);
}

resource_type Distribution::fetch_resource(
    const Building &building,
    const resource_storage_info info[RESOURCE_SLOT_COUNT],
    int default_stock,
    int priority_only,
    int pick_first) const
{
    resource_type best_resource = RESOURCE_NONE;
    int best_stock = INT_MAX;
    int best_priority = INT_MIN;

    for (const DistributionResourceRule &rule : resources_) {
        if (priority_only && rule.priority <= 0) {
            continue;
        }

        resource_type resource = rule.resource;
        if (!info[resource].needed || !info[resource].building_id) {
            continue;
        }

        int target_stock = default_stock;
        if (rule.baseline_stock > 0) {
            target_stock = rule.baseline_stock;
        }
        if (rule.max_stock > 0 && default_stock <= 0) {
            target_stock = rule.max_stock;
        }
        if (target_stock <= 0) {
            target_stock = 1;
        }

        int stock = building.resource_amount(resource);
        if (stock >= target_stock) {
            continue;
        }
        if (pick_first) {
            return resource;
        }
        if (rule.priority > best_priority || (rule.priority == best_priority && stock < best_stock)) {
            best_priority = rule.priority;
            best_stock = stock;
            best_resource = resource;
        }
    }

    return best_resource;
}

int find_distribution_sources_for_building(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    const Building &start,
    int max_distance)
{
    int size = building_properties_for_type(start.type_id())->size;
    return find_distribution_sources(
        info,
        start.type_id(),
        start.road_network_id(),
        start.x(),
        start.y(),
        size,
        size,
        max_distance);
}

int find_distribution_sources_for_figure(
    resource_storage_info info[RESOURCE_SLOT_COUNT],
    building_type type,
    int road_network,
    figure *start,
    int max_distance)
{
    return start ? find_distribution_sources(info, type, road_network, start->x, start->y, 1, 1, max_distance) : 0;
}

} // namespace building_type_registry_impl
