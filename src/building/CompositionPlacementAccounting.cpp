#include "building/CompositionPlacementAccounting.h"

#include <map>

namespace building_construction {

bool PlacementAccountingResult::publication_valid() const
{
    return !duplicate_world_cells && unique_cell_count > 0 && owner_charge_count == 1;
}

bool PlacementAccountingResult::repair_coverage_valid() const
{
    // Missing original rubble is valid when ordinary foundation validation
    // found the cell clear/permitted. Only foreign-origin rubble is an
    // ownership conflict; terrain and other objects are rejected separately.
    return foreign_rubble_cell_count == 0;
}

int PlacementAccountingResult::total_cost(int owner_cost, int clear_cost_per_cell) const
{
    return owner_charge_count * owner_cost + unique_clear_cell_count * clear_cost_per_cell;
}

PlacementAccountingResult summarize_placement_cells(
    const std::vector<PlacementAccountingCell> &cells)
{
    std::map<int, PlacementAccountingCell> unique;
    PlacementAccountingResult result;
    for (const PlacementAccountingCell &cell : cells) {
        auto [it, inserted] = unique.emplace(cell.world_cell, cell);
        if (inserted) {
            continue;
        }
        result.duplicate_world_cells = true;
        PlacementAccountingCell &merged = it->second;
        merged.belongs_to_owner = merged.belongs_to_owner || cell.belongs_to_owner;
        merged.cleared = merged.cleared || cell.cleared;
        merged.requires_rubble = merged.requires_rubble || cell.requires_rubble;
        if (cell.rubble == RepairRubbleOccupancy::ForeignOrigin ||
            merged.rubble == RepairRubbleOccupancy::ForeignOrigin) {
            merged.rubble = RepairRubbleOccupancy::ForeignOrigin;
        } else if (cell.rubble == RepairRubbleOccupancy::MatchingOrigin) {
            merged.rubble = RepairRubbleOccupancy::MatchingOrigin;
        }
    }

    bool has_owner = false;
    result.unique_cell_count = static_cast<int>(unique.size());
    for (const auto &[world_cell, cell] : unique) {
        (void) world_cell;
        has_owner = has_owner || cell.belongs_to_owner;
        result.unique_clear_cell_count += cell.cleared ? 1 : 0;
        result.required_rubble_cell_count += cell.requires_rubble ? 1 : 0;
        if (cell.requires_rubble && cell.rubble == RepairRubbleOccupancy::MatchingOrigin) {
            ++result.matching_rubble_cell_count;
        }
        if (cell.rubble == RepairRubbleOccupancy::ForeignOrigin) {
            ++result.foreign_rubble_cell_count;
        }
    }
    result.owner_charge_count = has_owner ? 1 : 0;
    return result;
}

} // namespace building_construction
