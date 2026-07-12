#pragma once


#include "building/building_fwd.h"

void building_destroy_by_collapse(building *b);

void building_destroy_by_fire(building *b);

void building_destroy_by_earthquake(building *b);

void building_destroy_without_rubble(building *b);

void building_destroy_by_plague(building *b);

void building_destroy_by_rioter(building *b);

int building_destroy_first_of_type(building_type type);

void building_destroy_last_placed(void);

void building_apply_enemy_damage(int grid_offset);

void building_destroy_by_enemy(int grid_offset);
