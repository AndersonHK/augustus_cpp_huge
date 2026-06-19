#pragma once

#include "graphics/image.h"

#include "game/resource.h"

#include <array>

class ResourceGraphics {
public:
    void set_storage_images(std::array<ImageGroupEntryRef, 4> images);
    void set_cart_images(ImageGroupEntryRef single_load, ImageGroupEntryRef multiple_loads, ImageGroupEntryRef eight_loads);
    void set_panel_icon(ImageGroupEntryRef image);
    void set_empire_icon(ImageGroupEntryRef image);
    void set_editor_icon(ImageGroupEntryRef image);
    void set_editor_empire_icon(ImageGroupEntryRef image);

    const ImageGroupEntryRef &storage_image(int loads) const;
    const ImageGroupEntryRef &cart_image(int carried_loads, int use_food_eight_load_variant = 0) const;
    ImageGroupEntryRef cart_image_for_direction(
        int carried_loads,
        int use_food_eight_load_variant,
        int direction) const;
    const ImageGroupEntryRef &panel_icon() const;
    const ImageGroupEntryRef &empire_icon() const;
    const ImageGroupEntryRef &editor_icon() const;
    const ImageGroupEntryRef &editor_empire_icon() const;

private:
    static const ImageGroupEntryRef &empty_cart_image();
    static const ImageGroupEntryRef &empty_storage_image();

    std::array<ImageGroupEntryRef, 4> storage_images_ = {};
    ImageGroupEntryRef cart_single_load_;
    ImageGroupEntryRef cart_multiple_loads_;
    ImageGroupEntryRef cart_eight_loads_;
    ImageGroupEntryRef panel_icon_;
    ImageGroupEntryRef empire_icon_;
    ImageGroupEntryRef editor_icon_;
    ImageGroupEntryRef editor_empire_icon_;
};

const ResourceGraphics &resource_graphics(resource_type resource);
ResourceGraphics &mutable_resource_graphics(resource_type resource);
void resource_graphics_reset();

int resource_graphics_cart_marker_for_direction(int direction);
int resource_graphics_cart_marker_is(unsigned int image_id);
int resource_graphics_cart_marker_direction(unsigned int image_id);
