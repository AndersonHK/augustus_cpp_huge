#include "route.h"

#include "core/log.h"
#include "game/save_version.h"
#include "map/routing.h"
#include "map/routing_path.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#define MAX_ORIGINAL_PATH_LENGTH 500

static std::vector<figure_path_data> paths;

static unsigned int path_pool_size(void)
{
    return static_cast<unsigned int>(paths.size());
}

static bool path_is_active(const figure_path_data &path)
{
    return path.figure_id != 0;
}

static figure_path_data *path_slot(unsigned int id)
{
    return id < paths.size() ? &paths[id] : nullptr;
}

static void reset_path_slot(unsigned int id)
{
    free(paths[id].directions);
    paths[id] = {};
    paths[id].id = id;
}

static figure_path_data *append_path(void)
{
    paths.emplace_back();
    reset_path_slot(path_pool_size() - 1);
    return &paths.back();
}

static figure_path_data *new_path_after_index(unsigned int start)
{
    while (start > path_pool_size()) {
        append_path();
    }
    for (unsigned int i = start; i < path_pool_size(); i++) {
        if (!path_is_active(paths[i])) {
            reset_path_slot(i);
            return &paths[i];
        }
    }
    return append_path();
}

static void release_path(figure_path_data &path)
{
    free(path.directions);
    path.figure_id = 0;
    path.total_directions = 0;
    path.directions = nullptr;
    path.current_step = 0;
    path.same_direction_count = 0;
}

static void trim_inactive_paths(void)
{
    while (paths.size() > 1 && !path_is_active(paths.back())) {
        release_path(paths.back());
        paths.pop_back();
    }
}

static void resize_paths_for_load(unsigned int size)
{
    for (figure_path_data &path : paths) {
        release_path(path);
    }
    paths.clear();
    paths.resize(size);
    for (unsigned int i = 0; i < size; i++) {
        paths[i].id = i;
    }
}

void figure_route_clear_all(void)
{
    for (figure_path_data &path : paths) {
        release_path(path);
    }
    paths.clear();
}

void figure_route_clean(void)
{
    for (unsigned int i = 0; i < path_pool_size(); i++) {
        figure_path_data *path = &paths[i];
        unsigned int figure_id = path->figure_id;
        if (figure_id > 0 && figure_id < Figure::count()) {
            const Figure *f = Figure::get(figure_id);
            if (f->state != FIGURE_STATE_ALIVE || f->routing_path_id != i) {
                release_path(*path);
            }
        }
    }
    trim_inactive_paths();
}

void figure_route_add(Figure *f)
{
    f->routing_path_id = 0;
    f->routing_path_current_tile = 0;
    f->routing_path_length = 0;
    int direction_limit = 8;
    if (f->disallow_diagonal) {
        direction_limit = 4;
    }
    figure_path_data *path = new_path_after_index(1);
    if (!path) {
        return;
    }
    int path_length;
    if (f->is_boat) {
        if (f->is_boat == 2) { // flotsam
            map_routing_calculate_distances_water_flotsam(f->x, f->y);
            path_length = map_routing_get_path_on_water(path, f->destination_x, f->destination_y, 1);
        } else {
            map_routing_calculate_distances_water_boat(f->x, f->y);
            path_length = map_routing_get_path_on_water(path, f->destination_x, f->destination_y, 0);
        }
    } else {
        // land figure
        int can_travel;
        switch (f->terrain_usage) {
            case TERRAIN_USAGE_ENEMY:
                // check to see if we can reach our destination by going around the city walls
                can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit, f->destination_building.id(), 5000);
                if (!can_travel) {
                    can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit, 0, 25000);
                    if (!can_travel) {
                        can_travel = map_routing_noncitizen_can_travel_through_everything(
                            f->x, f->y, f->destination_x, f->destination_y, direction_limit);
                    }
                }
                break;
            case TERRAIN_USAGE_WALLS:
                can_travel = map_routing_can_travel_over_walls(f->x, f->y,
                    f->destination_x, f->destination_y, 4);
                break;
            case TERRAIN_USAGE_ANIMAL:
                can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit, -1, 5000);
                break;
            case TERRAIN_USAGE_PREFER_ROADS:
                can_travel = map_routing_citizen_can_travel_over_road_garden(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                if (!can_travel) {
                    can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit);
                }
                break;
            case TERRAIN_USAGE_ROADS:
                can_travel = map_routing_citizen_can_travel_over_road_garden(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
            case TERRAIN_USAGE_PREFER_ROADS_HIGHWAY:
                can_travel = map_routing_citizen_can_travel_over_road_garden_highway(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                if (!can_travel) {
                    can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit);
                }
                break;
            case TERRAIN_USAGE_ROADS_HIGHWAY:
                can_travel = map_routing_citizen_can_travel_over_road_garden_highway(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
            default:
                can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
        }
        if (can_travel) {
            if (f->terrain_usage == TERRAIN_USAGE_WALLS) {
                path_length = map_routing_get_path(path, f->destination_x, f->destination_y, 4);
                if (path_length <= 0) {
                    path_length = map_routing_get_path(path, f->destination_x, f->destination_y, direction_limit);
                }
            } else {
                path_length = map_routing_get_path(path, f->destination_x, f->destination_y, direction_limit);
            }
        } else { // cannot travel
            path_length = 0;
        }
    }
    if (path_length) {
        path->figure_id = f->id();
        f->routing_path_id = path->id;
        f->routing_path_length = path_length;
    } else {
        release_path(*path);
        trim_inactive_paths();
    }
}

void figure_route_remove(Figure *f)
{
    if (f->routing_path_id > 0) {
        figure_path_data *path = path_slot(f->routing_path_id);
        if (path && path->figure_id == f->id()) {
            release_path(*path);
        }
        f->routing_path_id = 0;
    }
    trim_inactive_paths();
}

int figure_route_get_current_direction(int path_id)
{
    figure_path_data *path = path_slot(path_id);
    if (!path) {
        return 8;
    }
    if (path->current_step >= path->total_directions) {
        return 8;
    }
    return path->directions[path->current_step] >> ROUTING_PATH_DIRECTION_BIT_OFFSET;
}

void figure_route_advance_tile(int path_id)
{
    figure_path_data *path = path_slot(path_id);
    if (!path) {
        return;
    }

    if (path->current_step >= path->total_directions) {
        return;
    }

    int tiles_in_direction = (path->directions[path->current_step] & ROUTING_PATH_DIRECTION_COUNT_BIT_MASK) + 1;

    path->same_direction_count++;
    if (path->same_direction_count >= tiles_in_direction) {
        path->current_step++;
        path->same_direction_count = 0;
    }
}

void figure_route_save_state(buffer *figures, buffer *buf_paths)
{
    unsigned int size = path_pool_size() * sizeof(uint32_t);
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(size));
    buffer_init(figures, buf_data, size);

    unsigned int paths_memory_size = 0;

    for (figure_path_data &path : paths) {
        paths_memory_size += sizeof(uint32_t) + path.total_directions * sizeof(uint8_t);
    }

    size = sizeof(uint32_t) + paths_memory_size;
    buf_data = static_cast<uint8_t *>(malloc(size));
    buffer_init(buf_paths, buf_data, size);
    buffer_write_u32(buf_paths, path_pool_size());

    for (figure_path_data &path : paths) {
        buffer_write_u32(figures, path.figure_id);
        buffer_write_u32(buf_paths, path.total_directions);
        if (path.total_directions) {
            buffer_write_raw(buf_paths, path.directions, path.total_directions * sizeof(uint8_t));
        }
    }
}

static int convert_old_directions_to_new_format(figure_path_data *path, const uint8_t *directions)
{
    Figure *f = Figure::get(path->figure_id);

    // Invalid figure or no path
    if (!f || f->id() == 0 || f->routing_path_length == 0) {
        return 1;
    }

    uint8_t new_directions[MAX_ORIGINAL_PATH_LENGTH] = { 0 };

    int current_direction = -1;
    unsigned int total_direction_changes = 0;
    uint8_t current_count = 0;

    for (unsigned int index = 0; index < f->routing_path_length; index++) {
        if (directions[index] != current_direction || current_count == ROUTING_PATH_DIRECTION_COUNT_BIT_MASK) {
            new_directions[total_direction_changes] = (directions[index] << ROUTING_PATH_DIRECTION_BIT_OFFSET);
            total_direction_changes++;
            current_direction = directions[index];
            current_count = 0;
        } else {
            new_directions[total_direction_changes - 1]++;
            current_count++;
        }
    }

    path->total_directions = total_direction_changes;
    path->directions = static_cast<uint8_t *>(malloc(path->total_directions * sizeof(uint8_t)));

    if (!path->directions) {
        log_error("Unable to allocate memory for routing path directions. The game will likely crash.", 0, 0);
        return 0;
    }

    memcpy(path->directions, new_directions, path->total_directions * sizeof(uint8_t));

    return 1;
}

static void update_current_tile(figure_path_data *path)
{
    Figure *f = Figure::get(path->figure_id);

    // Invalid figure or no path
    if (!f || f->id() == 0 || f->routing_path_length == 0) {
        return;
    }

    unsigned int index = f->routing_path_current_tile + 1;

    for (unsigned int i = 0; i < path->total_directions; i++) {
        unsigned int tiles_in_direction = (path->directions[i] & ROUTING_PATH_DIRECTION_COUNT_BIT_MASK) + 1;
        if (tiles_in_direction >= index) {
            path->current_step = i;
            path->same_direction_count = index - 1;
            return;
        }
        index -= tiles_in_direction;
    }

    path->current_step = path->total_directions;
    path->same_direction_count = 0;
}


void figure_route_load_state(buffer *figures, buffer *buf_paths, int version)
{
    unsigned int elements_to_load;

    if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
        elements_to_load = (unsigned int) buf_paths->size / MAX_ORIGINAL_PATH_LENGTH;
    } else {
        elements_to_load = buffer_read_u32(buf_paths);
    }

    resize_paths_for_load(elements_to_load);

    for (unsigned int i = 0; i < elements_to_load; i++) {
        figure_path_data *path = path_slot(i);
        if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
            path->figure_id = buffer_read_i16(figures);
            if (path->figure_id) {
                uint8_t directions[MAX_ORIGINAL_PATH_LENGTH];
                buffer_read_raw(buf_paths, directions, MAX_ORIGINAL_PATH_LENGTH);

                if (!convert_old_directions_to_new_format(path, directions)) {
                    log_error("Unable to convert old routing path directions. The game will likely crash.", 0, 0);
                    return;
                }
            } else {
                buffer_skip(buf_paths, MAX_ORIGINAL_PATH_LENGTH);
            }
        } else {
            path->figure_id = buffer_read_u32(figures);
            path->total_directions = buffer_read_u32(buf_paths);
            if (path->figure_id) {
                path->directions = static_cast<uint8_t *>(malloc(path->total_directions * sizeof(uint8_t)));
                if (!path->directions) {
                    log_error("Unable to allocate memory for routing path directions. The game will likely crash.", 0, 0);
                    return;
                }
                buffer_read_raw(buf_paths, path->directions, path->total_directions);
            } else {
                buffer_skip(buf_paths, path->total_directions);
            }
        }
        if (path->figure_id) {
            update_current_tile(path);
        }
    }
    trim_inactive_paths();
}
