#pragma once

#include "core/image.h"
#include "graphics/renderer.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int legacy_image_extractor_extract_climate(
    const image *images,
    int image_count,
    const uint16_t *group_image_ids,
    int group_count,
    const char *source_name,
    const image_atlas_data *atlas_data);
int legacy_image_extractor_get_group_key(int group_id, char *buffer, size_t buffer_size);
int legacy_image_extractor_get_group_image_key(
    int group_id,
    int image_id,
    char *group_buffer,
    size_t group_buffer_size,
    char *image_buffer,
    size_t image_buffer_size);

#ifdef __cplusplus
}
#endif
