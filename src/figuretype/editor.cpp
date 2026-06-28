#include "editor.h"

#include "core/image.h"
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
    f->select_legacy_directional_frame_image(
        image_group(GROUP_FIGURE_MAP_FLAG_FLAGS),
        0,
        f->image_offset / 2,
        1);
    map_figure_delete(f);

    map_point point = {0, 0};
    int id = f->resource_id;
    int image_base = image_group(GROUP_FIGURE_MAP_FLAG_ICONS);
    if (id == MAP_FLAG_EARTHQUAKE) {
        point = scenario_editor_earthquake_point();
        f->select_legacy_cart_overlay_base_image(image_base);
    } else if (id == MAP_FLAG_ENTRY) {
        point = scenario_map_entry();
        f->select_legacy_cart_overlay_base_image(image_base + 2);
    } else if (id == MAP_FLAG_EXIT) {
        point = scenario_map_exit();
        f->select_legacy_cart_overlay_base_image(image_base + 3);
    } else if (id == MAP_FLAG_RIVER_ENTRY) {
        point = scenario_map_river_entry();
        f->select_legacy_cart_overlay_base_image(image_base + 4);
    } else if (id == MAP_FLAG_RIVER_EXIT) {
        point = scenario_map_river_exit();
        f->select_legacy_cart_overlay_base_image(image_base + 5);
    } else if (id >= MAP_FLAG_INVASION_MIN && id < MAP_FLAG_INVASION_MAX) {
        point = scenario_editor_invasion_point(id - MAP_FLAG_INVASION_MIN);
        f->select_legacy_cart_overlay_base_image(image_base + 1);
    } else if (id >= MAP_FLAG_FISHING_MIN && id < MAP_FLAG_FISHING_MAX) {
        point = scenario_editor_fishing_point(id - MAP_FLAG_FISHING_MIN);
        f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 3);
    } else if (id >= MAP_FLAG_HERD_MIN && id < MAP_FLAG_HERD_MAX) {
        point = scenario_editor_herd_point(id - MAP_FLAG_HERD_MIN);
        f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_FORT_STANDARD_ICONS) + 4);
    }
    f->x = static_cast<unsigned char>(point.x);
    f->y = static_cast<unsigned char>(point.y);

    f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
    f->cross_country_x = static_cast<short>(figure_movement_tile_center_cross_country(f->x));
    f->cross_country_y = static_cast<short>(figure_movement_tile_center_cross_country(f->y));
    map_figure_add(f);
}
