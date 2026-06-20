#pragma once


void tile_runtime_reset(void);
void tile_runtime_clear(int grid_offset);
void tile_runtime_clear_gardens(void);
void tile_runtime_clear_plazas(void);
int tile_runtime_set_garden_image_id(int grid_offset, int is_large, int is_overgrown, int option_index);
int tile_runtime_garden_option_count(int is_large, int is_overgrown);
void tile_runtime_set_plaza_image_id(int grid_offset, const char *image_id);
const char *tile_runtime_plaza_single_image_id(int index);
const char *tile_runtime_plaza_large_image_id(int index);
int tile_runtime_plaza_single_option_count(void);
int tile_runtime_plaza_large_option_count(void);
int tile_runtime_plaza_single_map_image_id(int index);
int tile_runtime_plaza_large_map_image_id(int index);

