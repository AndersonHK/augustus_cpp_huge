#pragma once

#include "building/building.h"

class Armoury : public Building {
public:
    using Building::Building;
    explicit Armoury(Building building) : Building(building) {}

    int is_needed() const;
};
