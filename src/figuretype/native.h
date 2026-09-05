#pragma once

#include "figure/figure.h"


void figure_indigenous_native_action(Figure *f);

class NativeBuildingSpawner {
public:
    explicit NativeBuildingSpawner(Building &owner) : owner_(owner) {}

    void spawn_hut_resident();
    void spawn_meeting_trader();
    void spawn_missionary();
    void advance_crop_graphics();

private:
    building *record() const;
    bool has_primary_figure(figure_type type) const;
    Figure *create_primary_figure(figure_type type, int x, int y, int action_state, bool roaming);

    Building &owner_;
};
