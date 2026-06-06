#pragma once

#include "figure/formation.h"
#ifdef __cplusplus
extern "C" {
#endif


int formation_rioter_get_target_building(int *x_tile, int *y_tile);

int formation_rioter_get_target_building_for_robbery(int x, int y, int* x_tile, int* y_tile);

int formation_enemy_move_formation_to(const formation *m, int x, int y, int *x_tile, int *y_tile);

void formation_enemy_update(void);

#ifdef __cplusplus
}
#endif
