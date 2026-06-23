#pragma once

#include "building/building_type.h"
#include "game/resource.h"


void building_construction_warning_reset(void);
void building_construction_warning_show_missing_resource(resource_type resource);
void building_construction_warning_check_food_stocks(building_type type);
void building_construction_warning_check_reservoir(building_type type);
void building_construction_warning_check_all(building_type type, int x, int y, int size);

