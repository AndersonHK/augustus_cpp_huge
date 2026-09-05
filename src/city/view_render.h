#pragma once

#include "building/building.h"
#include "game/performance_tracker.h"

#include <cstddef>
#include <vector>

class Figure;

struct CityDrawTileCommand {
    int x = 0;
    int y = 0;
    int grid_offset = -1;
    Building *building = nullptr;
    Figure *first_figure = nullptr;
};

typedef void(city_draw_tile_command_callback)(const CityDrawTileCommand &command);

struct CityViewRenderPhase {
    city_draw_tile_command_callback *callback = nullptr;
    performance_tracker_bucket bucket = PERFORMANCE_TRACKER_BUCKET_MAX;
};

class CityViewRenderCommandBuffer {
public:
    void build();
    void execute(const CityViewRenderPhase *phases, int phase_count) const;

private:
    std::vector<CityDrawTileCommand> commands_;
    std::vector<std::size_t> row_ends_;
};
