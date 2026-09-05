#pragma once

#include "building/FoundationDef.h"
#include "map/terrain.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>

class Building;

namespace building_type_registry_impl {

struct BuildingGeometryPoint {
    int x = 0;
    int y = 0;
};

struct BuildingGeometryBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;

    int width() const { return max_x - min_x; }
    int height() const { return max_y - min_y; }
};

struct BuildingGeometryCell {
    int x = 0;
    int y = 0;
    const Building *member = nullptr;
    const FoundationCellDefinition *definition = nullptr;
    FoundationPassage passage = FoundationPassage::None;
    bool requires_water = false;
};

struct BuildingGeometrySquareExtent {
    int x = 0;
    int y = 0;
    int size = 0;
};

// Immutable normalized geometry for one live building owner. A composed child
// resolves to its owner, and every published member foundation contributes to
// the same deduplicated world-cell set.
class BuildingGeometry {
public:
    static BuildingGeometry query(const Building &building);
    static BuildingGeometry from_foundation(
        const FoundationDef &foundation,
        int origin_x,
        int origin_y,
        int rotation);

    // Pure construction seam used by geometry tests and future planned-layout
    // consumers. Runtime callers should use query(), which performs publication
    // and map-bound validation before constructing the snapshot.
    static BuildingGeometry from_world_cells(std::vector<BuildingGeometryCell> cells)
    {
        if (cells.empty()) {
            return {};
        }

        std::sort(cells.begin(), cells.end(), [](const BuildingGeometryCell &left,
            const BuildingGeometryCell &right) {
            return left.y != right.y ? left.y < right.y : left.x < right.x;
        });

        BuildingGeometry geometry;
        geometry.valid_ = true;
        geometry.cells_.reserve(cells.size());
        for (const BuildingGeometryCell &cell : cells) {
            if (!geometry.cells_.empty() &&
                geometry.cells_.back().x == cell.x && geometry.cells_.back().y == cell.y) {
                BuildingGeometryCell &existing = geometry.cells_.back();
                if (!existing.member) {
                    existing.member = cell.member;
                }
                if (!existing.definition) {
                    existing.definition = cell.definition;
                }
                if (static_cast<int>(cell.passage) > static_cast<int>(existing.passage)) {
                    existing.passage = cell.passage;
                }
                existing.requires_water |= cell.requires_water;
                continue;
            }
            geometry.cells_.push_back(cell);
        }
        geometry.rebuild_derived_geometry();
        return geometry;
    }

    bool valid() const { return valid_; }
    const Building *owner() const { return owner_; }
    const std::vector<BuildingGeometryCell> &cells() const { return cells_; }
    const BuildingGeometryBounds &bounds() const { return bounds_; }
    const std::vector<BuildingGeometryPoint> &access_candidates() const { return access_candidates_; }
    const std::vector<BuildingGeometryPoint> &water_candidates() const { return water_candidates_; }
    bool occupies_multiple_cells() const { return cells_.size() > 1; }

    std::vector<BuildingGeometryPoint> points_at_distance(int distance) const
    {
        std::vector<BuildingGeometryPoint> result;
        if (!valid_ || distance <= 0) {
            return result;
        }
        for (int y = bounds_.min_y - distance; y < bounds_.max_y + distance; ++y) {
            for (int x = bounds_.min_x - distance; x < bounds_.max_x + distance; ++x) {
                if (distance_to(x, y) == distance) {
                    result.push_back({ x, y });
                }
            }
        }
        return result;
    }

    std::vector<BuildingGeometryPoint> access_points_at_distance(int distance) const
    {
        if (!valid_ || distance <= 0 || access_candidates_.empty()) {
            return {};
        }
        if (distance == 1) {
            return access_candidates_;
        }
        std::vector<BuildingGeometryPoint> result;
        for (int y = bounds_.min_y - distance; y < bounds_.max_y + distance; ++y) {
            for (int x = bounds_.min_x - distance; x < bounds_.max_x + distance; ++x) {
                if (contains(x, y)) {
                    continue;
                }
                int nearest = distance;
                for (const BuildingGeometryPoint &access : access_candidates_) {
                    nearest = std::min(nearest,
                        std::max(std::abs(x - access.x), std::abs(y - access.y)));
                }
                if (nearest == distance - 1) {
                    result.push_back({ x, y });
                }
            }
        }
        return result;
    }

    const BuildingGeometryCell *interior_cell_for_access(int x, int y) const
    {
        const bool has_passage = std::any_of(cells_.begin(), cells_.end(),
            [](const BuildingGeometryCell &cell) {
                return cell.passage != FoundationPassage::None;
            });
        const auto found = std::find_if(cells_.begin(), cells_.end(),
            [x, y, has_passage](const BuildingGeometryCell &cell) {
                return (!has_passage || cell.passage != FoundationPassage::None) &&
                    std::abs(cell.x - x) + std::abs(cell.y - y) == 1;
            });
        return found == cells_.end() ? nullptr : &*found;
    }

    bool contains(int x, int y) const
    {
        return std::binary_search(cells_.begin(), cells_.end(), BuildingGeometryCell{ x, y },
            [](const BuildingGeometryCell &left, const BuildingGeometryCell &right) {
            return left.y != right.y ? left.y < right.y : left.x < right.x;
        });
    }

    int distance_to(int x, int y) const
    {
        if (!valid_) {
            return -1;
        }
        int nearest = std::max(std::abs(x - cells_.front().x), std::abs(y - cells_.front().y));
        for (const BuildingGeometryCell &cell : cells_) {
            nearest = std::min(nearest, std::max(std::abs(x - cell.x), std::abs(y - cell.y)));
        }
        return nearest;
    }

    int distance_to(const BuildingGeometry &other) const
    {
        if (!valid_ || !other.valid_) {
            return -1;
        }
        int nearest = std::max(
            std::abs(cells_.front().x - other.cells_.front().x),
            std::abs(cells_.front().y - other.cells_.front().y));
        for (const BuildingGeometryCell &cell : cells_) {
            for (const BuildingGeometryCell &other_cell : other.cells_) {
                nearest = std::min(nearest,
                    std::max(std::abs(cell.x - other_cell.x),
                        std::abs(cell.y - other_cell.y)));
            }
        }
        return nearest;
    }

    bool contains_within_range(int x, int y, int range) const
    {
        if (!valid_ || range < 0 || x < bounds_.min_x - range || x >= bounds_.max_x + range ||
            y < bounds_.min_y - range || y >= bounds_.max_y + range) {
            return false;
        }
        return distance_to(x, y) <= range;
    }

    BuildingGeometrySquareExtent centered_square_extent(int maximum_size) const
    {
        if (!valid_ || maximum_size <= 0) {
            return {};
        }
        const int size = std::min(maximum_size, std::max(bounds_.width(), bounds_.height()));
        return {
            bounds_.min_x + std::max(0, (bounds_.width() - size) / 2),
            bounds_.min_y + std::max(0, (bounds_.height() - size) / 2),
            size
        };
    }

private:
    void rebuild_derived_geometry()
    {
        if (cells_.empty()) {
            valid_ = false;
            return;
        }

        bounds_ = { cells_.front().x, cells_.front().y, cells_.front().x + 1, cells_.front().y + 1 };
        std::set<std::pair<int, int>> occupied;
        bool has_passage = false;
        for (const BuildingGeometryCell &cell : cells_) {
            bounds_.min_x = std::min(bounds_.min_x, cell.x);
            bounds_.min_y = std::min(bounds_.min_y, cell.y);
            bounds_.max_x = std::max(bounds_.max_x, cell.x + 1);
            bounds_.max_y = std::max(bounds_.max_y, cell.y + 1);
            occupied.emplace(cell.x, cell.y);
            has_passage |= cell.passage != FoundationPassage::None;
        }

        static constexpr int neighbors[][2] = {
            { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 }
        };
        std::set<std::pair<int, int>> access;
        std::set<std::pair<int, int>> water;
        for (const BuildingGeometryCell &cell : cells_) {
            const bool contributes_access = !has_passage || cell.passage != FoundationPassage::None;
            for (const auto &neighbor : neighbors) {
                const std::pair<int, int> candidate = {
                    cell.x + neighbor[0], cell.y + neighbor[1]
                };
                if (occupied.count(candidate)) {
                    continue;
                }
                if (contributes_access) {
                    access.insert(candidate);
                }
                if (cell.requires_water) {
                    water.insert(candidate);
                }
            }
        }

        const auto append_points = [](const std::set<std::pair<int, int>> &source,
            std::vector<BuildingGeometryPoint> *destination) {
            destination->reserve(source.size());
            for (const auto &point : source) {
                destination->push_back({ point.first, point.second });
            }
            std::sort(destination->begin(), destination->end(), [](const BuildingGeometryPoint &left,
                const BuildingGeometryPoint &right) {
                return left.y != right.y ? left.y < right.y : left.x < right.x;
            });
        };
        append_points(access, &access_candidates_);
        append_points(water, &water_candidates_);
    }

    bool valid_ = false;
    const Building *owner_ = nullptr;
    BuildingGeometryBounds bounds_;
    std::vector<BuildingGeometryCell> cells_;
    std::vector<BuildingGeometryPoint> access_candidates_;
    std::vector<BuildingGeometryPoint> water_candidates_;
};

inline BuildingGeometry BuildingGeometry::from_foundation(
    const FoundationDef &foundation,
    int origin_x,
    int origin_y,
    int rotation)
{
    const int normalized_rotation = foundation.rotates() ? (rotation % 4 + 4) % 4 : 0;
    std::vector<BuildingGeometryCell> world_cells;
    for (const RotatedFoundationCell &cell : foundation.rotated_cells(normalized_rotation)) {
        if (!cell.definition) {
            return {};
        }
        world_cells.push_back({
            origin_x + cell.x,
            origin_y + cell.y,
            nullptr,
            cell.definition,
            cell.definition->passage,
            (cell.definition->required_terrain & TERRAIN_WATER) != 0
        });
    }
    return from_world_cells(std::move(world_cells));
}

} // namespace building_type_registry_impl
