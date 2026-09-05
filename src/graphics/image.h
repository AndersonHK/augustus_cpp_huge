#pragma once

#include "core/image.h"
#include "graphics/color.h"
#include "graphics/font.h"
#include "graphics/runtime_texture.h"

#include <optional>
#include <string>
#include <string_view>

constexpr float SCALE_NONE = 1.0f;

class Image;
class ImageGroupEntry;

class ImageGroupEntryRef {
public:
    ImageGroupEntryRef() = default;

    static ImageGroupEntryRef from_group(std::string group_path, std::string entry_id = {});
    int is_bound() const;
    const std::string &group_path() const;
    const std::string &entry_id() const;
    RuntimeDrawSlice runtime_slice() const;
    RuntimeDrawSlice top_runtime_slice() const;
    int width() const;
    int height() const;
    int source_pixel_width() const;
    int source_pixel_height() const;
    render_logical_size fixed_logical_size() const;

    void draw(int x, int y, color_t color = COLOR_MASK_NONE, float scale = SCALE_NONE) const;
    void draw_top(int x, int y, color_t color = COLOR_MASK_NONE, float scale = SCALE_NONE) const;
    void draw_scaled_centered(int x, int y, color_t color, int draw_scale_percent) const;

private:
    const ImageGroupEntry *entry() const;

    std::string group_path_;
    std::string entry_id_;
};

class ImageManager {
public:
    Image &from_id(int image_id);
    Image &from_legacy(image &legacy_image);
    const Image &from_legacy(const image &legacy_image);
    Image *find(std::string_view path_key);
    const Image *find(std::string_view path_key) const;

    Image &register_image(image &legacy_image, std::string_view path_key);
    Image *acquire(image &legacy_image, std::string_view path_key);
    Image *load_png(image &legacy_image, std::string_view path_key, const char *file_path);
    Image *load_png(std::string_view path_key, const char *file_path);
    Image *load_pixels(std::string_view path_key, const image &metadata, const color_t *pixels, int width, int height);
    void retain(std::string_view path_key);
    void release(std::string_view path_key);
    void release(image &legacy_image);
    void clear();

private:
    friend class Image;

    Image &store_legacy_image(image &legacy_image);
    Image &store_keyed_image(std::string_view path_key, const image &legacy_image);
};

ImageManager &image_manager();

class Image {
public:
    Image() = default;
    explicit Image(const image &legacy_image);

    static Image &from_id(int image_id);
    static Image &from_legacy(image &legacy_image);
    static const Image &from_legacy(const image &legacy_image);
    static Image &letter(int letter_id);
    static Image &enemy(int image_id);
    static int group(int group_id);

    static int load_climate(int climate_id, int is_editor, int force_reload, int keep_atlas_buffers, int extract_legacy_graphics);
    static int load_fonts(encoding_type encoding);
    static int load_enemy_graphics(int enemy_id);

    static void copy(const image_copy_info &copy_info);
    static void copy_isometric_footprint(const image_copy_info &copy_info);
    static void blend_footprint_color(int x, int y, color_t color, float scale);

    const std::string &key() const;
    const char *key_c_str() const;
    image_handle render_handle() const;
    int ref_count() const;

    int x_offset() const;
    int y_offset() const;
    int width() const;
    int height() const;
    int original_width() const;
    int original_height() const;
    int is_isometric() const;
    const image_animation *animation() const;
    image_animation *animation();
    const Image *top() const;
    Image *top();

    RuntimeDrawSlice runtime_slice() const;
    const image &legacy() const;
    image &mutable_legacy_for_image_subsystem();

    void sync_from_legacy(const image &legacy_image);
    void set_render_handle(image_handle handle);
    void retain();
    int release_ref();
    void release_renderer_resource();

    int is_external() const;
    void load_external_data() const;
    int get_external_dimensions(int &out_width, int &out_height) const;
    void crop(const color_t *pixels);

    void draw(int x, int y, color_t color = COLOR_MASK_NONE, float scale = SCALE_NONE) const;
    void draw_silhouette(int x, int y, color_t color, float scale = SCALE_NONE) const;
    void draw_scaled_centered(int x, int y, color_t color, int draw_scale_percent) const;
    void draw_letter(font_t font, int x, int y, color_t color, float scale = SCALE_NONE) const;
    void draw_fullscreen_background() const;
    void draw_blurred_fullscreen(int intensity) const;
    void draw_isometric_footprint(int x, int y, color_t color_mask = COLOR_MASK_NONE, float scale = SCALE_NONE) const;
    void draw_isometric_footprint_from_draw_tile(int x, int y, color_t color_mask = COLOR_MASK_NONE, float scale = SCALE_NONE, render_destination_geometry_policy destination_geometry_policy = RENDER_DESTINATION_GEOMETRY_DEFAULT) const;
    void draw_isometric_top(int x, int y, color_t color_mask = COLOR_MASK_NONE, float scale = SCALE_NONE) const;
    void draw_isometric_top_from_draw_tile(int x, int y, color_t color_mask = COLOR_MASK_NONE, float scale = SCALE_NONE) const;
    void draw_set_isometric_top_from_draw_tile(int x, int y, color_t color_mask = COLOR_MASK_NONE, float scale = SCALE_NONE) const;

private:
    friend class ImageManager;

    void bind_key(std::string_view path_key);
    void bind_legacy(image &legacy_image);
    void clear_bound_legacy();
    void ensure_ready_to_draw() const;
    void set_source_image_id(int image_id);

    image legacy_image_ = {};
    std::string key_;
    int ref_count_ = 0;
    image *bound_legacy_ = nullptr;
    int source_image_id_ = -1;
};
