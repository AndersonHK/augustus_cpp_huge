#pragma once

#include <cstddef>
#include <cstdint>

constexpr int ROUTING_PATH_DIRECTION_BIT_OFFSET = 5;
constexpr uint8_t ROUTING_PATH_DIRECTION_COUNT_BIT_MASK =
    static_cast<uint8_t>((1u << ROUTING_PATH_DIRECTION_BIT_OFFSET) - 1);

struct figure_path_data {
    unsigned int id;
    unsigned int figure_id;
    unsigned int total_directions;
    uint8_t *directions;

    size_t current_step;
    uint8_t same_direction_count;
};

int map_routing_get_path(figure_path_data *path, int dst_x, int dst_y, int num_directions);

int map_routing_get_path_on_water(figure_path_data *path, int dst_x, int dst_y, int is_flotsam);
