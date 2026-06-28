#pragma once

#include "building/building.h"

#include <functional>
#include <vector>

namespace building_local_workforce {

class RuntimeBuildingLists {
public:
    void clear();
    void markDirty();
    void forEachLaborSourceHouse(const std::function<void(Building, building &)> &visitor);
    void forEachPopulatedLaborSourceHouse(const std::function<void(Building, building &)> &visitor);

private:
    int buildingCountChanged() const;
    int isLaborSourceHouse(const building *record) const;
    void ensureCurrent();
    void rebuild();

    std::vector<unsigned int> labor_source_house_ids_;
    int building_count_snapshot_ = 0;
    bool dirty_ = true;
};

} // namespace building_local_workforce
