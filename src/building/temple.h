#pragma once

#include "building/building.h"

class Temple : public Building {
public:
    using Building::Building;
};

Building building_temple_get_storage_destination(Building temple);
int building_temple_mars_food_to_deliver(Building temple, Building mess_hall);
