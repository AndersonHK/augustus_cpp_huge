#include "building/storage_runtime.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"

#include "building/building_record.h"
#include "figure/figure.h"
#include "game/resource.h"

#include <algorithm>

int BuildingStorage::handles_resource(resource_type resource) const
{
    return type_ ? type_->handles_resource(resource) : 0;
}

int BuildingStorage::amount(resource_type resource) const
{
    if (!building_ || !handles_resource(resource) || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    return building_->resources[resource];
}

int BuildingStorage::reserved_inbound(resource_type resource)
{
    prune_inbound_reservations();
    int total = 0;
    for (const InboundReservation &reservation : inbound_reservations_) {
        if (reservation.resource == resource) {
            total += reservation.amount;
        }
    }
    return total;
}

int BuildingStorage::available_space(resource_type resource)
{
    if (!building_ || !handles_resource(resource)) {
        return 0;
    }
    const int capacity = type_->capacity();
    if (capacity <= 0) {
        return resource_units_per_load();
    }
    return std::max(0, capacity - amount(resource) - reserved_inbound(resource));
}

int BuildingStorage::add(resource_type resource, int amount)
{
    if (!building_ || !handles_resource(resource) || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    const int current = building_->resources[resource];
    const int next = current + amount;
    if (next < 0 || (amount > 0 && type_->capacity() > 0 &&
        next + reserved_inbound(resource) > type_->capacity())) {
        return 0;
    }
    building_->resources[resource] = static_cast<short>(next);
    return 1;
}

int BuildingStorage::remove_loads(resource_type resource, int max_loads)
{
    const int load_units = resource_units_per_load();
    if (load_units <= 0 || max_loads <= 0) {
        return 0;
    }
    const int loads = std::min(max_loads, amount(resource) / load_units);
    if (loads > 0) {
        if (!add(resource, -loads * load_units)) {
            return 0;
        }
    }
    return loads;
}

BuildingStorage::InboundReservation *BuildingStorage::reservation_for(unsigned int figure_id)
{
    if (!figure_id) {
        return nullptr;
    }
    prune_inbound_reservations();
    for (InboundReservation &reservation : inbound_reservations_) {
        if (reservation.figure_id == figure_id) {
            return &reservation;
        }
    }
    return nullptr;
}

void BuildingStorage::release_inbound(InboundReservation *reservation)
{
    if (!reservation) {
        return;
    }
    const unsigned int figure_id = reservation->figure_id;
    inbound_reservations_.erase(
        std::remove_if(inbound_reservations_.begin(), inbound_reservations_.end(),
            [figure_id](const InboundReservation &current) { return current.figure_id == figure_id; }),
        inbound_reservations_.end());
}

int BuildingStorage::reservation_is_current(const InboundReservation &reservation) const
{
    if (!building_ || !reservation.figure_id || reservation.amount <= 0) {
        return 0;
    }

    Figure *figure = Figure::get(reservation.figure_id);
    if (!figure || figure->id() != reservation.figure_id || figure->is_dead() ||
        static_cast<resource_type>(figure->resource_id) != reservation.resource ||
        !figure->destination_building || figure->destination_building->id != building_->id) {
        return 0;
    }

    switch (figure->action_state) {
        case FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP:
        case FIGURE_ACTION_26_CARTPUSHER_AT_WORKSHOP:
        case FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE:
        case FIGURE_ACTION_52_WAREHOUSEMAN_AT_DELIVERY_BUILDING:
            return 1;
        default:
            return 0;
    }
}

void BuildingStorage::prune_inbound_reservations()
{
    inbound_reservations_.erase(
        std::remove_if(inbound_reservations_.begin(), inbound_reservations_.end(),
            [this](const InboundReservation &reservation) { return !reservation_is_current(reservation); }),
        inbound_reservations_.end());
}

int BuildingStorage::reserve_inbound_load(resource_type resource, unsigned int figure_id)
{
    const int load_units = resource_units_per_load();
    if (!type_ || !type_->is_input() || figure_id == 0 || load_units <= 0 ||
        !handles_resource(resource) || available_space(resource) < load_units) {
        return 0;
    }

    if (InboundReservation *existing = reservation_for(figure_id)) {
        return existing->resource == resource && existing->amount == load_units;
    }

    inbound_reservations_.push_back({ figure_id, resource, load_units });
    return 1;
}

void BuildingStorage::release_inbound(unsigned int figure_id)
{
    release_inbound(reservation_for(figure_id));
}

int BuildingStorage::receive_inbound_loads(resource_type resource, int loads, unsigned int figure_id)
{
    const int load_units = resource_units_per_load();
    if (loads <= 0 || load_units <= 0 || !type_ || !type_->is_input()) {
        return 0;
    }

    release_inbound(figure_id);
    const int units = loads * load_units;
    return add(resource, units) ? loads : 0;
}

namespace storage_runtime_impl {

std::vector<std::vector<std::unique_ptr<BuildingStorage>>> g_city_storage_slots;

void reset()
{
    g_city_storage_slots.clear();
}

BuildingStorage *get_or_create(::building *building, size_t slot_index)
{
    if (!building || !building->id) {
        return nullptr;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building);
    if (!runtime || !runtime->definition()) {
        return nullptr;
    }

    const std::vector<const building_type_registry_impl::StorageType *> &storage_types =
        runtime->definition()->storage_types();
    if (slot_index >= storage_types.size()) {
        return nullptr;
    }

    if (g_city_storage_slots.size() <= building->id) {
        g_city_storage_slots.resize(building->id + 1);
    }

    std::vector<std::unique_ptr<BuildingStorage>> &slots = g_city_storage_slots[building->id];
    if (slots.size() < storage_types.size()) {
        slots.resize(storage_types.size());
    }

    std::unique_ptr<BuildingStorage> &slot = slots[slot_index];
    if (!slot || slot->building() != building || slot->type() != storage_types[slot_index]) {
        slot = std::make_unique<BuildingStorage>(building, storage_types[slot_index], slot_index);
    }
    return slot.get();
}

size_t get_slot_count(::building *building)
{
    if (!building || !building->id) {
        return 0;
    }

    building_runtime *runtime = building_runtime_impl::get_or_create_instance(building);
    if (!runtime || !runtime->definition()) {
        return 0;
    }
    return runtime->definition()->storage_types().size();
}

void initialize_city()
{
    reset();

    Building::for_each([](Building *building_object) {
        ::building *building = const_cast<::building *>(building_object->record());
        const size_t slot_count = get_slot_count(building);
        for (size_t i = 0; i < slot_count; i++) {
            get_or_create(building, i);
        }
    });
}

} // namespace storage_runtime_impl
