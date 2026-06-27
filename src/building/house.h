#pragma once

#include "building/building.h"
#include "building/properties.h"


int building_house_is_active(Building house);
int building_house_legacy_level(Building house);
const model_house *building_house_get_model(Building house);
int building_house_has_plebeian_residents(Building house);
int building_house_has_patrician_residents(Building house);

void building_house_change_to(Building house, building_type type);
void building_house_change_to_vacant_lot(Building house);

unsigned int building_house_merge(Building house);

int building_house_can_expand(Building house, int num_tiles);

int building_house_expand_to_type(Building house, building_type type);

void building_house_devolve_to_type(Building house, building_type type);

void building_house_check_for_corruption(Building house);

void building_house_restore_population_after_undo(Building house);

