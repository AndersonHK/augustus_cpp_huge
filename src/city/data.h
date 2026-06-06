#pragma once

#include "core/buffer.h"
#ifdef __cplusplus
extern "C" {
#endif


void city_data_init(void);

void city_data_init_scenario(void);

void city_data_init_campaign_mission(void);

void city_data_save_state(buffer *main, buffer *graph_order, buffer *entry_exit_xy, buffer *entry_exit_grid_offset);

void city_data_load_state(buffer *main, buffer *graph_order, buffer *entry_exit_xy, buffer *entry_exit_grid_offset,
    int version);

void city_data_load_basic_info(buffer *main, int *population, int *treasury, unsigned int *caravanserai_id, int version);

#ifdef __cplusplus
}
#endif
