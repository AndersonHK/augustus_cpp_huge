#include "route.h"

#include "building/building_record.h"
#include "building/roadblock.h"
#include "core/random.h"
#include "figure/PathingMode.h"
#include "figure/figure_runtime_api.h"
#include "game/save_version.h"
#include "game/performance_tracker.h"
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

static roadblock_permission route_permission(const Route::Request &request)
{
    return request.permission.value_or(PERMISSION_NONE);
}

static int route_direction_limit(const Route::Request &request)
{
    return request.direction_limit > 0 ? request.direction_limit : 4;
}

static int route_destination_offset(const Route::Request &request)
{
    return map_grid_offset(request.destination.x, request.destination.y);
}

static void append_route_points(
    std::vector<map_point> &path,
    const map_point &source,
    const std::vector<uint8_t> &directions)
{
    int grid_offset = map_grid_offset(source.x, source.y);
    path.push_back(source);
    for (uint8_t packed_direction : directions) {
        const int direction = packed_direction >> kRouteDirectionBitOffset;
        const int tile_count = (packed_direction & kRouteDirectionCountMask) + 1;
        for (int i = 0; i < tile_count; i++) {
            grid_offset += map_grid_direction_delta(direction);
            path.push_back({
                map_grid_offset_to_x(grid_offset),
                map_grid_offset_to_y(grid_offset)
            });
        }
    }
}

static Route::Result route_result_from_built_path(
    const Route::Request &request,
    const BuiltPath &built_path)
{
    if (!built_path) {
        return {};
    }

    Route::Result result;
    result.distance = map_routing_distance(route_destination_offset(request));
    result.distance_generation = map_routing_distance_generation();
    result.reached = result.distance > 0;
    if (!result.reached) {
        return {};
    }

    result.path.reserve(static_cast<size_t>(built_path.tile_count) + 1);
    append_route_points(result.path, request.source, built_path.directions);
    return result;
}

static routed_building_type routed_building_type_for_surface(Route::Surface surface)
{
    switch (surface) {
        case Route::Surface::ConstructionHighway:
            return ROUTED_BUILDING_HIGHWAY;
        case Route::Surface::ConstructionWall:
            return ROUTED_BUILDING_WALL;
        case Route::Surface::ConstructionAqueduct:
            return ROUTED_BUILDING_AQUEDUCT;
        case Route::Surface::ConstructionRoad:
        default:
            return ROUTED_BUILDING_ROAD;
    }
}

static int can_route_over_surface(const Route::Request &request)
{
    const int direction_limit = route_direction_limit(request);
    switch (request.surface) {
        case Route::Surface::CitizenRoadGarden:
            return map_routing_citizen_can_travel_over_road_garden(
                request.source.x,
                request.source.y,
                request.destination.x,
                request.destination.y,
                direction_limit,
                route_permission(request));
        case Route::Surface::CitizenRoadGardenHighway:
            return map_routing_citizen_can_travel_over_road_garden_highway(
                request.source.x,
                request.source.y,
                request.destination.x,
                request.destination.y,
                direction_limit,
                route_permission(request));
        case Route::Surface::NonCitizenLand:
            return map_routing_noncitizen_can_travel_over_land(
                request.source.x,
                request.source.y,
                request.destination.x,
                request.destination.y,
                direction_limit,
                request.only_through_building_id,
                request.max_tiles);
        case Route::Surface::Walls:
            return map_routing_can_travel_over_walls(
                request.source.x,
                request.source.y,
                request.destination.x,
                request.destination.y,
                4);
        case Route::Surface::ConstructionRoad:
        case Route::Surface::ConstructionHighway:
        case Route::Surface::ConstructionWall:
        case Route::Surface::ConstructionAqueduct:
            return map_routing_calculate_distances_for_building(
                routed_building_type_for_surface(request.surface),
                request.source.x,
                request.source.y);
        case Route::Surface::CitizenLand:
        default:
            return map_routing_citizen_can_travel_over_land(
                request.source.x,
                request.source.y,
                request.destination.x,
                request.destination.y,
                direction_limit,
                route_permission(request));
    }
}

static int path_direction_limit_for_surface(const Route::Request &request)
{
    switch (request.surface) {
        case Route::Surface::Walls:
        case Route::Surface::ConstructionRoad:
        case Route::Surface::ConstructionHighway:
        case Route::Surface::ConstructionWall:
        case Route::Surface::ConstructionAqueduct:
            return 4;
        default:
            return route_direction_limit(request);
    }
}

static int route_request_has_valid_endpoints(const Route::Request &request)
{
    return request.has_destination &&
        map_grid_is_valid_offset(map_grid_offset(request.source.x, request.source.y)) &&
        map_grid_is_valid_offset(route_destination_offset(request));
}

static int road_network_at(const map_point &point)
{
    const int grid_offset = map_grid_offset(point.x, point.y);
    if (!map_grid_is_valid_offset(grid_offset) ||
        !figure_type_registry_impl::PathingMode::citizenIsRoadLike(grid_offset)) {
        return 0;
    }
    return map_road_network_get(grid_offset);
}

static bool terrain_is_road_like(int grid_offset)
{
    return figure_type_registry_impl::PathingMode::citizenIsRoadLike(grid_offset);
}

static bool route_request_pruned_by_network(const Route::Request &request)
{
    if (!request.require_same_road_network) {
        return false;
    }

    const int source_network = road_network_at(request.source);
    const int destination_network = road_network_at(request.destination);
    if (source_network <= 0 || destination_network <= 0 || source_network == destination_network) {
        return false;
    }

    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
        request.purpose,
        1);
    return true;
}

static bool route_destination_within_max_tiles(const Route::Request &request)
{
    if (request.max_tiles <= 0) {
        return true;
    }

    const int destination_distance = map_routing_distance(route_destination_offset(request));
    return destination_distance > 0 && destination_distance <= request.max_tiles;
}

static bool can_reach_with_road_garden_distance_field(const Route::Request &request)
{
    const int source_offset = map_grid_offset(request.source.x, request.source.y);
    if (!map_grid_is_valid_offset(source_offset) ||
        !figure_type_registry_impl::PathingMode::citizenIsRoadLike(source_offset)) {
        return false;
    }

    map_routing_calculate_distances_road_garden(
        request.source.x,
        request.source.y,
        route_permission(request));
    return route_destination_within_max_tiles(request);
}

static bool request_needs_bounded_road_garden_field(const Route::Request &request)
{
    return request.max_tiles > 0 &&
        request.surface == Route::Surface::CitizenRoadGarden &&
        route_direction_limit(request) == 4;
}

static BuiltPath build_seeded_land_path(
    const Route::Request &request,
    int path_direction_limit,
    int fallback_direction_limit = 0)
{
    BuiltPath built_path = build_land_path(
        request.destination.x,
        request.destination.y,
        path_direction_limit);
    if (!built_path &&
        fallback_direction_limit > 0 &&
        fallback_direction_limit != path_direction_limit) {
        built_path = build_land_path(
            request.destination.x,
            request.destination.y,
            fallback_direction_limit);
    }
    return built_path;
}

static BuiltPath build_route_path(
    const Route::Request &request,
    int path_direction_limit,
    int fallback_direction_limit = 0)
{
    switch (request.surface) {
        case Route::Surface::WaterBoat:
            map_routing_calculate_distances_water_boat(request.source.x, request.source.y);
            return build_water_path(request.destination.x, request.destination.y, false);
        case Route::Surface::WaterFlotsam:
            map_routing_calculate_distances_water_flotsam(request.source.x, request.source.y);
            return build_water_path(request.destination.x, request.destination.y, true);
        default:
            if (!can_route_over_surface(request)) {
                return {};
            }
            return build_seeded_land_path(request, path_direction_limit, fallback_direction_limit);
    }
}

static BuiltPath build_route_path(const Route::Request &request)
{
    return build_route_path(request, path_direction_limit_for_surface(request));
}

static Route::Surface route_surface_for_terrain(
    const figure_type_registry_impl::PathingMode::TerrainAccess &terrain)
{
    if (terrain.wall_grid) {
        return Route::Surface::Walls;
    }
    if (terrain.enemy_land || terrain.animal_land) {
        return Route::Surface::NonCitizenLand;
    }
    if (terrain.requires_roads || terrain.prefers_roads) {
        return terrain.allows_highways ?
            Route::Surface::CitizenRoadGardenHighway :
            Route::Surface::CitizenRoadGarden;
    }
    return Route::Surface::CitizenLand;
}

static int route_path_direction_limit(
    int default_direction_limit,
    const figure_type_registry_impl::PathingMode::TerrainAccess &terrain)
{
    return terrain.wall_grid ? 4 : default_direction_limit;
}

static BuiltPath build_enemy_land_route(
    Route::Request request,
    int path_direction_limit,
    int fallback_direction_limit)
{
    request.surface = Route::Surface::NonCitizenLand;
    request.max_tiles = 5000;
    if (can_route_over_surface(request)) {
        return build_seeded_land_path(request, path_direction_limit, fallback_direction_limit);
    }

    request.only_through_building_id = 0;
    request.max_tiles = 25000;
    if (can_route_over_surface(request)) {
        return build_seeded_land_path(request, path_direction_limit, fallback_direction_limit);
    }

    if (!map_routing_noncitizen_can_travel_through_everything(
            request.source.x,
            request.source.y,
            request.destination.x,
            request.destination.y,
            route_direction_limit(request))) {
        return {};
    }
    return build_seeded_land_path(request, path_direction_limit, fallback_direction_limit);
}

static BuiltPath build_figure_route_path(
    const Figure &figure,
    const figure_type_registry_impl::PathingMode::TerrainAccess &terrain,
    int path_direction_limit,
    performance_tracker_route_purpose purpose)
{
    Route::Request request = Route::Request::fromFigure(
        figure,
        route_surface_for_terrain(terrain),
        purpose);
    const int direction_limit = route_direction_limit(request);

    if (terrain.enemy_land) {
        return build_enemy_land_route(request, path_direction_limit, direction_limit);
    }

    if (terrain.animal_land) {
        request.only_through_building_id = -1;
        request.max_tiles = 5000;
        return build_route_path(request, path_direction_limit, direction_limit);
    }

    if (terrain.requires_roads || terrain.prefers_roads) {
        if (can_route_over_surface(request)) {
            return build_seeded_land_path(request, path_direction_limit, direction_limit);
        }
        if (terrain.requires_roads) {
            return {};
        }
        request.surface = Route::Surface::CitizenLand;
    }

    return build_route_path(request, path_direction_limit, direction_limit);
}

class AnyRoadAccessCandidateVisitor final : public RoadAccessCandidateVisitor {
public:
    void visit(const road_access_candidate &) override { found_ = true; }
    bool found() const { return found_; }

private:
    bool found_ = false;
};

class BestRouteRoadAccessVisitor final : public RoadAccessCandidateVisitor {
public:
    void visit(const road_access_candidate &candidate) override
    {
        const int distance = map_routing_distance(candidate.grid_offset);
        if (distance <= 0) {
            return;
        }

        if (candidate.network_index < best_network_index_) {
            best_network_index_ = candidate.network_index;
            best_network_road_ = road_result(candidate, distance);
        }
        if (distance < shortest_distance_) {
            shortest_distance_ = distance;
            shortest_road_ = road_result(candidate, distance);
        }
    }

    Route::RoadResult bestNetworkRoad() const { return best_network_road_; }
    Route::RoadResult shortestRoad() const { return shortest_road_; }

private:
    int best_network_index_ = std::numeric_limits<int>::max();
    int shortest_distance_ = std::numeric_limits<int>::max();
    Route::RoadResult best_network_road_;
    Route::RoadResult shortest_road_;
};

template <typename VisitCandidates, typename SeedDistanceGrid>
static Route::RoadResult best_road_to_largest_network(
    const VisitCandidates &visit_candidates,
    const SeedDistanceGrid &seed_distance_grid,
    performance_tracker_route_purpose purpose,
    bool fallback_to_shortest_distance)
{
    AnyRoadAccessCandidateVisitor any_candidate;
    visit_candidates(any_candidate);
    if (!any_candidate.found()) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
            purpose,
            1);
        return {};
    }

    seed_distance_grid();

    BestRouteRoadAccessVisitor best_candidate;
    visit_candidates(best_candidate);
    if (best_candidate.bestNetworkRoad() || !fallback_to_shortest_distance) {
        return best_candidate.bestNetworkRoad();
    }
    return best_candidate.shortestRoad();
}

class SameNetworkAccessRoadVisitor final {
public:
    explicit SameNetworkAccessRoadVisitor(int sourceNetwork) : sourceNetwork_(sourceNetwork) {}

    void visit(int grid_offset)
    {
        if (found_ ||
            !terrain_is_road_like(grid_offset) ||
            map_road_network_get(grid_offset) != sourceNetwork_) {
            return;
        }
        found_ = true;
    }

    bool found() const { return found_; }
    bool done() const { return found_; }

private:
    int sourceNetwork_ = 0;
    bool found_ = false;
};

class BestAccessRoadVisitor final {
public:
    BestAccessRoadVisitor(bool requireSameNetwork, int sourceNetwork, int maxDistance)
        : requireSameNetwork_(requireSameNetwork),
          sourceNetwork_(sourceNetwork),
          maxDistance_(maxDistance)
    {
    }

    void visit(int grid_offset)
    {
        if (!terrain_is_road_like(grid_offset)) {
            return;
        }
        if (requireSameNetwork_ &&
            sourceNetwork_ > 0 &&
            map_road_network_get(grid_offset) != sourceNetwork_) {
            return;
        }

        const int distance = map_routing_distance(grid_offset);
        if (distance <= 0 || (maxDistance_ > 0 && distance > maxDistance_)) {
            return;
        }
        if (!best_road_ || distance < best_road_.distance) {
            best_road_ = road_result(grid_offset, distance);
        }
    }

    Route::RoadResult bestRoad() const { return best_road_; }
    bool done() const { return false; }

private:
    bool requireSameNetwork_ = false;
    int sourceNetwork_ = 0;
    int maxDistance_ = 0;
    Route::RoadResult best_road_;
};

template <typename Visitor>
static void visit_access_road_area_tiles(
    int x_min,
    int y_min,
    int x_max,
    int y_max,
    Visitor &visitor)
{
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            visitor.visit(map_grid_offset(x, y));
            if (visitor.done()) {
                return;
            }
        }
    }
}

} // namespace

Route::Request Route::Request::between(
    const map_point &source,
    const map_point &destination,
    Surface surface,
    performance_tracker_route_purpose purpose)
{
    Request request;
    request.source = source;
    request.destination = destination;
    request.surface = surface;
    request.purpose = purpose;
    request.has_destination = true;
    return request;
}

Route::Request Route::Request::fromFigure(
    const Figure &figure,
    Surface surface,
    performance_tracker_route_purpose purpose)
{
    Request request = between(
        { figure.x, figure.y },
        { figure.destination_x, figure.destination_y },
        surface,
        purpose);
    request.figure = &figure;
    request.owner = figure.building.record();
    request.permission = Roadblock::permission_for(figure);
    request.direction_limit = figure.disallow_diagonal ? 4 : 8;
    request.max_tiles = figure.max_roam_length > 0 ? figure.max_roam_length : 0;
    request.only_through_building_id = figure.destination_building.id();
    return request;
}

Route::DistanceQuery Route::Planner::distancesFrom(const Request &request)
{
    switch (request.surface) {
        case Route::Surface::CitizenRoadGarden:
        case Route::Surface::CitizenRoadGardenHighway:
            return DistanceQuery::fromPoint(request.source, route_permission(request), request.purpose);
        default:
            return DistanceQuery::fromPoint(request.source, std::nullopt, request.purpose);
    }
}

bool Route::Planner::canReach(const Request &request)
{
    PerformanceTrackerRouteScope route_scope(request.purpose);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        request.purpose,
        1);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PLANS,
        request.purpose,
        1);

    if (!route_request_has_valid_endpoints(request)) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            request.purpose,
            1);
        return false;
    }
    if (route_request_pruned_by_network(request)) {
        return false;
    }
    const bool reached = request_needs_bounded_road_garden_field(request) ?
        can_reach_with_road_garden_distance_field(request) :
        can_route_over_surface(request) && route_destination_within_max_tiles(request);
    if (!reached) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            request.purpose,
            1);
        return false;
    }
    return true;
}

Route::Result Route::Planner::route(const Request &request)
{
    PerformanceTrackerRouteScope route_scope(request.purpose);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        request.purpose,
        1);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PLANS,
        request.purpose,
        1);

    if (!route_request_has_valid_endpoints(request)) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            request.purpose,
            1);
        return {};
    }
    if (route_request_pruned_by_network(request)) {
        return {};
    }

    BuiltPath built_path = build_route_path(request);
    Route::Result result = route_result_from_built_path(request, built_path);
    if (!result) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            request.purpose,
            1);
    }
    return result;
}

Route::DistanceQuery::DistanceQuery(
    const map_point &source,
    int sourceNetwork,
    std::optional<roadblock_permission> permission,
    performance_tracker_route_purpose purpose,
    bool valid)
    : source_(source),
      sourceNetwork_(sourceNetwork),
      permission_(permission),
      purpose_(purpose),
      valid_(valid)
{
}

Route::DistanceQuery Route::DistanceQuery::fromRoad(
    const map_point &road,
    std::optional<roadblock_permission> permission,
    performance_tracker_route_purpose purpose)
{
    const int source_offset = map_grid_offset(road.x, road.y);
    if (!map_grid_is_valid_offset(source_offset) ||
        !figure_type_registry_impl::PathingMode::citizenIsRoadLike(source_offset)) {
        return DistanceQuery({ 0, 0 }, 0, std::nullopt, purpose, false);
    }

    return DistanceQuery(
        road,
        map_road_network_get(source_offset),
        permission,
        purpose,
        true);
}

Route::DistanceQuery Route::DistanceQuery::fromPoint(
    const map_point &point,
    std::optional<roadblock_permission> permission,
    performance_tracker_route_purpose purpose)
{
    const int source_offset = map_grid_offset(point.x, point.y);
    if (!map_grid_is_valid_offset(source_offset)) {
        return DistanceQuery({ 0, 0 }, 0, std::nullopt, purpose, false);
    }

    const int source_network = figure_type_registry_impl::PathingMode::citizenIsRoadLike(source_offset) ?
        map_road_network_get(source_offset) :
        0;
    return DistanceQuery(point, source_network, permission, purpose, true);
}

Route::DistanceQuery Route::DistanceQuery::fromFigure(
    const Figure &figure,
    performance_tracker_route_purpose purpose)
{
    return fromRoad({ figure.x, figure.y }, Roadblock::permission_for(figure), purpose);
}

void Route::DistanceQuery::seedDistanceGrid() const
{
    PerformanceTrackerRouteScope route_scope(purpose_);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        purpose_,
        1);
    if (distanceGridGeneration_ == map_routing_distance_generation()) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_CACHE_HITS,
            purpose_,
            1);
        return;
    }
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_CACHE_MISSES,
        purpose_,
        1);
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
                if (requireRoad && !terrain_is_road_like(grid_offset)) {
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
    if (!valid_ || size <= 0) {
        return {};
    }
    return best_road_to_largest_network(
        [x, y, size](RoadAccessCandidateVisitor &visitor) {
            map_road_access_visit_candidates(x, y, size, visitor);
        },
        [this]() { seedDistanceGrid(); },
        purpose_,
        true);
}

Route::RoadResult Route::DistanceQuery::findHippodromeRoadToLargestNetwork(
    int x,
    int y,
    bool rotated) const
{
    if (!valid_) {
        return {};
    }
    return best_road_to_largest_network(
        [x, y, rotated](RoadAccessCandidateVisitor &visitor) {
            map_road_access_visit_hippodrome_candidates(x, y, rotated ? 1 : 0, visitor);
        },
        [this]() { seedDistanceGrid(); },
        purpose_,
        true);
}

Route::RoadResult Route::DistanceQuery::findMonumentConstructionRoadToLargestNetwork(
    int x,
    int y,
    int size) const
{
    if (!valid_) {
        return {};
    }
    return best_road_to_largest_network(
        [x, y, size](RoadAccessCandidateVisitor &visitor) {
            map_road_access_visit_monument_construction_candidates(x, y, size, visitor);
        },
        [this]() { seedDistanceGrid(); },
        purpose_,
        size < 3);
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

    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(target.x, target.y, target.size, radius, &x_min, &y_min, &x_max, &y_max);

    bool has_same_network_candidate = !requireSameNetwork || sourceNetwork_ <= 0;
    if (!has_same_network_candidate) {
        SameNetworkAccessRoadVisitor same_network(sourceNetwork_);
        visit_access_road_area_tiles(x_min, y_min, x_max, y_max, same_network);
        if (!same_network.found()) {
            performance_tracker_record_route_metric(
                PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
                purpose_,
                1);
            return {};
        }
    }

    seedDistanceGrid();

    BestAccessRoadVisitor best_road(requireSameNetwork, sourceNetwork_, maxDistance);
    visit_access_road_area_tiles(x_min, y_min, x_max, y_max, best_road);
    return best_road.bestRoad();
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
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT,
        1);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PLANS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT,
        1);
    Route::Request request;
    request.source = source;
    request.destination = { -1, -1 };
    request.surface = Route::Surface::NonCitizenLand;
    request.purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT;
    request.direction_limit = directions;
    request.max_tiles = maxTiles;
    request.only_through_building_id = onlyThroughBuildingId;
    can_route_over_surface(request);
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
    performance_tracker_route_purpose purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT;
    if (figure.is_boat) {
        purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER;
    } else if (figure.terrain_usage == TERRAIN_USAGE_WALLS) {
        purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_WALL;
    }
    PerformanceTrackerRouteScope route_scope(purpose);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        purpose,
        1);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PLANS,
        purpose,
        1);

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
        built_path = build_route_path(Route::Request::fromFigure(
            figure,
            figure.is_boat == 2 ? Route::Surface::WaterFlotsam : Route::Surface::WaterBoat,
            purpose));
    } else {
        const figure_type_registry_impl::PathingPolicy *policy = figure_runtime_pathing_policy(&figure);
        const figure_type_registry_impl::PathingMode::TerrainAccess terrain = policy ?
            policy->terrain :
            figure_type_registry_impl::PathingMode::terrainFromLegacyUsage(figure.terrain_usage);
        built_path = build_figure_route_path(
            figure,
            terrain,
            route_path_direction_limit(direction_limit, terrain),
            purpose);
    }
    if (built_path) {
        path->figure_id = figure.id();
        path->directions = std::move(built_path.directions);
        figure.routing_path_id = path->id;
        figure.routing_path_length = built_path.tile_count;
    } else {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            purpose,
            1);
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
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PLANS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);
    const Route::Request request = Route::Request::between(
        source,
        destination,
        flotsam ? Route::Surface::WaterFlotsam : Route::Surface::WaterBoat,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER);
    const int path_length = build_route_path(request).tile_count;
    if (path_length <= 0) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
    }
    return path_length;
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
