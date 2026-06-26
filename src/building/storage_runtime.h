#pragma once

#include "building/storage_type.h"

#include "building/building_fwd.h"

#include <cstddef>
#include <memory>
#include <vector>

class BuildingStorage {
public:
    BuildingStorage(::building *building, const building_type_registry_impl::StorageType *type, size_t slot_index)
        : building_(building)
        , type_(type)
        , slot_index_(slot_index)
    {
    }

    ::building *building() const
    {
        return building_;
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
    int reserved_inbound(resource_type resource);
    int available_space(resource_type resource);
    int add(resource_type resource, int amount);
    int remove_loads(resource_type resource, int max_loads);
    int reserve_inbound_load(resource_type resource, unsigned int figure_id);
    void release_inbound(unsigned int figure_id);
    int receive_inbound_loads(resource_type resource, int loads, unsigned int figure_id);

private:
    struct InboundReservation {
        unsigned int figure_id = 0;
        resource_type resource = RESOURCE_NONE;
        int amount = 0;
    };

    InboundReservation *reservation_for(unsigned int figure_id);
    void release_inbound(InboundReservation *reservation);
    int reservation_is_current(const InboundReservation &reservation) const;
    void prune_inbound_reservations();

    ::building *building_ = nullptr;
    const building_type_registry_impl::StorageType *type_ = nullptr;
    size_t slot_index_ = 0;
    std::vector<InboundReservation> inbound_reservations_;
};

namespace storage_runtime_impl {

extern std::vector<std::vector<std::unique_ptr<BuildingStorage>>> g_city_storage_slots;

void reset();
void initialize_city();
BuildingStorage *get_or_create(::building *building, size_t slot_index);
size_t get_slot_count(::building *building);

}
