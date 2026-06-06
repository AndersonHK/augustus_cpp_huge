#pragma once
#ifdef __cplusplus
extern "C" {
#endif


void map_road_network_clear(void);

int map_road_network_get(int grid_offset);

void map_road_network_update(void);

#ifdef __cplusplus
}
#endif
