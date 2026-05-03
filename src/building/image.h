#pragma once

#include "building/building.h"

int building_image_get_base_farm_crop(building_type type);

int building_image_get_garden_gate_image(int grid_offset);

int building_image_get(const building *b);

int building_image_get_for_type(building_type type);
