#pragma once

#include "core/direction.h"

class GameOrientationRequest {
public:
    static GameOrientationRequest turn_quarter_steps(int steps)
    {
        return {Mode::turn_quarter_steps, steps, DIR_0_TOP};
    }

    static GameOrientationRequest face(direction_type orientation)
    {
        return {Mode::face, 0, orientation};
    }

    bool has_target_orientation() const
    {
        return mode_ == Mode::face;
    }

    int relative_quarter_steps() const
    {
        return relative_quarter_steps_;
    }

    direction_type target_orientation() const
    {
        return target_orientation_;
    }

private:
    enum class Mode {
        turn_quarter_steps,
        face
    };

    GameOrientationRequest(Mode mode, int relative_quarter_steps, direction_type target_orientation)
        : mode_(mode)
        , relative_quarter_steps_(relative_quarter_steps)
        , target_orientation_(target_orientation)
    {}

    Mode mode_;
    int relative_quarter_steps_;
    direction_type target_orientation_;
};

void game_orientation_apply(GameOrientationRequest request);
