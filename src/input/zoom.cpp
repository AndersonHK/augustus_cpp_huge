#

#include "input/zoom.h"

#include "graphics/menu.h"

#include "core/calc.h"
#include "core/config.h"
#include "core/speed.h"
#include "input/hotkey.h"

#include <cmath>

namespace {

constexpr int kZoomStep = 2;
constexpr int kZoomDelta = 25;

int scale_delta_from_display_delta(int delta)
{
    int scale_delta = city_view_display_percentage_to_scale(delta);
    if (!scale_delta && delta) {
        return delta < 0 ? -1 : 1;
    }
    return scale_delta;
}

} // namespace

static struct {
    int delta;
    int restore;
    pixel_offset input_offset;
    speed_type step;
    struct {
        int active;
        int start_zoom;
        int current_zoom;
    } touch;
} data;

static void start_touch(const touch *first, const touch *last, int scale)
{
    (void) last;
    data.restore = 0;
    data.touch.active = 1;
    data.input_offset.x = first->current_point.x;
    data.input_offset.y = first->current_point.y;
    data.touch.start_zoom = scale;
    data.touch.current_zoom = scale;
}

void zoom_update_touch(const touch *first, const touch *last, int scale)
{
    if (!data.touch.active) {
        start_touch(first, last, scale);
        return;
    }
    int original_distance, current_distance;
    pixel_offset temp;
    temp.x = first->start_point.x - last->start_point.x;
    temp.y = first->start_point.y - last->start_point.y;
    original_distance = (int) std::sqrt((double) (temp.x * temp.x + temp.y * temp.y));
    temp.x = first->current_point.x - last->current_point.x;
    temp.y = first->current_point.y - last->current_point.y;
    current_distance = (int) std::sqrt((double) (temp.x * temp.x + temp.y * temp.y));

    if (!original_distance || !current_distance) {
        data.touch.active = 0;
        return;
    }

    int finger_distance_percentage = calc_percentage(current_distance, original_distance);
    data.touch.current_zoom = calc_percentage(data.touch.start_zoom, finger_distance_percentage);
}

void zoom_end_touch(void)
{
    data.touch.active = 0;
}

void zoom_map(const mouse *m, const hotkeys *h, int current_zoom)
{
    if (data.touch.active || m->is_touch) {
        return;
    }
    if (h->reset_zoom) {
        data.restore = 1;
        speed_clear(&data.step);
        data.input_offset.x = m->x;
        data.input_offset.y = m->y - TOP_MENU_HEIGHT;
        return;
    }
    if (h->zoom_in || h->zoom_out) {
        data.restore = 0;
        int zoom_offset;
        int zoom_delta;
        if (h->zoom_out) {
            zoom_offset = 0;
            zoom_delta = kZoomDelta;
        } else {
            zoom_offset = -1;
            zoom_delta = -kZoomDelta;
        }
        int current_display_zoom = city_view_scale_to_display_percentage(current_zoom);
        int multiplier = (current_display_zoom + zoom_offset) / 100 + 1;
        data.delta = scale_delta_from_display_delta(zoom_delta * multiplier);
        if (config_get(static_cast<config_key>(CONFIG_UI_SMOOTH_SCROLLING))) {
            speed_clear(&data.step);
            speed_set_target(&data.step, kZoomStep, SPEED_CHANGE_IMMEDIATE, 1);
        }
        data.input_offset.x = m->x;
        data.input_offset.y = m->y - TOP_MENU_HEIGHT;
    }
}

int zoom_update_value(int *zoom, int max, pixel_offset *camera_position)
{
    int step;
    if (!data.touch.active) {
        if (data.restore) {
            data.delta = city_view_get_default_scale() - *zoom;
            if (config_get(static_cast<config_key>(CONFIG_UI_SMOOTH_SCROLLING))) {
                speed_set_target(&data.step, kZoomStep, SPEED_CHANGE_IMMEDIATE, 1);
            }
            data.restore = 0;
        }
        if (data.delta == 0) {
            return 0;
        }
        if (config_get(static_cast<config_key>(CONFIG_UI_SMOOTH_SCROLLING))) {
            int current_display_zoom = city_view_scale_to_display_percentage(*zoom);
            int display_step = speed_get_delta(&data.step);
            display_step *= (current_display_zoom / 100) + 1;
            step = scale_delta_from_display_delta(display_step);
            if (!step) {
                return 1;
            }
        } else {
            step = data.delta;
        }

        data.delta = calc_absolute_decrement(data.delta, &step);

        if (data.delta == 0) {
            speed_clear(&data.step);
        }
    } else {
        speed_clear(&data.step);
        data.restore = 0;
        int current_zoom = data.touch.current_zoom;
        int current_display_zoom = city_view_scale_to_display_percentage(current_zoom);
        if (current_display_zoom > 90 && current_display_zoom < 110) {
            current_zoom = city_view_get_default_scale();
        }
        step = current_zoom - *zoom;
    }

    int min = city_view_get_min_scale();
    if (max < min) {
        max = min;
    }
    int result = calc_bound(*zoom + step, min, max);
    if (*zoom == result) {
        speed_clear(&data.step);
        data.delta = 0;
        return 0;
    }
    pixel_offset old_offset, new_offset;
    old_offset.x = calc_adjust_with_percentage(data.input_offset.x, *zoom);
    old_offset.y = calc_adjust_with_percentage(data.input_offset.y, *zoom);

    new_offset.x = calc_adjust_with_percentage(data.input_offset.x, result);
    new_offset.y = calc_adjust_with_percentage(data.input_offset.y, result);

    camera_position->x -= new_offset.x - old_offset.x;
    camera_position->y -= new_offset.y - old_offset.y;

    if (!config_get(static_cast<config_key>(CONFIG_UI_SMOOTH_SCROLLING)) && !data.touch.active) {
        int remaining_x = camera_position->x & 60;
        int remaining_y = camera_position->y & 15;
        if (remaining_x >= 30) {
            remaining_x -= 60;
        }
        if (remaining_y >= 8) {
            remaining_y -= 15;
        }
        camera_position->x -= remaining_x;
        camera_position->y -= remaining_y;
    }
    *zoom = result;
    return 1;
}
