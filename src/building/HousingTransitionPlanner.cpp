#include "building/HousingTransitionPlanner.h"

#include <algorithm>
#include <set>

namespace building_type_registry_impl {

namespace {

using Cell = std::pair<int, int>;

Cell key(const HousingFootprintCell &cell)
{
    return { cell.x, cell.y };
}

HousingMergePlan failure(HousingMergeFailure reason)
{
    HousingMergePlan result;
    result.failure = reason;
    return result;
}

} // namespace

HousingMergePlan plan_housing_merge(
    const BuildingType *target_type,
    const std::vector<HousingFootprintCell> &target_cells,
    const std::vector<HousingMergeParticipant> &participants)
{
    if (!target_type || target_cells.empty()) {
        return failure(HousingMergeFailure::EmptyTarget);
    }

    std::set<Cell> target;
    for (const HousingFootprintCell &cell : target_cells) {
        if (!target.insert(key(cell)).second) {
            return failure(HousingMergeFailure::DuplicateTargetCell);
        }
    }

    std::set<Cell> covered;
    std::set<uint32_t> building_ids;
    HousingMergePlan result;
    for (const HousingMergeParticipant &participant : participants) {
        if (!participant.building_id || !building_ids.insert(participant.building_id).second) {
            return failure(HousingMergeFailure::DuplicateBuilding);
        }
        if (participant.cells.empty()) {
            return failure(HousingMergeFailure::EmptyParticipant);
        }
        if (!participant.qualifies || participant.merge_target != target_type) {
            return failure(HousingMergeFailure::IncompatibleParticipant);
        }

        std::set<Cell> participant_cells;
        for (const HousingFootprintCell &cell : participant.cells) {
            const Cell cell_key = key(cell);
            if (!participant_cells.insert(cell_key).second || !target.count(cell_key)) {
                return failure(HousingMergeFailure::ParticipantOutsideTarget);
            }
            if (!covered.insert(cell_key).second) {
                return failure(HousingMergeFailure::OverlappingParticipants);
            }
        }
        result.participant_ids.push_back(participant.building_id);
    }

    if (covered != target) {
        return failure(HousingMergeFailure::IncompleteCoverage);
    }

    std::sort(result.participant_ids.begin(), result.participant_ids.end());
    return result;
}

} // namespace building_type_registry_impl
