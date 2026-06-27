# Deep Refactor Implementation Progress

Snapshot: 2026-06-27, branch `Deep-Refactors-Part5`

This is the live checklist for the current implementation push. Detailed requirements live in `docs/deep_refactor_requirements.md`; detailed designs live in the linked plan files. This file should stay short enough to scan before coding.

Line-count target: move the net diff from `master` toward `-5k` by deleting legacy branches, duplicated helpers, compatibility accessors, and old id/string bridges after their object/XML-owned replacements are in place.

Current measured state:

- Baseline branch: `master`.
- Committed branch delta: `git diff --shortstat master...HEAD` reports no committed branch delta.
- Current working-tree delta: `git diff --shortstat` reports 46 tracked files, 756 insertions, 852 deletions, plus `RendererSeamTest` project files and seven new focused boundary/runtime/test sources.
- Latest local Release runtime build: `x64\Release\Vespasian.exe`, 2026-06-27 04:51:02.
- Latest known deployed runtime build: `D:\Games\GOG Games\Caesar 3\Vespasian.exe`, 2026-06-27 03:30:38, deployed as narrow exe/PDB copy after full Mods deploy hit an access-denied lock on `Mods\Augustus`.
- Latest local validation: Release x64 solution build passed; `git diff --check` passed with only LF/CRLF warnings. Previous focused startup/seam checks remain: `StartupParserTest.exe --game-root "D:\Games\GOG Games\Caesar 3"` passed; `RendererSeamTest.exe --game-root "D:\Games\GOG Games\Caesar 3" --matrix terrain-water --artifacts out\renderer_seams` passed with 1 real-pixel software fixture case and 4607 expected skips.
- Current validation priority: solution Release x64 now builds all projects, including `StartupParserTest`.

## Coordination

Requirement: keep work planned, delegated, build-gated, and regression-testable.

- [x] Start from checkpoint branch `Deep-Refactors-Part2`.
- [x] Use agents for disjoint implementation/research slices without letting them compile.
- [x] Keep stable requirements separate in `docs/deep_refactor_requirements.md`.
- [x] Split verbose requirement text out of this tracker.
- [x] Keep two agents busy with long-lived implementation or cleanup slices.
- [x] Update this checklist when a slice lands or a dependency order changes.
- [ ] Record manual regression findings only as short current-state notes.
- [ ] Treat manual-test/deploy milestones as: terrain/water seam pixel checks, Vespasian half-size FigureType XML, deletion of strategy-obsolete compatibility branches, deletion of 16-soldier formation constants, and HDR scene target plus shader-side lighting/material policies.

## Validation And Deployment

Requirement: catch parser/startup failures from PowerShell before manual runtime testing.

- [x] Add loud deployment failure handling and Explorer pause behavior to `tools/deploy_release_to_game.py`.
- [x] Add `StartupParserTest` headless startup/XML parser executable.
- [x] Make startup parser tests run from the actual game folder via `--game-root`.
- [x] Rebuild Release x64 from the main session after agent patches settle.
- [x] Add deploy preflight for running game processes that lock the target Mods folder.
- [x] Close running deployed `Vespasian.exe` before the next deploy attempt.
- [x] Resolve deploy blocker by preserving the `Mods` root and replacing mod folder contents with backup/restore cleanup.
- [x] Deploy the next Release x64 build for manual regression testing.
- [x] Make `StartupParserTest.exe --game-root "D:\Games\GOG Games\Caesar 3"` require `JuliusGraphicsExtractor` and `AugustusGraphicsExtractor` outputs before XML graphics validation.
- [x] Validate post-deploy extraction order: `JuliusGraphicsExtractor`, `AugustusGraphicsExtractor --no-force --stamp`, then `StartupParserTest`.
- [x] Make missing extraction stamps an early, explicit `StartupParserTest` failure with exact extractor commands and no XML parser execution.
- [x] Remove Win32/x86 solution and project configurations; all current projects are x64-only.
- [x] Include `StartupParserTest` in the Release/Debug x64 solution build gate.
- [x] Restore fast deploy runtime by replacing full Mods staging/backup copies with per-run folder-move rollback and direct source copies.

## Runtime DLL Boundary Refactor

Source plan: `docs/runtime_dll_boundary_refactor_plan.md`

Requirement: split extraction, XML startup parsing, and save/load bridge work into strict call-and-unload runtime boundaries.

- [x] Document the DLL boundary intent and shared crash-context rule.
- [x] Shape the startup parser test as the future XML parser DLL caller.
- [x] Record compile-dependency smell: a focused tester depending on most runtime files means the DLL boundary is still too porous.
- [x] Remove the startup parser tester's direct `building_runtime.h` dependency.
- [x] Document the strict handoff contract: startup owns immutable definitions, save/load owns record conversion, runtime owns live object behavior.
- [x] Add the first static `startup_parser::parse_startup_definitions()` facade and route `StartupParserTest` through it.
- [x] Add an explicit pre-registry graphics-validation preparation step to the startup parser facade without coupling it to extraction.
- [x] Add a separate `JuliusGraphicsExtractor` test executable so legacy extraction and XML startup parsing can be validated independently.
- [x] Move StartupParserTest environment/mod-stack reporting behind the startup parser facade.
- [ ] Define a tiny stable ABI/shared-core boundary.
- [ ] Move normal game startup onto the facade after init-failure mapping and localization request behavior are explicit.
- [x] Replace the startup parser test's `src\**\*.cpp` project wildcard with curated startup/static-boundary source groups.
- [x] Move BuildingType graphics definition data/selection methods out of draw-time animation code and compile the shared source into `StartupParserTest`.
- [x] Move XML graphics path/source resolution into a shared asset startup source and remove the parser-test duplicate.
- [x] Move generated image-copy primitives into a shared asset source and remove the parser-test duplicate.
- [x] Move parser-visible `PathingMode` metadata, XML lookup, legacy terrain mapping, and route-policy validation into a shared source.
- [x] Split parser-visible `ProductionMethod` data access/validation from live production eligibility/progress so `StartupParserTest` no longer needs production-only city finance, calendar, mothball, or shipyard water-spawn shims.
- [x] Split parser-visible `Distribution` rule data from live source lookup/acceptance behavior so `StartupParserTest` no longer needs distribution-only storage permission, stockpile, source traversal, and acceptance shims.
- [x] Fence live `BuildingGraphics` condition evaluation and construction-phase building-state selection out of `StartupParserTest` so it no longer needs fake live `Building`, climate, or festival shims.
- [x] Remove stale StartupParserTest timing/resize/fullscreen/folder-dialog/exit system shims after the curated source list proved none are referenced.
- [x] Remove dead StartupParserTest external-pixel loader and runtime `PathingMode` terrain probe shims after the curated source list proved they are no longer referenced.
- [x] Remove the duplicate `building_runtime_reset()` from BuildingType registry load; full runtime startup still resets live building state after definitions load, and `StartupParserTest` no longer needs that shim.
- [x] Move generated image materialization/runtime image loader test shims into a startup graphics boundary source.
- [x] Move the headless startup renderer implementation out of parser orchestration and into the startup graphics boundary source.
- [x] Move building/scenario validation bridge test shims into a startup validation boundary source.
- [x] Move menu/monument cache invalidation test shims into a startup cache-invalidation boundary source.
- [x] Split remaining parser-test shims into owned parser/shared-core sources: crash dialog and minimal figure image helpers.
- [ ] Extract graphics extraction into a self-contained module boundary.
- [ ] Extract XML startup parsing into a self-contained module boundary.
- [ ] Extract save/load bridging into a self-contained module boundary.
- [ ] Add a save bridge tester for known-good saves.
- [ ] Move legacy-id compatibility tables toward mod-owned XML bridge declarations after current refactors stabilize.
- [ ] Keep runtime code from depending on module internals after handoff.

## Object-Owned Runtime Architecture

Source plan: `docs/object_owned_runtime_refactor.md`

Requirement: data-owning objects should own their behavior, references, cleanup, and typed relationships.

- [x] Delete `building_type_api.h` compatibility layer.
- [x] Move many one-line BuildingType facts from C accessors to object methods.
- [x] Remove local `building_matches` and `type_matches` helper islands.
- [x] Add the requirement that BuildingType spawn declarations create figure/profile pairs while FigureType profiles own walker behavior.
- [x] Move profiled-spawn initial action and roaming setup onto `FigureTypeProfile`.
- [x] Move native producer cart/effect spawning into `building_runtime` and delete dead wharf/shipyard legacy spawn branches.
- [x] Make `Figure::remove()` clear owning, destination, and immigrant building figure slots by matching the removing figure id before type-specific cleanup.
- [x] Add a narrow save-load sanity repair for producer primary output-cart slots that point to missing or dead figures.
- [x] Replace truthy figure-state spawn guards with alive/dead checks for native slots, legacy primary slots, labor seekers, dockers, depots, and tower sentries.
- [x] Fix raw-material cart retargeting so a cart cannot switch to workshop delivery while retaining an older warehouse destination.
- [x] Move docker behavior from the legacy `figure_docker_action` callback into `figuretype::Docker::docker_action()` and call the object method directly from the remaining dispatcher.
- [x] Convert docker trade/storage behavior from raw `building *`/record-field access to `Building` object calls.
- [x] Promote shared warehouse/granary first-iterator helpers into their storage modules and remove docker's duplicate storage-discovery helpers.
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
- [x] Move venue-seeker roadblock behavior into `PathingMode` for route planning and per-step movement.
- [x] Record route handoff requirement so destination selection can pass its validated route to the figure.
- [x] Delete unused road-access roaming adjacency export that duplicated figure-aware movement checks.
- [x] Move draggable-reservoir construction distance seeding behind `RoutePolicy`.
- [x] Remove compiled direct `map_routing_*` calls/includes outside `Route`, except save/load serialization.
- [x] Move figure route-policy selection onto `PathingMode`, including XML profile lookup, legacy terrain fallback, and roadblock permission selection.
- [x] Consolidate `Route::DistanceQuery` reachable-area and access-road candidate scans behind one route-owned selector.
- [x] Move legacy road-access footprint, hippodrome, and monument candidate scans behind a `RoadAccessQuery` boundary.
- [x] Add the first `Building` road-access cache boundary for committed cached points and storage-destination spawn queries.
- [x] Finish splitting `PathingMode` policy creation from route planning.
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

Gate: half-size Vespasian FigureType XML must wait until every `Renderer Scaling Seams` checklist item is complete, especially the source-pixels versus fixed-point-logical-size split.

- [x] Retire the old `figure_graphics.h` facade and route figure draw requests through the native figure runtime API.
- [x] Add figure draw request type for base slice plus overlays.
- [x] Route `city_figure.cpp` through the facade first while keeping fallbacks.
- [x] Add debug counters/logging for native figure draws versus legacy fallback.
- [x] Remove public compatibility wrappers and several one-use graphics accessors.
- [x] Move more legacy image arithmetic onto `Figure` or private native helpers.
- [x] Add cached native payload bindings for runtime-bound figures.
- [x] Move legacy atlas base arithmetic onto `FigureGraphics` and stop native-covered policies from re-entering draw-request fallback checks.
- [x] Move cached default/action/corpse role binding selection onto `FigureTypeDefinition`.
- [x] Move figure-state direction and corpse-frame cached binding lookup onto `FigureGraphics`.
- [x] Move generic legacy default/static/corpse/directional image-id formulas onto `FigureGraphics`.
- [x] Move prefect/charioteer/gladiator legacy attack-row ids onto `FigureGraphics`.
- [x] Move depot cart resource-load completeness and layer offsets onto `FigureGraphics`.
- [x] Move depot carried-resource cart slice selection onto `FigureGraphics`.
- [x] Establish `GraphicsDefinition -> BuildingGraphics | FigureGraphics | ResourceGraphics`.
- [x] Split resource graphics so warehouse storage refs belong to `BuildingGraphics`, cart/load refs belong to `FigureGraphics`, and `ResourceGraphics` only owns icons.
- [x] Promote shared graphics target-role names, point/offset value object, and integer comparison helper into `GraphicsDefinition`.
- [x] Rename legacy `animation.*` into `Animation.*`, make `Animation` own frame data/metadata, and route building/figure draw slices through `Animation::frame_slice_at_offset(...)`.
- [x] Move legacy cart overlay image loading and resource-cart slice selection into `FigureGraphics`.
- [x] Move legacy figure overlay layer construction and stacked standard/map-flag helpers onto `FigureGraphicsLayer`.
- [x] Use explicit logical width/height in figure draw requests.
- [x] Add structured child-node FigureType graphics parsing.
- [x] Validate figure graphics targets at FigureType load time.
- [x] Let nested FigureType `<path>` graphics targets bind to the payload default entry without requiring a duplicate `<image>` child.
- [x] Consolidate fort/map flag and enemy fallback layer assembly behind private draw-request helpers.
- [x] Move lion-tamer whip atlas ownership into structured FigureType action graphics.
- [x] Retire controller-owned `f->image_id` mutation in converted controllers.
- [ ] Move cart/resource/animal/standard overlays into figure graphics layers.
- [ ] Transition figure graphics to image group payload manager ownership with real file-path references instead of legacy group/image-id references.
- [ ] Add Vespasian half-size FigureType XML overrides using existing source art after all Renderer Scaling Seams work and figure payload ownership are complete.
- [ ] Delete legacy figure image-id arithmetic and duplicate corpse/direction/cart tables.

## Renderer Scaling Seams

Source plan: `docs/renderer_scaling_seam_plan.md`

Requirement: scaling filters and logical sizes must produce exact seamless city-view geometry across native and remaining atlas paths.

Gate: this entire section blocks Vespasian half-size FigureType XML. The XML slice depends on source pixel dimensions being split from fixed-point logical image dimensions, and on the seam fixes being validated first.

- [x] Add focused terrain render test matrix for scale filters, grid state, zoom, and atlas/native paths; `RendererSeamTest` now generates the matrix, JSON results, and one real-pixel software fixture smoke case.
- [x] Remove grid-rendering tile-size mutation.
- [ ] Introduce exact city-tile destination geometry with shared rounded edges.
- [ ] Add temporary atlas edge padding while atlas fallback remains.
- [ ] Move terrain, water, and climate images into managed native image resources.
- [ ] Split source pixel dimensions from fixed-point logical image dimensions.
- [ ] Add pixel checks for terrain/water seams; passing the matrix is the deployment-worthy threshold for renderer seam changes.
- [ ] Unlock Vespasian half-size FigureType XML only after every preceding Renderer Scaling Seams item and figure-owned native graphics payload ownership are complete.

## Render Performance And Vulkan Direction

Source plan: `docs/render_performance_plans.md`

Requirement: move rendering toward command lists, snapshots, GPU-owned resources, and an eventual Vulkan backend.

- [x] Add city draw metrics for top, figure, animation, submission, render requests, managed image requests, texture misses, tile rows, and rendered tiles.
- [x] Consolidate several visible-row traversal and draw-adapter compatibility paths.
- [x] Add transitional fixed logical-size fields at the renderer request boundary.
- [x] Convert `FigureGraphicDrawRequest` to `render_logical_size`.
- [x] Centralize render-domain classification and `scale_filter` interpretation in `Render2DPipeline`.
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
- [x] Move fort-bound legion initialization, declared recruit selection, overflow ejection, and movement prep onto `formation`.
- [x] Move herd movement and enemy formation-wide combat/city predicates onto `formation`.
- [x] Replace modulo-wrapped layout position accessors with a batch helper that accepts live slots, declared capacity, and footprint.
- [x] Move enemy movement layout-offset generation onto `formation` using the resolved `FormationType` footprint.
- [x] Move fixed 16-slot roster save/load serialization behind `formation` legacy storage bridge methods.
- [x] Split live formation roster storage from the fixed 16-slot legacy save prefix and add an extended roster save section.
- [ ] Add fort-owned `Formation` object links with live formations pointing to resolved `FormationType` definitions.
- [ ] Replace remaining hardcoded formation-size loops with `Formation` object iteration.
- [x] Split fixed roster save/storage from 16-offset layout tables before enabling more than 16 live slots.
- [ ] Move soldier stats and combat abilities fully into `UnitType`.
- [ ] Bridge old saves and vanilla formation enum values into declared fort formations.
- [ ] Delete remaining legacy constants that assume 16 soldiers; only legacy save-prefix and legacy layout-table names should remain.

## Current Regression Notes

Requirement: keep only active manual-test notes here; resolved findings belong in git history or focused docs.

- [ ] Garden area placement has a global garden recomposition bridge after preview restore/placement; full isolated placement and atomic garden replacement remain open.
