#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void tile_runtime_reset(void);
void tile_runtime_clear(int grid_offset);
void tile_runtime_set_plaza_image_id(int grid_offset, const char *image_id);
const char *tile_runtime_plaza_single_image_id(int index);
const char *tile_runtime_plaza_large_image_id(int index);

#ifdef __cplusplus
}
#endif
