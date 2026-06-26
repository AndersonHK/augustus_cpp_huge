#pragma once

#include "building/building.h"

class Armoury : public Building {
public:
    using Building::Building;

    int is_needed() const;
};
