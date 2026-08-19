#pragma once

inline int access_ramp_connects_to_road_direction(
    int image_orientation,
    int view_orientation,
    int direction)
{
    if (image_orientation < 0 || image_orientation >= 4 ||
        view_orientation < 0 || view_orientation >= 8 || (view_orientation & 1) ||
        direction < 0 || direction >= 8 || (direction & 1)) {
        return 0;
    }
    const int world_orientation = (image_orientation + view_orientation / 2) % 4;
    return (world_orientation & 1) == ((direction / 2) & 1);
}

inline int access_ramp_edge_allows_road_direction(
    int source_image_orientation,
    int target_image_orientation,
    int view_orientation,
    int direction)
{
    if (source_image_orientation < -1 || source_image_orientation >= 4 ||
        target_image_orientation < -1 || target_image_orientation >= 4) {
        return 0;
    }
    return (source_image_orientation < 0 ||
            access_ramp_connects_to_road_direction(
                source_image_orientation, view_orientation, direction)) &&
        (target_image_orientation < 0 ||
            access_ramp_connects_to_road_direction(
                target_image_orientation, view_orientation, direction));
}
