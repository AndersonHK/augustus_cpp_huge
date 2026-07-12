#pragma once

#include "building/building.h"
#include "game/performance_tracker.h"

struct CityViewRenderTile {
    int x = 0;
    int y = 0;
    int grid_offset = -1;
    Building *building = nullptr;
};

typedef void(city_view_render_tile_callback)(const CityViewRenderTile &tile);

struct CityViewRenderPhase {
    city_view_render_tile_callback *callback = nullptr;
    performance_tracker_bucket bucket = PERFORMANCE_TRACKER_BUCKET_MAX;
};

void city_view_foreach_valid_render_tile_row(const CityViewRenderPhase *phases, int phase_count);
