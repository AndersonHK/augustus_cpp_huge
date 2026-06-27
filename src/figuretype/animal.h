#pragma once

#include "figure/figure.h"

namespace figuretype {

class Animal : public Figure {
public:
    void draw(building_info_context *c);
};

} // namespace figuretype


void figure_create_fishing_points(void);

void figure_create_herds(void);

void figure_seagulls_action(Figure *f);

void figure_sheep_action(Figure *f);

void figure_wolf_action(Figure *f);

void figure_zebra_action(Figure *f);

void figure_hippodrome_horse_action(Figure *f);

void figure_hippodrome_horse_reroute(void);

void figure_animal_try_nudge_at(int building_center_tile_grid_offset, int animal_tile_offset, int building_size);
