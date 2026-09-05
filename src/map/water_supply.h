#pragma once


class Building;

enum {
    BUILDING_NECESSARY = 0,
    BUILDING_UNNECESSARY_FOUNTAIN = 1,
    BUILDING_UNNECESSARY_NO_HOUSES = 2
};

int map_water_supply_is_building_unnecessary(Building *building, int radius);

