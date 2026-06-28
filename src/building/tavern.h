#pragma once

#include "building/building.h"
#include "building/distribution.h"

class Tavern : public Building {
public:
    using Building::Building;
    explicit Tavern(Building building) : Building(building) {}

    Building *storage_destination();
};
