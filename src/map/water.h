#pragma once

#include "figure/figure.h"
#include "map/point.h"

class Building;

Building *map_water_assign_wharf_for_new_fishing_boat(Figure *boat, map_point *tile);
int map_water_assign_fishing_boat_to_wharf(Figure *boat, Building wharf, map_point *tile);
void map_water_clear_fishing_boat_from_wharf(Building wharf, unsigned int boat_id);
int map_water_wharf_live_fishing_boats(Building wharf);
Figure *map_water_wharf_live_fishing_boat(Building wharf);
int map_water_wharf_has_self_fishing_boat_room(Building wharf);
int map_water_has_wharf_for_new_fishing_boat(void);
int map_water_shipyard_can_spawn_fishing_boat(Building shipyard);
int map_water_spawn_fishing_boat_from_shipyard(Building shipyard);
int map_water_spawn_fishing_boat_from_wharf(Building wharf);

int map_water_find_alternative_fishing_boat_tile(Figure *boat, map_point *tile);

int map_water_find_shipwreck_tile(Figure *wreck, map_point *tile);

int map_water_can_spawn_fishing_boat(const Building &building, map_point *tile);

