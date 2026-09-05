#pragma once

#include "building/storage_type.h"

#include "building/building_fwd.h"

#include <cstddef>
#include <vector>

class Figure;

class BuildingStorage {
public:
    BuildingStorage(Building &owner, const building_type_registry_impl::StorageType *type, size_t slot_index)
        : owner_(&owner)
        , type_(type)
        , slot_index_(slot_index)
    {
    }

    Building *owner() const
    {
        return owner_;
    }

    const building_type_registry_impl::StorageType *type() const
    {
        return type_;
    }

    size_t slot_index() const
    {
        return slot_index_;
    }

    int handles_resource(resource_type resource) const;
    int amount(resource_type resource) const;
    int reserved_inbound(resource_type resource) const;
    int available_space(resource_type resource) const;
    int add(resource_type resource, int amount);
    int remove_loads(resource_type resource, int max_loads);
    int reserve_inbound_load(resource_type resource, Figure &figure);
    void release_inbound(const Figure &figure);
    void release_all_inbound();
    int receive_inbound_loads(resource_type resource, int loads, Figure &figure);

private:
    struct InboundReservation {
        Figure *figure = nullptr;
        resource_type resource = RESOURCE_NONE;
        int amount = 0;
    };

    InboundReservation *reservation_for(const Figure &figure);
    void release_inbound(InboundReservation *reservation);

    Building *owner_ = nullptr;
    const building_type_registry_impl::StorageType *type_ = nullptr;
    size_t slot_index_ = 0;
    std::vector<InboundReservation> inbound_reservations_;
};

namespace storage_runtime_impl {

void reset();
void initialize_city();
BuildingStorage *get_or_create(Building &building, size_t slot_index);
const BuildingStorage *get_or_create(const Building &building, size_t slot_index);
size_t get_slot_count(const Building &building);
void release_all_reservations(const Building &building);

}
