#

#include "building/storage_runtime.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"

extern "C" {
#include "building/building_record.h"
}

namespace storage_runtime_impl {

std::vector<std::vector<std::unique_ptr<StorageSlot>>> g_city_storage_slots;

void reset()
{
    g_city_storage_slots.clear();
}

StorageSlot *get_or_create(::building *building, size_t slot_index)
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

    std::vector<std::unique_ptr<StorageSlot>> &slots = g_city_storage_slots[building->id];
    if (slots.size() < storage_types.size()) {
        slots.resize(storage_types.size());
    }

    std::unique_ptr<StorageSlot> &slot = slots[slot_index];
    if (!slot || slot->building() != building || slot->type() != storage_types[slot_index]) {
        slot = std::make_unique<StorageSlot>(building, storage_types[slot_index], slot_index);
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

    const int total_buildings = building_count();
    for (int id = 1; id < total_buildings; id++) {
        ::building *building = building_get(id);
        if (!building || !building->id) {
            continue;
        }

        const size_t slot_count = get_slot_count(building);
        for (size_t i = 0; i < slot_count; i++) {
            get_or_create(building, i);
        }
    }
}

} // namespace storage_runtime_impl
