#include "visited_buildings.h"

#include "city/data_private.h"
#include "figure/figure.h"

#include <cstdlib>
#include <vector>

#define VISITED_BUILDINGS_BUFFER_SIZE (sizeof(int32_t) * 2)

static std::vector<visited_building> visited_buildings;

static unsigned int visited_building_pool_size(void)
{
    return static_cast<unsigned int>(visited_buildings.size());
}

static bool visited_building_is_active(const visited_building &visited)
{
    return visited.building_id != 0;
}

static visited_building *visited_building_slot(unsigned int index)
{
    return index < visited_buildings.size() ? &visited_buildings[index] : nullptr;
}

static void reset_visited_building_slot(unsigned int index)
{
    visited_buildings[index] = {};
    visited_buildings[index].index = index;
}

static visited_building *append_visited_building(void)
{
    visited_buildings.emplace_back();
    reset_visited_building_slot(visited_building_pool_size() - 1);
    return &visited_buildings.back();
}

static visited_building *new_visited_building_after_index(unsigned int start)
{
    while (start > visited_building_pool_size()) {
        append_visited_building();
    }
    for (unsigned int i = start; i < visited_building_pool_size(); i++) {
        if (!visited_building_is_active(visited_buildings[i])) {
            reset_visited_building_slot(i);
            return &visited_buildings[i];
        }
    }
    return append_visited_building();
}

static void trim_inactive_visited_buildings(void)
{
    while (visited_buildings.size() > 1 && !visited_building_is_active(visited_buildings.back())) {
        visited_buildings.pop_back();
    }
}

static void resize_visited_buildings_for_load(unsigned int size)
{
    visited_buildings.resize(size);
    for (unsigned int i = 0; i < size; i++) {
        reset_visited_building_slot(i);
    }
}

void figure_visited_buildings_init(void)
{
    visited_buildings.clear();
}

int figure_visited_building_in_list(int index, int building_id)
{
    while (index) {
        const visited_building *visited = visited_building_slot(index);
        if (!visited) {
            return 0;
        }
        if (visited->building_id == building_id) {
            return 1;
        }
        index = visited->prev_index;
    }
    return 0;
}

int figure_visited_buildings_add(int index, int building_id)
{
    if (figure_visited_building_in_list(index, building_id)) {
        return index;
    }
    visited_building *visited = new_visited_building_after_index(1);
    visited->building_id = building_id;
    visited->prev_index = index;
    return visited->index;
}

void figure_visited_buildings_remove_list(int index)
{
    while (index) {
        visited_building *visited = visited_building_slot(index);
        if (!visited) {
            break;
        }
        index = visited->prev_index;
        visited->prev_index = 0;
        visited->building_id = 0;
    }
    trim_inactive_visited_buildings();
}

void figure_visited_buildings_save_state(buffer *buf)
{
    int buf_size = sizeof(int32_t) + visited_building_pool_size() * VISITED_BUILDINGS_BUFFER_SIZE;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, VISITED_BUILDINGS_BUFFER_SIZE);
    for (const visited_building &visited : visited_buildings) {
        buffer_write_i32(buf, visited.building_id);
        buffer_write_i32(buf, visited.prev_index);
    }
}

void figure_visited_buildings_load_state(buffer *buf)
{
    int visited_buildings_to_load = (int) (buf->size - sizeof(int32_t)) / buffer_read_i32(buf);

    resize_visited_buildings_for_load(visited_buildings_to_load);
    for (int i = 0; i < visited_buildings_to_load; i++) {
        visited_building *visited = visited_building_slot(i);
        visited->building_id = buffer_read_i32(buf);
        visited->prev_index = buffer_read_i32(buf);
    }
}

void figure_visited_buildings_migrate(void)
{
    visited_buildings.clear();
    for (unsigned int i = 0; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        const unsigned int visited_dock_mask = f->legacy_visited_dock_mask;
        if (f->type != FIGURE_TRADE_SHIP || f->state == FIGURE_STATE_DEAD || !visited_dock_mask) {
            continue;
        }
        for (int j = 0; j < 10; j++) { // Magic number: 10 is the original allowed maximum number of docks
            int dock_id = city_data.building.legacy_working_dock_ids[j];
            if (!dock_id) {
                continue;
            }
            if (visited_dock_mask & (1 << j)) {
                visited_building *visited = new_visited_building_after_index(1);
                visited->building_id = dock_id;
                visited->prev_index = f->last_visited_index;
                f->last_visited_index = visited->index;
            }
        }
        f->legacy_visited_dock_mask = 0;
    }
}
