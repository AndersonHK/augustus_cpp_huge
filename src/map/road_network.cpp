#include "road_network.h"

#include "city/map.h"
#include "figure/PathingMode.h"
#include "map/data.h"
#include "map/grid.h"

#include <string.h>

#define MAX_QUEUE 1000

static const int ADJACENT_OFFSETS[] = {-GRID_SIZE, 1, GRID_SIZE, -1};

static grid_u8 network;

static struct {
    int items[MAX_QUEUE];
    int head;
    int tail;
} queue;

void map_road_network_clear(void)
{
    map_grid_clear_u8(network.items);
}

int map_road_network_get(int grid_offset)
{
    return network.items[grid_offset];
}

static int mark_road_network(int grid_offset, uint8_t network_id)
{
    memset(&queue, 0, sizeof(queue));
    int guard = 0;
    int next_offset;
    int size = 1;
    do {
        if (++guard >= GRID_SIZE * GRID_SIZE) {
            break;
        }
        network.items[grid_offset] = network_id;
        next_offset = -1;
        for (int i = 0; i < 4; i++) {
            int new_offset = grid_offset + ADJACENT_OFFSETS[i];
            if (figure_type_registry_impl::PathingMode::citizenIsPassable(new_offset) && !network.items[new_offset]) {
                if (figure_type_registry_impl::PathingMode::citizenIsRoadLike(new_offset) ||
                    figure_type_registry_impl::PathingMode::citizenIsHighway(new_offset)) {
                    network.items[new_offset] = network_id;
                    size++;
                    if (next_offset == -1) {
                        next_offset = new_offset;
                    } else {
                        queue.items[queue.tail++] = new_offset;
                        if (queue.tail >= MAX_QUEUE) {
                            queue.tail = 0;
                        }
                    }
                }
            }
        }
        if (next_offset == -1) {
            if (queue.head == queue.tail) {
                return size;
            }
            next_offset = queue.items[queue.head++];
            if (queue.head >= MAX_QUEUE) {
                queue.head = 0;
            }
        }
        grid_offset = next_offset;
    } while (next_offset > -1);
    return size;
}

void map_road_network_update(void)
{
    city_map_clear_largest_road_networks();
    map_grid_clear_u8(network.items);
    int network_id = 1;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (figure_type_registry_impl::PathingMode::citizenIsRoadLike(grid_offset) && !network.items[grid_offset]) {
                int size = mark_road_network(grid_offset, network_id);
                city_map_add_to_largest_road_networks(network_id, size);
                network_id++;
            }
        }
    }
}
