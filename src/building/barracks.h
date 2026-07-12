#pragma once

#include "building/building.h"

#define MAX_WEAPONS_BARRACKS 4

struct formation;

typedef enum {
	PRIORITY_FORT = 0,
	PRIORITY_FORT_JAVELIN = 1,
	PRIORITY_FORT_MOUNTED = 2,
	PRIORITY_FORT_AUXILIA_INFANTRY = 3,
	PRIORITY_FORT_AUXILIA_ARCHERY = 4,
	PRIORITY_TOWER = 5,
	PRIORITY_WATCHTOWER = 6,
} barracks_priority;

class Barracks : public Building {
public:
    using Building::Building;
    explicit Barracks(Building building) : Building(building) {}

    static Building *for_weapon(int x, int y, resource_type resource, int road_network_id, map_point *dst);

    int priority() const;
    void set_priority(int priority);
    int create_soldier(int x, int y);
    Building *unmanned_tower(map_point *road) const;
    int create_tower_sentry(int x, int y);

private:
    int closest_legion_needing_soldiers() const;
    int can_recruit_soldier_for(const formation &legion) const;
};
