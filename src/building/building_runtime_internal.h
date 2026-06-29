#pragma once

#include "building/building_runtime.h"

#include <memory>
#include <unordered_map>

namespace building_runtime_impl {

extern std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

struct EphemeralBuildingRuntimeBinding {
    unsigned int runtime_id = 0;
    unsigned int main_runtime_id = 0;
    ::building *record = nullptr;
    const building_type_registry_impl::BuildingType *definition = nullptr;
    BuildingGraphicsState graphics_state;
};

class ScopedEphemeralBuildingRuntime {
public:
    explicit ScopedEphemeralBuildingRuntime(const std::vector<EphemeralBuildingRuntimeBinding> &bindings);
    ~ScopedEphemeralBuildingRuntime();

    building_runtime *runtime_for_record(::building *record) const;
    building_runtime *runtime_for_id(unsigned int runtime_id) const;
    building_runtime *main_runtime_for_record(::building *record) const;
    building_runtime *next_runtime_for_record(::building *record) const;

private:
    ScopedEphemeralBuildingRuntime *previous_ = nullptr;
    std::vector<std::unique_ptr<building_runtime>> runtimes_;
    std::unordered_map<const ::building *, unsigned int> runtime_id_by_record_;
    std::unordered_map<unsigned int, unsigned int> main_id_by_runtime_id_;
    std::unordered_map<unsigned int, building_runtime *> runtime_by_id_;
};

building_runtime *get_city_building(::building *building_data);
building_runtime *get_or_create_instance(::building *building_data);
building_runtime *get_ephemeral_instance(::building *building_data);
building_runtime *get_ephemeral_instance(unsigned int runtime_id);
building_runtime *get_ephemeral_main_instance(::building *building_data);
building_runtime *get_ephemeral_next_instance(::building *building_data);

}
