#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *game_root_path;
    const char *source_graphics_path;
    const char *output_graphics_path;
    const char *julius_graphics_path;
    int force;
    int write_stamp;
} augustus_asset_extractor_config;

int augustus_asset_extractor_bootstrap(void);
int augustus_asset_extractor_extract_with_config(const augustus_asset_extractor_config *config);

#ifdef __cplusplus
}
#endif
