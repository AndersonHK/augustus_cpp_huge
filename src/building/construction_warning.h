#pragma once

#include "building/building_type.h"
#include "game/resource.h"

#include <string>

void building_construction_warning_reset(void);
std::string building_construction_warning_type_name(building_type type);
std::string building_construction_warning_resource_name(resource_type resource);
void building_construction_warning_show_missing_resource(resource_type resource);
void building_construction_warning_check_food_stocks(building_type type);
void building_construction_warning_check_reservoir(building_type type);
void building_construction_warning_check_all(building_type type, int x, int y, int size);

