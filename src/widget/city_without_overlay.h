#pragma once

#include "map/point.h"
#include "widget/city.h"

#ifdef __cplusplus
extern "C" {
#endif

void city_without_overlay_draw(int selected_figure_id, pixel_coordinate *figure_coord,
                            const map_tile *tile, unsigned int roamer_preview_building_id);

#ifdef __cplusplus
}
#endif
