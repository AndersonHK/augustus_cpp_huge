#pragma once

#include "core/image.h"

#include <stdint.h>


typedef enum {
    ATLAS_FIRST,
    ATLAS_MAIN = ATLAS_FIRST,
    ATLAS_ENEMY,
    ATLAS_FONT,
    ATLAS_EXTRA_ASSET,
    ATLAS_UNPACKED_EXTRA_ASSET,
    ATLAS_CUSTOM,
    ATLAS_EXTERNAL,
    ATLAS_MAX
} atlas_type;

typedef enum {
    CUSTOM_IMAGE_NONE,
    CUSTOM_IMAGE_EXTERNAL,
    CUSTOM_IMAGE_MINIMAP,
    CUSTOM_IMAGE_VIDEO,
    CUSTOM_IMAGE_EMPIRE_MAP,
    CUSTOM_IMAGE_CLOUDS,
    CUSTOM_IMAGE_MINIMAP_PREVIEW,
    CUSTOM_IMAGE_MAX
} custom_image_type;

typedef enum {
    IMAGE_FILTER_NEAREST = 0,
    IMAGE_FILTER_LINEAR = 1,
    IMAGE_FILTER_BEST = 2
} image_filter;

typedef enum {
    RENDER_DOMAIN_UI = 0,
    RENDER_DOMAIN_PIXEL = 1,
    RENDER_DOMAIN_TOOLTIP_UI = 2,
    RENDER_DOMAIN_TOOLTIP_PIXEL = 3,
    RENDER_DOMAIN_SNAPSHOT_UI = 4,
    RENDER_DOMAIN_SNAPSHOT_PIXEL = 5
} render_domain;

typedef enum {
    RENDER_SCALING_POLICY_AUTO = 0,
    RENDER_SCALING_POLICY_PIXEL_ART = 1,
    RENDER_SCALING_POLICY_HIGH_QUALITY = 2
} render_scaling_policy;

typedef enum {
    RENDER_DESTINATION_GEOMETRY_DEFAULT = 0,
    RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE = 1
} render_destination_geometry_policy;

typedef int32_t render_logical_unit;

enum {
    RENDER_LOGICAL_UNITS_PER_PIXEL = 120
};

typedef struct {
    render_logical_unit width;
    render_logical_unit height;
} render_logical_size;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} render_destination_rect;

typedef struct {
    const image *img;
    image_handle handle;
    float x;
    float y;
    float logical_width;
    float logical_height;
    render_logical_size fixed_logical_size;
    color_t color;
    render_domain domain;
    render_scaling_policy scaling_policy;
    render_destination_geometry_policy destination_geometry_policy;
    double angle;
    int disable_coord_scaling;
    int use_silhouette;
} render_2d_request;

typedef struct {
    image_handle handle;
    int width;
    int height;
    int x_offset;
    int y_offset;
    int is_isometric;
    float x;
    float y;
    float logical_width;
    float logical_height;
    render_logical_size fixed_logical_size;
    color_t color;
    render_domain domain;
    render_scaling_policy scaling_policy;
    render_destination_geometry_policy destination_geometry_policy;
    double angle;
    int disable_coord_scaling;
} managed_image_request;

typedef enum {
    RENDER_COMMAND_CLEAR,
    RENDER_COMMAND_SET_VIEWPORT,
    RENDER_COMMAND_RESET_VIEWPORT,
    RENDER_COMMAND_SET_CLIP_RECTANGLE,
    RENDER_COMMAND_RESET_CLIP_RECTANGLE,
    RENDER_COMMAND_DRAW_LINE,
    RENDER_COMMAND_DRAW_RECT,
    RENDER_COMMAND_FILL_RECT,
    RENDER_COMMAND_SET_OUTPUT_SCALE,
    RENDER_COMMAND_SET_RENDER_DOMAIN,
    RENDER_COMMAND_PUSH_STATE,
    RENDER_COMMAND_POP_STATE,
    RENDER_COMMAND_DRAW_IMAGE,
    RENDER_COMMAND_DRAW_MANAGED_IMAGE,
    RENDER_COMMAND_DRAW_CUSTOM_IMAGE,
    RENDER_COMMAND_DRAW_SAVED_IMAGE
} renderer_command_kind;

typedef struct {
    renderer_command_kind kind;
    int x;
    int y;
    int width;
    int height;
    color_t color;
    float scale;
    render_domain domain;
    custom_image_type custom_image;
    int texture_id;
    int disable_filtering;
    render_2d_request image;
    struct image source_image;
    managed_image_request managed_image;
} renderer_command;

typedef struct {
    uint64_t revision;
    uint64_t resource_revision;
    const renderer_command *commands;
    int command_count;
} renderer_command_snapshot;

typedef struct {
    renderer_command_snapshot snapshot;
    void *ownership;
} renderer_command_snapshot_handle;

typedef struct {
    atlas_type type;
    int num_images;
    color_t **buffers;
    int *image_widths;
    int *image_heights;
} image_atlas_data;

typedef struct {
    void (*clear_screen)(void);

    void (*set_viewport)(int x, int y, int width, int height);
    void (*reset_viewport)(void);

    void (*set_clip_rectangle)(int x, int y, int width, int height);
    void (*reset_clip_rectangle)(void);

    void (*draw_line)(int x_start, int x_end, int y_start, int y_end, color_t color);
    void (*draw_rect)(int x_start, int x_end, int y_start, int y_end, color_t color);
    void (*fill_rect)(int x_start, int x_end, int y_start, int y_end, color_t color);
    void (*set_output_scale)(float scale);
    void (*set_render_domain)(render_domain domain);
    render_domain (*get_render_domain)(void);
    void (*push_state)(void);
    void (*pop_state)(void);

    void (*draw_image)(const image *img, int x, int y, color_t color, float scale);
    void (*draw_image_request)(const render_2d_request *request);
    void (*draw_managed_image_request)(const managed_image_request *request);
    void (*draw_image_advanced)(const image *img, float x, float y, color_t color,
        float scale_x, float scale_y, double angle, int disable_coord_scaling);
    void (*draw_silhouette)(const image *img, int x, int y, color_t color, float scale);
    void (*begin_command_recording)(void);
    void (*end_command_recording)(void);
    renderer_command_snapshot (*command_snapshot)(void);
    renderer_command_snapshot_handle (*acquire_command_snapshot)(void);
    void (*release_command_snapshot)(renderer_command_snapshot_handle *snapshot);
    void (*replay_recorded_commands)(void);

    void (*create_custom_image)(custom_image_type type, int width, int height, int is_yuv);
    int (*has_custom_image)(custom_image_type type);
    color_t *(*get_custom_image_buffer)(custom_image_type type, int *actual_texture_width);
    void (*release_custom_image_buffer)(custom_image_type type);
    void (*update_custom_image)(custom_image_type type);
    void (*update_custom_image_from)(custom_image_type type, const color_t *buffer,
        int x_offset, int y_offset, int width, int height);
    void (*update_custom_image_yuv)(custom_image_type type, const uint8_t *y_data, int y_width,
        const uint8_t *cb_data, int cb_width, const uint8_t *cr_data, int cr_width);
    void (*draw_custom_image)(custom_image_type type, int x, int y, float scale, int disable_filtering);
    int (*supports_yuv_image_format)(void);

    int (*start_tooltip_creation)(int width, int height);
    int (*start_tooltip_creation_for_domain)(render_domain domain, int width, int height);
    void (*finish_tooltip_creation)(void);
    int (*has_tooltip)(void);
    void (*set_tooltip_position)(int x, int y);
    void (*set_tooltip_position_for_domain)(render_domain domain, int x, int y);
    void (*set_tooltip_opacity)(int opacity);

    int (*save_image_from_screen)(int image_id, int x, int y, int width, int height);
    int (*save_image_from_screen_for_domain)(render_domain domain, int image_id, int x, int y, int width, int height);
    void (*draw_image_to_screen)(int image_id, int x, int y);
    void (*draw_image_to_screen_for_domain)(render_domain domain, int image_id, int x, int y);
    int (*save_screen_buffer)(color_t *pixels, int x, int y, int width, int height, int row_width);

    void (*get_max_image_size)(int *width, int *height);

    const image_atlas_data *(*prepare_image_atlas)(atlas_type type, int num_images, int last_width, int last_height);
    int (*create_image_atlas)(const image_atlas_data *data, int delete_buffers);
    const image_atlas_data *(*get_image_atlas)(atlas_type type);
    int (*has_image_atlas)(atlas_type type);
    void (*free_image_atlas)(atlas_type type);

    void (*load_unpacked_image)(const image *img, const color_t *pixels);
    void (*free_unpacked_image)(const image *img);
    void (*upload_image_resource)(image *img, const color_t *pixels, int width, int height);
    void (*release_image_resource)(image *img);

    int (*should_pack_image)(int width, int height);

    void (*update_scale)(int city_scale);
} graphics_renderer_interface;

const graphics_renderer_interface *graphics_renderer(void);

void graphics_renderer_set_interface(const graphics_renderer_interface *new_renderer);
void graphics_renderer_begin_command_recording(void);
void graphics_renderer_end_command_recording(void);
renderer_command_snapshot graphics_renderer_command_snapshot(void);
renderer_command_snapshot_handle graphics_renderer_acquire_command_snapshot(void);
void graphics_renderer_release_command_snapshot(renderer_command_snapshot_handle *snapshot);
void graphics_renderer_replay_recorded_commands(void);


