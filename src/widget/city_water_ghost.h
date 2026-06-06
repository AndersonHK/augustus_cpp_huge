#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "building/building_type.h"

void city_water_ghost_draw_water_structure_ranges(void);
void city_water_ghost_draw_reservoir_ranges(void);
void city_water_ghost_draw_preview(building_type type, int primary_grid_offset, int secondary_grid_offset);

#ifdef __cplusplus
}
#endif
