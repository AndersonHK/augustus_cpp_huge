#include "building/production_runtime.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"

extern "C" {
#include "building/count.h"
#include "building/building_record.h"
}

namespace production_runtime_impl {

std::vector<std::vector<std::unique_ptr<Production>>> g_city_productions;

void reset()
{
    g_city_productions.clear();
}

Production *get_or_create(Building building, size_t method_index)
{
    if (!building.id()) {
        return nullptr;
    }

    ::building *record = building.legacy_record();
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record);
    if (!runtime || !runtime->definition()) {
        return nullptr;
    }

    const std::vector<building_type_registry_impl::ProductionMethod *> &methods =
        runtime->definition()->production_methods();
    if (method_index >= methods.size()) {
        return nullptr;
    }

    if (g_city_productions.size() <= building.id()) {
        g_city_productions.resize(building.id() + 1);
    }

    std::vector<std::unique_ptr<Production>> &productions = g_city_productions[building.id()];
    if (productions.size() < methods.size()) {
        productions.resize(methods.size());
    }

    std::unique_ptr<Production> &slot = productions[method_index];
    if (!slot || slot->building().legacy_record() != record || slot->method() != methods[method_index]) {
        slot = std::make_unique<Production>(building, methods[method_index], method_index);
    }
    return slot.get();
}

Production *get_or_create_primary(Building building)
{
    return get_or_create(building, 0);
}

size_t get_method_count(Building building)
{
    if (!building.id()) {
        return 0;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building.legacy_record());
    if (!runtime || !runtime->definition()) {
        return 0;
    }
    return runtime->definition()->production_methods().size();
}

void initialize_city()
{
    reset();

    const int total_buildings = building_count();
    for (int id = 1; id < total_buildings; id++) {
        Building building(building_get(id));
        if (!building.id()) {
            continue;
        }

        const size_t method_count = get_method_count(building);
        for (size_t i = 0; i < method_count; i++) {
            get_or_create(building, i);
        }
    }
}

} // namespace production_runtime_impl
