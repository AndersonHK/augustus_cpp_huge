#pragma once

#include "building/building.h"

class Caravanserai : public Building {
public:
    using Building::Building;
};

int building_caravanserai_enough_foods(Building caravanserai);
int building_caravanserai_food_required_monthly(void);
Building building_caravanserai_get_storage_destination(Building caravanserai);
int building_caravanserai_is_fully_functional(void);
