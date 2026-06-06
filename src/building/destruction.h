#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "building/building_fwd.h"

void building_destroy_by_collapse(building *b);

void building_destroy_by_fire(building *b);

void building_destroy_by_earthquake(building *b);

void building_destroy_without_rubble(building *b);

void building_destroy_by_plague(building *b);

void building_destroy_by_rioter(building *b);

int building_destroy_first_of_type(building_type type);

void building_destroy_last_placed(void);

void building_destroy_increase_enemy_damage(int grid_offset, int max_damage);

void building_destroy_by_enemy(int x, int y, int grid_offset);

#ifdef __cplusplus
}
#endif
