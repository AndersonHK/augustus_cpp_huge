#pragma once

#include "building/building.h"
#include "game/performance_tracker.h"

struct CityViewRenderTile {
    int x = 0;
    int y = 0;
    int grid_offset = -1;
    Building building = Building(nullptr);
};

typedef void(city_view_render_tile_callback)(const CityViewRenderTile &tile);

void city_view_foreach_valid_render_tile_row(
    city_view_render_tile_callback *callback1,
    city_view_render_tile_callback *callback2,
    city_view_render_tile_callback *callback3,
    performance_tracker_bucket bucket1 = PERFORMANCE_TRACKER_BUCKET_MAX,
    performance_tracker_bucket bucket2 = PERFORMANCE_TRACKER_BUCKET_MAX,
    performance_tracker_bucket bucket3 = PERFORMANCE_TRACKER_BUCKET_MAX);
