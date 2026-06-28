#include "building/count.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "figure/action.h"
#include "map/building.h"
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
#include "game/Animation.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "core/log.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <string>

building_runtime::building_runtime(::building *building_data, const building_type_registry_impl::BuildingType *definition)
    : record_(building_data)
    , definition_(definition)
    , graphics_state_()
    , building_(building_data, definition, graphics_state_)
    , data(*building_data)
    , building(building_)
{
}

BuildingGraphicsState &building_runtime::graphics_state()
{
    return graphics_state_;
}

const BuildingGraphicsState &building_runtime::graphics_state() const
{
    return graphics_state_;
}

BuildingGraphicsState building_runtime::graphics_state_snapshot() const
{
    return graphics_state_;
}

void building_runtime::restore_graphics_state(const BuildingGraphicsState &state)
{
    graphics_state_ = state;
    invalidate_graphics_cache();
}

namespace building_runtime_impl {

std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

struct GraphicsStateBackup {
    int valid = 0;
    BuildingGraphicsState state;
};

struct LoadedBuildingRuntimeState {
    int valid = 0;
    int graphics_state_valid = 0;
    BuildingGraphicsState graphics_state;
    int original_type_valid = 0;
    building_type original_type = BUILDING_NONE;
};

std::vector<GraphicsStateBackup> g_graphics_state_backup;
std::vector<LoadedBuildingRuntimeState> g_loaded_building_runtime_state;

void reset_live_runtime_modules()
{
    g_runtime_instances.clear();
    g_graphics_state_backup.clear();
    production_runtime_impl::reset();
    storage_runtime_impl::reset();
}

const LoadedBuildingRuntimeState *loaded_runtime_state_for(unsigned int building_id)
{
    return building_id < g_loaded_building_runtime_state.size() && g_loaded_building_runtime_state[building_id].valid ?
        &g_loaded_building_runtime_state[building_id] :
        nullptr;
}

void clear_loaded_runtime_state()
{
    g_loaded_building_runtime_state.clear();
}

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
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(building_data->type);

    std::unique_ptr<building_runtime> &slot = g_runtime_instances[building_data->id];
    const building_type_registry_impl::BuildingType *original_type = slot ? slot->building.og_type : nullptr;
    if (!slot || &slot->data != building_data || slot->definition() != definition) {
        slot = std::make_unique<building_runtime>(building_data, definition);
        slot->building.og_type = original_type;
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
            building_runtime_impl::building_has_required_workers_for_runtime_water(building) &&
            water_access_runtime_building_has_required_access(&record()) ? 1 : 0;
    }

    if (type().has_graphic()) {
        city_culture_remove_building_module_capacity(&record());
        record().upgrade_level = type().upgrade_level_for(building);
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

unsigned char building_runtime::graphics_variant() const
{
    return graphics_state_.variant();
}

void building_runtime::set_graphics_variant(int variant)
{
    const int changed = graphics_state_.set_variant(variant);
    if (changed) {
        invalidate_graphics_cache();
    }
}

building_runtime::LegacyStorageReservation *building_runtime::legacy_storage_reservation_for(unsigned int figure_id)
{
    if (!figure_id) {
        return nullptr;
    }
    prune_legacy_storage_reservations();
    for (LegacyStorageReservation &reservation : legacy_storage_reservations_) {
        if (reservation.figure_id == figure_id) {
            return &reservation;
        }
    }
    return nullptr;
}

int building_runtime::legacy_storage_reservation_is_current(const LegacyStorageReservation &reservation) const
{
    if (!record_ || !reservation.figure_id || reservation.resource == RESOURCE_NONE || reservation.loads <= 0) {
        return 0;
    }

    Figure *figure = Figure::get(reservation.figure_id);
    if (!figure || figure->id() != reservation.figure_id || figure->is_dead() ||
        figure->destination_building.id != record_->id ||
        static_cast<resource_type>(figure->resource_id) != reservation.resource ||
        figure->loads_sold_or_carrying <= 0) {
        return 0;
    }

    switch (figure->action_state) {
        case FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE:
        case FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY:
        case FIGURE_ACTION_24_CARTPUSHER_AT_WAREHOUSE:
        case FIGURE_ACTION_25_CARTPUSHER_AT_GRANARY:
        case FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE:
        case FIGURE_ACTION_52_WAREHOUSEMAN_AT_DELIVERY_BUILDING:
            return 1;
        default:
            return 0;
    }
}

void building_runtime::prune_legacy_storage_reservations()
{
    legacy_storage_reservations_.erase(
        std::remove_if(legacy_storage_reservations_.begin(), legacy_storage_reservations_.end(),
            [this](const LegacyStorageReservation &reservation) {
                return !legacy_storage_reservation_is_current(reservation);
            }),
        legacy_storage_reservations_.end());
}

int building_runtime::reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id)
{
    prune_legacy_storage_reservations();
    int total = 0;
    for (const LegacyStorageReservation &reservation : legacy_storage_reservations_) {
        if (reservation.figure_id != ignore_figure_id &&
            (resource == RESOURCE_NONE || reservation.resource == resource)) {
            total += reservation.loads;
        }
    }
    return total;
}

int building_runtime::reserve_legacy_storage_loads(resource_type resource, int loads, unsigned int figure_id)
{
    if (!record_ || !figure_id || resource == RESOURCE_NONE || loads <= 0) {
        return 0;
    }

    if (LegacyStorageReservation *existing = legacy_storage_reservation_for(figure_id)) {
        existing->resource = resource;
        existing->loads = loads;
        return 1;
    }

    legacy_storage_reservations_.push_back({ figure_id, resource, loads });
    return 1;
}

void building_runtime::release_legacy_storage_reservation(unsigned int figure_id)
{
    if (!figure_id) {
        return;
    }
    legacy_storage_reservations_.erase(
        std::remove_if(legacy_storage_reservations_.begin(), legacy_storage_reservations_.end(),
            [figure_id](const LegacyStorageReservation &reservation) {
                return reservation.figure_id == figure_id;
            }),
        legacy_storage_reservations_.end());
}

void building_runtime_reset(void)
{
    building_runtime_impl::reset_live_runtime_modules();
    building_runtime_impl::clear_loaded_runtime_state();
}

void building_runtime_begin_load_bridge(int building_count)
{
    using building_runtime_impl::g_loaded_building_runtime_state;

    g_loaded_building_runtime_state.clear();
    g_loaded_building_runtime_state.resize(building_count > 0 ? static_cast<size_t>(building_count) : 0);
}

void building_runtime_stage_loaded_graphics_state(
    unsigned int building_id,
    const BuildingGraphicsState &state)
{
    using building_runtime_impl::g_loaded_building_runtime_state;

    if (!building_id) {
        return;
    }
    if (g_loaded_building_runtime_state.size() <= building_id) {
        g_loaded_building_runtime_state.resize(static_cast<size_t>(building_id) + 1);
    }
    g_loaded_building_runtime_state[building_id].valid = 1;
    g_loaded_building_runtime_state[building_id].graphics_state_valid = 1;
    g_loaded_building_runtime_state[building_id].graphics_state = state;
}

void building_runtime_stage_loaded_original_type(unsigned int building_id, building_type type)
{
    using building_runtime_impl::g_loaded_building_runtime_state;

    if (!building_id) {
        return;
    }
    if (g_loaded_building_runtime_state.size() <= building_id) {
        g_loaded_building_runtime_state.resize(static_cast<size_t>(building_id) + 1);
    }
    g_loaded_building_runtime_state[building_id].valid = 1;
    g_loaded_building_runtime_state[building_id].original_type_valid = 1;
    g_loaded_building_runtime_state[building_id].original_type = type;
}

int building_runtime_loaded_graphics_state(unsigned int building_id, BuildingGraphicsState *state)
{
    if (!state) {
        return 0;
    }
    const building_runtime_impl::LoadedBuildingRuntimeState *loaded =
        building_runtime_impl::loaded_runtime_state_for(building_id);
    if (!loaded || !loaded->graphics_state_valid) {
        return 0;
    }
    *state = loaded->graphics_state;
    return 1;
}

void building_runtime_backup_graphics_state(void)
{
    using building_runtime_impl::g_graphics_state_backup;
    using building_runtime_impl::g_runtime_instances;

    g_graphics_state_backup.clear();
    g_graphics_state_backup.resize(g_runtime_instances.size());
    for (size_t id = 1; id < g_runtime_instances.size(); id++) {
        building_runtime *instance = g_runtime_instances[id].get();
        if (!instance) {
            continue;
        }
        g_graphics_state_backup[id].valid = 1;
        g_graphics_state_backup[id].state = instance->graphics_state_snapshot();
    }
}

void building_runtime_restore_graphics_state(void)
{
    using building_runtime_impl::g_graphics_state_backup;
    using building_runtime_impl::g_runtime_instances;

    const size_t count = std::min(g_runtime_instances.size(), g_graphics_state_backup.size());
    for (size_t id = 1; id < count; id++) {
        building_runtime *instance = g_runtime_instances[id].get();
        if (!instance || !g_graphics_state_backup[id].valid) {
            continue;
        }
        instance->restore_graphics_state(g_graphics_state_backup[id].state);
    }
}

// After save load/new city init, bind each live building instance to its runtime wrapper, rebuild native storage/production instances,
// and precompute cached image-group bindings.
void building_runtime_initialize_city_graphics_cache(void)
{
    building_runtime_impl::reset_live_runtime_modules();
    map_building_rebind_runtime_references();
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
            if (const building_runtime_impl::LoadedBuildingRuntimeState *loaded =
                    building_runtime_impl::loaded_runtime_state_for(b->id)) {
                if (loaded->graphics_state_valid) {
                    instance->restore_graphics_state(loaded->graphics_state);
                }
                if (loaded->original_type_valid) {
                    instance->building.og_type =
                        building_type_registry_impl::definition_for_type(loaded->original_type);
                }
            }
            instance->set_building_graphic();
        }
    }

    storage_runtime_impl::initialize_city();
    production_runtime_impl::initialize_city();
    building_runtime_impl::clear_loaded_runtime_state();
}
