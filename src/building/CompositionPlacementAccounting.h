#pragma once

#include <vector>

namespace building_construction {

enum class RepairRubbleOccupancy {
    None,
    MatchingOrigin,
    ForeignOrigin
};

struct PlacementAccountingCell {
    int world_cell = 0;
    bool belongs_to_owner = false;
    bool cleared = false;
    bool requires_rubble = false;
    RepairRubbleOccupancy rubble = RepairRubbleOccupancy::None;
};

struct PlacementAccountingResult {
    int unique_cell_count = 0;
    int unique_clear_cell_count = 0;
    int owner_charge_count = 0;
    int required_rubble_cell_count = 0;
    int matching_rubble_cell_count = 0;
    int foreign_rubble_cell_count = 0;
    bool duplicate_world_cells = false;

    bool publication_valid() const;
    bool repair_coverage_valid() const;
    int total_cost(int owner_cost, int clear_cost_per_cell) const;
};

// Reduces per-member cells to one world-cell set. Duplicate member cells are
// reported and never multiply clearance, rubble coverage, or economics.
PlacementAccountingResult summarize_placement_cells(
    const std::vector<PlacementAccountingCell> &cells);

} // namespace building_construction
