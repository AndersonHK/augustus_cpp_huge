#pragma once

#include "figure/figure.h"

namespace figuretype {

class Animal : public Figure {
public:
    void action();
    void draw(building_info_context *c);
    void update_graphics();
};

} // namespace figuretype


void figure_create_fishing_points(void);

void figure_create_herds(void);

void figure_seagulls_action(Figure *f);

void figure_seagulls_update_graphics(Figure *f);

void figure_hippodrome_horse_action(Figure *f);

void figure_hippodrome_horse_update_graphics(Figure *f);

void figure_hippodrome_horse_reroute(void);

void figure_animal_try_nudge_at(int building_center_tile_grid_offset, int animal_tile_offset, int building_size);
