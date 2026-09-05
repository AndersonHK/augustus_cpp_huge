#pragma once

#include "building/HousingDef.h"

#include <cstdint>
#include <vector>

namespace building_type_registry_impl {

struct HousingFootprintCell {
    int x = 0;
    int y = 0;

    bool operator==(const HousingFootprintCell &other) const
    {
        return x == other.x && y == other.y;
    }
};

struct HousingMergeParticipant {
    uint32_t building_id = 0;
    const BuildingType *merge_target = nullptr;
    std::vector<HousingFootprintCell> cells;
    bool qualifies = false;
};

enum class HousingMergeFailure {
    None,
    EmptyTarget,
    EmptyParticipant,
    DuplicateBuilding,
    DuplicateTargetCell,
    ParticipantOutsideTarget,
    OverlappingParticipants,
    IncompatibleParticipant,
    IncompleteCoverage
};

struct HousingMergePlan {
    HousingMergeFailure failure = HousingMergeFailure::None;
    std::vector<uint32_t> participant_ids;

    explicit operator bool() const
    {
        return failure == HousingMergeFailure::None;
    }
};

HousingMergePlan plan_housing_merge(
    const BuildingType *target_type,
    const std::vector<HousingFootprintCell> &target_cells,
    const std::vector<HousingMergeParticipant> &participants);

} // namespace building_type_registry_impl
