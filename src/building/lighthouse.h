#pragma once

#ifdef __cplusplus
extern "C++" {
class Building;

int building_lighthouse_enough_timber(Building lighthouse);
int building_lighthouse_get_storage_destination(Building lighthouse);
int building_lighthouse_is_fully_functional(void);
void building_lighthouse_consume_timber(void);
}
#endif
