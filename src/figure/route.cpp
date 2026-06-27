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
#include "map/routing.h"
#include "map/terrain.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <optional>
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

struct RouteIntent {
    map_point destination = { 0, 0 };
    RoutePolicy policy;
    figure_type_registry_impl::PathingMode::TerrainAccess terrain;
    int max_tiles = 0;
    int only_through_building_id = 0;

    bool operator==(const RouteIntent &other) const
    {
        return destination.x == other.destination.x &&
            destination.y == other.destination.y &&
            policy == other.policy &&
            terrain == other.terrain &&
            max_tiles == other.max_tiles &&
            only_through_building_id == other.only_through_building_id;
    }

    Route::Request requestFrom(const Figure &figure) const
    {
        Route::Request request = Route::Request::between(
            { figure.x, figure.y },
            destination,
            policy,
            policy.performancePurpose());
        request.max_tiles = max_tiles;
        request.only_through_building_id = only_through_building_id;
        return request;
    }
};

struct FigureRoute {
    unsigned int id = 0;
    unsigned int figure_id = 0;
    RouteIntent intent;
    bool has_intent = false;
    std::vector<uint8_t> directions;
    size_t current_step = 0;
    uint8_t same_direction_count = 0;

    bool active() const { return figure_id != 0; }

    bool belongsTo(const Figure &figure) const
    {
        return figure_id == figure.id();
    }

    bool isCurrentFor(const Figure &figure) const
    {
        return active() && belongsTo(figure) && figure.routing_path_id == id;
    }

    bool matchesIntent(const RouteIntent &route_intent) const
    {
        return has_intent && intent == route_intent;
    }

    void stampIntent(const RouteIntent &route_intent)
    {
        intent = route_intent;
        has_intent = true;
    }

    void assign(Figure &figure, const RouteIntent &route_intent, BuiltPath &&built_path)
    {
        figure_id = figure.id();
        stampIntent(route_intent);
        directions = std::move(built_path.directions);
        current_step = 0;
        same_direction_count = 0;
        figure.routing_path_id = id;
        figure.routing_path_length = built_path.tile_count;
    }

    bool hasCurrentStep() const
    {
        return current_step < directions.size();
    }

    int currentDirection() const
    {
        return hasCurrentStep() ?
            directions[current_step] >> kRouteDirectionBitOffset :
            8;
    }

    void advanceTile()
    {
        if (!hasCurrentStep()) {
            return;
        }

        const int tiles_in_direction = (directions[current_step] & kRouteDirectionCountMask) + 1;
        same_direction_count++;
        if (same_direction_count >= tiles_in_direction) {
            current_step++;
            same_direction_count = 0;
        }
    }

    void reset(unsigned int new_id)
    {
        id = new_id;
        figure_id = 0;
        intent = {};
        has_intent = false;
        directions.clear();
        current_step = 0;
        same_direction_count = 0;
    }

    void release()
    {
        figure_id = 0;
        intent = {};
        has_intent = false;
        directions.clear();
        current_step = 0;
        same_direction_count = 0;
    }
};

struct FailedRouteAttempt {
    map_point source = { 0, 0 };
    RouteIntent intent;
    bool skip_next_retry = false;
};

} // namespace

namespace {

static std::vector<FigureRoute> paths;
static std::vector<FailedRouteAttempt> failed_route_attempts;

static FigureRoute *path_slot(unsigned int id)
{
    return id < paths.size() ? &paths[id] : nullptr;
}

static FigureRoute *path_for_figure(const Figure &figure)
{
    if (figure.routing_path_id == 0) {
        return nullptr;
    }

    FigureRoute *path = path_slot(figure.routing_path_id);
    return path && path->belongsTo(figure) ? path : nullptr;
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

class AmbientCostMapHandle {
public:
    int distanceAt(int gridOffset) const
    {
        return map_routing_distance(gridOffset);
    }

    int reachableDistanceAt(int gridOffset, int maxDistance = 0) const
    {
        const int distance = distanceAt(gridOffset);
        if (distance <= 0 || (maxDistance > 0 && distance > maxDistance)) {
            return 0;
        }
        return distance;
    }
};

static const AmbientCostMapHandle &ambient_cost_map()
{
    static const AmbientCostMapHandle cost_map;
    return cost_map;
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
    if (next_distance != distance) {
        return 0;
    }
    if (direction == -1) {
        return 1;
    }
    return direction % 2 == 1 && next_direction % 2 == 0;
}

static BuiltPath build_land_path(int dst_x, int dst_y, int num_directions)
{
    BuiltPath result;
    const AmbientCostMapHandle &cost_map = ambient_cost_map();
    const int dst_grid_offset = map_grid_offset(dst_x, dst_y);
    int distance = cost_map.distanceAt(dst_grid_offset);
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
        const int base_distance = cost_map.distanceAt(grid_offset);
        distance = base_distance;
        int direction = -1;
        int is_highway = 0;
        for (int next_direction = 0; next_direction < 8; next_direction += step) {
            if (next_direction == last_direction) {
                continue;
            }
            const int next_offset = grid_offset + map_grid_direction_delta(next_direction);
            const int next_distance = cost_map.distanceAt(next_offset);
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
    const AmbientCostMapHandle &cost_map = ambient_cost_map();
    const int rand = random_byte() & 3;
    const int dst_grid_offset = map_grid_offset(dst_x, dst_y);
    int distance = cost_map.distanceAt(dst_grid_offset);
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
        distance = cost_map.distanceAt(grid_offset);
        if (is_flotsam) {
            current_rand = map_random_get(grid_offset) & 3;
        }
        int direction = -1;
        for (int next_direction = 0; next_direction < 8; next_direction++) {
            if (next_direction == last_direction) {
                continue;
            }
            const int next_offset = grid_offset + map_grid_direction_delta(next_direction);
            const int next_distance = cost_map.distanceAt(next_offset);
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

static std::optional<BuiltPath> build_reachable_seeded_land_path(
    const Route::Request &request,
    int path_direction_limit,
    int fallback_direction_limit = 0)
{
    if (!request.canReachOverSurface()) {
        return std::nullopt;
    }
    return build_seeded_land_path(request, path_direction_limit, fallback_direction_limit);
}

static BuiltPath build_route_path(
    const Route::Request &request,
    int path_direction_limit,
    int fallback_direction_limit = 0)
{
    if (request.policy.isWater()) {
        if (request.policy.isWaterFlotsam()) {
            map_routing_calculate_distances_water_flotsam(request.source.x, request.source.y);
        } else {
            map_routing_calculate_distances_water_boat(request.source.x, request.source.y);
        }
        return build_water_path(
            request.destination.x,
            request.destination.y,
            request.policy.isWaterFlotsam());
    }
    if (auto built_path = build_reachable_seeded_land_path(request, path_direction_limit, fallback_direction_limit)) {
        return std::move(*built_path);
    }
    return {};
}

static RouteIntent route_intent_from_figure(Figure &figure)
{
    RouteIntent intent;
    const int direction_limit = figure.disallow_diagonal ? 4 : 8;
    const RouteNeighborhood neighborhood = route_neighborhood_from_direction_limit(direction_limit);
    const roadblock_permission permission = Roadblock::permission_for(figure);
    intent.destination = { figure.destination_x, figure.destination_y };
    intent.max_tiles = figure.max_roam_length > 0 ? figure.max_roam_length : 0;
    intent.only_through_building_id = figure.destination_building.id();

    if (figure.is_boat) {
        intent.policy = RoutePolicy::water(figure.is_boat == 2, neighborhood);
        intent.policy.permission = permission;
    } else {
        const figure_type_registry_impl::PathingPolicy *pathing = figure_runtime_pathing_policy(&figure);
        intent.terrain = pathing ?
            pathing->terrain :
            figure_type_registry_impl::PathingMode::terrainFromLegacyUsage(figure.terrain_usage);
        intent.policy = figure_type_registry_impl::PathingMode::routePolicyForTerrain(
            intent.terrain,
            permission,
            neighborhood);
    }

    return intent;
}

static void clear_stale_failed_route_attempts()
{
    for (size_t figure_id = 0; figure_id < failed_route_attempts.size(); figure_id++) {
        FailedRouteAttempt &attempt = failed_route_attempts[figure_id];
        if (!attempt.skip_next_retry) {
            continue;
        }
        if (figure_id >= Figure::count()) {
            attempt = {};
            continue;
        }
        const Figure *figure = Figure::get(static_cast<unsigned int>(figure_id));
        if (!figure || figure->state != FIGURE_STATE_ALIVE) {
            attempt = {};
        }
    }
}

static bool failed_route_retry_deferred(const Figure &figure, const RouteIntent &intent)
{
    if (figure.id() >= failed_route_attempts.size()) {
        return false;
    }
    const map_point source = { figure.x, figure.y };
    FailedRouteAttempt &attempt = failed_route_attempts[figure.id()];
    if (!attempt.skip_next_retry ||
        attempt.source.x != source.x ||
        attempt.source.y != source.y ||
        !(attempt.intent == intent)) {
        attempt = {};
        return false;
    }
    attempt = {};
    return true;
}

static void record_failed_route_attempt(const Figure &figure, const RouteIntent &intent)
{
    if (failed_route_attempts.size() <= figure.id()) {
        failed_route_attempts.resize(static_cast<size_t>(figure.id()) + 1);
    }
    FailedRouteAttempt &attempt = failed_route_attempts[figure.id()];
    attempt.source = { figure.x, figure.y };
    attempt.intent = intent;
    attempt.skip_next_retry = true;
}

static void clear_failed_route_attempt(const Figure &figure)
{
    if (figure.id() < failed_route_attempts.size()) {
        failed_route_attempts[figure.id()] = {};
    }
}

static BuiltPath build_enemy_land_route(
    Route::Request request,
    int path_direction_limit,
    int fallback_direction_limit)
{
    request.policy = request.policy.asNonCitizenLand();
    request.max_tiles = 5000;
    if (auto built_path = build_reachable_seeded_land_path(request, path_direction_limit, fallback_direction_limit)) {
        return std::move(*built_path);
    }

    request.only_through_building_id = 0;
    request.max_tiles = 25000;
    if (auto built_path = build_reachable_seeded_land_path(request, path_direction_limit, fallback_direction_limit)) {
        return std::move(*built_path);
    }

    if (!map_routing_noncitizen_can_travel_through_everything(
            request.source.x,
            request.source.y,
            request.destination.x,
            request.destination.y,
            request.policy.directionLimit())) {
        return {};
    }
    return build_seeded_land_path(request, path_direction_limit, fallback_direction_limit);
}

static BuiltPath build_figure_route_path(
    Route::Request request,
    const RouteIntent &intent)
{
    const int direction_limit = request.policy.directionLimit();

    if (intent.terrain.usesEnemyLandRoute()) {
        return build_enemy_land_route(request, direction_limit, direction_limit);
    }

    if (intent.terrain.usesAnimalLandRoute()) {
        request.only_through_building_id = -1;
        request.max_tiles = 5000;
        return build_route_path(request, direction_limit, direction_limit);
    }

    if (intent.terrain.usesRoadAccess()) {
        if (auto built_path = build_reachable_seeded_land_path(request, direction_limit, direction_limit)) {
            return std::move(*built_path);
        }
        if (!intent.terrain.allowsRoadAccessFallback()) {
            return {};
        }
        request.policy = request.policy.withoutRoadAccess();
    }

    return build_route_path(request, direction_limit, direction_limit);
}

template <typename DistanceAt>
class BestRouteRoadAccessVisitor final : public RoadAccessCandidateVisitor {
public:
    explicit BestRouteRoadAccessVisitor(const DistanceAt &distanceAt) : distanceAt_(distanceAt) {}

    void visit(const road_access_candidate &candidate) override
    {
        found_ = true;
        if (!select_reachable_) {
            return;
        }

        const int distance = distanceAt_(candidate.grid_offset);
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

    bool found() const { return found_; }
    void selectReachableCandidates() { select_reachable_ = true; }
    Route::RoadResult bestNetworkRoad() const { return best_network_road_; }
    Route::RoadResult shortestRoad() const { return shortest_road_; }

private:
    bool found_ = false;
    bool select_reachable_ = false;
    int best_network_index_ = std::numeric_limits<int>::max();
    int shortest_distance_ = std::numeric_limits<int>::max();
    Route::RoadResult best_network_road_;
    Route::RoadResult shortest_road_;
    DistanceAt distanceAt_;
};

template <typename VisitCandidates, typename SeedCostMap, typename DistanceAt>
static Route::RoadResult best_road_to_largest_network(
    const VisitCandidates &visit_candidates,
    const SeedCostMap &seed_cost_map,
    const DistanceAt &distance_at,
    performance_tracker_route_purpose purpose,
    bool fallback_to_shortest_distance)
{
    BestRouteRoadAccessVisitor<DistanceAt> best_candidate(distance_at);
    visit_candidates(best_candidate);
    if (!best_candidate.found()) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
            purpose,
            1);
        return {};
    }

    seed_cost_map();

    best_candidate.selectReachableCandidates();
    visit_candidates(best_candidate);
    if (best_candidate.bestNetworkRoad() || !fallback_to_shortest_distance) {
        return best_candidate.bestNetworkRoad();
    }
    return best_candidate.shortestRoad();
}

} // namespace

void Route::DistanceQuery::CostMapHandle::seed(
    const map_point &source,
    const std::optional<roadblock_permission> &permission,
    performance_tracker_route_purpose purpose)
{
    PerformanceTrackerRouteScope route_scope(purpose);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        purpose,
        1);
    if (generation_ == map_routing_distance_generation()) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_CACHE_HITS,
            purpose,
            1);
        return;
    }
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_CACHE_MISSES,
        purpose,
        1);
    if (permission) {
        map_routing_calculate_distances_road_garden(source.x, source.y, *permission);
    } else {
        map_routing_calculate_distances(source.x, source.y);
    }
    generation_ = map_routing_distance_generation();
}

int Route::DistanceQuery::CostMapHandle::distanceAt(int gridOffset) const
{
    return ambient_cost_map().distanceAt(gridOffset);
}

int Route::DistanceQuery::CostMapHandle::reachableDistanceAt(int gridOffset, int maxDistance) const
{
    return ambient_cost_map().reachableDistanceAt(gridOffset, maxDistance);
}

Route::Request Route::Request::between(
    const map_point &source,
    const map_point &destination,
    RoutePolicy policy,
    performance_tracker_route_purpose purpose)
{
    Request request;
    request.source = source;
    request.destination = destination;
    request.policy = policy;
    request.purpose = purpose;
    request.has_destination = true;
    return request;
}

int Route::Request::sourceOffset() const
{
    return map_grid_offset(source.x, source.y);
}

int Route::Request::destinationOffset() const
{
    return map_grid_offset(destination.x, destination.y);
}

bool Route::Request::hasValidEndpoints() const
{
    return has_destination &&
        map_grid_is_valid_offset(sourceOffset()) &&
        map_grid_is_valid_offset(destinationOffset());
}

bool Route::Request::acceptsDestinationDistance(int distance) const
{
    return max_tiles <= 0 || (distance > 0 && distance <= max_tiles);
}

bool Route::Request::canReachOverSurface() const
{
    if (policy.isConstruction()) {
        return map_routing_calculate_distances_for_building(
            policy.constructionBuildingType(),
            source.x,
            source.y);
    }

    if (policy.isNonCitizenLand()) {
        return map_routing_noncitizen_can_travel_over_land(
            source.x,
            source.y,
            destination.x,
            destination.y,
            policy.directionLimit(),
            only_through_building_id,
            max_tiles);
    }
    if (policy.isWalls()) {
        return map_routing_can_travel_over_walls(
            source.x,
            source.y,
            destination.x,
            destination.y,
            policy.pathDirectionLimit());
    }

    auto can_travel = map_routing_citizen_can_travel_over_land;
    if (policy.isCitizenRoadGarden()) {
        can_travel = map_routing_citizen_can_travel_over_road_garden;
    } else if (policy.isCitizenRoadGardenHighway()) {
        can_travel = map_routing_citizen_can_travel_over_road_garden_highway;
    }
    return can_travel(
        source.x,
        source.y,
        destination.x,
        destination.y,
        policy.directionLimit(),
        policy.roadblockPermission());
}

bool Route::Request::canReachWithBoundedRoadGardenDistanceField() const
{
    const int source_offset = sourceOffset();
    if (!map_grid_is_valid_offset(source_offset) ||
        !figure_type_registry_impl::PathingMode::citizenIsRoadLike(source_offset)) {
        return false;
    }

    map_routing_calculate_distances_road_garden(
        source.x,
        source.y,
        policy.roadblockPermission());
    return acceptsDestinationDistance(ambient_cost_map().distanceAt(destinationOffset()));
}

bool Route::Request::prunesByRoadNetwork() const
{
    if (!require_same_road_network) {
        return false;
    }

    const int source_network = figure_type_registry_impl::PathingMode::citizenRoadNetworkAt(sourceOffset());
    const int destination_network = figure_type_registry_impl::PathingMode::citizenRoadNetworkAt(destinationOffset());
    return source_network > 0 && destination_network > 0 && source_network != destination_network;
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

    if (!request.hasValidEndpoints()) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            request.purpose,
            1);
        return false;
    }
    if (request.prunesByRoadNetwork()) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
            request.purpose,
            1);
        return false;
    }
    const bool reached = request.policy.needsBoundedRoadGardenDistanceField(request.max_tiles) ?
        request.canReachWithBoundedRoadGardenDistanceField() :
        request.canReachOverSurface() &&
            request.acceptsDestinationDistance(ambient_cost_map().distanceAt(request.destinationOffset()));
    if (!reached) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            request.purpose,
            1);
        return false;
    }
    return true;
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
        figure_type_registry_impl::PathingMode::citizenRoadNetworkAt(source_offset),
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

    const int source_network = figure_type_registry_impl::PathingMode::citizenRoadNetworkAt(source_offset);
    return DistanceQuery(point, source_network, permission, purpose, true);
}

Route::DistanceQuery Route::DistanceQuery::fromFigure(
    const Figure &figure,
    performance_tracker_route_purpose purpose)
{
    return fromRoad({ figure.x, figure.y }, Roadblock::permission_for(figure), purpose);
}

const Route::DistanceQuery::CostMapHandle &Route::DistanceQuery::costMap() const
{
    costMap_.seed(source_, permission_, purpose_);
    return costMap_;
}

int Route::DistanceQuery::distanceTo(int gridOffset, int maxDistance) const
{
    if (!valid_ || !map_grid_is_valid_offset(gridOffset)) {
        return 0;
    }
    return costMap().reachableDistanceAt(gridOffset, maxDistance);
}

Route::RoadResult Route::DistanceQuery::findRoad(const map_point &road, int maxDistance) const
{
    if (!valid_) {
        return {};
    }
    const CostMapHandle &cost_map = costMap();

    const int grid_offset = map_grid_offset(road.x, road.y);
    if (!map_grid_is_valid_offset(grid_offset)) {
        return {};
    }

    const int distance = cost_map.reachableDistanceAt(grid_offset, maxDistance);
    if (distance <= 0) {
        return {};
    }
    return road_result(grid_offset, distance);
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
    const CostMapHandle &cost_map = costMap();

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
                if (requireRoad &&
                    !figure_type_registry_impl::PathingMode::citizenIsRoadLike(grid_offset)) {
                    continue;
                }

                const int distance = cost_map.reachableDistanceAt(grid_offset, maxDistance);
                if (distance <= 0 || distance >= best_distance) {
                    continue;
                }

                best_distance = distance;
                best_grid_offset = grid_offset;
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
        [this]() { costMap(); },
        [this](int gridOffset) { return costMap_.distanceAt(gridOffset); },
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
        [this]() { costMap(); },
        [this](int gridOffset) { return costMap_.distanceAt(gridOffset); },
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
        [this]() { costMap(); },
        [this](int gridOffset) { return costMap_.distanceAt(gridOffset); },
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

    if (requireSameNetwork && sourceNetwork_ > 0 &&
        !figure_type_registry_impl::PathingMode::citizenAreaTouchesRoadNetwork(
            x_min,
            y_min,
            x_max,
            y_max,
            sourceNetwork_)) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_PRUNED_BY_NETWORK,
            purpose_,
            1);
        return {};
    }

    const CostMapHandle &cost_map = costMap();

    Route::RoadResult best_road;
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            const int grid_offset = map_grid_offset(x, y);
            if (!figure_type_registry_impl::PathingMode::citizenIsRoadLike(grid_offset)) {
                continue;
            }
            if (requireSameNetwork &&
                sourceNetwork_ > 0 &&
                !figure_type_registry_impl::PathingMode::citizenIsInRoadNetwork(grid_offset, sourceNetwork_)) {
                continue;
            }

            const int distance = cost_map.reachableDistanceAt(grid_offset, maxDistance);
            if (distance > 0 && (!best_road || distance < best_road.distance)) {
                best_road = road_result(grid_offset, distance);
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
    request.policy = RoutePolicy::nonCitizenLand(route_neighborhood_from_direction_limit(directions));
    request.purpose = PERFORMANCE_TRACKER_ROUTE_PURPOSE_MOVEMENT;
    request.max_tiles = maxTiles;
    request.only_through_building_id = onlyThroughBuildingId;
    request.canReachOverSurface();
    return TerrainQuery(true);
}

int Route::TerrainQuery::distanceTo(int gridOffset) const
{
    if (!valid_ || !map_grid_is_valid_offset(gridOffset)) {
        return 0;
    }
    return ambient_cost_map().distanceAt(gridOffset);
}

void Route::clearAll()
{
    paths.clear();
    failed_route_attempts.clear();
}

void Route::clean()
{
    for (unsigned int i = 0; i < paths.size(); i++) {
        FigureRoute &path = paths[i];
        unsigned int figure_id = path.figure_id;
        if (figure_id > 0 && figure_id < Figure::count()) {
            const Figure *f = Figure::get(figure_id);
            if (f->state != FIGURE_STATE_ALIVE || !path.isCurrentFor(*f)) {
                path.release();
            }
        }
    }
    trim_inactive_paths();
    clear_stale_failed_route_attempts();
}

void Route::add(Figure &figure)
{
    const RouteIntent intent = route_intent_from_figure(figure);
    if (failed_route_retry_deferred(figure, intent)) {
        return;
    }

    const performance_tracker_route_purpose purpose = intent.policy.performancePurpose();
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
    FigureRoute *path = new_path_after_index(1);
    if (!path) {
        return;
    }
    const Route::Request request = intent.requestFrom(figure);
    BuiltPath built_path = intent.policy.isWater() ?
        build_route_path(request, request.policy.pathDirectionLimit()) :
        build_figure_route_path(request, intent);
    if (built_path) {
        path->assign(figure, intent, std::move(built_path));
        clear_failed_route_attempt(figure);
    } else {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            purpose,
            1);
        record_failed_route_attempt(figure, intent);
        path->release();
        trim_inactive_paths();
    }
}

bool Route::hasReusablePath(Figure &figure)
{
    if (figure.routing_path_id == 0) {
        return false;
    }

    FigureRoute *path = path_for_figure(figure);
    if (!path) {
        Route::remove(figure);
        return false;
    }

    const RouteIntent intent = route_intent_from_figure(figure);
    if (!path->has_intent) {
        path->stampIntent(intent);
        return true;
    }
    if (path->matchesIntent(intent)) {
        return true;
    }

    Route::remove(figure);
    return false;
}

void Route::remove(Figure &figure)
{
    if (figure.routing_path_id > 0) {
        FigureRoute *path = path_for_figure(figure);
        if (path) {
            path->release();
        }
        figure.routing_path_id = 0;
    }
    trim_inactive_paths();
}

int Route::currentDirection(const Figure &figure)
{
    FigureRoute *path = path_for_figure(figure);
    return path ? path->currentDirection() : 8;
}

void Route::advanceTile(Figure &figure)
{
    FigureRoute *path = path_for_figure(figure);
    if (!path) {
        return;
    }

    path->advanceTile();
}

bool Route::calculateConstructionDistances(RoutePolicyKind kind, const map_point &source)
{
    performance_tracker_record_route_plan(PERFORMANCE_TRACKER_ROUTE_PURPOSE_CONSTRUCTION);
    const RoutePolicy policy = RoutePolicy::fromKind(kind);
    return map_routing_calculate_distances_for_building(
        policy.constructionBuildingType(),
        source.x,
        source.y);
}

int Route::constructionDistanceTo(int gridOffset)
{
    return ambient_cost_map().distanceAt(gridOffset);
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
        RoutePolicy::water(flotsam),
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER);
    const int path_length = build_route_path(
        request,
        request.policy.pathDirectionLimit()).tile_count;
    if (path_length <= 0) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
    }
    return path_length;
}

bool Route::waterCanReachAdjacentOpenWater(const map_point &source, int x, int y, int size)
{
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER);
    performance_tracker_record_route_plan(PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER);
    map_routing_calculate_distances_water_boat(source.x, source.y);

    const int base_offset = map_grid_offset(x, y);
    const AmbientCostMapHandle &cost_map = ambient_cost_map();
    for (const int *tile_delta = map_grid_adjacent_offsets(size); *tile_delta; tile_delta++) {
        const int grid_offset = base_offset + *tile_delta;
        if (map_terrain_is(grid_offset, TERRAIN_WATER) && cost_map.distanceAt(grid_offset) > 0) {
            return true;
        }
    }
    return false;
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
    failed_route_attempts.clear();
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
