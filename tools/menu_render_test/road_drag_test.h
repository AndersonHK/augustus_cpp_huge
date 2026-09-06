#pragma once

#include "building/construction.h"
#include "building/building_type_registry_internal.h"
#include "city/view.h"
#include "city/finance.h"
#include "city/view_render.h"
#include "core/config.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "widget/city_without_overlay.h"
#include "window/city.h"

#include <cstdio>
#include <cstring>

inline bool run_city_road_drag_render_test()
{
    window_city_show();
    window_draw(1);
    static CityDrawTileCommand endpoint;
    endpoint = {};
    CityViewRenderCommandBuffer commands;
    commands.build();
    const CityViewRenderPhase phases[] = { { [](const CityDrawTileCommand &command) {
        if (endpoint.grid_offset >= 0 || command.building || command.first_figure || command.grid_offset < 0 ||
            !map_property_is_draw_tile(command.grid_offset)) return;
        // A three-tile drag through open land, away from viewport clipping.
        for (int dx = -2; dx <= 0; dx++) {
            const int offset = map_grid_add_delta(command.grid_offset, dx, 0);
            if (offset < 0 || map_terrain_is(offset, TERRAIN_NOT_CLEAR)) return;
        }
        int x, y, width, height;
        city_view_get_viewport(&x, &y, &width, &height);
        const float scale = city_view_get_scale() / 100.0f;
        const int pixel_x = static_cast<int>((command.x + 30) / scale);
        const int pixel_y = static_cast<int>((command.y + 15) / scale);
        if (pixel_x > x + 160 && pixel_x < x + width - 160 && pixel_y > y + 160 && pixel_y < y + height - 160) endpoint = command;
    }, PERFORMANCE_TRACKER_BUCKET_MAX } };
    commands.execute(phases, 1);
    if (endpoint.grid_offset < 0) {
        std::fprintf(stdout, "Road drag render test skipped: no visible clear three-tile route.\n");
        return true;
    }
    const auto *road = building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr("road"));
    if (!road) return false;
    const map_tile end = { map_grid_offset_to_x(endpoint.grid_offset), map_grid_offset_to_y(endpoint.grid_offset), endpoint.grid_offset };
    const map_tile no_tile = {};
    view_tile selected_view;
    city_view_grid_offset_to_xy_view(end.grid_offset, &selected_view.x, &selected_view.y);
    city_view_set_selected_view_tile(&selected_view);
    const int previous_shadow = config_get(CONFIG_UI_CV_CURSOR_SHADOW);
    const int previous_treasury = city_finance_treasury();
    // The render fixture needs an affordable preview even in a bankrupt save.
    if (previous_treasury < 1000) city_finance_treasury_add(1000 - previous_treasury);
    config_set(CONFIG_UI_CV_CURSOR_SHADOW, 0);
    building_construction_set_type(road, 0);
    building_construction_start(end.x - 2, end.y, map_grid_add_delta(end.grid_offset, -2, 0));
    building_construction_update(end.x, end.y, end.grid_offset);
    bool passed = building_construction_in_progress() && building_construction_can_place() &&
        building_construction_cost() > 0 && map_terrain_is(end.grid_offset, TERRAIN_ROAD);
    if (!passed) std::fprintf(stderr, "Road drag setup failed: active=%d valid=%d cost=%d road=%d.\n", building_construction_in_progress(), building_construction_can_place(), building_construction_cost(), map_terrain_is(end.grid_offset, TERRAIN_ROAD));
    if (passed) {
        const float scale = city_view_get_scale() / 100.0f;
        const int x = static_cast<int>((endpoint.x + 30) / scale) - 4;
        const int y = static_cast<int>((endpoint.y + 15) / scale) - 2;
        color_t baseline[32] = {}, valid[32] = {}, invalid[32] = {};
        screen_set_pixel_render_scale();
        const auto capture = [&](const map_tile &tile, color_t *pixels) {
            city_without_overlay_draw(0, nullptr, &tile, 0);
            return graphics_renderer()->save_screen_buffer(pixels, x, y, 8, 4, 8) != 0;
        };
        passed = capture(no_tile, baseline) && capture(end, valid);
        // The route already draws its endpoint. A valid hover must not tint it red.
        passed = passed && std::memcmp(baseline, valid, sizeof(baseline)) == 0;
        building_construction_set_can_place(0);
        const bool invalid_captured = capture(end, invalid);
        passed = passed && invalid_captured && std::memcmp(baseline, invalid, sizeof(baseline)) != 0;
        if (!passed) std::fprintf(stderr, "Road drag pixels: baseline=%08x valid=%08x invalid=%08x valid_equal=%d invalid_equal=%d.\n", baseline[0], valid[0], invalid[0], std::memcmp(baseline, valid, sizeof(baseline)) == 0, std::memcmp(baseline, invalid, sizeof(baseline)) == 0);
        screen_set_ui_render_scale();
    }
    building_construction_cancel();
    building_construction_clear_type();
    city_finance_treasury_add(previous_treasury - city_finance_treasury());
    config_set(CONFIG_UI_CV_CURSOR_SHADOW, previous_shadow);
    passed = passed && !map_terrain_is(end.grid_offset, TERRAIN_ROAD);
    std::fprintf(passed ? stdout : stderr, "Road drag endpoint render test %s: tile=%d.\n", passed ? "passed" : "failed", end.grid_offset);
    return passed;
}
