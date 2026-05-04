#pragma once

#include "building/building.h"

#ifdef __cplusplus
extern "C" {
#endif

int building_lighthouse_enough_timber(building *lighthouse);
int building_lighthouse_get_storage_destination(building *lighthouse);
int building_lighthouse_is_fully_functional(void);
void building_lighthouse_consume_timber(void);

#ifdef __cplusplus
}
#endif
