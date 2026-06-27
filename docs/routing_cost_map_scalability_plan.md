# Routing Cost Map Scalability Plan

Snapshot: 2026-06-26

This note records the next routing performance refactor direction after the
route consolidation work. The current build is close enough to the target
baseline that the next gains should come from architecture, not another pass of
local micro-fixes.

## Goals

- Generate routing cost maps only when a caller genuinely needs one.
- Keep figures on their current route until the route becomes invalid, their
  destination changes, or a behavior explicitly requests a new route.
- Make cost-map generation local, cacheable, and eventually thread-safe.
- Move route ownership toward runtime objects, matching
  `docs/object_owned_runtime_refactor.md`.
- Preserve existing gameplay semantics: roadblocks, highway permissions,
  gardens, water routing, walls, construction checks, ghosts, and XML-defined
  pathing modes.

## Baseline Notes

The current and historical logs must be compared by timestamp. The useful
references are:

- `vespasian-performance post-refactor 1 with large city and version 10.log`,
  last written on 2026-06-18 at 7:06 AM.
- `vespasian-performance.log`, last written on 2026-06-26 at 2:25 AM during
  this planning pass.

The samples are not directly comparable by raw bucket totals because the number
of processed ticks per sample differs. A quick per-processed-tick parse showed
today's current active samples around `0.65 ms/tick`, with `figure` around
`0.35 ms/tick`; the 2026-06-18 large-city baseline was around `0.53 ms/tick`,
with `figure` around `0.12 ms/tick`. Treat these as directional only. The
first slice below should add route-specific counters so future comparisons do
not infer routing behavior from broad `figure` and `advance` buckets.

## Current Findings

- `src/map/routing.h/.cpp` still exposes one global mutable
  `map_routing_distance_grid`. Almost every route or reachability query
  overwrites it.
- `src/figure/PathingMode.cpp::canTravel()` still validates a trip by calling
  `map_routing_*` functions that seed the global cost map. A method shaped like
  a predicate still has global routing side effects.
- `src/figure/route.cpp::Route::add()` depends on the last generated global
  distance grid to build a route path. This couples validation, cost-map
  generation, and path extraction.
- `src/figure/route.cpp::Route::DistanceQuery` improved destination selection
  by seeding the distance grid once per query, but it still relies on the same
  singleton cost map and is not reentrant.
- `src/figure/movement.cpp` already performs next-tile validation while figures
  walk. That is the natural lazy invalidation point: a figure can keep its route
  until the next tile no longer matches the active route policy.
- `src/building/local_workforce.cpp` refreshes all local workforce access
  scores every tick from `game_tick_run()`, and several local workforce queries
  scan all buildings. This defeats lazy routing even when few things changed.
- `src/map/road_access.cpp` repeatedly allocates vectors for tiny road-access
  candidate lists. These candidates belong on the building or composed footprint
  that owns the access relationship.
- `src/map/routing_terrain.cpp` rebuilds full terrain passability grids and
  still performs type/string checks for some hot-derived state. Passability
  should become compact tile flags derived from `BuildingType` and map state.
- `src/map/road_network.cpp` clears and rebuilds all road networks on a schedule
  with a fixed queue. Road networks should be epoch-based and dirty-region
  driven after road and roadblock mutations.
- The existing performance tracker is useful but too coarse for this phase. We
  need route attempts, cost-map generations, cache hits, pruned route checks,
  async queue depth, and per-policy route counters.

## Target Shape

### Route Policy

`PathingMode` should stop generating routes. Its hot-path role should be to
produce a compact, immutable `RoutePolicy`:

- passability mask: road, highway, garden/passable terrain, water, wall,
  clear terrain, building pass-through;
- roadblock permission;
- movement neighborhood: 4-way or 8-way;
- terrain costs or bucket ids;
- maximum search radius when known;
- whether the route is strict-road, road-preferred, water, wall, enemy, or
  construction-only.

The XML-selected pathing mode remains the authoring surface. Code should
consume a policy object rather than ask string/type questions inside the search.

### Route World

Add a `RouteWorld` or equivalent owner for routing-derived map data:

- compact tile passability flags;
- terrain, roadblock, water, wall, and road-network epochs;
- dirty tile and dirty rectangle queues;
- immutable snapshots for worker threads in later slices;
- helper methods such as `policy.canEnter(offset)` and
  `policy.cost(offset)`.

This should replace the current loose set of global `terrain_land_citizen`,
`terrain_land_noncitizen`, `terrain_water`, and `terrain_walls` grids once the
callers migrate.

### Route Planner

Move cost-map generation behind a planner object:

- `planRoute(request) -> RouteResult`
- `canReach(request) -> bool`
- `distanceField(request) -> CostMapHandle`
- `nearestCandidate(request, candidates) -> RoadResult`

`RouteRequest` should include source, destination or target set, `RoutePolicy`,
maximum distance, and a purpose enum for telemetry. `RouteResult` should include
the path, distance, and the route-world epochs it was generated against.

### Cost Map Cache

Cost maps should become cache entries, not process-global scratch state.

Good first keys:

- route policy key;
- source offset for single-source fields;
- target-set id for reverse fields;
- terrain, roadblock, road-network, water, and wall epochs used by the policy;
- maximum radius bucket when the caller has a strict limit.

Good invalidation rules:

- road placement/removal increments road and terrain epochs;
- roadblock changes increment a roadblock epoch, ideally per permission mask;
- building placement/removal increments terrain and affected access epochs;
- water or bridge changes increment water epochs;
- wall changes increment wall epochs.

Use an LRU or per-frame/per-tick arena first. Keep the interface small enough
that the cache can later become thread-local or snapshot-backed.

### Figure Route Lifecycle

Figures should not ask for a new route every time a controller wonders whether
the destination is reachable.

Each routed figure should own:

- current path id or native path storage;
- current path index;
- destination identity and destination epoch;
- route policy key;
- route-world epochs used to build the route;
- retry/backoff state after failed route attempts.

Re-path only when:

- destination changes;
- current route is absent;
- next-tile validation fails in movement;
- owner behavior explicitly invalidates the destination;
- route policy changes;
- a debug or forced repair path asks for a refresh.

Do not invalidate every route when terrain changes. Lazy validation of the next
tile is usually enough. For rare cases where a destination itself disappears,
the destination owner should notify or mark its linked figures dirty.

## Cheap Cost Maps

### Algorithm Choices

- Use BFS for uniform-cost route policies.
- Use Dial's bucketed queue for small integer costs such as road/highway/garden
  variants. Avoid a binary heap until costs truly require it.
- Use bounded searches when the caller has `max_roam_length`,
  `max_distance`, or a small placement radius.
- Stop early once all requested candidates have known distances.
- Precheck road-network ids before searching when both endpoints are road-bound
  and the policy cannot leave the road network.
- Use reverse fields for "many sources to nearest target" problems. Labor,
  maintenance access, venue seekers, and nearest storage often want the nearest
  eligible target, not many independent point-to-point routes.

### Memory Shape

- Use `uint16_t` or `int16_t` distances where map limits allow it.
- Use generation stamps instead of clearing whole grids for every route.
- Keep frontier queues, visited stamps, and distances in reusable planner
  scratch buffers.
- Prefer aligned, contiguous arrays over nested structs in inner loops.
- Avoid `std::vector` allocation in candidate collection. Use building-owned
  small buffers, spans, fixed arrays, or per-thread scratch vectors.
- Dispatch on route policy outside the inner loop. Inner loops should be
  branch-light and free of string/type lookups.

### AVX-Friendly Direction

The first win is data layout, not hand-written SIMD. After passability is a
compact flag grid:

- full-grid passability rebuilds can classify several tiles at a time;
- dirty-region rebuilds can copy/fill compact arrays faster;
- candidate scans can compare batches of distances;
- road-service or coverage recency scans can use vectorized min/max later.

Do not start with AVX inside BFS neighbor expansion. Start by making the data
contiguous, aligned, and branch-light so the compiler and future explicit SIMD
both have something sane to work with.

## Lazy Runtime Upgrades

### Local Workforce

Current problem: `game_tick_run()` calls
`building_local_workforce::refresh_access_scores()` every tick, which scans all
buildings even if no relevant roads, roadblocks, houses, workforce settings, or
industries changed.

Target:

- workforce buildings register in a typed runtime list;
- houses with unemployed workers register in a house labor-source list;
- buildings cache their access road candidates;
- road/roadblock/house/workforce mutations mark affected sources and workplaces
  dirty;
- refresh work is time-sliced or event-driven;
- nearest-house lookup uses a reverse field from eligible unemployed house
  roads, grouped by road network and permission.

### Road Access

Current problem: access candidates are recomputed as vectors from footprint
areas.

Target:

- `Building` owns a `RoadAccessCache`;
- composed buildings build one combined access cache from main and child parts;
- the cache stores candidate offsets, network ids, and permissions;
- adjacent road or roadblock edits dirty only nearby building access caches;
- callers receive a span or small fixed collection instead of a new vector.

### Road Networks

Current problem: `map_road_network_update()` clears the whole grid and
flood-fills every road network on a schedule.

Target:

- road and roadblock placement mark dirty components;
- small additions use local component assignment;
- removals either repair the affected component or schedule a component rebuild;
- large edits can still fall back to a full rebuild;
- road network ids should not be limited by an accidental `uint8_t` ceiling if
  larger maps or dense edits make that fragile.

### Terrain Passability

Current problem: routing terrain updates rebuild whole grids and still use
building type string checks for some derived behavior.

Target:

- `BuildingType` exposes route/passability declarations loaded from XML;
- tile-backed buildings and roadblocks write passability flags when placed;
- dirty tile updates repair only local routing flags;
- full rebuild remains as a debug/save-load repair path, not the normal tick
  path.

### Figure Action Lists

Current problem: `figure_action_handle()` scans all figure slots every tick.

Target:

- keep a live active-figure list;
- add typed lists for route-bound figures, roaming service walkers, storage
  carts, transient figures, enemies, boats, and formations;
- route work can then time-slice or prioritize by list without scanning empty or
  inactive records.

## Threading Plan

Do not add worker threads before route data is reentrant. The current global
distance grid makes concurrent routing unsafe.

Once `RoutePlanner` operates on immutable `RouteWorldSnapshot` data:

- create a small thread pool at startup and shut it down during game shutdown;
- keep simulation mutation on the main thread;
- workers receive snapshots, requests, and scratch buffers;
- workers return immutable route or cost-map results tagged with epochs;
- main thread discards stale results if epochs or destination ids changed;
- UI, ghost validation, and construction placement keep synchronous fast paths;
- non-critical figure route repair, labor access refresh, road-network repair,
  and expensive reverse fields can run asynchronously.

Likely async tasks:

- route batches for figures that hit a retry gate;
- reverse cost maps for labor and service destinations;
- dirty road-network component rebuilds;
- dirty terrain/passability rebuild batches;
- maintenance and water access scans after they are snapshot-safe.

Determinism guardrails:

- route tie-breaking must remain stable;
- worker completion order must not change saved game state directly;
- async results should be optional improvements, not required for save/load;
- stale async results must be dropped silently and counted in telemetry.

## Slice Plan

### Slice 0: Instrument First

- Add route-specific counters to the performance tracker:
  `route_requests`, `route_plans`, `route_cost_maps`, `route_cache_hits`,
  `route_cache_misses`, `route_pruned_by_network`, `route_pruned_by_current_path`,
  `route_failed`, and `route_async_jobs`.
- Count by route purpose: movement, distance query, local workforce, venue,
  storage, construction, water, wall, debug.
- Include counters in `vespasian-performance.log` only when the tracker is
  enabled.
- Keep the old broad buckets so historical logs remain readable.

### Slice 1: Split Policy From Planning

- Replace `PathingMode::canTravel()` side effects with
  `PathingMode::routePolicy()`.
- Add `RoutePlanner::canReach()` and `RoutePlanner::planRoute()` wrappers that
  still use the legacy backend internally.
- Move all direct `map_routing_citizen_can_travel_*` callers behind the planner
  facade.
- Preserve C-compatible wrappers only at subsystem boundaries that have not
  migrated yet.

### Slice 2: Figure Route Reuse

- Add destination/policy/epoch stamps to routed figures or native route records.
- Ensure destination mutation routes through one method that invalidates the
  route once.
- Let movement reuse current paths until next-tile validation fails.
- Add failed-route backoff so blocked figures do not retry the same impossible
  route every tick.
- Audit `Route::remove()` callers and split "destination changed", "route
  invalid", and "figure died" semantics where needed.

### Slice 3: Candidate Caches

- Add building-owned road access caches.
- Replace vector-returning `map_road_access_candidates()` call sites with spans
  over cached candidates.
- Dirty access caches from nearby road, roadblock, and building placement
  events.
- Convert local workforce house/workplace scans to typed runtime lists.

### Slice 4: CostMapCache

- Introduce local cost-map objects with reusable scratch buffers.
- Add generation-stamped distance and visited arrays.
- Preparation landed: `route.cpp` now reads ambient distances through one
  private backend accessor, and `Route::DistanceQuery` carries a private legacy
  cost-map handle wrapper instead of a raw distance-grid generation integer.
  Its local distance-query APIs and road-candidate selectors now seed and read
  through that handle, including max-distance filtering and legacy generation
  validation; the handle still wraps the global grid until a real local cost
  map lands.
- Remaining `route_distance_at()` bridge users are path reconstruction,
  planner max-tile validation after policy-specific legacy seeding,
  `TerrainQuery`, construction distance reads, and water reachability; moving
  them needs policy-specific cost-map handles rather than the citizen-road
  `DistanceQuery` handle.
- Pathing ownership cleanup: citizen road-network lookup now lives on
  `PathingMode`, so route planning no longer owns a local road-like/network
  helper or includes the road-network map header directly.
- Route-policy cleanup: wall route classification now lives on `RoutePolicy`,
  and route intent performance purposes are derived from the constructed policy
  instead of a separate figure-field helper.
- Cache common fields by policy, source/target set, and epochs.
- Convert `Route::DistanceQuery` to hold a `CostMapHandle` instead of reseeding
  the global grid.
- Add reverse fields for nearest-target searches.

### Slice 5: Dirty RouteWorld

- Move passability grids into `RouteWorld`.
- Replace full terrain rebuilds with dirty tile and dirty rectangle updates.
- Keep full rebuild as a debug/save-load verification tool.
- Remove hot string/type checks from routing terrain classification by moving
  those declarations into `BuildingType`.
- Make road-network updates dirty-component driven.

### Slice 6: Async Routing

- Add a startup thread pool.
- Add immutable `RouteWorldSnapshot`.
- Move non-critical route requests to async jobs with stale-result checks.
- Add route job budgets so the main thread never waits on bulk pathfinding.
- Keep synchronous routing for construction, UI placement validation, and any
  behavior that must resolve immediately.

### Slice 7: Data-Oriented Cleanup

- Retire the legacy global `map_routing_distance_grid` API.
- Remove duplicate point-to-point pathfinding functions outside the route
  planner.
- Replace route C buffers and manual allocation patterns with owned C++ storage.
- Collapse local helper copies that only exist to work around id-based records.
- Make route debug overlays read named debug fields, not "whatever route grid was
  generated last".

## Quick Wins Before The Full Rewrite

- Stop calling `building_local_workforce::refresh_access_scores()` every tick;
  gate it behind dirty state or a time-sliced scheduler.
- Add route failure backoff to figures that repeatedly fail the same route.
- Cache road access candidates on buildings.
- Split `PathingMode::canTravel()` into `RoutePolicy` plus planner call even
  before the planner backend is fully rewritten.
- Count route grid generations by purpose in the performance log.
- Use road-network prechecks before any road-only route search.

## Risks

- Roadblock permissions must be part of every route policy and cache key.
- Highway/garden permissions differ by mod and must remain XML-driven.
- Construction and ghost validation need synchronous answers and cannot depend
  on async completion.
- Boats, flotsam, walls, enemy routes, and construction routing have different
  passability semantics; they should share the planner shape, not necessarily
  the exact same policy table.
- Save/load must not persist worker state or stale cache state.
- Threading should not be introduced until route data is local or snapshot-based.
- Debug overlays and editor tools may currently rely on the global last distance
  grid. Those callers need explicit debug fields.

## Definition Of Done

- Large-city active tick cost is at or below the 2026-06-18 post-refactor
  baseline under comparable speed and viewport conditions.
- Route-specific counters show fewer cost-map generations per processed tick.
- Figures do not re-path unless their current route is absent, invalid, or
  retargeted.
- Local workforce and venue selection do not scan every building every tick.
- Road/roadblock edits invalidate the minimum relevant route/access state.
- Construction and ghost validation still match live placement behavior.
- No new gameplay branches infer behavior from BuildingType names or legacy enum
  slots.
