#include "building/count.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "figure/action.h"
#include "map/building_tiles.h"
#include "map/road_access.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/water_access_runtime.h"

#include "assets/image_group_payload.h"
#include "building/armoury.h"
#include "building/barracks.h"
#include "building/building_runtime_graphics.h"
#include "building/caravanserai.h"
#include "building/lighthouse.h"
#include "building/production_runtime.h"
#include "building/storage_runtime.h"
#include "building/temple.h"
#include "city/culture.h"
#include "core/crash_context.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"

#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/movement.h"
#include "game/animation.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "core/log.h"

#include <cstdio>
#include <cstdint>
#include <string>

building_runtime::building_runtime(::building *building_data, const building_type_registry_impl::BuildingType *definition)
    : building_runtime(Building(building_data, definition))
{
}

building_runtime::building_runtime(const Building &building_object)
    : data(*building_object.record_)
    , record_(building_object.record_)
    , definition_(building_object.type)
{
}

Building building_runtime::building() const
{
    return Building(record_, definition_);
}

namespace building_runtime_impl {

std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

static int building_has_required_workers_for_runtime_water(const Building &building)
{
    if (!building.type) {
        return 0;
    }
    const int required_workers = building.employment_required_workers();
    return required_workers <= 0 || building.employment_worker_count() > 0;
}

building_runtime *get_city_building(::building *building_data)
{
    return get_or_create_instance(building_data);
}

building_runtime *get_or_create_instance(::building *building_data)
{
    if (!building_data || !building_data->id) {
        return nullptr;
    }

    // For now runtime wrappers are materialized lazily, but they are owned here rather than by the type registry.
    if (g_runtime_instances.size() <= building_data->id) {
        g_runtime_instances.resize(building_data->id + 1);
    }

    // Every live building gets a runtime object, even before that type has migrated to XML-driven behavior.
    const Building runtime_building(building_data);
    const building_type_registry_impl::BuildingType *definition = runtime_building.type;

    std::unique_ptr<building_runtime> &slot = g_runtime_instances[building_data->id];
    if (!slot || &slot->data != building_data || slot->definition() != definition) {
        slot = std::make_unique<building_runtime>(building_data, definition);
    }
    return slot.get();
}

}

void building_runtime::refresh_runtime_state()
{
    if (!record_ || !definition()) {
        return;
    }

    if (type().water_access().has_requirements()) {
        record().has_water_access =
            building_runtime_impl::building_has_required_workers_for_runtime_water(building()) &&
            water_access_runtime_building_has_required_access(&record()) ? 1 : 0;
    }

    if (type().has_graphic()) {
        city_culture_remove_building_module_capacity(&record());
        record().upgrade_level = type().upgrade_level_for(building());
        city_culture_add_building_module_capacity(&record());
    }
}

int building_runtime::owns_native_storage() const
{
    return definition() && type().has_native_storage();
}

int building_runtime::owns_native_production() const
{
    return definition() && type().has_native_production();
}

void building_runtime_reset(void)
{
    building_runtime_impl::g_runtime_instances.clear();
    production_runtime_impl::reset();
    storage_runtime_impl::reset();
}

// After save load/new city init, bind each live building instance to its runtime wrapper, rebuild native storage/production instances,
// and precompute cached image-group bindings.
void building_runtime_initialize_city_graphics_cache(void)
{
    building_runtime_reset();
    building_local_workforce_initialize_city();

    const int total_buildings = building_count();
    for (int id = 1; id < total_buildings; id++) {
        building *b = building_get(id);
        if (!b || !b->id) {
            continue;
        }
        if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_MOTHBALLED &&
            b->state != BUILDING_STATE_CREATED) {
            continue;
        }
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
            instance->set_building_graphic();
        }
    }

    storage_runtime_impl::initialize_city();
    production_runtime_impl::initialize_city();
}
