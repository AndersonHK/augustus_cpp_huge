#pragma once

#include "building/building.h"

#define ROADBLOCK_PERMISSION_ALL 0xffff

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
} roadblock_permission;

typedef enum {
	ROADBLOCK_NONE = 0,
	ROADBLOCK_STANDARD = 1,
	ROADBLOCK_STORAGE = 2,
    ROADBLOCK_BRIDGE = 3,
}roadblock_type;

class Roadblock : public Building {
public:
    using Building::Building;

    roadblock_type kind() const; //By the time we are asking the kind, we already estabilished it is a roadblock, so it should be safe to run this, modify on callers instead. Removed: static roadblock_type kind_for(building_type type);
    int exceptions() const;
    void set_exceptions(int exceptions);
    void toggle_permission(roadblock_permission permission);
    int has_permission(roadblock_permission permission) const;
    void accept_none();
    void accept_all();
};
