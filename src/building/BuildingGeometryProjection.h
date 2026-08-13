#pragma once

#include "building/BuildingGeometry.h"
#include "core/direction.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

struct ProjectedBuildingGeometryCell {
    int x = 0;
    int y = 0;
    int overlay_image_variant = 0;
};

inline std::vector<ProjectedBuildingGeometryCell> project_building_geometry(
    const BuildingGeometry &geometry,
    int draw_x,
    int draw_y,
    int view_orientation)
{
    if (!geometry.valid()) {
        return {};
    }

    std::vector<ProjectedBuildingGeometryCell> projected;
    projected.reserve(geometry.cells().size());
    std::set<std::pair<int, int>> occupied;
    for (const BuildingGeometryCell &cell : geometry.cells()) {
        const int dx = cell.x - draw_x;
        const int dy = cell.y - draw_y;
        ProjectedBuildingGeometryCell position;
        switch (view_orientation) {
            case DIR_2_RIGHT:
                position.x = 30 * (dx + dy);
                position.y = 15 * (dy - dx);
                break;
            case DIR_4_BOTTOM:
                position.x = 30 * (dy - dx);
                position.y = -15 * (dx + dy);
                break;
            case DIR_6_LEFT:
                position.x = -30 * (dx + dy);
                position.y = 15 * (dx - dy);
                break;
            case DIR_0_TOP:
            default:
                position.x = 30 * (dx - dy);
                position.y = 15 * (dx + dy);
                break;
        }
        projected.push_back(position);
        occupied.emplace(position.x, position.y);
    }

    for (ProjectedBuildingGeometryCell &cell : projected) {
        const bool has_upper_right = occupied.count({ cell.x + 30, cell.y - 15 }) != 0;
        const bool has_upper_left = occupied.count({ cell.x - 30, cell.y - 15 }) != 0;
        cell.overlay_image_variant = (has_upper_right ? 1 : 0) + (has_upper_left ? 2 : 0);
    }
    std::sort(projected.begin(), projected.end(), [](const ProjectedBuildingGeometryCell &left,
        const ProjectedBuildingGeometryCell &right) {
        return left.y != right.y ? left.y < right.y : left.x < right.x;
    });
    return projected;
}

} // namespace building_type_registry_impl
