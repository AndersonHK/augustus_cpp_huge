#pragma once

#include "core/buffer.h"
#include "building/roadblock.h"


enum routed_building_type {
    ROUTED_BUILDING_ROAD = 0,
    ROUTED_BUILDING_WALL = 1,
    ROUTED_BUILDING_AQUEDUCT = 2,
    ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC = 4,
    ROUTED_BUILDING_HIGHWAY = 5,
    ROUTED_BUILDING_DRAGGABLE_RESERVOIR = 6
};

int map_routing_distance_generation(void);

void map_routing_calculate_distances(int x, int y);
void map_routing_calculate_distances_road_garden(int x, int y, roadblock_permission permission);
void map_routing_calculate_distances_road_garden_highway(int x, int y, roadblock_permission permission);
int map_routing_calculate_distances_for_building(routed_building_type type, int x, int y);

void map_routing_delete_first_wall_or_aqueduct(int x, int y);

int map_routing_distance(int grid_offset);

int building_destroyable_at(int grid_offset);

int map_routing_citizen_can_travel_over_land(
    int src_x, int src_y, int dst_x, int dst_y, int num_directions, roadblock_permission permission);
int map_routing_citizen_can_travel_over_road_garden(
    int src_x, int src_y, int dst_x, int dst_y, int num_directions, roadblock_permission permission);
int map_routing_citizen_can_travel_over_road_garden_highway(
    int src_x, int src_y, int dst_x, int dst_y, int num_directions, roadblock_permission permission);
int map_routing_can_travel_over_walls(int src_x, int src_y, int dst_x, int dst_y, int num_directions);

int map_routing_noncitizen_can_travel_over_land(
    int src_x, int src_y, int dst_x, int dst_y, int num_directions, int only_through_building_id, int max_tiles);
int map_routing_noncitizen_can_travel_through_everything(int src_x, int src_y, int dst_x, int dst_y, int num_directions);

void map_routing_block(int x, int y, int size);

void map_routing_save_state(buffer *buf);

void map_routing_load_state(buffer *buf);
