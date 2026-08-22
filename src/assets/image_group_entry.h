#pragma once

#include "game/Animation.h"
#include "graphics/color.h"
#include "graphics/runtime_texture.h"

#include <string>
#include <vector>

class ImageGroupEntry {
public:
    ImageGroupEntry() = default;
    explicit ImageGroupEntry(std::string id);

    const std::string &id() const;
    const RuntimeDrawSlice *footprint() const;
    const RuntimeDrawSlice *top() const;
    int has_top() const;
    int has_animation() const;
    const Animation &animation() const;
    int has_sprite_offset() const;
    int sprite_offset_x() const;
    int sprite_offset_y() const;
    int is_isometric() const;
    int tile_span() const;
    int source_pixel_width() const;
    int source_pixel_height() const;
    render_logical_size fixed_logical_size() const;
    const std::vector<color_t> &split_pixels() const;
    int split_width() const;
    int split_height() const;
    int top_height() const;

    void set_base_slice(RuntimeDrawSlice footprint, int is_isometric, int tile_span);
    void set_top_slice(RuntimeDrawSlice top);
    void clear_top_slice();
    void set_animation(Animation animation);
    void set_sprite_offset(int x, int y);
    void set_split_pixels(std::vector<color_t> split_pixels, int split_width, int split_height, int top_height);

private:
    std::string id_;
    RuntimeDrawSlice footprint_;
    RuntimeDrawSlice top_;
    Animation animation_;
    int has_top_ = 0;
    int has_animation_ = 0;
    int has_sprite_offset_ = 0;
    int sprite_offset_x_ = 0;
    int sprite_offset_y_ = 0;
    int is_isometric_ = 0;
    int tile_span_ = 0;
    std::vector<color_t> split_pixels_;
    int split_width_ = 0;
    int split_height_ = 0;
    int top_height_ = 0;
};
