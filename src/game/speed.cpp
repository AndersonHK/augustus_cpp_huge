#include "building/construction.h"
#include "game/state.h"

#include "game/performance_tracker.h"
#include "game/speed.h"

#include "game/settings.h"
#include "core/time.h"
#include "graphics/window.h"
#include "input/scroll.h"

#define MAX_TICKS_PER_FRAME 20
#define MILLIS_PER_TICK_SCALE 1000

static const time_millis MILLIS_PER_TICK_PER_SPEED[] = {
    702, 502, 352, 242, 162, 112, 82, 57, 37, 22, 16
};
static const time_millis MILLIS_PER_HYPER_SPEED_X1000[] = {
    702000, 16000, 8000, 5000, 3000, 2000, 2000, 1333, 1250, 1111, 1000
};
const int game_speeds[] = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 500, 750, 1000 };
//if updaing array, update TOTAL_GAME_SPEEDS, 0-based
static struct {
    int last_check_was_valid;
    time_millis last_update;
    time_millis leftover_millis_x1000;
} data;

int game_speed_get_index(int speed)
{
    int index = 0;
    while (index < static_cast<int>(sizeof(game_speeds) / sizeof(int))) {
        if (game_speeds[index] >= speed) {
            break;
        }
        index++;
    }
    if (index >= static_cast<int>(sizeof(game_speeds) / sizeof(int))) {
        index = static_cast<int>(sizeof(game_speeds) / sizeof(int)) - 1;
    }
    return index;
}

int game_speed_get_speed(int index)
{
    if (index < 0) {
        index = 0;
    }
    return game_speeds[index];
}

int game_speed_get_elapsed_ticks(void)
{
    int last_check_was_valid = data.last_check_was_valid;
    data.last_check_was_valid = 0;
    if (game_state_is_paused()) {
        return 0;
    }

    time_millis millis_per_tick_x1000 = MILLIS_PER_TICK_SCALE;
    switch (window_get_id()) {
        default:
            return 0;
        case WINDOW_CITY:
        case WINDOW_CITY_MILITARY:
        case WINDOW_SLIDING_SIDEBAR:
        case WINDOW_OVERLAY_MENU:
        case WINDOW_MILITARY_MENU:
        case WINDOW_BUILD_MENU:
        {
            int speed = setting_game_speed();
            if (speed < 10) {
                return 0;
            } else if (speed <= 100) {
                millis_per_tick_x1000 = MILLIS_PER_TICK_PER_SPEED[speed / 10] * MILLIS_PER_TICK_SCALE;
            } else {
                if (speed > 1000) {
                    speed = 1000;
                }
                millis_per_tick_x1000 = MILLIS_PER_HYPER_SPEED_X1000[speed / 100];
            }
            break;
        }
        case WINDOW_EDITOR_MAP:
            millis_per_tick_x1000 = MILLIS_PER_TICK_PER_SPEED[7] * MILLIS_PER_TICK_SCALE; // 70%, nice speed for flag animations
            break;
    }

    if (building_construction_in_progress()) {
        return 0;
    }
    if (scroll_in_progress() && !scroll_is_smooth()) {
        return 0;
    }

    const time_millis now = time_get_millis();
    const time_millis diff = now - data.last_update;
    data.last_check_was_valid = 1;
    if (!last_check_was_valid) {
        // returning to map from another window or pause: always force a tick
        data.last_update = now;
        data.leftover_millis_x1000 = 0;
        return 1;
    }

    const time_millis scaled_diff = diff * MILLIS_PER_TICK_SCALE + data.leftover_millis_x1000;
    performance_tracker_record_speed_goal(diff, millis_per_tick_x1000);
    const int ticks = static_cast<int>(scaled_diff / millis_per_tick_x1000);
    if (!ticks) {
        performance_tracker_record_speed_wait(diff);
        data.leftover_millis_x1000 = scaled_diff;
        data.last_update = now;
        return 0;
    } else if (ticks <= MAX_TICKS_PER_FRAME) {
        data.leftover_millis_x1000 = scaled_diff % millis_per_tick_x1000;
        data.last_update = now;
        return ticks;
    } else {
        data.leftover_millis_x1000 = 0;
        data.last_update = now;
        return MAX_TICKS_PER_FRAME;
    }
}
