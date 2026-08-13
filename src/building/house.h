#pragma once

#include "building/building.h"

void building_house_change_to(Building house, building_type type);
void building_house_change_to_vacant_lot(Building house);

unsigned int building_house_merge(Building house);

int building_house_can_expand(Building house, building_type target_type);

int building_house_expand_to_type(Building house, building_type type);

void building_house_devolve_to_type(Building house, building_type type);

