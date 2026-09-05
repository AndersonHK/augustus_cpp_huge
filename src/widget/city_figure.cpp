#include "city_figure.h"

#include "city/view.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/figure_runtime_native.h"
#include "graphics/orthographic_camera.h"

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

static void adjust_pixel_offset(const Figure *f, int *pixel_x, int *pixel_y, const FigureGraphicDrawRequest *draw_request)
{
    // determining x/y offset on tile
    int x_offset = 0;
    int y_offset = 0;
    const OrthographicCamera camera(city_view_orientation());
    if (f->use_cross_country) {
        const render_screen_point screen_offset = camera.to_legacy_screen(camera.figure_cross_country_offset(figure_movement_normalized_cross_country_offset(f->cross_country_x), figure_movement_normalized_cross_country_offset(f->cross_country_y)));
        x_offset = screen_offset.x;
        y_offset = screen_offset.y;
        y_offset -= f->missile_height;
    } else {
        const int direction = figure_image_normalize_direction(f->direction);
        const render_screen_point screen_offset = camera.to_legacy_screen(camera.figure_tile_progress_offset(direction, figure_movement_normalized_progress(f->progress_on_tile)));
        x_offset = screen_offset.x;
        y_offset = screen_offset.y;
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
    y_offset += FIGURE_LEGACY_TILE_PROGRESS_MAX;

    if (draw_request) {
        *pixel_x += x_offset - draw_request->scaled_sprite_offset_x();
        *pixel_y += y_offset - draw_request->scaled_sprite_offset_y();
    } else {
        *pixel_x += x_offset;
        *pixel_y += y_offset;
    }
}

static void draw_figure(int x, int y, float scale, int highlight, const FigureGraphicDrawRequest *draw_request)
{
    color_t color_mask = get_highlight_mask(highlight);
    if (draw_request) {
        draw_request->draw(x, y, color_mask, scale);
        return;
    }
}

void city_draw_figure(const Figure *f, int x, int y, float scale, int highlight)
{
    FigureGraphicDrawRequest draw_request;
    const FigureGraphicDrawRequest *request = f->graphic_draw_request(draw_request) ? &draw_request : nullptr;
    adjust_pixel_offset(f, &x, &y, request);
    draw_figure(x, y, scale, highlight, request);
}

void city_draw_selected_figure(const Figure *f, int x, int y, float scale, pixel_coordinate *coord)
{
    FigureGraphicDrawRequest draw_request;
    const FigureGraphicDrawRequest *request = f->graphic_draw_request(draw_request) ? &draw_request : nullptr;
    adjust_pixel_offset(f, &x, &y, request);
    draw_figure(x, y, scale, 0, request);
    coord->x = x;
    coord->y = y;
}
