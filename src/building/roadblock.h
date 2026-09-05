#pragma once

#include "building/building.h"

#define ROADBLOCK_PERMISSION_ALL 0xffff

class Figure;

typedef enum {
	PERMISSION_NONE = 0,
	PERMISSION_MAINTENANCE = 1,
	PERMISSION_PRIEST = 2,
	PERMISSION_MARKET = 3,
	PERMISSION_ENTERTAINER = 4,
	PERMISSION_EDUCATION = 5,
	PERMISSION_MEDICINE = 6,
	PERMISSION_TAX_COLLECTOR = 7,
	PERMISSION_LABOR_SEEKER = 8,
	PERMISSION_MISSIONARY = 9,
	PERMISSION_WATCHMAN = 10,
	PERMISSION_FREIGHT = 11,
} roadblock_permission;

typedef enum {
	ROADBLOCK_NONE = 0,
    ROADBLOCK_FOUNDATION = 1,
    ROADBLOCK_BRIDGE = 2,
}roadblock_type;

class Roadblock : public Building {
public:
    using Building::Building;
    explicit Roadblock(Building building) : Building(building) {}

    roadblock_type kind() const;
    int exceptions() const;
    void set_exceptions(int exceptions);
    void toggle_permission(roadblock_permission permission);
    int has_permission(roadblock_permission permission) const;
    int can_configure(roadblock_permission permission) const;
    int allows_at(int grid_offset, roadblock_permission permission) const;
    int allows(const Figure &figure) const;
    void accept_none();
    void accept_all();

    static roadblock_permission permission_for(const Figure &figure);
};
