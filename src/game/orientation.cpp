#include "city/warning.h"
#include "map/orientation.h"
#include "translation/translation.h"
#include "widget/minimap.h"

#include "orientation.h"

#include "city/view.h"

namespace {

constexpr int kQuarterOrientations = 4;
constexpr int kDirectionStep = 2;
constexpr int kLeftQuarterTurn = 1;
constexpr int kRightQuarterTurn = -1;

int normalize_quarter_turns(int steps)
{
    int normalized = steps % kQuarterOrientations;
    if (normalized > 2) {
        normalized -= kQuarterOrientations;
    } else if (normalized < -2) {
        normalized += kQuarterOrientations;
    }
    return normalized == -2 ? 2 : normalized;
}

int orientation_index(direction_type orientation)
{
    const int value = static_cast<int>(orientation);
    if (value < static_cast<int>(DIR_0_TOP) || value > static_cast<int>(DIR_6_LEFT)) {
        return 0;
    }
    return (value / kDirectionStep) % kQuarterOrientations;
}

direction_type normalized_orientation(direction_type orientation)
{
    return static_cast<direction_type>(orientation_index(orientation) * kDirectionStep);
}

int quarter_turns_to(direction_type target)
{
    const int current_index = orientation_index(static_cast<direction_type>(city_view_orientation()));
    const int target_index = orientation_index(target);
    int left_turns = (target_index - current_index + kQuarterOrientations) % kQuarterOrientations;
    if (left_turns > 2) {
        return left_turns - kQuarterOrientations;
    }
    return left_turns;
}

static translation_key orientation_warning_key(void)
{
    switch (normalized_orientation(static_cast<direction_type>(city_view_orientation()))) {
        case DIR_0_TOP:
            return "TR_CITY_WARNING_ORIENTATION_NORTH";
        case DIR_2_RIGHT:
            return "TR_CITY_WARNING_ORIENTATION_EAST";
        case DIR_4_BOTTOM:
            return "TR_CITY_WARNING_ORIENTATION_SOUTH";
        case DIR_6_LEFT:
            return "TR_CITY_WARNING_ORIENTATION_WEST";
        default:
            return "TR_CITY_WARNING_ORIENTATION_NORTH";
    }
}

static void show_orientation_warning(void)
{
    city_warning_show(WARNING_ORIENTATION, translation_for(orientation_warning_key()));
}

void rotate_one_quarter_step(int step)
{
    if (step > 0) {
        city_view_rotate_left();
        map_orientation_change(kLeftQuarterTurn);
    } else if (step < 0) {
        city_view_rotate_right();
        map_orientation_change(kRightQuarterTurn);
    }
}

int planned_quarter_turns(GameOrientationRequest request)
{
    if (request.has_target_orientation()) {
        return quarter_turns_to(request.target_orientation());
    }
    return normalize_quarter_turns(request.relative_quarter_steps());
}

}

void game_orientation_apply(GameOrientationRequest request)
{
    int turns = planned_quarter_turns(request);
    if (!turns) {
        return;
    }

    const int step = turns > 0 ? kLeftQuarterTurn : kRightQuarterTurn;
    while (turns) {
        rotate_one_quarter_step(step);
        turns -= step;
    }
    widget_minimap_invalidate();
    show_orientation_warning();
}
