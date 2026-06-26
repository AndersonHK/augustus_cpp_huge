#pragma once

#include "figure/figure.h"
#include "map/point.h"


void figure_combat_handle_corpse(Figure *f);
void figure_combat_handle_attack(Figure *f);

int figure_combat_get_target_for_soldier(int x, int y, int max_distance);
int figure_combat_get_target_for_wolf(int x, int y, int max_distance);
int figure_combat_get_target_for_enemy(int x, int y);

int figure_combat_get_missile_target_for_soldier(Figure *shooter, int max_distance, map_point *tile);
int figure_combat_get_missile_target_for_enemy(Figure *enemy, int max_distance, int attack_citizens, map_point *tile);

void figure_combat_attack_figure_at(Figure *f, int grid_offset);
