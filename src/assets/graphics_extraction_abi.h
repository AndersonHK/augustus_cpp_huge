#ifndef GRAPHICS_EXTRACTION_ABI_H
#define GRAPHICS_EXTRACTION_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GRAPHICS_EXTRACTION_ABI_VERSION 1u

typedef enum graphics_extraction_status_v1 {
    GRAPHICS_EXTRACTION_STATUS_SUCCEEDED = 0,
    GRAPHICS_EXTRACTION_STATUS_FAILED = 1,
    GRAPHICS_EXTRACTION_STATUS_INVALID_ABI = 2,
    GRAPHICS_EXTRACTION_STATUS_UNAVAILABLE = 3
} graphics_extraction_status_v1;

typedef enum graphics_extraction_flags_v1 {
    GRAPHICS_EXTRACTION_FORCE = 1u << 0,
    GRAPHICS_EXTRACTION_WRITE_STAMP = 1u << 1
} graphics_extraction_flags_v1;

typedef struct graphics_extraction_augustus_request_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t flags;
    uint32_t reserved;
    const char *game_root;
    const char *source_graphics;
    const char *output_graphics;
    const char *julius_graphics;
} graphics_extraction_augustus_request_v1;

typedef struct graphics_extraction_climate_request_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t flags;
    uint32_t reserved;
    const void *images;
    int32_t image_count;
    const uint16_t *group_image_ids;
    int32_t group_count;
    const char *source_name;
    const void *atlas_data;
} graphics_extraction_climate_request_v1;

typedef struct graphics_extraction_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t succeeded;
    int32_t source_files;
    int32_t groups_exported;
    int32_t images_exported;
    int32_t pngs_written;
} graphics_extraction_result_v1;

graphics_extraction_status_v1 graphics_extraction_run_augustus_v1(
    const graphics_extraction_augustus_request_v1 *request,
    graphics_extraction_result_v1 *result);

graphics_extraction_status_v1 graphics_extraction_bootstrap_climate_v1(
    const graphics_extraction_climate_request_v1 *request,
    graphics_extraction_result_v1 *result);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_EXTRACTION_ABI_H
