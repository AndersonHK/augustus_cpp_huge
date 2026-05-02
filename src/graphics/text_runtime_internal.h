#pragma once

#include "graphics/color.h"
#include "graphics/font.h"

#include <string_view>

int text_get_width_utf8(std::string_view text, font_t font, int pixel_size);
unsigned int text_get_max_utf8_bytes_for_width(
    std::string_view text,
    font_t font,
    int pixel_size,
    unsigned int requested_width,
    int invert);
int text_draw_utf8(std::string_view text, int x, int y, font_t font, int pixel_size, color_t color);
int text_draw_utf8_styled(
    std::string_view text,
    int x,
    int y,
    font_t font,
    int pixel_size,
    color_t color,
    unsigned style_flags);
