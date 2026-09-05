#pragma once

#include "graphics/renderer.h"

#include <math.h>

inline render_destination_rect render_shared_city_tile_destination(const render_destination_rect &rect)
{
    const float left = floorf(rect.x);
    const float top = floorf(rect.y);
    const float right = ceilf(rect.x + rect.width);
    const float bottom = ceilf(rect.y + rect.height);
    return { left, top, right - left, bottom - top };
}
