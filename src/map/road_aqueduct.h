#pragma once

#include "building/building_type.h"

struct road_preview_graphic {
    int image_id = 0;
    building_type building = BUILDING_NONE;
    int blocked = 0;
};

int map_can_place_road_under_aqueduct(int grid_offset);

road_preview_graphic map_road_preview_graphic_at(int grid_offset);

int map_can_place_aqueduct_on_road(int grid_offset);

int map_get_aqueduct_with_road_image(int grid_offset);

int map_is_straight_road_for_aqueduct(int grid_offset);

int map_can_place_aqueduct_on_highway(int grid_offset, int check_aqueduct_routing);

int map_can_place_highway_under_aqueduct(int grid_offset, int check_highway_routing);

