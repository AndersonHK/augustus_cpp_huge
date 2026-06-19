#pragma once

#include "building/storage_type.h"

#include "building/building_fwd.h"

#include <cstddef>
#include <memory>
#include <vector>

class StorageSlot {
public:
    StorageSlot(::building *building, const building_type_registry_impl::StorageType *type, size_t slot_index)
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

private:
    ::building *building_ = nullptr;
    const building_type_registry_impl::StorageType *type_ = nullptr;
    size_t slot_index_ = 0;
};

namespace storage_runtime_impl {

extern std::vector<std::vector<std::unique_ptr<StorageSlot>>> g_city_storage_slots;

void reset();
void initialize_city();
StorageSlot *get_or_create(::building *building, size_t slot_index);
size_t get_slot_count(::building *building);

}
