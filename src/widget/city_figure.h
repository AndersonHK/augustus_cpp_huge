#pragma once

#include "figure/figure.h"
#include "widget/city.h"

enum {
    FIGURE_HIGHLIGHT_NONE = 0,
    FIGURE_HIGHLIGHT_RED = 1,
    FIGURE_HIGHLIGHT_GREEN = 2,
};

void city_draw_figure(const Figure *f, int x, int y, float scale, int highlight);

void city_draw_selected_figure(const Figure *f, int x, int y, float scale, pixel_coordinate *coord);
