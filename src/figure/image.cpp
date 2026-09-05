#include "image.h"

#include "city/view.h"

void figure_image_update(Figure *f, int image_base)
{
    f->select_legacy_default_or_corpse_image(image_base);
}

void figure_image_increase_offset(Figure *f, int max)
{
    f->image_offset++;
    if (f->image_offset >= max) {
        f->image_offset = 0;
    }
}

int figure_image_direction(Figure *f)
{
    int dir = f->direction - city_view_orientation();
    if (dir < 0) {
        dir += 8;
    }
    return dir;
}

int figure_image_normalize_direction(int direction)
{
    int normalized_direction = direction - city_view_orientation();
    if (normalized_direction < 0) {
        normalized_direction += 8;
    }
    return normalized_direction;
}

int figure_image_offset_direction(int direction, int offset)
{
    direction += offset;
    if (direction < 0) {
        direction += 8;
    }
    return direction;
}
