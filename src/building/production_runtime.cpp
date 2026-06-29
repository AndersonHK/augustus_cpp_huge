#include "building/production_runtime.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"

#include "building/building_record.h"

namespace production_runtime_impl {

std::vector<std::vector<std::unique_ptr<Production>>> g_city_productions;

void reset()
{
    g_city_productions.clear();
}

Production *get_or_create(Building building, size_t method_index)
{
    if (!building.id) {
        return nullptr;
    }

    building_runtime *runtime = building.runtime_instance();
    if (!runtime || runtime->is_ephemeral() || !runtime->definition()) {
        return nullptr;
    }

    const std::vector<building_type_registry_impl::ProductionMethod *> &methods =
        runtime->definition()->production_methods();
    if (method_index >= methods.size()) {
        return nullptr;
    }

    if (g_city_productions.size() <= building.id) {
        g_city_productions.resize(building.id + 1);
    }

    std::vector<std::unique_ptr<Production>> &productions = g_city_productions[building.id];
    if (productions.size() < methods.size()) {
        productions.resize(methods.size());
    }

    std::unique_ptr<Production> &slot = productions[method_index];
    if (!slot || slot->building().id != building.id || slot->method() != methods[method_index]) {
        slot = std::make_unique<Production>(building, methods[method_index], method_index);
    }
    return slot.get();
}

Production *get_or_create_primary(Building building)
{
    building_runtime *runtime = building.runtime_instance();
    if (!runtime || runtime->is_ephemeral() || !runtime->definition()) {
        return nullptr;
    }

    const std::vector<building_type_registry_impl::ProductionMethod *> &methods =
        runtime->definition()->production_methods();
    const resource_type selected_output = runtime->data.output_resource_id ?
        static_cast<resource_type>(runtime->data.output_resource_id) : RESOURCE_NONE;
    for (size_t i = 0; i < methods.size(); i++) {
        const building_type_registry_impl::ProductionMethod *method = methods[i];
        if (method && method->has_resource_output() && method->output_resource() == selected_output) {
            return get_or_create(building, i);
        }
    }
    return get_or_create(building, 0);
}

size_t get_method_count(Building building)
{
    if (!building.id) {
        return 0;
    }

    building_runtime *runtime = building.runtime_instance();
    if (!runtime || runtime->is_ephemeral() || !runtime->definition()) {
        return 0;
    }
    return runtime->definition()->production_methods().size();
}

void initialize_city()
{
    reset();

    Building::for_each([] (Building *building)
    {
        const size_t method_count = get_method_count(*building);
        for (size_t i = 0; i < method_count; i++) {
            get_or_create(*building, i);
        }
    });
}

} // namespace production_runtime_impl
