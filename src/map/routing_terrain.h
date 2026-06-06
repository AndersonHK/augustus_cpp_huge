#pragma once
#ifdef __cplusplus
extern "C" {
#endif


void map_routing_update_all(void);
void map_routing_update_land(void);
void map_routing_update_land_citizen(void);
void map_routing_update_water(void);
void map_routing_update_walls(void);

int map_routing_is_wall_passable(int grid_offset);
int map_routing_wall_tile_in_radius(int x, int y, int radius, int *x_wall, int *y_wall);

int map_routing_citizen_is_passable(int grid_offset);
int map_routing_citizen_is_road(int grid_offset);
int map_routing_citizen_is_highway(int grid_offset);
int map_routing_citizen_is_passable_terrain(int grid_offset);
int map_routing_is_gate_transformable(int grid_offset);
int map_routing_noncitizen_is_passable(int grid_offset);
int map_routing_is_destroyable(int grid_offset);

enum {
    DESTROYABLE_BUILDING,
    DESTROYABLE_AQUEDUCT_GARDEN,
    DESTROYABLE_WALL,
    DESTROYABLE_GATEHOUSE,
    DESTROYABLE_NONE,
};
int map_routing_get_destroyable(int grid_offset);

#ifdef __cplusplus
}
#endif
