#include "building/image.h"
#include "building/building_type_registry_internal.h"
#include "editor/tool.h"
#include "editor/tool_restriction.h"
#include "graphics/image.h"

#include "map_editor_tool.h"

#include "assets/assets.h"
#include "building/properties.h"
#include "building/building_type_api.h"
#include "city/view.h"
#include "core/image_group.h"
#include "core/image_group_editor.h"
#include "input/scroll.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/property.h"

#include <cstring>

#define MAX_TILES 16

static const int X_VIEW_OFFSETS[MAX_TILES] = { 0, -30, 30, 0, -60, 60, -30, 30, 0, -90, 90, -60, 60, -30, 30, 0 };
static const int Y_VIEW_OFFSETS[MAX_TILES] = { 0, 15, 15, 30, 30, 30, 45, 45, 60, 45, 45, 60, 60, 75, 75, 90 };

static float scale = SCALE_NONE;

static const building_type_registry_impl::BuildingType *building_type_definition_from_attr(const char *text_id)
{
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (definition && std::strcmp(definition->attr(), text_id) == 0) {
            return definition.get();
        }
    }
    return nullptr;
}

static building_type building_type_from_attr(const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition = building_type_definition_from_attr(text_id);
    return definition ? definition->type() : BUILDING_NONE;
}

static int building_type_attr_is(building_type type, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

static void offset_to_view_offset(int dx, int dy, int *view_dx, int *view_dy)
{
    // we're assuming map is always oriented north
    *view_dx = (dx - dy) * 30;
    *view_dy = (dx + dy) * 15;
}

static void draw_flat_tile(int x, int y, color_t color_mask)
{
    if (color_mask == COLOR_MASK_GREEN && scenario_property_climate() != CLIMATE_DESERT) {
        Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, ALPHA_MASK_SEMI_TRANSPARENT & color_mask, scale);
    } else if (color_mask != COLOR_MASK_GREEN && color_mask != COLOR_MASK_RED) {
        Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, color_mask, scale);
    } else {
        Image::blend_footprint_color(x, y, color_mask, scale);
    }
}

static void draw_partially_blocked(int x, int y, int num_tiles, int *blocked_tiles)
{
    for (int i = 0; i < num_tiles; i++) {
        int x_offset = x + X_VIEW_OFFSETS[i];
        int y_offset = y + Y_VIEW_OFFSETS[i];
        if (blocked_tiles[i]) {
            draw_flat_tile(x_offset, y_offset, COLOR_MASK_RED);
        } else {
            draw_flat_tile(x_offset, y_offset, COLOR_MASK_GREEN);
        }
    }
}

static void draw_building_image(int image_id, int x, int y)
{
    Image::from_id(image_id).draw_isometric_footprint(x, y, COLOR_MASK_GREEN, scale);
    Image::from_id(image_id).draw_isometric_top(x, y, COLOR_MASK_GREEN, scale);
}

static void draw_building(const map_tile *tile, int x_view, int y_view, building_type type)
{
    const building_properties *props = building_properties_for_type(type);

    int num_tiles = props->size * props->size;
    int blocked_tiles[MAX_TILES];
    int blocked = !editor_tool_can_place_building(tile, num_tiles, blocked_tiles);

    if (blocked) {
        draw_partially_blocked(x_view, y_view, num_tiles, blocked_tiles);
    } else if (editor_tool_is_in_use()) {
        int image_id = Image::group(GROUP_TERRAIN_OVERLAY);
        for (int i = 0; i < num_tiles; i++) {
            int x_offset = x_view + X_VIEW_OFFSETS[i];
            int y_offset = y_view + Y_VIEW_OFFSETS[i];
            Image::from_id(image_id).draw_isometric_footprint(x_offset, y_offset, 0, scale);
        }
    } else {
        int image_id;
        if (building_type_attr_is(type, "native_crops")) {
            image_id = Image::group(GROUP_EDITOR_BUILDING_CROPS);
        } else if (building_type_attr_is(type, "native_hut_alt")) {
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    image_id = assets_get_image_id("Terrain_Maps\\Native_Hut_Northern_01", "Native_Hut_Northern_01");
                    break;
                case CLIMATE_DESERT:
                    image_id = assets_get_image_id("Terrain_Maps\\Native_Hut_Southern_01", "Native_Hut_Southern_01");
                    break;
                default:
                    image_id = assets_get_image_id("Terrain_Maps\\Native_Hut_Central_01", "Native_Hut_Central_01");
            };
        } else if (building_type_attr_is(type, "native_decor") || building_type_attr_is(type, "native_monument") ||
            building_type_attr_is(type, "native_watchtower")) {
            image_id = building_image_get_for_type(type);
        } else if (props->image_group <= 0) {
            image_id = building_image_get_for_type(type);
        } else {
            image_id = Image::group(props->image_group) + props->image_offset;
        }
        draw_building_image(image_id, x_view, y_view);
    }
}

static void draw_road(const map_tile *tile, int x, int y)
{
    int grid_offset = tile->grid_offset;
    int blocked = 0;
    int image_id = 0;
    if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
        blocked = 1;
    } else {
        image_id = Image::group(GROUP_TERRAIN_ROAD);
        if (!map_terrain_has_adjacent_x_with_type(grid_offset, TERRAIN_ROAD) &&
            map_terrain_has_adjacent_y_with_type(grid_offset, TERRAIN_ROAD)) {
            image_id++;
        }
    }
    if (blocked) {
        draw_flat_tile(x, y, COLOR_MASK_RED);
    } else {
        draw_building_image(image_id, x, y);
    }
}

typedef struct {
    int x;
    int y;
    tool_type type;
    int brush_size;
} brush_draw_data;

static void draw_terrain_preview(int x, int y, tool_type type, int ring)
{
    int image_id;
    switch (type) {
        case TOOL_TREES:
            image_id = Image::group(GROUP_TERRAIN_TREE);
            if (ring >= 3) {
                image_id += 24;
            } else if (ring >= 2) {
                image_id += 16;
            } else if (ring >= 1) {
                image_id += 8;
            }
            break;
        case TOOL_ROCKS:
            image_id = Image::group(GROUP_TERRAIN_ROCK);
            break;
        case TOOL_SHRUB:
            image_id = Image::group(GROUP_TERRAIN_SHRUB);
            break;
        case TOOL_MEADOW:
            image_id = Image::group(GROUP_TERRAIN_MEADOW);
            if (ring >= 2) {
                image_id += 8;
            } else if (ring >= 1) {
                image_id += 4;
            }
            break;
        case TOOL_EARTHQUAKE_CUSTOM:
            image_id = Image::group(GROUP_TERRAIN_EARTHQUAKE);
            if (ring >= 1) {
                image_id += 29;
            } else {
                image_id += 24;
            }
            break;
        default:
            draw_flat_tile(x, y, COLOR_MASK_GREEN);
            return;
    }
    Image::blend_footprint_color(x, y, COLOR_MASK_GREEN, scale);
    Image::from_id(image_id).draw_isometric_footprint(x, y, COLOR_MASK_BUILDING_GHOST, scale);
    Image::from_id(image_id).draw_isometric_top(x, y, COLOR_MASK_BUILDING_GHOST, scale);
}

static void draw_brush_tile(const void *data, int dx, int dy)
{
    brush_draw_data *brush = (brush_draw_data *) data;
    int view_dx, view_dy;
    offset_to_view_offset(dx, dy, &view_dx, &view_dy);
    int ring = (brush->brush_size - 1) - ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
    draw_terrain_preview(brush->x + view_dx, brush->y + view_dy, brush->type, ring);
}

static void draw_brush(const map_tile *tile, int x, int y)
{
    brush_draw_data bd = { x, y, editor_tool_type(), editor_tool_brush_size() };
    editor_tool_foreach_brush_tile(draw_brush_tile, &bd);
}

static void draw_access_ramp(const map_tile *tile, int x, int y)
{
    int orientation;
    if (editor_tool_can_place_access_ramp(tile, &orientation)) {
        int image_id = Image::group(GROUP_TERRAIN_ACCESS_RAMP) + orientation;
        draw_building_image(image_id, x, y);
    } else {
        int blocked[4] = { 1, 1, 1, 1 };
        draw_partially_blocked(x, y, 4, blocked);
    }
}

static void draw_map_flag(int x, int y, int is_ok)
{
    draw_flat_tile(x, y, is_ok ? COLOR_MASK_GREEN : COLOR_MASK_RED);
}

static void draw_selection_rectangle(const map_tile *current_tile, const map_tile *start_tile, color_t color)
{
    // Get the grid slice for the rectangle selection
    grid_slice *slice = map_grid_get_grid_slice_from_corners(
        start_tile->x, start_tile->y, current_tile->x, current_tile->y);

    if (!slice) {
        return;
    }
    int x_pixels, y_pixels;
    city_view_get_selected_tile_pixels(&x_pixels, &y_pixels);

    // Draw simple highlight for each tile in the selection
    for (int i = 0; i < slice->size; i++) {
        int offset = slice->grid_offsets[i];
        // Calculate the isometric view position for this tile
        int xx = map_grid_offset_to_x(offset);
        int yy = map_grid_offset_to_y(offset);
        int dx = xx - current_tile->x;
        int dy = yy - current_tile->y;
        int view_dx = (dx - dy) * 30;
        int view_dy = (dx + dy) * 15;
        // Draw flat tile highlight at the calculated position
        draw_flat_tile(x_pixels + view_dx, y_pixels + view_dy, color);
    }
}

void map_editor_tool_draw(const map_tile *tile)
{
    if (!tile->grid_offset || scroll_in_progress() || !editor_tool_is_active()) {
        return;
    }

    tool_type type = editor_tool_type();
    scale = city_view_get_scale() / 100.0f;
    int x, y;
    city_view_get_selected_tile_pixels(&x, &y);
    switch (type) {
        case TOOL_NATIVE_CENTER:
            draw_building(tile, x, y, building_type_from_attr("native_meeting"));
            break;
        case TOOL_NATIVE_HUT:
            draw_building(tile, x, y, building_type_from_attr("native_hut"));
            break;
        case TOOL_NATIVE_HUT_ALT:
            draw_building(tile, x, y, building_type_from_attr("native_hut_alt"));
            break;
        case TOOL_NATIVE_FIELD:
            draw_building(tile, x, y, building_type_from_attr("native_crops"));
            break;
        case TOOL_NATIVE_DECORATION:
            draw_building(tile, x, y, building_type_from_attr("native_decor"));
            break;
        case TOOL_NATIVE_MONUMENT:
            draw_building(tile, x, y, building_type_from_attr("native_monument"));
            break;
        case TOOL_NATIVE_WATCHTOWER:
            draw_building(tile, x, y, building_type_from_attr("native_watchtower"));
            break;
        case TOOL_EARTHQUAKE_POINT:
        case TOOL_ENTRY_POINT:
        case TOOL_EXIT_POINT:
        case TOOL_RIVER_ENTRY_POINT:
        case TOOL_RIVER_EXIT_POINT:
        case TOOL_INVASION_POINT:
        case TOOL_FISHING_POINT:
        case TOOL_HERD_POINT:
            draw_map_flag(x, y, editor_tool_can_place_flag(type, tile, 0));
            break;

        case TOOL_ACCESS_RAMP:
            draw_access_ramp(tile, x, y);
            break;

        case TOOL_GRASS:
        case TOOL_MEADOW:
        case TOOL_ROCKS:
        case TOOL_SHRUB:
        case TOOL_TREES:
        case TOOL_WATER:
        case TOOL_NATIVE_RUINS:
        case TOOL_RAISE_LAND:
        case TOOL_LOWER_LAND:
        case TOOL_EARTHQUAKE_CUSTOM:
        case TOOL_EARTHQUAKE_CUSTOM_REMOVE:
            draw_brush(tile, x, y);
            break;

        case TOOL_ROAD:
            draw_road(tile, x, y);
            break;

        case TOOL_SELECT_LAND:
            if (editor_tool_is_in_use()) {
                const map_tile *start_tile = editor_tool_get_start_tile();
                if (start_tile && start_tile->grid_offset) {
                    draw_selection_rectangle(tile, start_tile, COLOR_MASK_AMBER);
                }
            } else {
                // Just highlight the current tile when not dragging
                draw_flat_tile(x, y, COLOR_MASK_AMBER);
            }
            break;
    }
}
