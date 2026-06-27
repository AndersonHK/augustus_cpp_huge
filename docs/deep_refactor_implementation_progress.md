# Deep Refactor Implementation Progress

Snapshot: 2026-06-26, branch `Deep-Refactors-Part2`

This is the live checklist for the current implementation push. Detailed requirements live in `docs/deep_refactor_requirements.md`; detailed designs live in the linked plan files. This file should stay short enough to scan before coding.

Line-count target: move the net diff from `master` toward `-5k` by deleting legacy branches, duplicated helpers, compatibility accessors, and old id/string bridges after their object/XML-owned replacements are in place.

Current measured state:

- Baseline branch: `master`.
- Committed branch delta: `git diff --shortstat master...HEAD` reports 90 files, 1759 insertions, 1046 deletions.
- Current working-tree delta: `git diff --shortstat` reports 212 files, 4866 insertions, 6252 deletions.
- Latest known deployed runtime build: `D:\Games\GOG Games\Caesar 3\Vespasian.exe`, 2026-06-26 16:42:44.
- Current validation priority: build only from the main session; agents implement/read in disjoint areas.

## Coordination

Requirement: keep work planned, delegated, build-gated, and regression-testable.

- [x] Start from checkpoint branch `Deep-Refactors-Part2`.
- [x] Use agents for disjoint implementation/research slices without letting them compile.
- [x] Keep stable requirements separate in `docs/deep_refactor_requirements.md`.
- [x] Split verbose requirement text out of this tracker.
- [ ] Keep two agents busy with long-lived implementation or cleanup slices.
- [ ] Update this checklist when a slice lands or a dependency order changes.
- [ ] Record manual regression findings only as short current-state notes.

## Validation And Deployment

Requirement: catch parser/startup failures from PowerShell before manual runtime testing.

- [x] Add loud deployment failure handling and Explorer pause behavior to `tools/deploy_release_to_game.py`.
- [x] Add `StartupParserTest` headless startup/XML parser executable.
- [x] Make startup parser tests run from the actual game folder via `--game-root`.
- [ ] Run `StartupParserTest.exe --game-root "D:\Games\GOG Games\Caesar 3"` after the next deploy.
- [ ] Rebuild Release x64 from the main session after agent patches settle.
- [ ] Deploy the next Release x64 build for manual regression testing.

## Runtime DLL Boundary Refactor

Source plan: `docs/runtime_dll_boundary_refactor_plan.md`

Requirement: split extraction, XML startup parsing, and save/load bridge work into strict call-and-unload runtime boundaries.

- [x] Document the DLL boundary intent and shared crash-context rule.
- [x] Shape the startup parser test as the future XML parser DLL caller.
- [ ] Define a tiny stable ABI/shared-core boundary.
- [ ] Extract graphics extraction into a self-contained module boundary.
- [ ] Extract XML startup parsing into a self-contained module boundary.
- [ ] Extract save/load bridging into a self-contained module boundary.
- [ ] Add a save bridge tester for known-good saves.
- [ ] Keep runtime code from depending on module internals after handoff.

## Object-Owned Runtime Architecture

Source plan: `docs/object_owned_runtime_refactor.md`

Requirement: data-owning objects should own their behavior, references, cleanup, and typed relationships.

- [x] Delete `building_type_api.h` compatibility layer.
- [x] Move many one-line BuildingType facts from C accessors to object methods.
- [x] Remove local `building_matches` and `type_matches` helper islands.
- [x] Add the requirement that BuildingType spawn declarations create figure/profile pairs while FigureType profiles own walker behavior.
- [ ] Finish retiring redundant spawn `mode` versus FigureType `profile` behavior split.
- [ ] Convert remaining raw `building *` compatibility boundaries to `Building` object calls when touched.
- [ ] Replace repeated type/string scans with typed runtime lists.
- [ ] Move cleanup to owner lifecycle events instead of downstream repair scans.
- [ ] Remove `event_data attr` as a behavior selector outside compatibility paths.
- [ ] Remove normal-runtime id lookups where object references are available.

## Routing Cost Map Scalability

Source plan: `docs/routing_cost_map_scalability_plan.md`

Requirement: centralize route planning and make cost-map generation lazy, reusable, and policy-owned.

- [x] Add route-specific performance counters.
- [x] Replace public surface routing with `RoutePolicy`.
- [x] Add `Route::Planner::canReach()` facade.
- [x] Move citizen/wall/noncitizen reachability calls behind route facade code.
- [x] Add route-owned destination/policy intent stamps.
- [x] Reuse existing figure routes until intent or next-step validity changes.
- [x] Add failed-route retry/backoff gate.
- [x] Move same-road-network and road-network helper logic into `PathingMode`.
- [x] Add `Route::DistanceQuery::CostMapHandle` bridge over the legacy global grid.
- [ ] Finish splitting `PathingMode` policy creation from route planning.
- [ ] Add building-owned road-access caches.
- [ ] Convert road-access callers to cached spans or equivalent.
- [ ] Convert local workforce scans to dirty/runtime-list driven refresh.
- [ ] Replace ambient global routing grid with local generation-stamped cost maps.
- [ ] Move passability grids into `RouteWorld`.
- [ ] Add dirty-region terrain/passability updates.
- [ ] Add snapshot-safe async routing with startup thread pool.
- [ ] Retire legacy global `map_routing_distance_grid` API.
- [ ] Delete duplicate pathfinding outside the route planner.

## Figure Movement Precision And Weighted Routing

Source plan: `docs/figure_movement_precision_and_weighted_routing_plan.md`

Requirement: movement and rendering should use higher-grain logical positions and route cost should derive from inverse speed.

- [x] Introduce named legacy movement constants and conversion helpers.
- [x] Remove naked `15` assumptions from normal movement and cross-country setup.
- [x] Remove naked `15` assumptions from figure rendering offsets.
- [ ] Expose normalized progress/logical offsets to rendering while preserving legacy output.
- [ ] Version and migrate save progress values to 128-unit tile progress or the chosen finer integer grain.
- [ ] Convert cross-country movement math to 32-bit-safe units.
- [ ] Make figure rendering consume logical coordinates through camera transforms.
- [ ] Convert uniform distance fields to weighted Dijkstra.
- [ ] Derive route cost from inverse movement speed.
- [ ] Add XML-owned movement surface declarations.
- [ ] Make FigureType pathing policies declare allowed surfaces and speeds.
- [ ] Cache cost maps by policy, target set, and route-world epochs.
- [ ] Remove hardcoded highway bonus branches.

## Figure-Owned Native Graphics

Source plan: `docs/figure_owned_native_graphics_plan.md`

Requirement: the city draw loop should ask figures for XML-backed draw requests instead of reconstructing image ids.

- [x] Add `FigureGraphics` facade without behavior change.
- [x] Add figure draw request type for base slice plus overlays.
- [x] Route `city_figure.cpp` through the facade first while keeping fallbacks.
- [x] Add debug counters/logging for native figure draws versus legacy fallback.
- [x] Remove public compatibility wrappers and several one-use graphics accessors.
- [x] Move more legacy image arithmetic onto `Figure` or private native helpers.
- [ ] Add cached native payload bindings for runtime-bound figures.
- [ ] Use explicit logical width/height in figure draw requests.
- [ ] Add structured child-node FigureType graphics parsing.
- [ ] Validate figure graphics targets at FigureType load time.
- [ ] Retire controller-owned `f->image_id` mutation in converted controllers.
- [ ] Move cart/resource/animal/standard overlays into figure graphics layers.
- [ ] Add Vespasian half-size FigureType XML overrides using existing source art.
- [ ] Delete legacy figure image-id arithmetic and duplicate corpse/direction/cart tables.

## Renderer Scaling Seams

Source plan: `docs/renderer_scaling_seam_plan.md`

Requirement: scaling filters and logical sizes must produce exact seamless city-view geometry across native and remaining atlas paths.

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

Requirement: move rendering toward command lists, snapshots, GPU-owned resources, and an eventual Vulkan backend.

- [x] Add city draw metrics for top, figure, animation, submission, render requests, managed image requests, texture misses, tile rows, and rendered tiles.
- [x] Consolidate several visible-row traversal and draw-adapter compatibility paths.
- [x] Add transitional fixed logical-size fields at the renderer request boundary.
- [x] Convert `FigureGraphicDrawRequest` to `render_logical_size`.
- [ ] Extend metrics to native cache and legacy image buckets if still useful.
- [ ] Collapse remaining repeated visible-row traversal overhead.
- [ ] Move status-icon anchors and gatehouse draw mapping into BuildingType/native draw data.
- [ ] Add city draw command prepass carrying object references.
- [ ] Replace broad native graphics signatures with dirty flags or generation counters.
- [ ] Add renderer-facing command structs and recording mode behind existing draw API.
- [ ] Batch obvious same-texture UI/terrain/building runs where ordering allows.
- [ ] Add renderer snapshots with explicit revisions.
- [ ] Add thread-pool-backed render preparation.
- [ ] Create Vulkan backend behind the renderer interface.
- [ ] Move terrain/building/figure/UI resources into persistent GPU resources.
- [ ] Add orthographic camera and conservative depth model.
- [ ] Add HDR scene target and shader-side lighting/material policies.
- [ ] Delete immediate city draw hot paths after parity.

## BuildingType Native Draw Strategy

Source plan: `docs/building_type_native_draw_strategy_future_slice.md`

Requirement: BuildingType XML graphics should drive preview, overlay, and live-building drawing through the same native strategy.

- [x] Add BuildingType graphics target, variant, layer, option, condition, and construction-phase graphics XML support.
- [x] Add orientation/simple option selection support.
- [x] Move multi-part previews to authored part types, offsets, sizes, and draw order.
- [x] Move first network/water/model facts to native strategy data.
- [ ] Finish farm draw cleanup.
- [ ] Finish storage and tile composite cleanup.
- [ ] Finish network/water preview graphics cleanup.
- [ ] Delete compatibility branches made obsolete by strategy data.

## Unit And Formation XML

Source plan: `docs/unit_and_formation_xml_plan.md`

Requirement: forts should own XML-declared formations made from XML-declared combat units, including all combat actors.

- [x] Add `Mods/<mod>/UnitType` registry.
- [x] Add `Mods/<mod>/FormationType` registry.
- [x] Author Julius/Augustus 4x4 legacy-compatible formations.
- [x] Author Vespasian 8x8 formations.
- [x] Add `military/formation` references to fort BuildingType XML.
- [x] Move recruit categories and weapon requirements into `UnitType`.
- [x] Move many formation roster queries and mutations onto `formation`.
- [ ] Add `FormationInstance` ownership to forts.
- [ ] Replace remaining hardcoded formation-size loops with formation instance iteration.
- [ ] Move soldier stats and combat abilities fully into `UnitType`.
- [ ] Bridge old saves into declared fort formations.
- [ ] Delete legacy constants that assume 16 soldiers.

## Current Regression Notes

Requirement: keep only active manual-test notes here; resolved findings belong in git history or focused docs.

- [ ] Actor colonies may spawn actors that never create plays at theaters/amphitheaters.
- [ ] Vespasian theaters should only send actor service walkers when they have plays.
- [ ] Vespasian amphitheaters should only animate/send service walkers when they have plays or gladiator days.
