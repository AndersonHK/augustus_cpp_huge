#pragma once

#include "figure/figure.h"
void figure_image_update(Figure *f, int image_base);

void figure_image_increase_offset(Figure *f, int max);

int figure_image_direction(Figure *f);

int figure_image_normalize_direction(int direction);

int figure_image_offset_direction(int direction, int offset);
