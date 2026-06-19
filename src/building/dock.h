#pragma once

#include "building/building.h"
#include "game/resource.h"
#include "map/point.h"

class Figure;

class Dock : public Building {
public:
    using Building::Building;
};

typedef enum {
    SHIP_DOCK_REQUEST_1_DOCKING = 1,
    SHIP_DOCK_REQUEST_2_FIRST_QUEUE = 2,
    SHIP_DOCK_REQUEST_4_SECOND_QUEUE = 4,
    SHIP_DOCK_REQUEST_6_ANY_QUEUE = 6,
    SHIP_DOCK_REQUEST_7_ANY = 7,
} ship_dock_request_type;

int building_dock_count_idle_dockers(const Building &dock);
void building_dock_update_open_water_access(void);
int building_dock_is_connected_to_open_water(int x, int y);
Building building_dock_get_destination(Figure &ship, const Building *exclude_dock, map_point *tile);
Building building_dock_get_closer_free_destination(Figure &ship, ship_dock_request_type request_type, map_point *tile);
int building_dock_request_docking(Figure &ship, const Building &dock, map_point *tile);
int building_dock_is_working(const Building &dock);
int building_dock_accepts_ship(Figure &ship, const Building &dock);
Building building_dock_reposition_anchored_ship(Figure &ship, map_point *tile);
int building_dock_can_import_from_ship(const Building &dock, int ship_id);
int building_dock_can_export_to_ship(const Building &dock, int ship_id);
void building_dock_get_ship_request_tile(const Building &dock, ship_dock_request_type request_type, map_point *tile);
void building_dock_enable_resource_in_all_docks(resource_type resource);
int building_dock_can_trade_with_route(int route_id, const Building &dock);
void building_dock_set_can_trade_with_route(int route_id, Building &dock, int can_trade);
