#include "city_figure.h"

#include "city/view.h"
#include "figure/figure_graphics.h"
#include "figure/image.h"
#include "figure/movement.h"
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

static void draw_runtime_figure_slice(
    const RuntimeDrawSlice &slice,
    int x,
    int y,
    color_t color,
    float scale,
    render_logical_size fixed_logical_size,
    render_scaling_policy scaling_policy)
{
    if (!slice.is_valid()) {
        return;
    }

    if (fixed_logical_size.width > 0 && fixed_logical_size.height > 0) {
        RuntimeTextureDrawRequest request = {};
        request.slice = slice;
        request.x = scale ? static_cast<float>(x) / scale : static_cast<float>(x);
        request.y = scale ? static_cast<float>(y) / scale : static_cast<float>(y);
        request.fixed_logical_size = fixed_logical_size;
        request.color = color;
        request.domain = graphics_renderer()->get_render_domain();
        request.scaling_policy = scaling_policy;
        runtime_texture_draw_request(request);
        return;
    }
    runtime_texture_draw(slice, x, y, color, scale);
}

static void draw_figure_layers(
    const FigureGraphicDrawRequest &draw_request,
    int x,
    int y,
    color_t color_mask,
    float scale,
    int draw_before_base)
{
    for (int i = 0; i < draw_request.layer_count; i++) {
        const FigureGraphicDrawLayer &layer = draw_request.layers[i];
        if ((layer.draw_before_base != 0) != (draw_before_base != 0)) {
            continue;
        }
        draw_runtime_figure_slice(
            layer.slice,
            x + layer.x_offset,
            y + layer.y_offset,
            layer.use_figure_color_mask ? color_mask : COLOR_MASK_NONE,
            scale,
            layer.fixed_logical_size,
            layer.scaling_policy);
    }
}

static void draw_map_flag_number(const Figure *f, int x, int y, float scale)
{
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

static void tile_progress_to_pixel_offset(int direction, int progress, int *pixel_x, int *pixel_y)
{
    *pixel_x = 0;
    *pixel_y = 0;
    if (figure_movement_tile_progress_complete(progress)) {
        return;
    }

    switch (direction) {
        case DIR_0_TOP:
        case DIR_2_RIGHT:
            *pixel_x = 2 * progress - 28;
            break;
        case DIR_1_TOP_RIGHT:
            *pixel_x = 4 * progress - 56;
            break;
        case DIR_4_BOTTOM:
        case DIR_6_LEFT:
            *pixel_x = 28 - 2 * progress;
            break;
        case DIR_5_BOTTOM_LEFT:
            *pixel_x = 56 - 4 * progress;
            break;
        default:
            break;
    }

    switch (direction) {
        case DIR_0_TOP:
        case DIR_6_LEFT:
            *pixel_y = 14 - progress;
            break;
        case DIR_2_RIGHT:
        case DIR_4_BOTTOM:
            *pixel_y = progress - 14;
            break;
        case DIR_3_BOTTOM_RIGHT:
            *pixel_y = 2 * progress - 28;
            break;
        case DIR_7_TOP_LEFT:
            *pixel_y = 28 - 2 * progress;
            break;
        default:
            break;
    }
}

static void adjust_pixel_offset(
    const Figure *f,
    int *pixel_x,
    int *pixel_y,
    const FigureGraphicDrawRequest *draw_request)
{
    // determining x/y offset on tile
    int x_offset = 0;
    int y_offset = 0;
    if (f->use_cross_country) {
        tile_cross_country_offset_to_pixel_offset(
            figure_movement_cross_country_tile_offset(f->cross_country_x),
            figure_movement_cross_country_tile_offset(f->cross_country_y),
            &x_offset,
            &y_offset);
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
    y_offset += FIGURE_TILE_PROGRESS_MAX;

    if (!draw_request && f->image_id >= 10000) {
        // TODO
        // Ugly hack, remove
        // Draws new walkers at their proper spots
        x_offset -= 26;
        y_offset -= 29;
    }

    if (draw_request) {
        *pixel_x += x_offset - draw_request->sprite_offset_x;
        *pixel_y += y_offset - draw_request->sprite_offset_y;
    } else {
        const Image &img = Image::from_id(f->image_id);
        const image_animation *animation = img.animation();
        *pixel_x += x_offset - (animation ? animation->sprite_offset_x : 0);
        *pixel_y += y_offset - (animation ? animation->sprite_offset_y : 0);
    }
}

static void draw_figure(
    const Figure *f,
    int x,
    int y,
    float scale,
    int highlight,
    const FigureGraphicDrawRequest *draw_request)
{
    color_t color_mask = get_highlight_mask(highlight);
    if (draw_request) {
        draw_figure_layers(*draw_request, x, y, color_mask, scale, 1);
        if (draw_request->base_slice.is_valid()) {
            draw_runtime_figure_slice(
                draw_request->base_slice,
                x,
                y,
                color_mask,
                scale,
                draw_request->fixed_logical_size,
                draw_request->scaling_policy);
        }
        draw_figure_layers(*draw_request, x, y, color_mask, scale, 0);
        if (f->type == FIGURE_MAP_FLAG) {
            draw_map_flag_number(f, x, y, scale);
        }
        return;
    }
    Image::from_id(f->image_id).draw(x, y, color_mask, scale);
}

void city_draw_figure(const Figure *f, int x, int y, float scale, int highlight)
{
    FigureGraphicDrawRequest draw_request;
    const FigureGraphicDrawRequest *request = FigureGraphics(*f).resolve(draw_request) ? &draw_request : nullptr;
    adjust_pixel_offset(f, &x, &y, request);
    draw_figure(f, x, y, scale, highlight, request);
}

void city_draw_selected_figure(const Figure *f, int x, int y, float scale, pixel_coordinate *coord)
{
    FigureGraphicDrawRequest draw_request;
    const FigureGraphicDrawRequest *request = FigureGraphics(*f).resolve(draw_request) ? &draw_request : nullptr;
    adjust_pixel_offset(f, &x, &y, request);
    draw_figure(f, x, y, scale, 0, request);
    coord->x = x;
    coord->y = y;
}
