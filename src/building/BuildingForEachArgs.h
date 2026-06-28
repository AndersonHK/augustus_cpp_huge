#pragma once

#include <optional>

namespace building_type_registry_impl {
class BuildingType;
}

struct BuildingForEachArgs {
    const building_type_registry_impl::BuildingType *BuildingType = nullptr;
    std::optional<bool> hasHousing;
    std::optional<bool> hasLabor;
    std::optional<bool> hasProductionMethod;
};
