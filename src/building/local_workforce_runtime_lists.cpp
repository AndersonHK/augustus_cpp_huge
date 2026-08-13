#include "building/local_workforce_runtime_lists.h"

#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/count.h"

namespace building_local_workforce {

void RuntimeBuildingLists::clear()
{
    labor_source_house_ids_.clear();
    building_count_snapshot_ = 0;
    dirty_ = true;
}

void RuntimeBuildingLists::markDirty()
{
    dirty_ = true;
}

void RuntimeBuildingLists::forEachLaborSourceHouse(const std::function<void(Building &, building &)> &visitor)
{
    ensureCurrent();
    for (unsigned int house_id : labor_source_house_ids_) {
        if (house_id >= building_runtime_impl::g_runtime_instances.size()) {
            dirty_ = true;
            continue;
        }
        building_runtime *runtime = building_runtime_impl::g_runtime_instances[house_id].get();
        if (!runtime) {
            dirty_ = true;
            continue;
        }
        building *house = const_cast<building *>(runtime->building.record());
        if (!isLaborSourceHouse(house)) {
            dirty_ = true;
            continue;
        }
        visitor(runtime->building, *house);
    }
}

void RuntimeBuildingLists::forEachPopulatedLaborSourceHouse(
    const std::function<void(Building &, building &)> &visitor)
{
    forEachLaborSourceHouse([&visitor](Building &house_object, building &house) {
        if (house_object.Housing->state().population > 0) {
            visitor(house_object, house);
        }
    });
}

int RuntimeBuildingLists::buildingCountChanged() const
{
    return building_count_snapshot_ != building_count();
}

int RuntimeBuildingLists::isLaborSourceHouse(const building *record) const
{
    const auto *definition = record ? building_type_registry_impl::definition_for_type(record->type) : nullptr;
    return record && record->id && record->state == BUILDING_STATE_IN_USE && definition && definition->has_housing();
}

void RuntimeBuildingLists::ensureCurrent()
{
    if (dirty_ || buildingCountChanged()) {
        rebuild();
    }
}

void RuntimeBuildingLists::rebuild()
{
    labor_source_house_ids_.clear();
    building_count_snapshot_ = building_count();
    Building::for_each(BuildingRuntimeList::Housing, [this](Building *building) {
        ::building *house = const_cast<::building *>(building->record());
        if (isLaborSourceHouse(house)) {
            labor_source_house_ids_.push_back(building->id);
        }
    });
    dirty_ = false;
}

} // namespace building_local_workforce
