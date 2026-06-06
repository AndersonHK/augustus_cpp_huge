#pragma once

#include "graphics/color.h"

#ifdef __cplusplus
class Building;

struct BuildingDrawContext {
    int x = 0;
    int y = 0;
    int grid_offset = 0;
    color_t color_mask = 0;
    float scale = 1.0f;
    int force_draw_tile = 0;
};

extern "C" {
#endif

typedef struct building building;
typedef struct order order;

#ifdef __cplusplus
}
#endif

#include "building/building_type.h"
