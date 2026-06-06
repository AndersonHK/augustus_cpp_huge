#pragma once

#include "building/building_fwd.h"

#ifdef __cplusplus
extern "C" {
#endif

int building_image_get_base_farm_crop(building_type type);

int building_image_get_garden_gate_image(int grid_offset);

int building_image_get(const building *b);

int building_image_get_for_type(building_type type);

#ifdef __cplusplus
}
#endif
