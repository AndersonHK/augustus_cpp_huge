#pragma once

enum class road_aqueduct_axis {
    none,
    x,
    y,
};

inline road_aqueduct_axis road_aqueduct_axis_from_opposite_neighbors(int x_neighbors, int y_neighbors)
{
    if (x_neighbors == 2 && y_neighbors == 0) {
        return road_aqueduct_axis::x;
    }
    if (y_neighbors == 2 && x_neighbors == 0) {
        return road_aqueduct_axis::y;
    }
    return road_aqueduct_axis::none;
}

inline road_aqueduct_axis road_aqueduct_axis_from_connectable_option(int option, int view_swaps_axes)
{
    road_aqueduct_axis axis = road_aqueduct_axis::none;
    switch (option) {
        case 0:
        case 2:
        case 8:
            axis = road_aqueduct_axis::y;
            break;
        case 1:
        case 3:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            axis = road_aqueduct_axis::x;
            break;
    }

    if (view_swaps_axes && axis != road_aqueduct_axis::none) {
        axis = axis == road_aqueduct_axis::x ? road_aqueduct_axis::y : road_aqueduct_axis::x;
    }
    return axis;
}

inline int road_aqueduct_tile_allows_crossing(road_aqueduct_axis aqueduct_axis, int connection_count)
{
    return aqueduct_axis != road_aqueduct_axis::none && connection_count <= 2;
}

inline int road_aqueduct_axes_allow_crossing(
    road_aqueduct_axis aqueduct_axis,
    road_aqueduct_axis road_axis,
    int connection_count)
{
    return road_aqueduct_tile_allows_crossing(aqueduct_axis, connection_count) &&
        road_axis != road_aqueduct_axis::none && road_axis != aqueduct_axis;
}

inline int road_aqueduct_crossing_option(int road_runs_y, int paved_road)
{
    return (paved_road ? 0 : 2) + (road_runs_y ? 1 : 0);
}
