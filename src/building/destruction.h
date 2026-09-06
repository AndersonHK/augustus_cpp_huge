#pragma once


#include "building/building_fwd.h"

int building_destroy_first_of_type(building_type type);

void building_destroy_last_placed(void);

void building_apply_enemy_damage(int grid_offset);
int building_hit_points_at(int grid_offset);
int building_damage_at(int grid_offset);

void building_destroy_by_enemy(int grid_offset);
