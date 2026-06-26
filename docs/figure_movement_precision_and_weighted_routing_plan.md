# Figure Movement Precision And Weighted Routing Plan

## Intent

Move figure traversal away from the legacy one-speed, mostly tile-count model and toward two explicit systems:

- route eligibility: which mod-defined movement surfaces a figure may path over
- movement speed: how fast that figure crosses each allowed surface

Route cost should not be separately authored. The cost used by Dijkstra should be the inverse of movement speed, using a fixed-point reciprocal if integer weights are needed. This keeps path choice and on-map movement from drifting apart.

This lets roads, ramps, highways, gardens, open land, bus lanes, train tracks, bicycle paths, dragon routes, or any future transportation type be declared as meaningful movement surfaces without hardcoded special cases. Service walkers can remain road/ramp-only. A figure that declares only roads should still only path on roads; a figure that declares roads plus gardens should path only on roads and gardens. Military and other off-road figures can traverse gardens and open ground only when their pathing policy says they can, and they naturally prefer roads because roads are faster.

The game should move away from asking "is this that building or that legacy terrain bit?" Route code should ask data-owned questions such as "what movement surfaces does this tile expose?" and "what surfaces does this figure policy allow?" Buildings, terrain, and tiles publish surfaces by XML key; figures consume those keys by policy. The engine then resolves the keys into compact ids for runtime performance.

## Current State

Normal walkers store tile progress in `Figure::progress_on_tile` and complete a tile at the hardcoded value `15`. The renderer converts that same progress value directly into pixel offsets. Cross-country walkers use separate `cross_country_x/y` coordinates that are also based on `15 * tile`.

That `15` is more than a movement constant. It encodes the legacy assumption that a source-art pixel, a logical screen pixel, tile-progress math, and zoomed presentation all line up neatly. The high-resolution asset pipeline breaks that assumption: the renderer must be able to draw source art at arbitrary source resolution, preserve a separate logical size, and support fractional logical-to-physical scaling as zoom becomes continuous.

The routing system has two different shapes today:

- point-to-point route searches already have an ordered queue and can express variable costs, currently used for the highway bonus
- many source distance fields still behave like uniform-cost tile-count maps and add one unit per tile

Highways are therefore split between routing, movement, and drawing as a special case. The target shape should remove that tack-on model and represent highways as one mod-defined movement surface among many, with declared movement speed. Route cost is derived from that speed.

## Target Model

Use fixed-point tile progress with a 128-unit tile as a transitional gameplay/runtime representation. The XML-facing movement declaration should be speed, not route cost:

| Surface | Example speed per tick | Notes |
| --- | ---: | --- |
| Highway | 16/128 | Twice road speed, but data-driven |
| Road/ramp | 8/128 | Current baseline feel |
| Garden | 6/128 | Walkable but slower, not for normal service walkers |
| Open land | 5/128 | Slow enough that soldiers prefer roads |

These values are initial targets, not permanent constants. The final implementation should allow FigureType XML or pathing policy declarations to override them. Dijkstra should compute tile-entry cost as the inverse of this speed, for example `route_weight = kRouteWeightScale / speed`, where `kRouteWeightScale` is chosen to preserve useful integer precision.

The 128-unit tile is not the final renderer contract. Movement should expose normalized progress or integer logical world coordinates to rendering, and the renderer should map those values through camera transforms. The long-term Vulkan/orthographic path should support continuous zoom and assets whose source pixels are 2x, 6x, 6.67x, or any future scale relative to their logical footprint. Any fractional-looking authored ratio should be represented in a finer integer grain, not in floats, so city-view positioning and sprite dimensions stay deterministic.

The runtime bit system must serve the tile and transportation definitions, not define them. XML tile declarations should be parsed into stable tile/surface identifiers first, and any compact bitsets should be generated as an internal acceleration structure from those identifiers. This mirrors the water-access XML direction: data owns the semantic model, while bits only make repeated checks cheap.

Hardcoded bit positions such as "road is bit 1" or "highway is bit 2" should disappear from gameplay code. If a mod defines `bus_lane`, `dragon_route`, `train_track`, or `bicycle_path`, the loader assigns ids and builds masks from those declarations. If the number of declared surfaces exceeds one machine word, the implementation should grow to a small dynamic bitset or indexed policy table instead of rejecting the data model.

## Slice 1: Central Movement Units

Add named movement constants and remove naked `15` assumptions from figure motion:

- `kFigureTileProgressMax = 128`
- `kFigureLegacyTileProgressMax = 15`
- helpers to convert legacy save values into the new range
- helpers to convert tile progress to a normalized fraction for rendering

Normal walking should become:

```cpp
progress += speed_for_current_surface(figure);
while (progress >= kFigureTileProgressMax) {
    progress -= kFigureTileProgressMax;
    enter_next_tile();
}
```

Do not implement terrain speed as duplicated branches in each movement function. Put the selection behind a figure/pathing policy method so service walkers, military, animals, boats, buses, trains, bicycles, dragons, and special figures can grow their own policy without scattering rules through `movement.cpp`.

## Slice 2: Save And Runtime Migration

`progress_on_tile` can remain an unsigned byte because 128 fits, but old saves encode progress in the 0..15 range. Add a save version and migrate on load:

```cpp
new_progress = old_progress >= 15 ? 128 : old_progress * 128 / 15;
```

Cross-country state should not remain `short` if the coordinate scale changes to 128. On a 160x160 map, `128 * tile` fits in signed 16-bit only narrowly, and deltas plus Bresenham-style error terms can overflow. Convert these fields or at least the active math to 32-bit:

- `cross_country_x`
- `cross_country_y`
- `cc_destination_x`
- `cc_destination_y`
- `cc_delta_x`
- `cc_delta_y`
- `cc_delta_xy`

This is also the right moment to rename comments and helper names away from "1/15th of a tile" so future sessions do not accidentally preserve the old model.

## Slice 3: Rendering Offsets

The renderer should consume normalized tile progress instead of hardcoded progress values. The screen position should be computed from:

- current tile
- previous tile or direction
- progress fraction
- sprite offset / logical size
- height adjustment

`city_figure.cpp` currently maps progress values directly to pixel offsets. Replace those formulas with a helper that takes a progress fraction. This keeps current road-speed visuals identical while allowing slower/faster surfaces to move smoothly.

Cross-country rendering should use `cross_country_x % kFigureTileProgressMax` and matching conversion helpers rather than `% 15`.

The helper should return logical-space offsets in the chosen integer grain, not source-pixel offsets. The renderer then decides how those logical offsets map to physical pixels through the active camera, zoom, asset logical size, and scale filter. This is the bridge toward continuous zoom and an orthographic z-buffered renderer, where figure quads are interpolated in world/logical space rather than snapped to legacy pixel increments.

## Slice 4: Weighted Dijkstra Cost Maps

Replace the remaining uniform tile-count distance fields with proper weighted Dijkstra maps. The cost map should ask the active route surface/policy for:

- whether the neighbor tile is traversable
- what speed entering that tile has
- whether diagonal travel is allowed
- whether roadblock permissions allow entry

The Dijkstra tile-entry weight should be derived from speed:

```cpp
route_weight = route_weight_scale / surface_speed;
```

Highways should become ordinary movement-surface speed:

- movement speed higher than road, if the active pathing policy allows highways
- no separate "receive_highway_bonus" branch
- no route-specific highway correction in path reconstruction

This gives a single model for highways, roads, ramps, gardens, open terrain, and any mod-defined transportation surface. It also prevents route selection and movement speed from disagreeing.

The route layer should not branch on concrete building or terrain names. It should resolve the neighbor tile to a set of movement surface ids, intersect that with the active figure policy, pick the allowed surface speed, and derive Dijkstra weight from that speed. If a tile exposes multiple allowed surfaces, the policy should define whether to take the fastest surface or an explicit priority order.

## Slice 5: XML-Owned Transportation And Pathing Policy

Extend mod data so transportation surfaces are declared independently from the figures that use them. A target shape could be:

```xml
<movement_surfaces>
  <surface key="road" />
  <surface key="ramp" />
  <surface key="highway" />
  <surface key="garden" />
  <surface key="clear_land" />
  <surface key="train_track" />
  <surface key="dragon_route" />
</movement_surfaces>
```

Terrain, tiles, and buildings then expose one or more of those surfaces by key:

```xml
<tile key="vespasian_highway">
  <movement_surface key="highway" />
</tile>

<building key="train_station">
  <foundation>
    <tile offset="0,0" movement_surface="train_track" />
  </foundation>
</building>
```

FigureType pathing/movement XML should declare which surfaces a figure profile may use and how fast it crosses each allowed surface:

```xml
<pathing mode="military_land">
  <surface key="road" speed="8" />
  <surface key="ramp" speed="8" />
  <surface key="highway" speed="16" />
  <surface key="garden" speed="6" />
  <surface key="clear_land" speed="5" />
</pathing>
```

Service walkers should declare only the surfaces they may use, usually road and ramp. If a service walker does not declare gardens, open land, or highways, those surfaces are not pathable for that figure, even if they have valid speed declarations for other figures. Vespasian can later choose to allow specific service providers on highways by adding that surface to their pathing XML without inventing another hardcoded terrain mode.

The same structure should support arbitrary mod transport:

```xml
<pathing mode="train">
  <surface key="train_track" speed="32" />
</pathing>

<pathing mode="dragon">
  <surface key="dragon_route" speed="24" />
  <surface key="mountain_air" speed="12" />
</pathing>
```

Tile and surface declarations should be name/key driven in XML. The runtime may compress them into generated bitsets or indexed tables, but the mask values should be allocated from the loaded definitions and should never become the authored source of truth. Adding a new tile kind or transportation type should mean adding XML data and speed declarations, then letting the loader populate the relevant masks. C++ gameplay code should consume the resolved policy object, not compare strings or switch on "road/highway/train" names.

## Slice 6: Route Cache And Performance

Weighted Dijkstra is more expressive but can be more expensive if every figure regenerates a full map. It should be paired with the broader routing performance plan:

- cache cost maps by source, permission set, resolved movement policy id, and terrain generation
- invalidate lazily on roadblock/terrain/building changes
- avoid rebuilding routes unless the existing route becomes invalid
- prefer bounded searches for local questions
- keep per-policy speed and derived-cost arrays compact and cache-friendly

This is also an opportunity to make terrain-speed lookup branch-light enough to support SIMD-friendly scans later. The fast path should look up compact tile/surface ids, test them against the active policy mask or table, then load speed and derived reciprocal cost from arrays populated from XML. The branch-light shape is an implementation detail; the authored model remains keys and declarations.

## Risks

- Old save compatibility: progress and cross-country coordinate migration must be versioned.
- Animation timing: several figure actions reuse `progress_on_tile` as a generic timer, so movement progress should be separated from non-movement timers where possible.
- Combat/projectile code: missiles and soldiers use cross-country coordinates and must be tested separately.
- Roamer previews: preview simulation uses the same movement values and must be updated in lockstep.
- Route distance semantics: callers that use `max_tiles` need a decision between "max geometric tiles" and "max route cost".
- Cost precision: reciprocal route weights need enough fixed-point precision that speeds such as 5, 6, 8, and 16 produce stable route choices without making Dijkstra maps too large.
- Surface identity migration: legacy terrain/building constants will need a bridge during migration, but new code should consume resolved surface ids from data-owned definitions.
- Mod scale: dynamic surface definitions mean cache keys and bitsets cannot assume a fixed built-in list of transport types.
- Renderer coupling: movement code must stop returning pixel-scale offsets. Any remaining pixel-sized movement math will become visible when assets use 6x source resolution, fractional zoom, or future continuous camera transforms.

## Recommended Order

1. Introduce named constants and helpers without changing behavior.
2. Convert rendering helpers to normalized progress while preserving 15-step output.
3. Make rendering consume logical offsets/camera transforms instead of source-pixel offsets.
4. Change normal walking to 128-unit progress with road speed matching old behavior.
5. Migrate cross-country fields and save/load.
6. Convert route distance fields to weighted Dijkstra.
7. Replace highway bonus branches with terrain speed declarations and reciprocal route weights.
8. Add XML-owned movement surface declarations and generated runtime ids/masks.
9. Add XML surface speed declarations to FigureType/pathing policy.
10. Remove gameplay branches that identify routes by concrete building or terrain name.
11. Tune gardens/open land/military behavior after runtime testing.
