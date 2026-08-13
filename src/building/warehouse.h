#pragma once

#include "building/building.h"
#include "building/storage.h"
#include "building/building_type.h"
#include "game/resource.h"
#include "map/point.h"


#define FULL_WAREHOUSE 32
#define THREEQ_WAREHOUSE 24
#define HALF_WAREHOUSE 16
#define QUARTER_WAREHOUSE 8

enum {
    WAREHOUSE_REMOVING_RESOURCE = 0,
    WAREHOUSE_ADDING_RESOURCE = 1
};

enum {
    WAREHOUSE_ROOM = 0,
    WAREHOUSE_FULL = 1,
    WAREHOUSE_SOME_ROOM = 2
};

enum {
    WAREHOUSE_TASK_NONE = -1,
    WAREHOUSE_TASK_GETTING = 0,
    WAREHOUSE_TASK_DELIVERING = 1
};

int building_warehouse_get_space_info(const Building &warehouse);
int building_warehouse_get_main_grid_offset(const Building &warehouse);
int building_warehouse_get_tower_grid_offset(const Building &warehouse);
int building_warehouse_get_amount(const Building &warehouse, resource_type resource);
int building_warehouse_get_free_space_amount(Building &warehouse);
int building_warehouse_get_available_amount(const Building &warehouse, resource_type resource);
int building_warehouse_maximum_receptible_amount(
    Building &warehouse, resource_type resource, unsigned int ignore_figure_id = 0);
int building_warehouses_count_available_resource(resource_type resource, int respect_maintaining, int caesars_request);
int building_warehouse_accepts_storage(Building &warehouse, resource_type resource, int *understaffed);
int building_warehouse_add_import(Building &warehouse, resource_type resource, int amount, int trader_type);
int building_warehouse_try_add_resource(
    Building &b, resource_type resource, int quantity, int respect_settings, unsigned int ignore_figure_id = 0);
int building_warehouses_add_resource(resource_type resource, int amount, int respect_settings);
int building_warehouse_remove_export(Building &warehouse, resource_type resource, int amount, int trader_type);
int building_warehouse_try_remove_resource(Building &warehouse, resource_type resource, int desired_amount);
void building_warehouse_remove_resource_curse(Building &warehouse, int amount);
int building_warehouses_remove_resource(resource_type resource, int amount);
int building_warehouse_amount_can_get_from(const Building &destination, resource_type resource);
Building *building_warehouse_for_getting(const Building &src, resource_type resource, map_point *dst);
Building *building_warehouse_for_storing(int src_building_id, int x, int y, resource_type resource, int road_network_id,
    int *understaffed, map_point *dst);
Building *building_warehouse_with_resource(int x, int y, resource_type resource, int road_network_id, int *understaffed,
    map_point *dst, building_storage_permission_states p);
int building_warehouse_determine_worker_task(Building &warehouse, int *resource);
void building_warehouse_recount_resources(Building &main);
int building_warehouses_send_resources_to_rome(resource_type resource, int amount);

