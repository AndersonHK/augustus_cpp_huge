#pragma once

#include "graphics/image.h"

class ImageBorder {
public:
    ImageBorder() = default;
    ImageBorder(
        ImageGroupEntryRef top,
        ImageGroupEntryRef left,
        ImageGroupEntryRef bottom,
        ImageGroupEntryRef right);

    static ImageBorder image_small();
    static ImageBorder image_medium();
    static ImageBorder image_large();
    static ImageBorder large_banner();
    static ImageBorder mission_selection();

    void draw(int x, int y, color_t color = COLOR_MASK_NONE) const;

private:
    ImageGroupEntryRef top_;
    ImageGroupEntryRef left_;
    ImageGroupEntryRef bottom_;
    ImageGroupEntryRef right_;
};
