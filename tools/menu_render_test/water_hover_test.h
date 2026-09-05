#pragma once

#include "core/image_group.h"
#include "graphics/image.h"
#include "graphics/graphics.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "city/view.h"
#include "city/view_render.h"
#include "core/config.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "widget/city_without_overlay.h"
#include "window/city.h"
#include "building/construction.h"
#include "building/building_type_registry_internal.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

inline bool run_city_water_hover_render_test()
{
    window_city_show();
    window_draw(1);
    static CityDrawTileCommand selected;
    selected = {};
    CityViewRenderCommandBuffer commands;
    commands.build();
    const CityViewRenderPhase phase[] = { { [](const CityDrawTileCommand &command) {
        if (selected.grid_offset >= 0 || command.building || command.first_figure || command.grid_offset < 0 ||
            !map_property_is_draw_tile(command.grid_offset) || !map_terrain_is(command.grid_offset, TERRAIN_WATER)) return;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                const int neighbor = map_grid_add_delta(command.grid_offset, dx, dy);
                if (neighbor < 0 || !map_terrain_is(neighbor, TERRAIN_WATER)) return;
            }
        }
        int x, y, width, height;
        city_view_get_viewport(&x, &y, &width, &height);
        const float scale = city_view_get_scale() / 100.0f;
        const int pixel_x = static_cast<int>((command.x + 30) / scale);
        const int pixel_y = static_cast<int>((command.y + 15) / scale);
        if (pixel_x > x + 80 && pixel_x < x + width - 80 && pixel_y > y + 80 && pixel_y < y + height - 80) selected = command;
    }, PERFORMANCE_TRACKER_BUCKET_MAX } };
    commands.execute(phase, 1);
    if (selected.grid_offset < 0) {
        std::fprintf(stdout, "City water hover test skipped: no unobstructed visible water.\n");
        return true;
    }
    const map_tile hovered_tile = { map_grid_offset_to_x(selected.grid_offset), map_grid_offset_to_y(selected.grid_offset), selected.grid_offset };
    view_tile selected_view;
    city_view_grid_offset_to_xy_view(hovered_tile.grid_offset, &selected_view.x, &selected_view.y);
    city_view_set_selected_view_tile(&selected_view);
    const map_tile no_tile = { 0, 0, 0 };
    const int previous_shadow = config_get(CONFIG_UI_CV_CURSOR_SHADOW);
    config_set(CONFIG_UI_CV_CURSOR_SHADOW, 1);
    const float scale = city_view_get_scale() / 100.0f;
    const int x = static_cast<int>((selected.x + 30) / scale) - 4;
    const int y = static_cast<int>((selected.y + 15) / scale) - 2;
    color_t normal[32] = {}, hovered[32] = {};
    screen_set_pixel_render_scale();
    city_without_overlay_draw(0, nullptr, &no_tile, 0);
    const bool normal_saved = graphics_renderer()->save_screen_buffer(normal, x, y, 8, 4, 8) != 0;
    bool passed = normal_saved;
    for (const char *tool : { "none", "clear_land", "road", "clear_trees", "repair_land" }) {
        if (std::strcmp(tool, "none") != 0) {
            const auto *definition = building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr(tool));
            if (!definition) { passed = false; break; }
            building_construction_set_type(definition, 0);
        }
        city_without_overlay_draw(0, nullptr, &hovered_tile, 0);
        passed = graphics_renderer()->save_screen_buffer(hovered, x, y, 8, 4, 8) != 0 && passed;
        building_construction_clear_type();
        int checked = 0;
        for (int i = 0; i < 32; i++) {
            const auto brightness = [](color_t pixel) { return int((pixel >> 16) & 255) + int((pixel >> 8) & 255) + int(pixel & 255); };
            if (brightness(normal[i]) <= 60) continue;
            checked++;
            if (brightness(hovered[i]) < brightness(normal[i]) / 2) {
                std::fprintf(stderr, "City water preview blackened tool=%s tile=%d scale=%g pixel=%d normal=%08x hover=%08x.\n", tool, selected.grid_offset, scale, i, normal[i], hovered[i]);
                passed = false;
                break;
            }
        }
        if (!checked) passed = false;
        if (!passed) break;
    }
    config_set(CONFIG_UI_CV_CURSOR_SHADOW, previous_shadow);
    screen_set_ui_render_scale();
    if (!passed) return false;
    std::fprintf(stdout, "City water preview render test passed for clear, road, trees and repair: tile=%d scale=%g.\n", selected.grid_offset, scale);
    return true;
}

inline bool run_water_hover_render_test(bool city_loaded = false)
{
    graphics_reset_dialog();
    graphics_reset_clip_rectangle();
    screen_set_pixel_render_scale();
    std::vector<color_t> normal(60 * 32), hovered(normal.size());
    for (int frame = 0; frame < 6; frame++) {
        const Image &water = Image::from_id(Image::group(GROUP_TERRAIN_WATER) + frame);
        const auto capture = [&](color_t color, std::vector<color_t> &pixels, color_t preview = 0) {
            graphics_clear_screen();
            water.draw_isometric_footprint_from_draw_tile(20, 20, color, 1.0f, RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE);
            if (preview) Image::draw_footprint_overlay(20, 20, preview, 1.0f);
            return graphics_renderer()->save_screen_buffer(pixels.data(), 20, 20, 60, 32, 60) != 0;
        };
        if (!capture(COLOR_MASK_NONE, normal)) return false;
        for (color_t preview : { color_t(COLOR_OVERLAY_RED), color_t(COLOR_OVERLAY_GREEN), color_t(COLOR_MASK_YELLOW_RANGE), color_t(COLOR_MASK_GRAY), color_t(COLOR_OVERLAY_HOVER) }) {
            if (!capture(COLOR_MASK_NONE, hovered, preview)) return false;
            const int center = 15 * 60 + 29;
            const int alpha = (preview >> 24) & 255;
            for (int shift : { 0, 8, 16 }) {
                const int expected = (((preview >> shift) & 255) * alpha + ((normal[center] >> shift) & 255) * (255 - alpha)) / 255;
                const int actual = (hovered[center] >> shift) & 255;
                if (std::abs(actual - expected) > 2) {
                    std::fprintf(stderr, "Footprint overlay did not use source-over blending: color=%08x frame=%d channel=%d expected=%d actual=%d.\n", preview, frame, shift, expected, actual);
                    return false;
                }
            }
            int checked = 0;
            for (size_t index = 0; index < normal.size(); index++) {
                const auto brightness = [](color_t pixel) { return int((pixel >> 16) & 255) + int((pixel >> 8) & 255) + int(pixel & 255); };
                const int original = brightness(normal[index]);
                if (original < 60) continue;
                checked++;
                if (brightness(hovered[index]) < original / 2) {
                    std::fprintf(stderr, "Water preview blackened color=%08x frame=%d pixel=%zu original=%08x hover=%08x.\n", preview, frame, index, normal[index], hovered[index]);
                    return false;
                }
            }
            if (!checked) {
                std::fprintf(stderr, "Water hover test rendered no water pixels for frame %d.\n", frame);
                return false;
            }
        }
    }
    screen_set_ui_render_scale();
    std::fprintf(stdout, "Water hover render test passed for all six frames.\n");
    return !city_loaded || run_city_water_hover_render_test();
}
