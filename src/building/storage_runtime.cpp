#include "building/storage_runtime.h"

#include "building/building.h"
#include "figure/figure.h"
#include "game/resource.h"

#include <algorithm>
#include <memory>

int BuildingStorage::handles_resource(resource_type resource) const
{
    return type_ ? type_->handles_resource(resource) : 0;
}

int BuildingStorage::amount(resource_type resource) const
{
    if (!owner_ || !handles_resource(resource) || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    return owner_->resource_amount(resource);
}

int BuildingStorage::reserved_inbound(resource_type resource) const
{
    int total = 0;
    for (const InboundReservation &reservation : inbound_reservations_) {
        if (reservation.resource == resource) {
            total += reservation.amount;
        }
    }
    return total;
}

int BuildingStorage::available_space(resource_type resource) const
{
    if (!owner_ || !handles_resource(resource)) {
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
    if (!owner_ || !handles_resource(resource) || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    const int current = owner_->resource_amount(resource);
    const int next = current + amount;
    if (next < 0 || (amount > 0 && type_->capacity() > 0 &&
        next + reserved_inbound(resource) > type_->capacity())) {
        return 0;
    }
    owner_->set_resource_amount(resource, next);
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

BuildingStorage::InboundReservation *BuildingStorage::reservation_for(const Figure &figure)
{
    for (InboundReservation &reservation : inbound_reservations_) {
        if (reservation.figure == &figure) {
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
    const Figure *figure = reservation->figure;
    inbound_reservations_.erase(
        std::remove_if(inbound_reservations_.begin(), inbound_reservations_.end(),
            [figure](const InboundReservation &current) { return current.figure == figure; }),
        inbound_reservations_.end());
}

int BuildingStorage::reserve_inbound_load(resource_type resource, Figure &figure)
{
    const int load_units = resource_units_per_load();
    if (!type_ || !type_->is_input() || !figure.id() || load_units <= 0 ||
        !handles_resource(resource) || available_space(resource) < load_units) {
        return 0;
    }

    if (InboundReservation *existing = reservation_for(figure)) {
        return existing->resource == resource && existing->amount == load_units;
    }

    inbound_reservations_.push_back({ &figure, resource, load_units });
    return 1;
}

void BuildingStorage::release_inbound(const Figure &figure)
{
    release_inbound(reservation_for(figure));
}

void BuildingStorage::release_all_inbound()
{
    inbound_reservations_.clear();
}

int BuildingStorage::receive_inbound_loads(resource_type resource, int loads, Figure &figure)
{
    const int load_units = resource_units_per_load();
    if (loads <= 0 || load_units <= 0 || !type_ || !type_->is_input()) {
        return 0;
    }

    release_inbound(figure);
    const int units = loads * load_units;
    return add(resource, units) ? loads : 0;
}

namespace storage_runtime_impl {

static std::vector<std::vector<std::unique_ptr<BuildingStorage>>> g_city_storage_slots;

void reset()
{
    g_city_storage_slots.clear();
}

BuildingStorage *get_or_create(Building &building, size_t slot_index)
{
    if (!building.id || !building.type) {
        return nullptr;
    }

    const std::vector<const building_type_registry_impl::StorageType *> &storage_types =
        building.type->storage_types();
    if (slot_index >= storage_types.size()) {
        return nullptr;
    }

    if (g_city_storage_slots.size() <= building.id) {
        g_city_storage_slots.resize(building.id + 1);
    }

    std::vector<std::unique_ptr<BuildingStorage>> &slots = g_city_storage_slots[building.id];
    if (slots.size() < storage_types.size()) {
        slots.resize(storage_types.size());
    }

    std::unique_ptr<BuildingStorage> &slot = slots[slot_index];
    if (!slot || slot->owner() != &building || slot->type() != storage_types[slot_index]) {
        slot = std::make_unique<BuildingStorage>(building, storage_types[slot_index], slot_index);
    }
    return slot.get();
}

const BuildingStorage *get_or_create(const Building &building, size_t slot_index)
{
    // Const building queries may populate the runtime-only slot cache; they do not mutate building state.
    return get_or_create(const_cast<Building &>(building), slot_index);
}

size_t get_slot_count(const Building &building)
{
    return building.id && building.type ? building.type->storage_types().size() : 0;
}

void release_all_reservations(const Building &building)
{
    if (!building.id || building.id >= g_city_storage_slots.size()) {
        return;
    }
    for (const std::unique_ptr<BuildingStorage> &storage : g_city_storage_slots[building.id]) {
        if (storage) {
            storage->release_all_inbound();
        }
    }
}

void initialize_city()
{
    reset();

    Building::for_each([](Building *building_object) {
        const size_t slot_count = get_slot_count(*building_object);
        for (size_t i = 0; i < slot_count; i++) {
            get_or_create(*building_object, i);
        }
    });
}

} // namespace storage_runtime_impl
