#include "building/production_runtime.h"

#include "building/building_runtime_internal.h"

extern "C" {
#include "building/count.h"
#include "building/building.h"
}

namespace production_runtime_impl {

std::vector<std::vector<std::unique_ptr<Production>>> g_city_productions;

void reset()
{
    g_city_productions.clear();
}

Production *get_or_create(::building *building, size_t method_index)
{
    if (!building || !building->id) {
        return nullptr;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building);
    if (!runtime || !runtime->definition()) {
        return nullptr;
    }

    const std::vector<const building_type_registry_impl::ProductionMethod *> &methods =
        runtime->definition()->production_methods();
    if (method_index >= methods.size()) {
        return nullptr;
    }

    if (g_city_productions.size() <= building->id) {
        g_city_productions.resize(building->id + 1);
    }

    std::vector<std::unique_ptr<Production>> &productions = g_city_productions[building->id];
    if (productions.size() < methods.size()) {
        productions.resize(methods.size());
    }

    std::unique_ptr<Production> &slot = productions[method_index];
    if (!slot || slot->building() != building || slot->method() != methods[method_index]) {
        slot = std::make_unique<Production>(building, methods[method_index], method_index);
    }
    return slot.get();
}

Production *get_or_create_primary(::building *building)
{
    return get_or_create(building, 0);
}

size_t get_method_count(::building *building)
{
    if (!building || !building->id) {
        return 0;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building);
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
        ::building *building = building_get(id);
        if (!building || !building->id) {
            continue;
        }

        const size_t method_count = get_method_count(building);
        for (size_t i = 0; i < method_count; i++) {
            get_or_create(building, i);
        }
    }
}

} // namespace production_runtime_impl

extern "C" void production_runtime_reset(void)
{
    production_runtime_impl::reset();
}

extern "C" void production_runtime_initialize_city(void)
{
    production_runtime_impl::initialize_city();
}

extern "C" int production_runtime_has_native_production(building *b)
{
    return production_runtime_impl::get_or_create_primary(b) ? 1 : 0;
}

extern "C" int production_runtime_get_method_count(building *b)
{
    return static_cast<int>(production_runtime_impl::get_method_count(b));
}

extern "C" int production_runtime_building_has_raw_materials(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->has_raw_materials() : 0;
}

extern "C" int production_runtime_get_max_progress(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->max_progress() : 0;
}

extern "C" int production_runtime_get_efficiency(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->efficiency() : -1;
}

extern "C" int production_runtime_update_building(building *b, int new_day, int *out_is_striking)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->update_daily(new_day, out_is_striking) : 0;
}

extern "C" int production_runtime_has_produced_resource(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->has_produced_resource() : 0;
}

extern "C" int production_runtime_get_output_cart_loads(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    return production ? production->output_cart_loads() : 0;
}

extern "C" int production_runtime_start_new_production(building *b)
{
    Production *production = production_runtime_impl::get_or_create_primary(b);
    if (!production) {
        return 0;
    }

    production->start_new_production();
    return 1;
}

extern "C" void production_runtime_advance_stats(building *b)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->advance_stats();
    }
}

extern "C" void production_runtime_bless_farm(building *b)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->bless_farm();
    }
}

extern "C" void production_runtime_curse_farm(building *b, int big_curse)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->curse_farm(big_curse);
    }
}

extern "C" void production_runtime_bless_industry(building *b)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(b)) {
        production->bless_industry();
    }
}
