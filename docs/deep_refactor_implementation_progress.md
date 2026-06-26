# Deep Refactor Implementation Progress

Snapshot: 2026-06-26, branch `Deep-Refactors-Part2`

This is the live checklist for the current implementation push. Check a box only when the item is verified in the current branch by source, docs, build output, or runtime test feedback. Keep the detailed design in the individual plan docs; use this file to avoid losing the thread.

Hard line-count target: net changed lines from `master` should move toward `-5k`. Prefer slices that remove compatibility branches, duplicated helpers, and legacy paths once their object/XML-owned replacement is in place. This checkout does not currently have a local `main` ref; `origin/HEAD` points to `origin/master`.

## Coordination

- [x] Start from clean branch checkpoint `Deep-Refactors-Part2`.
- [x] Re-read active architecture and plan docs before implementation.
- [x] Spawn scoped agents for independent inspections:
  - Arendt: routing and movement.
  - Halley: figure-owned native graphics.
  - Pasteur: renderer performance groundwork.
- [x] Reordered the working sequence around prerequisites: metrics and facade seams first, movement units before weighted routing, figure-owned graphics before half-size figures, command-list/Vulkan after request contracts are stable.
- [x] Define first-slice public contracts before broad implementation: `FigureGraphics`, `CityViewRenderPhase`, legacy movement helpers, and `RoutePolicy`.
- [x] Implement first safe scaffolding slices by source: render metrics, figure graphics facade, and legacy movement constants.
- [x] Build Release x64.
- [x] Deploy a runtime build for manual regression testing.
  - Standard deploy script dry-run passed, but the real delete-copy deploy hit a persistent lock on the target `Mods\Vespasian\BuildingType` directory.
  - Restored safely with non-destructive `robocopy /E` into the target Mods folder, then copied `Vespasian.exe` and `Vespasian.pdb`; repo and target Mods file counts both verified at 1163.
  - Latest deployed build verified as `D:\Games\GOG Games\Caesar 3\Vespasian.exe`, timestamp 2026-06-26 07:14:13, size 5,126,144 bytes.
- [ ] Record manual regression findings and update this tracker.
- [x] Baseline branch identified as `master`.
- [x] Initial branch delta checked: `git diff --shortstat master...HEAD` reports 15 files, 67 insertions, 62 deletions before new implementation work.
- [x] Track `git diff --shortstat master...HEAD` and working-tree doc/code churn before each deployment.
  - After the first deployed scaffolding build, tracked working-tree diff was 32 files, 475 insertions, 328 deletions. This excludes untracked new files, so the true pending line count is higher; next cleanup slices should bias toward deletions.
  - After the fixed logical-size and water-route cleanup build, tracked working-tree diff was 40 files, 544 insertions, 384 deletions, still excluding untracked new files.
  - Untracked additions at that checkpoint total 292 lines: progress tracker 149, `FigureGraphics` 92, `RoutePolicy` 51.
  - After the deletion-focused route/figure/renderer cleanup build, tracked working-tree diff was 40 files, 527 insertions, 523 deletions, still excluding untracked new files.
  - After figure fixed-size migration and renderer cleanup, tracked working-tree diff was 40 files, 526 insertions, 535 deletions, still excluding untracked new files.
  - After route reuse/backoff, render traversal consolidation, figure graphics mutator cleanup, and formation capacity pass, tracked diff was 49 files, 789 insertions, 596 deletions, with 298 untracked lines.
  - After the route/formation/figure-graphics cleanup agents plus `building/count.cpp` C++ cleanup, tracked diff was 52 files, 900 insertions, 799 deletions, with 265 untracked lines.
- [x] Captured movement/render coupling note: `15` was also a source-pixel/logical-pixel/zoom assumption; future movement must feed logical coordinates to the camera instead of pixel-scale offsets.
- [x] Captured logical-size grain decision: city-view logical sizes should use a fine-grained integer/fixed-point unit instead of authored floats.

## Already Verified Baseline

- [x] `src/building/building_object.h` is absent; `src/building/building.h` is the public `Building` object header.
- [x] No source include of `building_object.h` remains.
- [x] Vespasian Hippodrome XML declares finished graphics, phased construction graphics, and composed parts with rotation-aware offsets.

## Object-Owned Runtime Architecture

Source plan: `docs/object_owned_runtime_refactor.md` and `docs/building_reference_runtime_architecture.md`

- [x] Building object header split removed.
- [ ] Convert remaining building-facing C boundaries to C++ when touched.
  - First cleanup pass: `building/count.cpp` now uses a shared `std::vector`-backed unique-area counter instead of duplicated `malloc`/`memset`/`free` loops.
- [ ] Replace raw `building *` compatibility wrappers with `Building` object calls.
- [ ] Replace repeated type/string scans with typed runtime lists.
  - First cleanup pass: `building/figure.cpp` no longer carries a local fort string detector; `building/count.cpp` now has one local fort attr/figure mapping instead of separate fort type and fort figure tables.
- [ ] Move cleanup to owner lifecycle events instead of downstream repair scans.
- [ ] Remove `event_data attr` as a behavior selector outside compatibility paths.
- [ ] Remove normal-runtime id lookups where object references are available.
- [ ] Delete compatibility accessors that only wrap one-line object/type calls.

## Routing Cost Map Scalability

Source plan: `docs/routing_cost_map_scalability_plan.md`

- [x] Slice 0: add route-specific performance counters.
- [x] Slice 1: verify stale `PathingMode::canTravel()` cleanup target: the method no longer exists in this branch.
- [ ] Slice 1: finish splitting `PathingMode` policy creation from route planning.
- [x] Slice 1: add `Route::Planner::canReach()` facade over the legacy backend.
- [x] Slice 1: replace public `Route::Surface` with `RoutePolicy`; keep legacy surface mapping private to the backend.
- [ ] Slice 1: move direct `map_routing_citizen_can_travel_*` callers behind the planner facade.
- [x] Slice 1 cleanup: consolidate water open-access reachability behind `Route` and delete the terrain helper that read the ambient global routing grid.
- [x] Slice 1 cleanup: delete unused route result/path scaffolding and remove duplicated policy-to-legacy-surface mapping.
- [x] Slice 1 cleanup: move route policy equality/direction-limit logic into `RoutePolicy` and remove matching one-off route helpers.
- [ ] Slice 2: add route destination/policy/epoch stamps to routed figures.
- [x] Slice 2: add route-owned destination/policy intent stamps to routed figures.
- [x] Slice 2: reuse existing figure routes until destination/policy changes or next-tile validation fails.
- [x] Slice 2: add first failed-route retry/backoff gate.
  - Read pass finding: successful routes are already reused through route ids/cursors; the hot churn is failed or invalidated paths that leave `routing_path_id == 0` and retry on the next movement opportunity.
  - First safe target: route-owned intent stamp on `FigureRoute`, plus a reuse/stale-path gate before `Route::add()`.
  - Implemented gate skips one identical failed retry from the same source tile and same route intent, then retries normally.
- [ ] Slice 3: add building-owned road access caches.
- [ ] Slice 3: replace vector-returning road-access call sites with cached spans or equivalent.
- [ ] Slice 3: convert local workforce scans to dirty/runtime-list driven refresh.
- [ ] Slice 4: introduce local cost-map objects and generation-stamped buffers.
- [ ] Slice 4: convert `Route::DistanceQuery` to hold a cost-map handle instead of reseeding the global grid.
- [ ] Slice 5: move passability grids into `RouteWorld`.
- [ ] Slice 5: make terrain/passability updates dirty-region based.
- [ ] Slice 6: add snapshot-safe async routing with a startup thread pool.
- [ ] Slice 7: retire the legacy global `map_routing_distance_grid` API.
- [ ] Slice 7: remove duplicate pathfinding functions outside the route planner.

## Figure Movement Precision And Weighted Routing

Source plan: `docs/figure_movement_precision_and_weighted_routing_plan.md`

- [x] Slice 1: introduce named legacy movement constants and conversion helpers without changing runtime scale.
- [x] Slice 1: remove naked `15` assumptions from normal figure movement and cross-country setup call sites.
- [x] Slice 1: remove naked `15` assumptions from figure rendering offsets.
- [ ] Slice 1: expose normalized progress/logical offsets to rendering while preserving legacy output.
- [ ] Slice 2: version and migrate save progress values to 128-unit tile progress.
- [ ] Slice 2: convert cross-country movement math to 32-bit-safe units.
- [ ] Slice 3: make figure rendering consume logical coordinates through camera transforms instead of source-pixel offsets.
- [ ] Slice 4: convert remaining uniform distance fields to weighted Dijkstra.
- [ ] Slice 4: derive route cost from inverse movement speed.
- [ ] Slice 5: add XML-owned movement surface declarations.
- [ ] Slice 5: make FigureType pathing policies declare allowed surfaces and speeds.
- [ ] Slice 6: cache cost maps by policy, endpoints/target set, and route-world epochs.
- [ ] Remove hardcoded highway bonus branches.
- [ ] Remove gameplay branches that identify movement by concrete building or terrain name.

## Figure-Owned Native Graphics

Source plan: `docs/figure_owned_native_graphics_plan.md`

- [x] Slice 1: add `FigureGraphics` facade without behavior change.
- [x] Slice 1: add a figure draw request type for base slice plus overlays.
- [x] Slice 1: route `city_figure.cpp` through the facade first while keeping legacy fallbacks.
- [x] Slice 1 cleanup: remove unused logical-size accessors and one local render-domain helper after the facade landed.
- [x] Slice 1 cleanup: delete `figure_runtime_graphic_draw_request(...)` compatibility wrapper and call `FigureGraphics` directly from city figure drawing.
- [x] Slice 1 cleanup: remove figure graphics types from `figure_runtime_api.h` and make `figure_runtime_bind_profile(...)` private.
- [x] Slice 1 cleanup: consolidate duplicate native figure layer draw loops.
- [x] Slice 1 cleanup: hide XML graphics-policy image mutation behind `FigureGraphics` and delete exported native compatibility symbol.
- [x] Slice 1 cleanup: remove tiny draw-request accessors, forward-declare `Figure` in graphics headers, and delete the one-function `figure_graphics.cpp` translation unit.
- [ ] Slice 1: add debug counters/logging for native figure draw versus legacy fallback.
- [ ] Slice 2: add cached native payload bindings for runtime-bound figures.
- [ ] Slice 2: use explicit logical width/height in figure draw requests.
- [ ] Slice 3: add structured child-node FigureType graphics parsing.
- [ ] Slice 3: validate figure graphics targets at FigureType load time.
- [ ] Slice 4: retire controller-owned `f->image_id` mutation in converted figure controllers.
- [ ] Slice 5: move cart/resource/animal/standard overlays into figure graphics layers.
- [ ] Slice 6: add Vespasian half-size FigureType XML overrides using existing source art.
- [ ] Slice 7: delete legacy figure image-id arithmetic and duplicate corpse/direction/cart tables.

## Renderer Scaling Seams

Source plan: `docs/renderer_scaling_seam_plan.md`

- [ ] Add focused terrain render test matrix for scale filters, grid state, zoom, and atlas/native paths.
- [ ] Remove grid-rendering tile-size mutation.
- [ ] Introduce exact city-tile destination geometry with shared rounded edges.
- [ ] Add temporary atlas edge padding while atlas fallback remains.
- [ ] Move terrain, water, and climate images into managed native image resources.
- [ ] Split source pixel dimensions from fixed-point logical image dimensions.
- [ ] Add pixel checks for terrain/water seams.
- [ ] Add Vespasian half-size figures after figure-owned native graphics lands.

## Render Performance And Vulkan Direction

Source plan: `docs/render_performance_plans.md`

- [x] Plan A: split city draw metrics into top, figure, animation, and submission buckets.
- [x] Plan A: add first renderer request/source metrics: render requests, managed image requests, texture misses, tile rows, and rendered tiles.
- [x] Plan A cleanup: consolidate one old visible map-tile traversal through the shared row helper.
- [ ] Plan A: extend metrics to native cache and legacy image buckets if still useful after command-list work.
- [x] Plan A cleanup: consolidate renderer logical-size fallback and repeated legacy texture-request setup.
- [ ] Plan A: collapse repeated visible-row traversal overhead.
- [x] Plan A cleanup: delete the obsolete three-callback render-row overload and convert callers to `CityViewRenderPhase` arrays.
- [ ] Plan A: add city draw command prepass carrying object references.
- [ ] Plan A: replace broad native graphics signatures with dirty flags or generation counters.
- [ ] Plan B: add renderer-facing command structs and recording mode behind existing draw API.
- [ ] Plan B: batch obvious same-texture UI/terrain/building runs where ordering allows.
- [ ] Plan B: measure draw call count, texture switches, and command-list build time.
- [ ] Plan C: add renderer snapshots with explicit revisions.
- [ ] Plan C: add thread-pool-backed render preparation.
- [ ] Plan C: create Vulkan backend behind the renderer interface.
- [ ] Plan C: move terrain/building/figure/UI resources into persistent GPU resources.
- [ ] Plan C: add fixed-point logical-size/image-position contract for city-view instance data.
  - First seam identified: add fixed-point logical-size fields at the renderer request boundary while preserving current float fallback.
  - First seam implemented: renderer requests now carry optional fixed logical size with 6 logical units per pixel, while old float fields remain as fallback.
  - Figure graphic draw requests now use `render_logical_size` instead of float logical width/height.
  - The six-units-per-pixel bridge is transitional; the final city-view logical-size unit should be much finer and integer-authored so ratios such as 1/2, 1/3, and 6.67x do not require floats.
- [ ] Plan C: add orthographic camera and conservative depth model.
- [ ] Plan C: add HDR scene target and output selection.
- [ ] Plan C: add shader-side lighting/material policies.
- [ ] Plan C: delete immediate city draw hot paths after parity.

## BuildingType Native Draw Strategy

Source plan: `docs/building_type_native_draw_strategy_future_slice.md`

Current source audit note, 2026-06-26: this plan is mostly implemented on the data-contract side. Remaining work is mainly deletion and replacement of preview/overlay/live-draw compatibility branches that still name concrete special cases.

- [x] Add BuildingType graphics target, variant, layer, option, condition, and construction-phase graphics XML support.
  - Parser evidence: `building_type_registry_xml.cpp` accepts `graphics/default/variant/layer/options/condition`, `construction/phase`, `resource_storage`, and validates runtime graphics.
  - Runtime evidence: `BuildingType::resolve_graphics_target_for_image(...)` chooses construction-phase graphics before normal graphics; `GraphicsDefinition::draw_footprint/draw_top/draw_animation(...)` consumes native runtime slices.
- [x] Add BuildingType orientation/simple option selection support.
  - Parser/runtime evidence: graphics options support `stable_variant`, `build_rotation`, `connectable`, `storage_load`, `orientation`, and `production_progress`.
  - XML evidence: Vespasian uses `selection="orientation"` for dock/shipyard/wharf/forts, `selection="build_rotation"` for hippodrome pieces, `selection="connectable"` for paths/walls, and `selection="production_progress"` for farm fields.
- [x] Move multi-part previews to authored part types, offsets, sizes, and draw order.
  - Source evidence: `ConstructionPlacementPlan::build()` consumes `BuildingType::composition()` parts and rotation offsets before adding the main part; composed XML is reference-resolved and footprint-validated.
  - XML evidence: Vespasian hippodrome, warehouse, and farms author `composed` parts and offsets/roles.
- [x] Move first network/water/building model facts to native strategy data while keeping C++ terrain/network algorithms.
  - Source/XML evidence: `BuildingType` parses tool kinds, drag rotation, shoreline/open-water foundation cells, and `water_access`; Vespasian reservoir/aqueduct/dock XML declares those facts.
- [ ] Finish farm draw cleanup.
  - Data side exists: Vespasian farm buildings are composed from field part types, and farm fields use `production_progress` graphics options.
  - Remaining: `city_with_overlay.cpp` and `city_without_overlay.cpp` still branch on `building.type->is_farm()` for footprint/top/column/mothball drawing.
- [ ] Finish storage and tile composite cleanup.
  - Data side exists: warehouse is a composed 3x3 building with `warehouse_space` parts; granary top/resource layers are native graphics; `resource_storage` exists for warehouse space graphics.
  - Remaining: warehouse/granary overlay ornaments and gatehouse/garden-gate/road-surface branches still live outside generic BuildingType strategy data.
- [ ] Finish network/water preview graphics cleanup.
  - Remaining: `city_building_ghost.cpp` still has procedural `draw_draggable_reservoir`, `draw_aqueduct`, `draw_bridge`, and `draw_road` branches. Keep terrain/network algorithms in C++, but move selected graphics/model facts out of these hardcoded branches where possible.
- [ ] Delete compatibility branches made obsolete by strategy data.
  - Remaining examples: native draw fallback logging in `animations.cpp`, direct `resource_storage` draw branches, farm overlay branches, gatehouse branch, and reservoir/aqueduct/bridge/road ghost branches.

## Unit And Formation XML

Source plan: `docs/unit_and_formation_xml_plan.md`

- [ ] Add `Mods/<mod>/UnitType` registry.
- [ ] Add `Mods/<mod>/FormationType` registry.
- [ ] Author Julius/Augustus 4x4 legacy-compatible formations.
- [ ] Author Vespasian 8x8 formations.
- [ ] Add `military/formation` references to fort BuildingType XML.
- [ ] Add `FormationInstance` ownership to forts.
- [ ] Replace hardcoded formation-size loops with formation instance iteration.
  - First step: added `formation_slot_capacity(...)`, `formation_figure_count(...)`, `formation_has_open_slot(...)`, `formation_is_full(...)`, and `formation_overflow_count(...)`, then moved obvious runtime soldier/enemy/herd iteration, barracks recruitment fullness, recruit capacity, and overflow checks off direct `MAX_FORMATION_FIGURES`.
- [ ] Move soldier stats and combat abilities into `UnitType`.
- [ ] Bridge old saves into declared fort formations.
- [ ] Delete legacy constants that assume 16 soldiers.
