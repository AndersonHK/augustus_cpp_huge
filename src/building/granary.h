#pragma once

#include "building/building_type.h"
#include "game/resource.h"
#include "map/point.h"

class Building;

// make sure to update src/window/building/distribution.cpp so the number renders correctly
#define FULL_GRANARY 32
#define THREEQUARTERS_GRANARY 24
#define HALF_GRANARY 16
#define QUARTER_GRANARY 8

enum {
    GRANARY_TASK_NONE = -1,
    GRANARY_TASK_GETTING = 0
};

int building_granary_get_amount(const Building &b, resource_type resource);
Building *building_granary_first();
int building_granary_get_free_space_amount(const Building &b);
int building_granary_count_available_resource(const Building &b, resource_type resource, int respect_maintaining);
int building_granaries_count_available_resource(resource_type resource, int respect_maintaining, int caesars_request);
int building_granary_maximum_receptible_amount(
    const Building &b, resource_type resource, unsigned int ignore_figure_id = 0);
int building_granary_accepts_storage(const Building &b, resource_type resource, int *understaffed);
int building_granary_add_import(Building &granary, resource_type resource, int amount, int trader_type);
int building_granary_try_add_resource(
    Building &granary, resource_type resource, int amount, int is_produced, int respect_settings,
    unsigned int ignore_figure_id = 0);
int building_granaries_add_resource(resource_type resource, int amount, int respect_settings);
int building_granary_remove_export(Building &granary, resource_type resource, int amount, int trader_type);
int building_granary_try_remove_resource(Building &granary, resource_type resource, int desired_amount);
int building_granaries_remove_resource(resource_type resource, int amount);
Building *building_granary_get_granary_needing_food(const Building &source, resource_type resource, int getting);
int building_granary_for_storing(int x, int y, resource_type resource, int road_network_id,
    int force_on_stockpile, int *understaffed, map_point *dst);
int building_getting_granary_for_storing(int x, int y, resource_type resource, int road_network_id, map_point *dst);
int building_granary_amount_can_get_from(const Building &destination, const Building &origin, resource_type resource);
int building_granary_for_getting(const Building &src, map_point *dst, int min_amount);
int building_granary_remove_for_getting_deliveryman(Building &src, Building &dst, int *resource);
int building_granary_determine_worker_task(const Building &granary);
void building_granaries_calculate_stocks(void);
void building_granary_update_built_granaries_capacity(void);
int building_granaries_send_resources_to_rome(resource_type resource, int amount);
void building_granary_bless(void);
void building_granary_warehouse_curse(int big);
