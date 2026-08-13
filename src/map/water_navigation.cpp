#include "map/water_navigation.h"

#include "building/building.h"
#include "building/dock.h"
#include "core/direction.h"
#include "game/performance_tracker.h"
#include "map/bridge.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/map.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


namespace {

constexpr int kGridArea = GRID_SIZE * GRID_SIZE;
constexpr std::int16_t kUnreachable = 0;
constexpr int kDirections = 8;

enum class WaterTile : std::int8_t {
    Blocked,
    Passable,
    MapEdge,
    LowBridge,
};

struct DistanceField {
    int root = -1;
    WaterNavigationProfile profile = WaterNavigationProfile::Boat;
    std::array<std::int16_t, kGridArea> distance = {};

    bool valid() const { return root >= 0; }
};

struct NavigationState {
    bool topology_dirty = true;
    bool dock_endpoints_dirty = true;
    bool river_anchors_dirty = true;
    bool rebuilding = false;
    int world_load_depth = 0;
    std::array<WaterTile, kGridArea> topology = {};
    std::array<std::uint16_t, kGridArea> boat_component = {};
    DistanceField river_entry_boat;
    DistanceField river_exit_boat;
    DistanceField river_entry_flotsam;
    DistanceField river_exit_flotsam;
    std::unordered_set<std::uint32_t> registered_dock_destinations;
    std::unordered_map<std::uint32_t, DistanceField> destination_fields;
};

NavigationState state;

std::uint32_t field_key(int root, WaterNavigationProfile profile)
{
    return static_cast<std::uint32_t>(root) |
        (static_cast<std::uint32_t>(profile) << 24);
}

bool is_inside_map(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return false;
    }
    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    return x >= 0 && x < map_data.width && y >= 0 && y < map_data.height;
}

bool raw_passable(int grid_offset, WaterNavigationProfile profile)
{
    if (!is_inside_map(grid_offset)) {
        return false;
    }
    const WaterTile tile = state.topology[grid_offset];
    if (tile == WaterTile::Blocked) {
        return false;
    }
    return profile == WaterNavigationProfile::Flotsam || tile != WaterTile::LowBridge;
}

int step_cost(int grid_offset, WaterNavigationProfile profile)
{
    if (profile == WaterNavigationProfile::Boat &&
        state.topology[grid_offset] == WaterTile::MapEdge) {
        return 5;
    }
    return 1;
}

void classify_topology()
{
    state.topology.fill(WaterTile::Blocked);
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; ++y, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; ++x, ++grid_offset) {
            const bool surrounded =
                map_terrain_is(grid_offset, TERRAIN_WATER) &&
                map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_WATER) &&
                map_terrain_is(grid_offset + map_grid_delta(-1, 0), TERRAIN_WATER) &&
                map_terrain_is(grid_offset + map_grid_delta(1, 0), TERRAIN_WATER) &&
                map_terrain_is(grid_offset + map_grid_delta(0, 1), TERRAIN_WATER);
            if (!surrounded) {
                continue;
            }
            if (x == 0 || x == map_data.width - 1 || y == 0 || y == map_data.height - 1) {
                state.topology[grid_offset] = WaterTile::MapEdge;
                continue;
            }
            switch (map_bridge_legacy_section_at(grid_offset)) {
                case 5:
                case 6:
                    state.topology[grid_offset] = WaterTile::LowBridge;
                    break;
                case 13:
                    state.topology[grid_offset] = WaterTile::Blocked;
                    break;
                default:
                    state.topology[grid_offset] = WaterTile::Passable;
                    break;
            }
        }
    }
}

void rebuild_boat_components()
{
    state.boat_component.fill(0);
    std::vector<int> queue;
    queue.reserve(kGridArea);
    std::uint16_t component = 0;

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; ++y, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; ++x, ++grid_offset) {
            if (!raw_passable(grid_offset, WaterNavigationProfile::Boat) ||
                state.boat_component[grid_offset]) {
                continue;
            }
            if (component == std::numeric_limits<std::uint16_t>::max()) {
                return;
            }
            ++component;
            queue.clear();
            queue.push_back(grid_offset);
            state.boat_component[grid_offset] = component;
            for (std::size_t head = 0; head < queue.size(); ++head) {
                const int current = queue[head];
                for (int direction = 0; direction < kDirections; direction += 2) {
                    const int next = current + map_grid_direction_delta(direction);
                    if (raw_passable(next, WaterNavigationProfile::Boat) &&
                        !state.boat_component[next]) {
                        state.boat_component[next] = component;
                        queue.push_back(next);
                    }
                }
            }
        }
    }
}

struct FieldQueueNode {
    int distance;
    int grid_offset;
};

struct FieldQueueCompare {
    bool operator()(const FieldQueueNode &left, const FieldQueueNode &right) const
    {
        if (left.distance != right.distance) {
            return left.distance > right.distance;
        }
        return left.grid_offset > right.grid_offset;
    }
};

bool build_field(
    int root,
    WaterNavigationProfile profile,
    bool costs_to_root,
    DistanceField *field)
{
    if (!field) {
        return false;
    }
    field->root = -1;
    field->profile = profile;
    field->distance.fill(kUnreachable);
    if (!raw_passable(root, profile)) {
        return false;
    }

    std::priority_queue<FieldQueueNode, std::vector<FieldQueueNode>, FieldQueueCompare> queue;
    field->root = root;
    field->distance[root] = 1;
    queue.push({1, root});
    const int direction_step = profile == WaterNavigationProfile::Flotsam ? 1 : 2;

    while (!queue.empty()) {
        const FieldQueueNode current = queue.top();
        queue.pop();
        if (field->distance[current.grid_offset] != current.distance) {
            continue;
        }
        for (int direction = 0; direction < kDirections; direction += direction_step) {
            const int next = current.grid_offset + map_grid_direction_delta(direction);
            if (!raw_passable(next, profile)) {
                continue;
            }
            // Source-rooted fields pay the cost of entering the newly reached
            // tile. Destination-rooted fields traverse edges in reverse and
            // therefore pay the cost of the current (forward-entered) tile.
            const int next_distance = current.distance +
                step_cost(costs_to_root ? current.grid_offset : next, profile);
            if (next_distance >= std::numeric_limits<std::int16_t>::max()) {
                continue;
            }
            const int previous = field->distance[next];
            if (previous && previous <= next_distance) {
                continue;
            }
            field->distance[next] = static_cast<std::int16_t>(next_distance);
            queue.push({next_distance, next});
        }
    }
    return true;
}

DistanceField *anchor_field(bool entry, WaterNavigationProfile profile)
{
    if (entry) {
        return profile == WaterNavigationProfile::Boat ?
            &state.river_entry_boat : &state.river_entry_flotsam;
    }
    return profile == WaterNavigationProfile::Boat ?
        &state.river_exit_boat : &state.river_exit_flotsam;
}

void rebuild_anchor_fields()
{
    const map_point entry = scenario_map_river_entry();
    const map_point exit = scenario_map_river_exit();
    const int entry_offset = scenario_map_has_river_entry() && map_grid_is_inside(entry.x, entry.y, 1) ?
        map_grid_offset(entry.x, entry.y) : -1;
    const int exit_offset = scenario_map_has_river_exit() && map_grid_is_inside(exit.x, exit.y, 1) ?
        map_grid_offset(exit.x, exit.y) : -1;

    build_field(entry_offset, WaterNavigationProfile::Boat, false, &state.river_entry_boat);
    build_field(exit_offset, WaterNavigationProfile::Boat, true, &state.river_exit_boat);
    build_field(entry_offset, WaterNavigationProfile::Flotsam, false, &state.river_entry_flotsam);
    build_field(exit_offset, WaterNavigationProfile::Flotsam, true, &state.river_exit_flotsam);
}

DistanceField *cached_destination_field(int root, WaterNavigationProfile profile, bool create)
{
    const std::uint32_t key = field_key(root, profile);
    const auto found = state.destination_fields.find(key);
    if (found != state.destination_fields.end()) {
        return &found->second;
    }
    if (!create) {
        return nullptr;
    }
    auto inserted = state.destination_fields.try_emplace(key);
    DistanceField &field = inserted.first->second;
    if (!build_field(root, profile, true, &field)) {
        state.destination_fields.erase(inserted.first);
        return nullptr;
    }
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_ENDPOINT_FIELDS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);
    return &field;
}

void rebuild_dock_fields(bool eagerly_build_fields)
{
    state.registered_dock_destinations.clear();
    state.destination_fields.clear();
    Building::for_each([eagerly_build_fields](Building *building) {
        if (!building || !building->is_in_use() || !building->matches("dock")) {
            return;
        }
        static const ship_dock_request_type request_types[] = {
            SHIP_DOCK_REQUEST_1_DOCKING,
            SHIP_DOCK_REQUEST_2_FIRST_QUEUE,
            SHIP_DOCK_REQUEST_4_SECOND_QUEUE,
        };
        for (const ship_dock_request_type request_type : request_types) {
            map_point destination = {-1, -1};
            building_dock_get_ship_request_tile(*building, request_type, &destination);
            if (map_grid_is_inside(destination.x, destination.y, 1)) {
                const int destination_offset = map_grid_offset(destination.x, destination.y);
                state.registered_dock_destinations.insert(field_key(
                    destination_offset, WaterNavigationProfile::Boat));
                if (eagerly_build_fields) {
                    cached_destination_field(
                        destination_offset,
                        WaterNavigationProfile::Boat,
                        true);
                }
            }
        }
    });
}

#ifndef NDEBUG
void validate_field(const DistanceField &field)
{
    if (!field.valid()) {
        return;
    }
    assert(raw_passable(field.root, field.profile));
    assert(field.distance[field.root] == 1);
    for (int grid_offset = 0; grid_offset < kGridArea; ++grid_offset) {
        if (field.distance[grid_offset] > 0) {
            assert(raw_passable(grid_offset, field.profile));
        }
    }
}

void validate_cache_invariants()
{
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; ++y, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; ++x, ++grid_offset) {
            const WaterTile classification = state.topology[grid_offset];
            if (classification == WaterTile::Blocked) {
                continue;
            }
            assert(map_terrain_is(grid_offset, TERRAIN_WATER));
            assert(map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_WATER));
            assert(map_terrain_is(grid_offset + map_grid_delta(-1, 0), TERRAIN_WATER));
            assert(map_terrain_is(grid_offset + map_grid_delta(1, 0), TERRAIN_WATER));
            assert(map_terrain_is(grid_offset + map_grid_delta(0, 1), TERRAIN_WATER));
            if (classification == WaterTile::LowBridge) {
                assert(!raw_passable(grid_offset, WaterNavigationProfile::Boat));
                assert(raw_passable(grid_offset, WaterNavigationProfile::Flotsam));
            }
        }
    }
    validate_field(state.river_entry_boat);
    validate_field(state.river_exit_boat);
    validate_field(state.river_entry_flotsam);
    validate_field(state.river_exit_flotsam);
    for (const auto &entry : state.destination_fields) {
        validate_field(entry.second);
    }
}
#endif

void rebuild_dirty_state()
{
    if (state.rebuilding ||
        (!state.topology_dirty && !state.river_anchors_dirty && !state.dock_endpoints_dirty)) {
        return;
    }

    PerformanceTrackerScope rebuild_scope(PERFORMANCE_TRACKER_BUCKET_WATER_NAVIGATION_REBUILD);
    state.rebuilding = true;
    const bool topology_changed = state.topology_dirty;
    if (topology_changed) {
        classify_topology();
        rebuild_boat_components();
        state.topology_dirty = false;
        state.river_anchors_dirty = true;
        state.dock_endpoints_dirty = true;
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_TOPOLOGY_REBUILDS,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
    }
    if (state.river_anchors_dirty) {
        rebuild_anchor_fields();
        state.river_anchors_dirty = false;
    }
    if (state.dock_endpoints_dirty) {
        // A world/topology rebuild produces the complete runtime cache in one
        // transaction. A dock-only mutation merely registers its new goals;
        // those fields remain lazy until a ship actually requests them.
        rebuild_dock_fields(topology_changed);
        state.dock_endpoints_dirty = false;
    }
#ifndef NDEBUG
    validate_cache_invariants();
#endif
    state.rebuilding = false;
}

void ensure_ready()
{
    if (state.world_load_depth == 0) {
        rebuild_dirty_state();
    }
}

bool same_point(const map_point &left, const map_point &right)
{
    return left.x == right.x && left.y == right.y;
}

const DistanceField *reusable_field(
    const map_point &source,
    const map_point &destination,
    WaterNavigationProfile profile)
{
    const map_point entry = scenario_map_river_entry();
    if (scenario_map_has_river_entry() && same_point(source, entry)) {
        const DistanceField *field = anchor_field(true, profile);
        if (field->valid()) {
            return field;
        }
    }
    const map_point exit = scenario_map_river_exit();
    if (scenario_map_has_river_exit() && same_point(destination, exit)) {
        const DistanceField *field = anchor_field(false, profile);
        if (field->valid()) {
            return field;
        }
    }
    const int destination_offset = map_grid_offset(destination.x, destination.y);
    const std::uint32_t key = field_key(destination_offset, profile);
    return cached_destination_field(
        destination_offset,
        profile,
        state.registered_dock_destinations.contains(key));
}

bool build_path_from_field(
    int source,
    int destination,
    const DistanceField &field,
    int direction_limit,
    std::vector<std::uint8_t> *directions)
{
    directions->clear();
    if (!field.valid() || !is_inside_map(source) || !is_inside_map(destination) ||
        field.distance[source] == kUnreachable || field.distance[destination] == kUnreachable) {
        return false;
    }
    if (source == destination) {
        return true;
    }

    const bool root_is_source = field.root == source;
    const bool root_is_destination = field.root == destination;
    if (!root_is_source && !root_is_destination) {
        return false;
    }

    int current = root_is_source ? destination : source;
    const int target = field.root;
    const int direction_step = direction_limit == 8 ? 1 : 2;
    std::vector<std::uint8_t> toward_root;
    toward_root.reserve(kGridArea / 4);

    while (current != target) {
        const int current_distance = field.distance[current];
        int best_direction = -1;
        int best_distance = current_distance;
        for (int direction = 0; direction < kDirections; direction += direction_step) {
            const int next = current + map_grid_direction_delta(direction);
            if (!is_inside_map(next) || !raw_passable(next, field.profile)) {
                continue;
            }
            const int next_distance = field.distance[next];
            if (next_distance > 0 && next_distance < best_distance) {
                best_distance = next_distance;
                best_direction = direction;
            }
        }
        if (best_direction < 0) {
            directions->clear();
            return false;
        }
        toward_root.push_back(static_cast<std::uint8_t>(best_direction));
        current += map_grid_direction_delta(best_direction);
        if (toward_root.size() > kGridArea) {
            directions->clear();
            return false;
        }
    }

    if (root_is_destination) {
        *directions = std::move(toward_root);
    } else {
        directions->reserve(toward_root.size());
        for (auto iterator = toward_root.rbegin(); iterator != toward_root.rend(); ++iterator) {
            directions->push_back(static_cast<std::uint8_t>((*iterator + 4) % 8));
        }
    }
    return true;
}

int heuristic(int from, int destination, int direction_limit)
{
    const int dx = std::abs(map_grid_offset_to_x(from) - map_grid_offset_to_x(destination));
    const int dy = std::abs(map_grid_offset_to_y(from) - map_grid_offset_to_y(destination));
    return direction_limit == 8 ? std::max(dx, dy) : dx + dy;
}

struct AStarNode {
    int estimated_total;
    int cost;
    int grid_offset;
};

struct AStarCompare {
    bool operator()(const AStarNode &left, const AStarNode &right) const
    {
        if (left.estimated_total != right.estimated_total) {
            return left.estimated_total > right.estimated_total;
        }
        if (left.cost != right.cost) {
            return left.cost > right.cost;
        }
        return left.grid_offset > right.grid_offset;
    }
};

bool astar(
    int source,
    int destination,
    WaterNavigationProfile profile,
    int direction_limit,
    std::vector<std::uint8_t> *directions)
{
    PerformanceTrackerScope astar_scope(PERFORMANCE_TRACKER_BUCKET_WATER_NAVIGATION_ASTAR);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_ASTAR_REQUESTS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);
    directions->clear();
    if (!raw_passable(source, profile) || !raw_passable(destination, profile)) {
        return false;
    }
    if (source == destination) {
        return true;
    }
    if (profile == WaterNavigationProfile::Boat &&
        state.boat_component[source] != state.boat_component[destination]) {
        return false;
    }

    constexpr int infinity = std::numeric_limits<int>::max();
    std::array<int, kGridArea> cost;
    std::array<std::int8_t, kGridArea> predecessor_direction;
    std::array<std::uint8_t, kGridArea> closed;
    cost.fill(infinity);
    predecessor_direction.fill(-1);
    closed.fill(0);
    std::priority_queue<AStarNode, std::vector<AStarNode>, AStarCompare> queue;
    cost[source] = 0;
    queue.push({heuristic(source, destination, direction_limit), 0, source});
    const int direction_step = direction_limit == 8 ? 1 : 2;
    std::uint64_t expanded = 0;

    while (!queue.empty()) {
        const AStarNode current = queue.top();
        queue.pop();
        if (closed[current.grid_offset] || current.cost != cost[current.grid_offset]) {
            continue;
        }
        closed[current.grid_offset] = 1;
        ++expanded;
        if (current.grid_offset == destination) {
            break;
        }
        for (int direction = 0; direction < kDirections; direction += direction_step) {
            const int next = current.grid_offset + map_grid_direction_delta(direction);
            if (!raw_passable(next, profile) || closed[next]) {
                continue;
            }
            if (profile == WaterNavigationProfile::Boat &&
                state.boat_component[next] != state.boat_component[source]) {
                continue;
            }
            const int next_cost = current.cost + step_cost(next, profile);
            if (next_cost >= cost[next]) {
                continue;
            }
            cost[next] = next_cost;
            predecessor_direction[next] = static_cast<std::int8_t>(direction);
            queue.push({next_cost + heuristic(next, destination, direction_limit), next_cost, next});
        }
    }

    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_ASTAR_EXPANDED_NODES,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        expanded);
    if (predecessor_direction[destination] < 0) {
        return false;
    }

    std::vector<std::uint8_t> reverse;
    int current = destination;
    while (current != source) {
        const int direction = predecessor_direction[current];
        if (direction < 0) {
            return false;
        }
        reverse.push_back(static_cast<std::uint8_t>(direction));
        current -= map_grid_direction_delta(direction);
    }
    directions->assign(reverse.rbegin(), reverse.rend());
    return true;
}

} // namespace

namespace water_navigation {

void invalidate_topology()
{
    state.topology_dirty = true;
}

void invalidate_dock_endpoints()
{
    state.dock_endpoints_dirty = true;
}

void invalidate_river_anchors()
{
    state.river_anchors_dirty = true;
}

void begin_world_load()
{
    ++state.world_load_depth;
    state.topology_dirty = true;
    state.river_anchors_dirty = true;
    state.dock_endpoints_dirty = true;
}

void finish_world_load()
{
    if (state.world_load_depth > 0) {
        --state.world_load_depth;
    }
    if (state.world_load_depth == 0) {
        rebuild_dirty_state();
    }
}

bool is_passable(int grid_offset, WaterNavigationProfile profile)
{
    ensure_ready();
    return raw_passable(grid_offset, profile);
}

bool find_path(
    const map_point &source,
    const map_point &destination,
    WaterNavigationProfile profile,
    int direction_limit,
    std::vector<std::uint8_t> *directions)
{
    if (!directions || !map_grid_is_inside(source.x, source.y, 1) ||
        !map_grid_is_inside(destination.x, destination.y, 1)) {
        return false;
    }
    ensure_ready();
    const int source_offset = map_grid_offset(source.x, source.y);
    const int destination_offset = map_grid_offset(destination.x, destination.y);
    if (const DistanceField *field = reusable_field(source, destination, profile)) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_FIELD_LOOKUPS,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
        if (build_path_from_field(
                source_offset, destination_offset, *field, direction_limit, directions)) {
            return true;
        }
    }
    return astar(source_offset, destination_offset, profile, direction_limit, directions);
}

int path_length(
    const map_point &source,
    const map_point &destination,
    WaterNavigationProfile profile)
{
    PerformanceTrackerRouteScope route_scope(PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);
    if (!map_grid_is_inside(source.x, source.y, 1) ||
        !map_grid_is_inside(destination.x, destination.y, 1)) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
        return 0;
    }
    ensure_ready();
    const int source_offset = map_grid_offset(source.x, source.y);
    const int destination_offset = map_grid_offset(destination.x, destination.y);
    DistanceField *field = cached_destination_field(destination_offset, profile, true);
    if (!field) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
        return 0;
    }
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_FIELD_LOOKUPS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);
    std::vector<std::uint8_t> directions;
    if (!build_path_from_field(source_offset, destination_offset, *field, 8, &directions)) {
        performance_tracker_record_route_metric(
            PERFORMANCE_TRACKER_ROUTE_METRIC_FAILED,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
            1);
        return 0;
    }
    return static_cast<int>(directions.size());
}

bool can_reach_adjacent(
    const map_point &source,
    const std::vector<int> &foundation_water_cells,
    WaterNavigationProfile profile)
{
    if (foundation_water_cells.empty() || !map_grid_is_inside(source.x, source.y, 1)) {
        return false;
    }
    ensure_ready();
    const int source_offset = map_grid_offset(source.x, source.y);
    DistanceField *field = nullptr;
    const map_point entry = scenario_map_river_entry();
    if (scenario_map_has_river_entry() && same_point(source, entry)) {
        field = anchor_field(true, profile);
    } else {
        field = cached_destination_field(source_offset, profile, true);
    }
    if (!field || !field->valid()) {
        return false;
    }
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_WATER_FIELD_LOOKUPS,
        PERFORMANCE_TRACKER_ROUTE_PURPOSE_WATER,
        1);

    static const int neighbor_x[] = {0, 1, 0, -1};
    static const int neighbor_y[] = {-1, 0, 1, 0};
    for (const int water_cell : foundation_water_cells) {
        const int water_x = map_grid_offset_to_x(water_cell);
        const int water_y = map_grid_offset_to_y(water_cell);
        for (int index = 0; index < 4; ++index) {
            const int x = water_x + neighbor_x[index];
            const int y = water_y + neighbor_y[index];
            if (!map_grid_is_inside(x, y, 1)) {
                continue;
            }
            const int grid_offset = map_grid_offset(x, y);
            if (std::find(foundation_water_cells.begin(), foundation_water_cells.end(), grid_offset) !=
                foundation_water_cells.end()) {
                continue;
            }
            if (raw_passable(grid_offset, profile) &&
                (grid_offset == source_offset || field->distance[grid_offset] > 0)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace water_navigation
