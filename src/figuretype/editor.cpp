#include "editor.h"

#include "figure/image.h"
#include "figure/movement.h"
#include "map/figure.h"
#include "map/grid.h"
#include "scenario/editor_map.h"
#include "scenario/map.h"

void figure_create_editor_flags(void)
{
    for (int id = MAP_FLAG_MIN; id < MAP_FLAG_MAX; id++) {
        Figure::create(FIGURE_MAP_FLAG, -1, -1, DIR_0_TOP)->resource_id = static_cast<unsigned char>(id);
    }
}

void figure_editor_flag_action(Figure *f)
{
    figure_image_increase_offset(f, 16);
    f->clear_legacy_image();
    f->clear_legacy_cart_overlay_image();
    map_figure_delete(f);

    map_point point = {0, 0};
    int id = f->resource_id;
    if (id == MAP_FLAG_EARTHQUAKE) {
        point = scenario_editor_earthquake_point();
    } else if (id == MAP_FLAG_ENTRY) {
        point = scenario_map_entry();
    } else if (id == MAP_FLAG_EXIT) {
        point = scenario_map_exit();
    } else if (id == MAP_FLAG_RIVER_ENTRY) {
        point = scenario_map_river_entry();
    } else if (id == MAP_FLAG_RIVER_EXIT) {
        point = scenario_map_river_exit();
    } else if (id >= MAP_FLAG_INVASION_MIN && id < MAP_FLAG_INVASION_MAX) {
        point = scenario_editor_invasion_point(id - MAP_FLAG_INVASION_MIN);
    } else if (id >= MAP_FLAG_FISHING_MIN && id < MAP_FLAG_FISHING_MAX) {
        point = scenario_editor_fishing_point(id - MAP_FLAG_FISHING_MIN);
    } else if (id >= MAP_FLAG_HERD_MIN && id < MAP_FLAG_HERD_MAX) {
        point = scenario_editor_herd_point(id - MAP_FLAG_HERD_MIN);
    }
    f->x = static_cast<unsigned char>(point.x);
    f->y = static_cast<unsigned char>(point.y);

    f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
    f->cross_country_x = figure_movement_tile_center_cross_country(f->x);
    f->cross_country_y = figure_movement_tile_center_cross_country(f->y);
    map_figure_add(f);
}
