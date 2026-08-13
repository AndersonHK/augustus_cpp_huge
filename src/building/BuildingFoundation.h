#pragma once

#include "building/FoundationDef.h"
#include "building/FoundationState.h"
#include "core/direction.h"

class Building;

namespace building_type_registry_impl {

struct FoundationDrawAnchor {
    int x = 0;
    int y = 0;
    int valid = 0;
};

inline FoundationDrawAnchor foundation_draw_anchor(
    const std::vector<RotatedFoundationCell> &cells,
    int width,
    int height,
    int view_orientation)
{
    const auto score = [width, height, view_orientation](const RotatedFoundationCell &cell) {
        switch (view_orientation) {
            case DIR_0_TOP: return cell.x + (height - 1 - cell.y);
            case DIR_2_RIGHT: return cell.x + cell.y;
            case DIR_4_BOTTOM: return (width - 1 - cell.x) + cell.y;
            case DIR_6_LEFT: return (width - 1 - cell.x) + (height - 1 - cell.y);
            default: return cell.x + cell.y;
        }
    };
    const RotatedFoundationCell *best = nullptr;
    for (const RotatedFoundationCell &cell : cells) {
        if (!cell.definition || !cell.definition->binds_building) {
            continue;
        }
        if (!best || score(cell) < score(*best)) {
            best = &cell;
        }
    }
    return best ? FoundationDrawAnchor{ best->x, best->y, 1 } : FoundationDrawAnchor{};
}

class BuildingType;

// Owner-bound foundation geometry, map publication, and exact terrain rollback.
class BuildingFoundation {
public:
    BuildingFoundation(Building &owner, const FoundationDef &definition, FoundationState &state);
    ~BuildingFoundation();

    Building &owner() const;
    const FoundationDef &definition() const;
    FoundationState &state() const;
    int width(int rotation) const;
    int height(int rotation) const;
    std::vector<RotatedFoundationCell> cells(int rotation) const;
    int publish(int origin_x, int origin_y, int rotation, int image_id);
    int refresh(int image_id);
    int remove();
    int rebind(int origin_x, int origin_y, int rotation);
    int draw_grid_offset(int view_orientation) const;
    FoundationPassage passage_at(int grid_offset) const;
    int has_owner_controlled_passage() const;
    int has_unrestricted_road_crossing() const;
    int passage_axis() const;
    int allows_passage(int grid_offset, int permission) const;
    RoadblockState &roadblock_state() const;
    int contains_grid_offset(int grid_offset) const;
    const FoundationTerrainDelta *terrain_delta_at(int grid_offset) const;
    static Building *unbound_owner_at(
        int grid_offset,
        const BuildingType *type);
    void detach_unbound_ownership();
    void restore_unbound_ownership();

private:
    void register_unbound_cells();
    void unregister_unbound_cells();

    Building *owner_ = nullptr;
    const FoundationDef *definition_ = nullptr;
    FoundationState *state_ = nullptr;
    std::vector<int> registered_unbound_offsets_;
};

} // namespace building_type_registry_impl
