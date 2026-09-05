#include "building/production_runtime.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"

#include "building/building_record.h"

Production *building_runtime::production_for_method(size_t method_index)
{
    if (is_ephemeral() || !definition()) {
        return nullptr;
    }
    const auto &methods = definition()->production_methods();
    if (method_index >= methods.size()) {
        return nullptr;
    }

    if (productions_.size() < methods.size()) productions_.resize(methods.size());
    std::unique_ptr<Production> &slot = productions_[method_index];
    if (!slot) {
        slot = std::make_unique<Production>(building, methods[method_index], method_index);
    }
    return slot.get();
}

namespace production_runtime_impl {

Production *get_or_create(const Building &building, size_t method_index)
{
    building_runtime *runtime = building.runtime_instance();
    return runtime ? runtime->production_for_method(method_index) : nullptr;
}

Production *get_or_create_primary(const Building &building)
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

size_t get_method_count(const Building &building)
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

} // namespace production_runtime_impl
