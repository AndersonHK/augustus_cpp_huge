#pragma once

#include "building/building.h"

int building_lighthouse_enough_timber(Building lighthouse);
Building building_lighthouse_first(void);
Building building_lighthouse_get_storage_destination(Building lighthouse);
int building_lighthouse_is_fully_functional(void);
void building_lighthouse_consume_timber(void);
