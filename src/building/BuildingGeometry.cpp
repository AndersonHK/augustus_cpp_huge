#include "building/BuildingGeometry.h"

#include "building/BuildingComposition.h"
#include "building/BuildingFoundation.h"
#include "building/building.h"
#include "map/grid.h"
#include "map/terrain.h"

#include <set>
#include <utility>

namespace building_type_registry_impl {

BuildingGeometry BuildingGeometry::query(const Building &building)
{
    const Building *owner = building.Foundation ? &building.Foundation->owner() : nullptr;
    if (building.Composition && building.Composition->is_composed()) {
        owner = building.Composition->owner();
    }
    if (!owner || !owner->record()) {
        return {};
    }

    std::vector<const Building *> members;
    if (owner->Composition && owner->Composition->is_owner()) {
        if (!owner->Composition->complete()) {
            return {};
        }
        owner->Composition->for_each_member([&members](Building &member) {
            members.push_back(&member);
        });
    } else {
        members.push_back(owner);
    }

    std::vector<BuildingGeometryCell> world_cells;
    std::set<std::pair<int, int>> occupied;
    for (const Building *member : members) {
        if (!member || !member->record() || !member->Foundation ||
            !member->Foundation->state().is_published()) {
            return {};
        }
        const FoundationState &state = member->Foundation->state();
        const std::vector<RotatedFoundationCell> cells =
            member->Foundation->cells(state.rotation());
        if (cells.empty()) {
            return {};
        }
        for (const RotatedFoundationCell &cell : cells) {
            const int x = state.origin_x() + cell.x;
            const int y = state.origin_y() + cell.y;
            if (!cell.definition || !map_grid_is_inside(x, y, 1) ||
                !occupied.emplace(x, y).second) {
                return {};
            }
            world_cells.push_back({
                x,
                y,
                member,
                cell.definition,
                cell.definition->passage,
                (cell.definition->required_terrain & TERRAIN_WATER) != 0
            });
        }
    }

    BuildingGeometry geometry = from_world_cells(std::move(world_cells));
    geometry.owner_ = owner;
    const auto outside_map = [](const BuildingGeometryPoint &point) {
        return !map_grid_is_inside(point.x, point.y, 1);
    };
    geometry.access_candidates_.erase(
        std::remove_if(geometry.access_candidates_.begin(), geometry.access_candidates_.end(), outside_map),
        geometry.access_candidates_.end());
    geometry.water_candidates_.erase(
        std::remove_if(geometry.water_candidates_.begin(), geometry.water_candidates_.end(), outside_map),
        geometry.water_candidates_.end());
    return geometry;
}

} // namespace building_type_registry_impl
