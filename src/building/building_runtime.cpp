#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/water_access_runtime.h"

#include "assets/image_group_payload.h"
#include "building/building_runtime_graphics.h"
#include "building/production_runtime_api.h"
#include "building/storage_runtime_api.h"
#include "core/crash_context.h"

extern "C" {
#include "building/armoury.h"
#include "building/barracks.h"
#include "building/building_runtime_api.h"
#include "building/count.h"
#include "building/caravanserai.h"
#include "building/distribution.h"
#include "building/granary.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/lighthouse.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/temple.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/movement.h"
#include "figure/figure_runtime_api.h"
#include "game/animation.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/building_tiles.h"
#include "map/road_access.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "core/log.h"
}

#include <cstdio>
#include <cstdint>
#include <string>

namespace building_runtime_impl {

std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

static int building_has_required_workers_for_runtime_water(const ::building *building_data)
{
    if (!building_data) {
        return 0;
    }
    const model_building *model = model_get_building(building_data->type);
    return !model || model->laborers <= 0 || building_data->num_workers > 0;
}

building_runtime *get_city_building(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    return get_or_create_instance(building_get(id));
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
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::g_building_types[building_data->type].get();

    std::unique_ptr<building_runtime> &slot = g_runtime_instances[building_data->id];
    if (!slot || slot->building() != building_data || slot->definition() != definition) {
        slot = std::make_unique<building_runtime>(building_data, definition);
    }
    return slot.get();
}

}

void building_runtime::refresh_runtime_state()
{
    if (!building_ || !definition_) {
        return;
    }

    if (definition_->water_access().has_requirements()) {
        building_->has_water_access =
            building_runtime_impl::building_has_required_workers_for_runtime_water(building_) &&
            water_access_runtime_building_has_required_access(building_) ? 1 : 0;
    }

    if (definition_->has_graphic()) {
        building_->upgrade_level = definition_->upgrade_level_for(*building_);
    }
}

int building_runtime::owns_native_storage() const
{
    return definition_ && definition_->has_native_storage();
}

int building_runtime::owns_native_production() const
{
    return definition_ && definition_->has_native_production();
}

extern "C" void building_runtime_reset(void)
{
    building_runtime_impl::g_runtime_instances.clear();
    production_runtime_reset();
    storage_runtime_reset();
}

// After save load/new city init, bind each live building instance to its runtime wrapper, rebuild native storage/production instances,
// and precompute cached image-group bindings.
extern "C" void building_runtime_initialize_city_graphics_cache(void)
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

    storage_runtime_initialize_city();
    production_runtime_initialize_city();
}

extern "C" void building_runtime_apply_graphic(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->set_building_graphic();
    }
}

extern "C" int building_runtime_apply_graphic_if_native(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        if (instance->uses_new_graphics() &&
            (b->state == BUILDING_STATE_CREATED ||
                b->state == BUILDING_STATE_IN_USE ||
                b->state == BUILDING_STATE_MOTHBALLED)) {
            instance->set_building_graphic();
            return 1;
        }
    }
    return 0;
}

extern "C" void building_runtime_assign_graphic_variant(building *b, int force_reseed)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->assign_graphic_variant(force_reseed);
    }
}

extern "C" void building_runtime_spawn_figure(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->spawn_figure();
    }
}

const RuntimeDrawSlice *building_runtime_get_graphic_footprint_slice(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_footprint();
    }
    return nullptr;
}

const RuntimeDrawSlice *building_runtime_get_graphic_top_slice(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_top();
    }
    return nullptr;
}

int building_runtime_owns_graphics(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->owns_graphics();
    }
    return 0;
}

int building_runtime_owns_graphic_animation(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->owns_graphic_animation();
    }
    return 0;
}

const RuntimeDrawSlice *building_runtime_get_graphic_animation_slice(building *b, int animation_cursor)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_animation(animation_cursor);
    }
    return nullptr;
}

int building_runtime_advance_graphic_animation(building *b, int animation_cursor)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->advance_graphic_animation(animation_cursor);
        return instance->owns_graphic_animation();
    }
    return 0;
}
