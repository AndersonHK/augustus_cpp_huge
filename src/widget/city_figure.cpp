#include "city_figure.h"

#include "city/view.h"
#include "figure/figure_runtime_api.h"
#include "figure/image.h"
#include "figuretype/editor.h"
#include "graphics/runtime_texture.h"
#include "graphics/text.h"

static color_t get_highlight_mask(int highlight_mask)
{
    switch (highlight_mask) {
        case FIGURE_HIGHLIGHT_NONE:
            return COLOR_MASK_NONE;
        case FIGURE_HIGHLIGHT_RED:
            return COLOR_MASK_LEGION_HIGHLIGHT;
        case FIGURE_HIGHLIGHT_GREEN:
            return COLOR_MASK_GREEN;
        default:
            return COLOR_MASK_NONE;
    }
}

static void draw_map_flag(const Figure *f, int x, int y, float scale)
{
    // base
    Image::from_id(f->image_id).draw(x, y, COLOR_MASK_NONE, scale);
    // flag
    const Image &flag_image = Image::from_id(f->cart_image_id);
    flag_image.draw(x, y - flag_image.height(), COLOR_MASK_NONE, scale);
    // flag number
    int number = 0;
    int id = f->resource_id;
    if (id >= MAP_FLAG_INVASION_MIN && id < MAP_FLAG_INVASION_MAX) {
        number = id - MAP_FLAG_INVASION_MIN + 1;
    } else if (id >= MAP_FLAG_FISHING_MIN && id < MAP_FLAG_FISHING_MAX) {
        number = id - MAP_FLAG_FISHING_MIN + 1;
    } else if (id >= MAP_FLAG_HERD_MIN && id < MAP_FLAG_HERD_MAX) {
        number = id - MAP_FLAG_HERD_MIN + 1;
    }
    if (number > 0) {
        int pixel_size = (int) (font_definition_for(FONT_NORMAL_PLAIN)->line_height * scale);
        text_draw_number(number, '@', 0, x + 6, y + 7, FONT_NORMAL_PLAIN, pixel_size, COLOR_WHITE);
    }
}

static void tile_cross_country_offset_to_pixel_offset(int cross_country_x, int cross_country_y,
    int *pixel_x, int *pixel_y)
{
    int dir = city_view_orientation();
    if (dir == DIR_0_TOP || dir == DIR_4_BOTTOM) {
        int base_pixel_x = 2 * cross_country_x - 2 * cross_country_y;
        int base_pixel_y = cross_country_x + cross_country_y;
        *pixel_x = dir == DIR_0_TOP ? base_pixel_x : -base_pixel_x;
        *pixel_y = dir == DIR_0_TOP ? base_pixel_y : -base_pixel_y;
    } else {
        int base_pixel_x = 2 * cross_country_x + 2 * cross_country_y;
        int base_pixel_y = cross_country_x - cross_country_y;
        *pixel_x = dir == DIR_2_RIGHT ? base_pixel_x : -base_pixel_x;
        *pixel_y = dir == DIR_6_LEFT ? base_pixel_y : -base_pixel_y;
    }
}

static int tile_progress_to_pixel_offset_x(int direction, int progress)
{
    if (progress >= 15) {
        return 0;
    }
    switch (direction) {
        case DIR_0_TOP:
        case DIR_2_RIGHT:
            return 2 * progress - 28;
        case DIR_1_TOP_RIGHT:
            return 4 * progress - 56;
        case DIR_4_BOTTOM:
        case DIR_6_LEFT:
            return 28 - 2 * progress;
        case DIR_5_BOTTOM_LEFT:
            return 56 - 4 * progress;
        default:
            return 0;
    }
}

static int tile_progress_to_pixel_offset_y(int direction, int progress)
{
    if (progress >= 15) {
        return 0;
    }
    switch (direction) {
        case DIR_0_TOP:
        case DIR_6_LEFT:
            return 14 - progress;
        case DIR_2_RIGHT:
        case DIR_4_BOTTOM:
            return progress - 14;
        case DIR_3_BOTTOM_RIGHT:
            return 2 * progress - 28;
        case DIR_7_TOP_LEFT:
            return 28 - 2 * progress;
        default:
            return 0;
    }
}

static void tile_progress_to_pixel_offset(int direction, int progress, int *pixel_x, int *pixel_y)
{
    *pixel_x = tile_progress_to_pixel_offset_x(direction, progress);
    *pixel_y = tile_progress_to_pixel_offset_y(direction, progress);
}

static void adjust_pixel_offset(const Figure *f, int *pixel_x, int *pixel_y)
{
    // determining x/y offset on tile
    int x_offset = 0;
    int y_offset = 0;
    if (f->use_cross_country) {
        tile_cross_country_offset_to_pixel_offset(
            f->cross_country_x % 15, f->cross_country_y % 15, &x_offset, &y_offset);
        y_offset -= f->missile_height;
    } else {
        int direction = figure_image_normalize_direction(f->direction);
        tile_progress_to_pixel_offset(direction, f->progress_on_tile, &x_offset, &y_offset);
        y_offset -= f->current_height;
        if (f->figures_on_same_tile_index && f->type != FIGURE_BALLISTA) {
            // an attempt to not let people walk through each other
            static const int BUSY_ROAD_X_OFFSETS[] = {
                0, 8, 8, -8, -8, 0, 16, 0, -16, 8, -8, 16, -16, 16, -16, 8, -8, 0, 24, 0, -24
            };
            static const int BUSY_ROAD_Y_OFFSETS[] = {
                0, 0, 8, 8, -8, -16, 0, 16, 0, -16, 16, 8, -8, -8, 8, 16, -16, -24, 0, 24, 0
            };
            static const int BUSY_ROAD_OFFSET_LEN = 21;
            x_offset += BUSY_ROAD_X_OFFSETS[f->figures_on_same_tile_index % BUSY_ROAD_OFFSET_LEN];
            y_offset += BUSY_ROAD_Y_OFFSETS[f->figures_on_same_tile_index % BUSY_ROAD_OFFSET_LEN];
        }
    }

    x_offset += 29;
    y_offset += 15;

    FigureGraphicDrawRequest draw_request;
    const int has_native_graphics = figure_runtime_graphic_draw_request(f, &draw_request);
    if (!has_native_graphics && f->image_id >= 10000) {
        // TODO
        // Ugly hack, remove
        // Draws new walkers at their proper spots
        x_offset -= 26;
        y_offset -= 29;
    }


    if (has_native_graphics) {
        *pixel_x += x_offset - draw_request.sprite_offset_x;
        *pixel_y += y_offset - draw_request.sprite_offset_y;
    } else {
        const Image &img = f->is_enemy_image ? Image::enemy(f->image_id) : Image::from_id(f->image_id);
        const image_animation *animation = img.animation();
        *pixel_x += x_offset - (animation ? animation->sprite_offset_x : 0);
        *pixel_y += y_offset - (animation ? animation->sprite_offset_y : 0);
    }
}

static void draw_figure(const Figure *f, int x, int y, float scale, int highlight)
{
    color_t color_mask = get_highlight_mask(highlight);
    FigureGraphicDrawRequest draw_request;
    if (figure_runtime_graphic_draw_request(f, &draw_request)) {
        for (int i = 0; i < draw_request.layer_count; i++) {
            const FigureGraphicDrawLayer &layer = draw_request.layers[i];
            if (!layer.draw_before_base) {
                continue;
            }
            runtime_texture_draw(
                layer.slice,
                x + layer.x_offset,
                y + layer.y_offset,
                layer.draw_color(color_mask),
                scale);
        }
        if (draw_request.has_base_slice()) {
            runtime_texture_draw(draw_request.base_slice, x, y, color_mask, scale);
        }
        for (int i = 0; i < draw_request.layer_count; i++) {
            const FigureGraphicDrawLayer &layer = draw_request.layers[i];
            if (layer.draw_before_base) {
                continue;
            }
            runtime_texture_draw(
                layer.slice,
                x + layer.x_offset,
                y + layer.y_offset,
                layer.draw_color(color_mask),
                scale);
        }
        return;
    }
    if (f->cart_image_id && f->type == FIGURE_MAP_FLAG) {
        draw_map_flag(f, x, y, scale);
    } else if (f->cart_image_id) {
        Image::from_id(f->image_id).draw(x, y, color_mask, scale);
    } else {
        if (f->is_enemy_image) {
            Image::enemy(f->image_id).draw(x, y, COLOR_MASK_NONE, scale);
        } else {

            Image::from_id(f->image_id).draw(x, y, color_mask, scale);
        }
    }
}

void city_draw_figure(const Figure *f, int x, int y, float scale, int highlight)
{
    adjust_pixel_offset(f, &x, &y);
    draw_figure(f, x, y, scale, highlight);
}

void city_draw_selected_figure(const Figure *f, int x, int y, float scale, pixel_coordinate *coord)
{
    adjust_pixel_offset(f, &x, &y);
    draw_figure(f, x, y, scale, 0);
    coord->x = x;
    coord->y = y;
}
