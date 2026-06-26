#include "route.h"

#include "building/building_record.h"
#include "building/roadblock.h"
#include "core/random.h"
#include "figure/PathingMode.h"
#include "figure/figure_runtime_api.h"
#include "game/save_version.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/road_access.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/terrain.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

namespace {

constexpr int kMaxOriginalPathLength = 500;
constexpr int kRouteDirectionBitOffset = 5;
constexpr uint8_t kRouteDirectionCountMask =
    static_cast<uint8_t>((1u << kRouteDirectionBitOffset) - 1);

struct BuiltPath {
    int tile_count = 0;
    std::vector<uint8_t> directions;

    explicit operator bool() const { return tile_count > 0 && !directions.empty(); }
};

struct FigureRoute {
    unsigned int id = 0;
    unsigned int figure_id = 0;
    std::vector<uint8_t> directions;
    size_t current_step = 0;
    uint8_t same_direction_count = 0;

    bool active() const { return figure_id != 0; }

    void reset(unsigned int new_id)
    {
        id = new_id;
        figure_id = 0;
        directions.clear();
        current_step = 0;
        same_direction_count = 0;
    }

    void release()
    {
        figure_id = 0;
        directions.clear();
        current_step = 0;
        same_direction_count = 0;
    }
};

} // namespace

namespace {

static std::vector<FigureRoute> paths;

static FigureRoute *path_slot(unsigned int id)
{
    return id < paths.size() ? &paths[id] : nullptr;
}

static FigureRoute *append_path(void)
{
    paths.emplace_back();
    paths.back().reset(static_cast<unsigned int>(paths.size() - 1));
    return &paths.back();
}

static FigureRoute *new_path_after_index(unsigned int start)
{
    while (start > paths.size()) {
        append_path();
    }
    for (unsigned int i = start; i < paths.size(); i++) {
        if (!paths[i].active()) {
            paths[i].reset(i);
            return &paths[i];
        }
    }
    return append_path();
}

static void trim_inactive_paths(void)
{
    while (paths.size() > 1 && !paths.back().active()) {
        paths.pop_back();
    }
}

static void resize_paths_for_load(unsigned int size)
{
    paths.clear();
    paths.resize(size);
    for (unsigned int i = 0; i < size; i++) {
        paths[i].reset(i);
    }
}

static void add_direction_to_path(
    std::vector<uint8_t> &directions,
    int &current_direction,
    uint8_t &same_direction_count,
    int direction)
{
    if (direction == current_direction && same_direction_count < kRouteDirectionCountMask) {
        directions.back()++;
        same_direction_count++;
        return;
    }

    directions.push_back(static_cast<uint8_t>(direction << kRouteDirectionBitOffset));
    current_direction = direction;
    same_direction_count = 0;
}

static int is_equal_distance_but_better_direction(
    int distance,
    int next_distance,
    int direction,
    int next_direction)
{
    if (next_distance != distance) {
        return 0;
    }
    if (direction == -1) {
        return 1;
    }
    return direction % 2 == 1 && next_direction % 2 == 0;
}

static int next_is_better(
    int base_distance,
    int distance,
    int next_distance,
    int direction,
    int next_direction,
    int is_highway,
    int next_is_highway)
{
    if (!is_highway && next_is_highway && next_distance < base_distance) {
        return 1;
    }
    if (is_highway && !next_is_highway) {
        return 0;
    }
    if (next_distance < distance) {
        return 1;
    }
    return is_equal_distance_but_better_direction(distance, next_distance, direction, next_direction);
}

static BuiltPath build_land_path(int dst_x, int dst_y, int num_directions)
{
    BuiltPath result;
    const int dst_grid_offset = map_grid_offset(dst_x, dst_y);
    int distance = map_routing_distance(dst_grid_offset);
    if (distance <= 0) {
        return result;
    }

    std::vector<uint8_t> reverse_directions;
    int current_direction = -1;
    uint8_t same_direction_count = 0;
    int last_direction = -1;
    int grid_offset = dst_grid_offset;
    const int step = num_directions == 8 ? 1 : 2;

    while (distance > 1) {
        const int base_distance = map_routing_distance(grid_offset);
        distance = base_distance;
        int direction = -1;
        int is_highway = 0;
        for (int next_direction = 0; next_direction < 8; next_direction += step) {
            if (next_direction == last_direction) {
                continue;
            }
            const int next_offset = grid_offset + map_grid_direction_delta(next_direction);
            const int next_distance = map_routing_distance(next_offset);
            const int next_is_highway = map_terrain_is(next_offset, TERRAIN_HIGHWAY);
            if (next_distance && next_is_better(
                base_distance,
                distance,
                next_distance,
                direction,
                next_direction,
                is_highway,
                next_is_highway)) {
                distance = next_distance;
                direction = next_direction;
                is_highway = next_is_highway;
            }
        }
        if (direction == -1) {
            return {};
        }
        grid_offset += map_grid_direction_delta(direction);
        const int forward_direction = (direction + 4) % 8;
        add_direction_to_path(reverse_directions, current_direction, same_direction_count, forward_direction);
        last_direction = forward_direction;
        result.tile_count++;
    }

    result.directions.assign(reverse_directions.rbegin(), reverse_directions.rend());
    return result;
}

static BuiltPath build_water_path(int dst_x, int dst_y, bool is_flotsam)
{
    BuiltPath result;
    const int rand = random_byte() & 3;
    const int dst_grid_offset = map_grid_offset(dst_x, dst_y);
    int distance = map_routing_distance(dst_grid_offset);
    if (distance <= 0) {
        return result;
    }

    std::vector<uint8_t> reverse_directions;
    int current_direction = -1;
    uint8_t same_direction_count = 0;
    int last_direction = -1;
    int grid_offset = dst_grid_offset;

    while (distance > 1) {
        int current_rand = rand;
        distance = map_routing_distance(grid_offset);
        if (is_flotsam) {
            current_rand = map_random_get(grid_offset) & 3;
        }
        int direction = -1;
        for (int next_direction = 0; next_direction < 8; next_direction++) {
            if (next_direction == last_direction) {
                continue;
            }
            const int next_offset = grid_offset + map_grid_direction_delta(next_direction);
            const int next_distance = map_routing_distance(next_offset);
            if (!next_distance) {
                continue;
            }
            if (next_distance < distance || (next_distance == distance && rand == current_rand)) {
                distance = next_distance;
                direction = next_direction;
            }
        }
        if (direction == -1) {
            return {};
        }
        grid_offset += map_grid_direction_delta(direction);
        const int forward_direction = (direction + 4) % 8;
        add_direction_to_path(reverse_directions, current_direction, same_direction_count, forward_direction);
        last_direction = forward_direction;
        result.tile_count++;
    }

    result.directions.assign(reverse_directions.rbegin(), reverse_directions.rend());
    return result;
}

static Route::RoadResult road_result(int grid_offset, int distance)
{
    return {
        distance,
        grid_offset,
        { map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset) }
    };
}

static Route::RoadResult road_result(const road_access_candidate &candidate, int distance)
{
    return {
        distance,
        candidate.grid_offset,
        candidate.road
    };
}

} // namespace

Route::DistanceQuery::DistanceQuery(
    const map_point &source,
    int sourceNetwork,
    std::optional<roadblock_permission> permission,
    bool valid)
    : source_(source),
      sourceNetwork_(sourceNetwork),
      permission_(permission),
      valid_(valid)
{
}

Route::DistanceQuery Route::DistanceQuery::fromRoad(
    const map_point &road,
    std::optional<roadblock_permission> permission)
{
    const int source_offset = map_grid_offset(road.x, road.y);
    if (!map_grid_is_valid_offset(source_offset) ||
        !figure_type_registry_impl::PathingMode::citizenIsRoad(source_offset)) {
        return DistanceQuery({ 0, 0 }, 0, std::nullopt, false);
    }

    return DistanceQuery(road, map_road_network_get(source_offset), permission, true);
}

Route::DistanceQuery Route::DistanceQuery::fromPoint(
    const map_point &point,
    std::optional<roadblock_permission> permission)
{
    const int source_offset = map_grid_offset(point.x, point.y);
    if (!map_grid_is_valid_offset(source_offset)) {
        return DistanceQuery({ 0, 0 }, 0, std::nullopt, false);
    }

    const int source_network = figure_type_registry_impl::PathingMode::citizenIsRoad(source_offset) ?
        map_road_network_get(source_offset) :
        0;
    return DistanceQuery(point, source_network, permission, true);
}

Route::DistanceQuery Route::DistanceQuery::fromFigure(const Figure &figure)
{
    return fromRoad({ figure.x, figure.y }, Roadblock::permission_for(figure));
}

void Route::DistanceQuery::seedDistanceGrid() const
{
    if (distanceGridGeneration_ == map_routing_distance_generation()) {
        return;
    }
    if (permission_) {
        map_routing_calculate_distances_road_garden(source_.x, source_.y, *permission_);
    } else {
        map_routing_calculate_distances(source_.x, source_.y);
    }
    distanceGridGeneration_ = map_routing_distance_generation();
}

int Route::DistanceQuery::distanceFor(const RoadResult &candidate, int maxDistance) const
{
    if (candidate.distance <= 0 || (maxDistance > 0 && candidate.distance > maxDistance)) {
        return 0;
    }
    return candidate.distance;
}

Route::RoadResult Route::DistanceQuery::bestRoadToLargestNetwork(
    const std::vector<road_access_area> &areas,
    bool fallback_to_shortest_distance) const
{
    if (!valid_) {
        return {};
    }
    seedDistanceGrid();

    const std::vector<road_access_candidate> candidates = map_road_access_candidates(areas);
    Route::RoadResult best;
    int best_network_index = std::numeric_limits<int>::max();
    for (const road_access_candidate &candidate : candidates) {
        const int distance = map_routing_distance(candidate.grid_offset);
        if (distance <= 0 || candidate.network_index >= best_network_index) {
            continue;
        }

        const int reachable_distance = distanceFor(road_result(candidate, distance), 0);
        if (reachable_distance > 0) {
            best_network_index = candidate.network_index;
            best = road_result(candidate, reachable_distance);
        }
    }
    if (best || !fallback_to_shortest_distance) {
        return best;
    }

    int best_distance = std::numeric_limits<int>::max();
    for (const road_access_candidate &candidate : candidates) {
        const int distance = map_routing_distance(candidate.grid_offset);
        if (distance <= 0 || distance >= best_distance) {
            continue;
        }

        const int reachable_distance = distanceFor(road_result(candidate, distance), 0);
        if (reachable_distance > 0 && reachable_distance < best_distance) {
            best_distance = reachable_distance;
            best = road_result(candidate, reachable_distance);
        }
    }
    return best;
}

int Route::DistanceQuery::distanceTo(int gridOffset, int maxDistance) const
{
    if (!valid_ || !map_grid_is_valid_offset(gridOffset)) {
        return 0;
    }
    seedDistanceGrid();

    const int distance = map_routing_distance(gridOffset);
    if (distance <= 0 || (maxDistance > 0 && distance > maxDistance)) {
        return 0;
    }
    return distanceFor(road_result(gridOffset, distance), maxDistance);
}

Route::RoadResult Route::DistanceQuery::findRoad(const map_point &road, int maxDistance) const
{
    if (!valid_) {
        return {};
    }
    seedDistanceGrid();

    const int grid_offset = map_grid_offset(road.x, road.y);
    if (!map_grid_is_valid_offset(grid_offset)) {
        return {};
    }

    const int distance = map_routing_distance(grid_offset);
    if (distance <= 0 || (maxDistance > 0 && distance > maxDistance)) {
        return {};
    }

    const int reachable_distance = distanceFor(road_result(grid_offset, distance), maxDistance);
    return reachable_distance > 0 ? road_result(grid_offset, reachable_distance) : RoadResult{};
}

Route::RoadResult Route::DistanceQuery::findReachableRoad(
    int x,
    int y,
    int size,
    int radius,
    int maxDistance) const
{
    return findReachableAreaTile(x, y, size, radius, maxDistance, true);
}

Route::RoadResult Route::DistanceQuery::findReachableTile(
    int x,
    int y,
    int size,
    int radius,
    int maxDistance) const
{
    return findReachableAreaTile(x, y, size, radius, maxDistance, false);
}

Route::RoadResult Route::DistanceQuery::findReachableAreaTile(
    int x,
    int y,
    int size,
    int radius,
    int maxDistance,
    bool requireRoad) const
{
    if (!valid_ || radius <= 0) {
        return {};
    }
    seedDistanceGrid();

    int best_distance = std::numeric_limits<int>::max();
    int best_grid_offset = 0;
    for (int r = 1; r <= radius; r++) {
        int x_min = 0;
        int y_min = 0;
        int x_max = 0;
        int y_max = 0;
        map_grid_get_area(x, y, size, r, &x_min, &y_min, &x_max, &y_max);

        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                const int grid_offset = map_grid_offset(xx, yy);
                if (requireRoad && !map_terrain_is(grid_offset, TERRAIN_ROAD)) {
                    continue;
                }

                const int distance = map_routing_distance(grid_offset);
                if (distance <= 0 || distance >= best_distance ||
                    (maxDistance > 0 && distance > maxDistance)) {
                    continue;
                }

                const int reachable_distance =
                    distanceFor(road_result(grid_offset, distance), maxDistance);
                if (reachable_distance > 0 && reachable_distance < best_distance) {
                    best_distance = reachable_distance;
                    best_grid_offset = grid_offset;
                }
            }
        }

        if (best_grid_offset) {
            return road_result(best_grid_offset, best_distance);
        }
    }
    return {};
}

Route::RoadResult Route::DistanceQuery::findRoadToLargestNetwork(int x, int y, int size) const
{
    if (size <= 0) {
        return {};
    }
    return bestRoadToLargestNetwork({ { { x, y }, size } }, true);
}

Route::RoadResult Route::DistanceQuery::findHippodromeRoadToLargestNetwork(
    int x,
    int y,
    bool rotated) const
{
    return bestRoadToLargestNetwork(map_road_access_hippodrome_areas(x, y, rotated ? 1 : 0), true);
}

Route::RoadResult Route::DistanceQuery::findMonumentConstructionRoadToLargestNetwork(
    int x,
    int y,
    int size) const
{
    return bestRoadToLargestNetwork(map_road_access_monument_construction_areas(x, y, size), size < 3);
}

Route::RoadResult Route::DistanceQuery::findAccessRoad(
    const building &target,
    int radius,
    int maxDistance,
    bool requireSameNetwork) const
{
    if (!valid_ || !target.id || radius <= 0) {
        return {};
    }
    seedDistanceGrid();

    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(target.x, target.y, target.size, radius, &x_min, &y_min, &x_max, &y_max);

    RoadResult best_road;
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            const int grid_offset = map_grid_offset(x, y);
            if (!map_terrain_is(grid_offset, TERRAIN_ROAD)) {
                continue;
            }
            if (requireSameNetwork &&
                sourceNetwork_ > 0 &&
                map_road_network_get(grid_offset) != sourceNetwork_) {
                continue;
            }

            const int distance = map_routing_distance(grid_offset);
            if (distance <= 0 || (maxDistance > 0 && distance > maxDistance)) {
                continue;
            }

            const int reachable_distance = distanceFor(road_result(grid_offset, distance), maxDistance);
            if (reachable_distance > 0 && (!best_road || reachable_distance < best_road.distance)) {
                best_road = road_result(grid_offset, reachable_distance);
            }
        }
    }
    return best_road;
}

Route::TerrainQuery Route::TerrainQuery::enemyLandFrom(
    const map_point &source,
    int maxTiles,
    int onlyThroughBuildingId,
    int directions)
{
    const int source_offset = map_grid_offset(source.x, source.y);
    if (!map_grid_is_valid_offset(source_offset)) {
        return TerrainQuery(false);
    }
    map_routing_noncitizen_can_travel_over_land(
        source.x,
        source.y,
        -1,
        -1,
        directions,
        onlyThroughBuildingId,
        maxTiles);
    return TerrainQuery(true);
}

int Route::TerrainQuery::distanceTo(int gridOffset) const
{
    if (!valid_ || !map_grid_is_valid_offset(gridOffset)) {
        return 0;
    }
    return map_routing_distance(gridOffset);
}

void Route::clearAll()
{
    paths.clear();
}

void Route::clean()
{
    for (unsigned int i = 0; i < paths.size(); i++) {
        FigureRoute &path = paths[i];
        unsigned int figure_id = path.figure_id;
        if (figure_id > 0 && figure_id < Figure::count()) {
            const Figure *f = Figure::get(figure_id);
            if (f->state != FIGURE_STATE_ALIVE || f->routing_path_id != i) {
                path.release();
            }
        }
    }
    trim_inactive_paths();
}

void Route::add(Figure &figure)
{
    figure.routing_path_id = 0;
    figure.routing_path_current_tile = 0;
    figure.routing_path_length = 0;
    int direction_limit = 8;
    if (figure.disallow_diagonal) {
        direction_limit = 4;
    }
    FigureRoute *path = new_path_after_index(1);
    if (!path) {
        return;
    }
    BuiltPath built_path;
    if (figure.is_boat) {
        if (figure.is_boat == 2) {
            map_routing_calculate_distances_water_flotsam(figure.x, figure.y);
            built_path = build_water_path(figure.destination_x, figure.destination_y, true);
        } else {
            map_routing_calculate_distances_water_boat(figure.x, figure.y);
            built_path = build_water_path(figure.destination_x, figure.destination_y, false);
        }
    } else {
        const figure_type_registry_impl::PathingPolicy *policy = figure_runtime_pathing_policy(&figure);
        const figure_type_registry_impl::PathingMode *mode = policy && policy->mode ?
            policy->mode :
            &figure_type_registry_impl::VanillaRoaming;
        const figure_type_registry_impl::PathingMode::TerrainAccess terrain = policy ?
            policy->terrain :
            figure_type_registry_impl::PathingMode::terrainFromLegacyUsage(figure.terrain_usage);
        if (mode->canTravel(figure, direction_limit, terrain)) {
            const int preferred_direction_limit = mode->pathDirectionLimit(direction_limit, terrain);
            built_path = build_land_path(figure.destination_x, figure.destination_y, preferred_direction_limit);
            if (!built_path && preferred_direction_limit != direction_limit) {
                built_path = build_land_path(figure.destination_x, figure.destination_y, direction_limit);
            }
        }
    }
    if (built_path) {
        path->figure_id = figure.id();
        path->directions = std::move(built_path.directions);
        figure.routing_path_id = path->id;
        figure.routing_path_length = built_path.tile_count;
    } else {
        path->release();
        trim_inactive_paths();
    }
}

void Route::remove(Figure &figure)
{
    if (figure.routing_path_id > 0) {
        FigureRoute *path = path_slot(figure.routing_path_id);
        if (path && path->figure_id == figure.id()) {
            path->release();
        }
        figure.routing_path_id = 0;
    }
    trim_inactive_paths();
}

int Route::currentDirection(const Figure &figure)
{
    FigureRoute *path = path_slot(figure.routing_path_id);
    if (!path) {
        return 8;
    }
    if (path->current_step >= path->directions.size()) {
        return 8;
    }
    return path->directions[path->current_step] >> kRouteDirectionBitOffset;
}

void Route::advanceTile(Figure &figure)
{
    FigureRoute *path = path_slot(figure.routing_path_id);
    if (!path) {
        return;
    }

    if (path->current_step >= path->directions.size()) {
        return;
    }

    int tiles_in_direction = (path->directions[path->current_step] & kRouteDirectionCountMask) + 1;

    path->same_direction_count++;
    if (path->same_direction_count >= tiles_in_direction) {
        path->current_step++;
        path->same_direction_count = 0;
    }
}

int Route::waterPathLength(const map_point &source, const map_point &destination, bool flotsam)
{
    if (flotsam) {
        map_routing_calculate_distances_water_flotsam(source.x, source.y);
    } else {
        map_routing_calculate_distances_water_boat(source.x, source.y);
    }
    return build_water_path(destination.x, destination.y, flotsam).tile_count;
}

void Route::saveState(buffer *figures, buffer *buf_paths)
{
    unsigned int size = static_cast<unsigned int>(paths.size() * sizeof(uint32_t));
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(size));
    buffer_init(figures, buf_data, size);

    unsigned int paths_memory_size = 0;

    for (const FigureRoute &path : paths) {
        paths_memory_size += sizeof(uint32_t) + static_cast<unsigned int>(path.directions.size());
    }

    size = sizeof(uint32_t) + paths_memory_size;
    buf_data = static_cast<uint8_t *>(malloc(size));
    buffer_init(buf_paths, buf_data, size);
    buffer_write_u32(buf_paths, static_cast<uint32_t>(paths.size()));

    for (const FigureRoute &path : paths) {
        buffer_write_u32(figures, path.figure_id);
        buffer_write_u32(buf_paths, static_cast<uint32_t>(path.directions.size()));
        if (!path.directions.empty()) {
            buffer_write_raw(buf_paths, path.directions.data(), path.directions.size());
        }
    }
}

static void convert_old_directions_to_new_format(FigureRoute &path, const uint8_t *directions)
{
    Figure *f = Figure::get(path.figure_id);

    if (!f || f->id() == 0 || f->routing_path_length == 0) {
        return;
    }

    int current_direction = -1;
    uint8_t current_count = 0;
    path.directions.clear();

    for (unsigned int index = 0; index < f->routing_path_length; index++) {
        if (directions[index] != current_direction || current_count == kRouteDirectionCountMask) {
            path.directions.push_back(static_cast<uint8_t>(directions[index] << kRouteDirectionBitOffset));
            current_direction = directions[index];
            current_count = 0;
        } else {
            path.directions.back()++;
            current_count++;
        }
    }
}

static void update_current_tile(FigureRoute &path)
{
    Figure *f = Figure::get(path.figure_id);

    if (!f || f->id() == 0 || f->routing_path_length == 0) {
        return;
    }

    unsigned int index = f->routing_path_current_tile + 1;

    for (size_t i = 0; i < path.directions.size(); i++) {
        unsigned int tiles_in_direction = (path.directions[i] & kRouteDirectionCountMask) + 1;
        if (tiles_in_direction >= index) {
            path.current_step = i;
            path.same_direction_count = static_cast<uint8_t>(index - 1);
            return;
        }
        index -= tiles_in_direction;
    }

    path.current_step = path.directions.size();
    path.same_direction_count = 0;
}


void Route::loadState(buffer *figures, buffer *buf_paths, int version)
{
    unsigned int elements_to_load;

    if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
        elements_to_load = static_cast<unsigned int>(buf_paths->size / kMaxOriginalPathLength);
    } else {
        elements_to_load = buffer_read_u32(buf_paths);
    }

    resize_paths_for_load(elements_to_load);

    for (unsigned int i = 0; i < elements_to_load; i++) {
        FigureRoute *path = path_slot(i);
        if (!path) {
            return;
        }
        if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
            path->figure_id = buffer_read_i16(figures);
            if (path->figure_id) {
                std::array<uint8_t, kMaxOriginalPathLength> directions = {};
                buffer_read_raw(buf_paths, directions.data(), directions.size());
                convert_old_directions_to_new_format(*path, directions.data());
            } else {
                buffer_skip(buf_paths, kMaxOriginalPathLength);
            }
        } else {
            path->figure_id = buffer_read_u32(figures);
            const uint32_t direction_count = buffer_read_u32(buf_paths);
            if (path->figure_id) {
                path->directions.resize(direction_count);
                if (!path->directions.empty()) {
                    buffer_read_raw(buf_paths, path->directions.data(), path->directions.size());
                }
            } else {
                buffer_skip(buf_paths, direction_count);
            }
        }
        if (path->figure_id) {
            update_current_tile(*path);
        }
    }
    trim_inactive_paths();
}
