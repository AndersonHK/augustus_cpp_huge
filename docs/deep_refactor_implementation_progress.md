# Deep Refactor Implementation Progress

Snapshot: 2026-07-12, branch `Major-Foundation,-Housing,-and-Composed-Refactors`

This is the live checklist for the current implementation push. Detailed requirements live in `docs/deep_refactor_requirements.md`; detailed designs live in the linked plan files. This file should stay short enough to scan before coding.

Line-count target: move the net diff from `master` toward `-5k` by deleting legacy branches, duplicated helpers, compatibility accessors, and old id/string bridges after their object/XML-owned replacements are in place.

Current measured state:

- Stable reference: `Rubble-and-Foundations` commit `844b35b68`.
- Current branch: `Major-Foundation,-Housing,-and-Composed-Refactors`.
- The foundation, housing, and fixed-composition runtime initiative is integrated: external rotated foundations own geometry and road passage; housing data lives in native definitions/state/runtime modules with legacy bytes confined to a save DTO; fixed compositions own child relationships and normalized layouts; placement, repair, destruction, map publication, road access, and save hydration consume those modules.
- Farm terrain behavior is explicit per mod stack. Vespasian uses a land-only 2-by-2 owner plus five meadow-required field foundations. Julius and Augustus retain the original nearby-meadow owner check with ordinary-land field foundations; the intentional Vespasian divergence is documented in `docs/gameplay_divergences_from_augustus.md`.
- Legacy `<model size>`, inline foundation policy, `<composed>`, and `<roadblock>` XML are absent. Stored scalar model geometry, the synthetic square `Building::size()` API, standard/storage roadblock kinds, hardcoded fixed-composition coordinates, and record-backed housing state are deleted. Live building road, water, range, desirability, overlay, rubble, destruction, map-image, and distribution calculations now consume exact foundation/composition geometry; dynamic bridge chains, legacy map tile-size bitfields, and legacy save DTO synthesis remain compatibility boundaries.
- Current integrated checkpoint: arbitrary configured-stack vector-font fallback, versioned startup/environment and graphics-extraction C ABIs, and self-contained startup definition-loader and graphics-extraction implementation modules are built and deployed alongside profile-authoritative generic spawning, direct fort-to-`Formation` ownership, owner-lifecycle storage and immigrant relationships, transactional routed road/highway publication, exact live foundation/composition geometry, isolated legacy terrain-size decoding, centrally indexed semantic building ranges, stable runtime `Building` object identity, and reverse-ledgered figure/building relationships. The obsolete raw first-of-type API, combinable iterator-Boolean selector, generic multipart traversal APIs, five global housing query wrappers, housing overlay state adapter, one-off undo housing population adapter, nine Building housing-state proxies, free housing-capacity helper, mixed fixed/dynamic `Building::for_each_part()` facade, `Building::composition_owner()` facade, and `Building::is_main_part()` facade are deleted; dynamic bridge traversal is explicitly named and type-gated; `HousingModule` owns effective capacity; repair rollback follows exact rubble cells; reservoir identity follows rotated foundation cells; depot/storage state retains stable objects; military/native spawning lives with semantic owners; enemy/rioter target selection and plague treatment targeting carry stable `Building *` values through semantic runtime lists; housing health and advisor aggregation consume native housing profiles/state while generic sickness remains shared runtime data; fixed-composition lifecycle, labor, entertainment, construction, destruction, and overlays use complete owner/child graphs; formation recruitment limits, Roman, criminal/rioter, armed-supplier, and city-ballista combat behavior, enemy catapult/wolf combat, tactical figure positions, and enemy-army spacing are XML data; SmartTool XML owns road bridge and reservoir-routed modes; network previews use native graphics strategies; six gate variants use one XML-authoritative terrain-foundation underlay path; prefect bucket, Roman fort-standard, editor map-flag, hippodrome horse/cart, missile-launcher pose schedules, ballista idle/firing/corpse states, every production/storage/docker/lighthouse resource-cart presentation, and fishing/trade-ship directional poses are FigureGraphics data; XML-owned cart overlays replace the legacy offset tables and selection/finalization family, and one threshold policy replaces the 128-entry corpse timing table; stateful cart families bind owner-specific `FigureGraphicsState`; composed farms author their overlay-summary policy; small/medium statue and hippodrome part orientation are selected through generic BuildingGraphics options; and gatehouse/bridge/warehouse/fort-ground/statue/hippodrome base rendering no longer depends on obsolete type fallbacks. All 1,413 configured XML files parse; `git diff --check` passes; `Release|x64` builds pass for `StartupParserTest.vcxproj`, `JuliusGraphicsExtractor.vcxproj`, `AugustusGraphicsExtractor.vcxproj`, and `Vespasian.vcxproj`; the installed-root parser passes Julius -> Augustus -> Vespasian with 24 layered resources; all five guarded-deploy safety tests pass; the deploy preserved 7,288 Augustus and 9,516 Julius extracted files with no backup residue; and deployed `Vespasian.exe` SHA-256 `7D78BDA012D01F6335B615DBE768480ED6F1E6EF55D763406FEEDF1256294EB7` matches the Release artifact.
- Manual gameplay validation remains required for rotated placement retention, generic composed-building access/perimeter behavior, repair of partially missing rubble compositions, and foundation terrain restoration across save/load. Do not treat headless parser coverage as a substitute for those interactive checks.
- Manual-test redeploy, 2026-08-21: a full Release rebuild removed a stale `graphics/image.cpp` object compiled against an older `graphics_renderer_interface` layout, managed resource uploads remain legal while drawing is paused, and upload failures now identify the rejected boundary. The guarded full deploy completed with no backup residue. `StartupParserTest` now makes the sibling production executable startup mandatory before its parser fixtures, and the installed `Vespasian.exe --startup-test` completed the normal `direct3d` renderer, extraction bootstrap, five main atlases, managed uploads, fonts/definitions, and audio initialization with zero runtime-overlay or main-graphics failures before cleanly exiting 0. Installed `Vespasian.exe` SHA-256 `7521CDF934D8A504C77E8CBE9462C8EC816427E46915499885CCB9D39000F524` matches the Release artifact and supersedes earlier deployed-binary hashes above.
- Current validation priority: manual runtime testing of the active regression notes and foundation/composition repair, passage, demolition, and save/load cases.

Latest runtime crash finding, 2026-08-12:

- The `Aedile 1 14.svv` access violation at game tick 88 was a generic-building maintenance pass dereferencing `Housing` on a non-housing aqueduct. The scheduler now sends one immutable risk-tick context to each `Building`; the object resolves composition, surface, housing, compatibility-level, population, and legacy risk state internally and returns a transition outcome. Callers enact that transition directly through object methods. `Building::destroy_by_collapse()`, `destroy_by_fire()`, `destroy_by_plague()`, and `destroy_without_rubble()` now contain the destruction workflows themselves; the raw-record entry points and `destroy_with_rubble`/`destroy_on_fire` wrapper ladder are removed, and rubble initialization and part retirement are object-owned messages. The next manual load should confirm continued ticking and then address the separately reported incomplete legacy warehouse/hippodrome composition hydration warnings.

Latest manual checkpoint findings, 2026-06-28:

- Confirmed new regression: forts can briefly show 17 soldiers, then remove the overflow soldier and respawn the missing slot forever; current fix keeps barracks recruitment/status capped to the current 16-soldier fort recruitment capacity while leaving declared formation storage/counting intact for the larger-formation work. Manual spot test indicates the regression is fixed.
- Unconfirmed regression watch: native farms may be growing or animating too fast, but local timing matches the checked Augustus `upstream/master` source shape: `building_figure_generate()` advances crop progress once per day and cycles five frames. Leave Vespasian timing unchanged unless manual testing confirms a real divergence that should become XML-owned data.
- Confirmed old regression fix: land trader sound phrases no longer use the success phrase as a generic pre-trade line; success now requires `trader_has_traded(...)`, while no-trade remains tied to a leaving caravan with no completed exchange. Manual retest confirms the reported pre-trade success sound is fixed.
- Confirmed returned regression: garden area preview/placement can mutate adjacent committed garden graphics, and merely attempting to place over an existing real garden can glitch that real garden's composition. Manual retest confirms the runtime snapshot/stateless-preview fix resolves the reported symptoms.
- Future figure-native-graphics clue: inspect runtime logs for FigureType graphics fallback errors before finishing the remaining native figure graphics transition.

## Coordination

Requirement: keep work planned, delegated, build-gated, and regression-testable.

- [x] Start from checkpoint branch `Deep-Refactors-Part2`.
- [x] Use agents for disjoint implementation/research slices without letting them compile.
- [x] Keep stable requirements separate in `docs/deep_refactor_requirements.md`.
- [x] Split verbose requirement text out of this tracker.
- [x] Keep two agents busy with long-lived implementation or cleanup slices.
- [x] Update this checklist when a slice lands or a dependency order changes.
- [x] Record manual regression findings only as short current-state notes.
- [ ] Treat manual-test/deploy milestones as: terrain/water seam pixel checks, Vespasian half-size FigureType XML, deletion of strategy-obsolete compatibility branches, deletion of 16-soldier formation constants, and HDR scene target plus shader-side lighting/material policies.

## Validation And Deployment

Requirement: catch parser/startup failures from PowerShell before manual runtime testing.

- [x] Add loud deployment failure handling and Explorer pause behavior to `tools/deploy_release_to_game.py`.
- [x] Add the `StartupParserTest` CLI startup/XML executable. Its mandatory first phase now runs the sibling `Vespasian.exe --startup-test` through the real hidden SDL renderer, extraction bootstrap, atlas and managed-resource uploads, fonts, definitions, audio initialization, and `game_init()`, then exits before the game loop; only after that passes does the curated parser binary run its strict registry and negative-fixture suite. `StartupParserTest.vcxproj` references `Vespasian.vcxproj`, preventing the test from silently exercising an old executable, and Windows failures report through stderr/nonzero exit without modal dialogs.
- [~] Extend the mandatory executable gate with representative save soaks. `StartupParserTest` discovers the newest `.svv`/`.sav` files under the installed `savegames` folder with extension diversity, launches each in a separate hidden/no-audio production process, and requests 3,000 simulation-and-render frames. Load/soak warnings and errors are counted from the save-load boundary and fail through stderr/nonzero exit; one failed city does not prevent the remaining selected saves from running. The Release build and guarded deploy pass, and the installed full-duration gate exercised the newest recovered `.svv`, newest legacy `.sav`, and recent unrecovered `.svv`. The threshold is correctly red: it found saved surface/figure-slot repair warnings and unresolved native graphics for cart pushers, warehousemen, trade caravans/donkeys, flotsam, and fish gulls. Fixing those producers/assets and reaching zero warnings/errors remain before this item is complete.
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

## Foundation, Housing, And Composition Runtime

Source plan: `docs/foundation_composition_housing_runtime_plan.md`

- [x] Add required external foundation definitions for every buildable type and rotate complete cell data generically.
- [x] Migrate the three mod stacks away from scalar model size and inline foundation policies.
- [x] Make placement results own selected rotation, normalized member cells, clearing, cost, and specific failure state.
- [x] Retain a valid automatic rotation during the construction session and fall back clockwise from the user's preferred rotation when it becomes invalid.
- [x] Derive rich-geometry rotation capability from the loaded foundation/composition definition instead of building-type name lists.
- [x] Make docks use exact six-land/three-water foundations and explicit `WaterAccessDef` open-water connectivity.
- [x] Add native `HousingProfileDef`, `HousingDef`, `HousingState`, and owner-bound `HousingModule`; confine removed scalar housing bytes to `LegacyBuildingSaveDto`. Normal runtime has no free active/profile/compatibility-level/resident-class/capacity housing wrappers and no `Building` population, room, immigrant-id, generation-delay, or happiness proxies: lifecycle checks use the `Building` owner, scalar state lives in `HousingState`, effective capacity and validated immigrant ownership live in `HousingModule`, profile data comes from `HousingDef`, and resident classification comes from `HousingModule`.
- [x] Add owner-bound fixed composition with rotated child layouts, transactional creation/publication, lifecycle fanout, save hydration, and partial-rubble repair. Entertainment venue selection, show-state updates, hippodrome service, and problem-overlay publication resolve fixed ownership directly through `BuildingComposition`; they no longer use record-chain main-part traversal or hippodrome-specific child guards.
- [x] Replace storage/hippodrome road coordinate tables and standard/storage roadblock kinds with foundation/composition perimeter and passage data.
- [x] Move bridge identity out of `<roadblock>` metadata into a separate `<bridge>` definition while keeping dynamic segment chains separate from fixed composition.
- [x] Persist exact foundation terrain deltas behind save version `0xc0` and validate pure save bridges in `StartupParserTest`.
- [x] Validate farms, warehouses, hippodromes, forts, housing transitions, docks, rubble identity, terrain deltas, permissions, and retained/preferred rotation ordering in the test executable or parser-time contracts.
- [x] Delete the synthetic square `Building::size()` API and migrate live-building callers to exact foundation/composition geometry. `BuildingGeometry` owns deduplicated cells, union bounds, distance/range queries, and access/water perimeters; road and walker access, water supply and dock candidates, rubble/repair/destruction, desirability, overlays, map-image refresh, diagnostics, and distribution searches consume it. Exact sparse/rectangular and rotated overlay projections are covered in `StartupParserTest`.
- [x] Separate legacy map tile-size bitfields from live building geometry. Bound orientation anchors, minimap cells, missile collision, farm graphics, and overlay deletion use Foundation/Building ownership; ownerless multi-tile terrain and 8/16-bit scenario preview decoding retain explicitly named `map_property_legacy_multi_tile_size` bridges with one shared unchanged decoder. No ambiguous pre-refactor property names remain.
- [x] Delete `Building::for_each_part()`, which mixed fixed composition with record-chain traversal. `BuildingComposition::for_each_member()` now owns completeness validation and every fixed lifecycle, monument, labor, overlay, industry, construction, and destruction caller uses it directly. Aggregate employment is the sum of declared member labor rather than a farm-type exception; parser contracts lock the current farm, warehouse, fort, and hippodrome totals.
- [x] Delete `Building::composition_owner()`. Fixed lifecycle, graphics, labor, production, storage, service, overlay, scenario, and UI callers resolve `BuildingComposition::owner()` explicitly with standalone handling. Arbitrary map-selection callers enter `Building::main()` only behind an explicit dynamic-bridge type check; `main()` itself no longer resolves fixed composition.
- [x] Delete `Building::is_main_part()`. Fixed composition callers now state owner/child policy through `BuildingComposition`; dynamic bridge rotation, migration, counting, graphics, and selection check the record-chain predecessor only within bridge-gated paths. Housing no longer carries a redundant multipart test, and startup validation rejects housing owners or children in fixed composition.
- [~] Keep multipart record links only at explicit compatibility boundaries. Generic `Building::main()/next()/previous_part_id()/next_part_id()` and the raw free helpers are deleted. Dynamic bridge gameplay and ephemeral previews use `dynamic_bridge_owner()`, `dynamic_bridge_next()`, and bridge-owned predicates; bridge road-access inheritance enters through the same typed API. Raw `prev_part_building_id`/`next_part_building_id` access is confined to the typed bridge traversal implementation, bridge publication/removal, bridge and legacy-hippodrome migration, terrain recognition, save DTO synthesis/load hydration, runtime snapshot restoration, and diagnostics. No fixed-composition or broad multipart facade remains.
- [ ] Manually smoke-test the interactive placement, repair, walker-access, demolition, and save/load cases listed in the current snapshot.

## Sparse Mod XML Layering

Source: `research/pharaoh_mod_feasibility_report.md`, Phase 0.

Current seam: `mod_definition_loader` provides deterministic lower-to-upper enumeration, optional missing directories, source provenance, winner replacement/suppression/restoration, actionable visitor diagnostics, same-layer duplicate rejection, and traversal-safe upper-to-lower whole-file fallback. BuildingType/Tiles/Menu, Foundation, HousingProfile, Resource, ProductionMethod, StorageType, Distribution, WaterAccessType, CultureModule, Gods, Religions, FigureType, UnitType, and FormationType use registry-specific atomic staging and `StartupParserTest` fixtures. Root `defines.xml` was already an atomic lower-to-upper merge; graphics and localization already had layered ownership. A read-only audit found no remaining selected-top gameplay registry.

- [x] Layer every gameplay definition registry without requiring repeated upper-mod XML.
- [x] Make the boot-critical mission-briefing UI document inherit the nearest complete configured-layer file.
- [x] Replace the hardcoded selected/Augustus/Julius vector-font fallback with arbitrary configured-stack whole-pack fallback.

- [x] Make every gameplay registry load the ordered mod stack lower-to-upper, matching graphics precedence, so absent upper-layer definitions inherit unchanged lower-layer definitions.
- [x] Replace definitions deterministically by stable registry identity/path; reject duplicate identities within one layer while allowing an upper layer to replace the lower-layer winner.
- [x] Resolve cross-definition references only after all layers for the registry stack are collected, so sparse upper-layer definitions may refer to inherited lower-layer definitions and vice versa where dependency order permits.
- [x] Add an explicit `disabled="true"` suppression contract for hiding inherited definitions without dummy replacement files.
- [x] Report the winning source mod and source file for loaded definitions, with actionable diagnostics for illegal duplicates, unresolved references, and invalid suppression.
- [x] Treat a missing registry directory in an upper mod as an empty overlay, not a startup failure.
- [x] Add `StartupParserTest` fixtures for almost-empty upper mods, complete replacements, inherited/deferred references, same-layer duplicates, suppression, missing directories, immutable persisted identities, and atomic failure.
- [x] Prove the sparse contract with synthetic lower/upper stacks that do not copy the lower XML tree while keeping the complete Julius -> Augustus -> Vespasian startup unchanged.

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
- [x] Move StartupParserTest environment/mod-stack reporting onto caller-owned buffers and ordered mod callbacks in the startup parser ABI; only `startup_parser.cpp` includes the C++ implementation facade header.
- [x] Define the first tiny stable shared-core boundary: `startup_parser_abi.h` exposes a versioned C request/result contract with explicit flags, caller-owned diagnostics, transient step callbacks, reserved-field validation, and deterministic rejection of incompatible callers before parser state changes.
- [x] Move normal game startup and `StartupParserTest` onto the same startup ABI with explicit config/localization/validation ownership and structured failure mapping; the C++ parse facade is now implementation-only to those callers.
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
- [x] Extract graphics extraction into a self-contained module boundary. `graphics_extraction_abi.h` exposes versioned plain requests/results for Augustus extraction and climate-atlas bootstrap; runtime climate loading and both standalone extractor workflows enter through that boundary, while concrete extractor classes remain private implementation details. A future slice may move the same ABI from static linkage to a load/unload DLL.
- [x] Extract XML startup parsing into a self-contained module boundary. `startup_definition_loader` owns configured-environment inspection, ordered definition loading, step diagnostics, and graphics-validation preparation; `startup_parser.cpp` is now only the versioned C ABI adapter used by both the game and `StartupParserTest`.
- [ ] Extract save/load bridging into a self-contained module boundary.
- [ ] Add a save bridge tester for known-good saves.
- [x] Persist exact FigureType profile identity in saves and delete load-time profile inference. Save version `0xc3` appends the exact stable profile string to every figure record; current-format load rejects missing, unknown, definition-less, and owner-incompatible identities. New/runtime-created figures persist the exact default or explicitly selected profile when binding. The action/owner-based mapping is now named and reachable only as a one-time `<=0xc2` serialized-representation translation; live runtime rebinding consumes `Figure::runtime_profile_id()` directly and never guesses or falls back to a default for an invalid non-empty identity.
- [~] Replace orphan-figure removal, discarded owner references, invalid-building cleanup, and comparable load-time repair with strict save validation. Figure owner/optional/destination references, housing immigrant links, building figure/cart/boat slots, map-building references, formation ownership, and hippodrome composition/orientation now fail load without deleting, clearing, reconstructing, or normalizing serialized state. Save version `0xc2` makes the native surface-building and composition representation explicit: current saves strictly validate surface ownership, non-binding foundation cells, rubble disposition, reserved slot zero, exact composition children/coordinates/links/orientations, unrelated tails, and warehouse storage identity before attaching runtime modules. Save version `0xc3` additionally requires exact figure profile identity and exact native bridge/wall records: missing bridge segments, non-reciprocal or cyclic chains, mismatched coordinates/types, terrain-orphaned bridge/wall records, and wall terrain without a wall record fail load. Surface/rubble/composition synthesis and bridge/wall materialization now run only for their named `<=0xc1`/`<=0xc2` representations. Composition failures return load failure without a GUI fatal dialog or `terminate`. This remains partial until the old representation transforms move into the self-contained save DTO boundary and known-good/corrupt save fixtures exercise both version paths.
- [x] Make CLI/headless tests suppress Windows fault dialogs and emit exception code/address diagnostics to stderr. `StartupParserTest` and `RendererSeamTest` are Console-subsystem binaries that disable OS fault boxes and CRT abort/report-fault UI, install a process-wide unhandled-exception stderr filter for worker-thread faults, and retain top-level SEH boundaries for deterministic exit code `3`; the null optional `icon_path` parse that exposed the issue is fixed. Registry-dependent production checks now run before deliberately mutating layering fixtures, explicit animation materialization reports stage failures, and repeated installed-payload runs finish with `STARTUP_EXIT=0` rather than a GUI fault or fixture-contaminated false failure.
- [ ] Move legacy-id compatibility tables toward mod-owned XML bridge declarations after current refactors stabilize.
- [ ] Keep runtime code from depending on module internals after handoff.

## Object-Owned Runtime Architecture

Source plan: `docs/object_owned_runtime_refactor.md`

Module extraction plan: `docs/bound_runtime_module_extraction_plan.md`

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
- [x] Move figure-info draw helper bodies out of `figures.cpp` and onto `Figure`/figuretype child `draw(c)` methods.
- [x] Draft the first bound-runtime module extraction order from existing Vespasian XML folders.
- [x] Draft markdown-only class declarations for `BuildingWaterAccess`, `BuildingCulture`, `BuildingReligion`, `BuildingHousing`, `BuildingProduction`, `BuildingStorage`, and `BuildingFormation`, including XML source/future-folder mapping.
- [x] Delete local uppercase `MIN`/`MAX` macros from project C/C++ sources.
- [x] Delete one-line `Building::id()` and `Building::storage_id()` accessors and expose the simple object state directly.
- [x] Move garden/plaza area-tile placement and preview restore state out of `construction.cpp` static globals into `ConstructionAreaTilePlacement`, with bounded garden recomposition replacing the all-garden preview bridge.
- [x] Retire redundant generic spawn `mode` versus FigureType `profile` behavior: profiles own generic initialization, while mutually exclusive special modes retain only bespoke building-owned operations.
- [~] Convert remaining raw `building *` compatibility boundaries to `Building` object calls when touched; the global raw first-of-type API is deleted, monument/destruction/city-resource traversal uses `Building::of_type`, and industry, distribution, native-map, water, depot, warehouse, invasion/rioter target selection, exact-id consumers, housing health, housing economy/evolution/service/population/sentiment, and housing presentation carry stable objects. The obsolete free housing query/capacity family, housing overlay record-to-empty-state adapter, one-off undo population-restore helper, and nine `Building` housing-state proxies are deleted rather than retained as adapters. Housing overlay callbacks resolve their legacy record once through the generic map adapter, explicitly require `HousingModule`, and then read native state. Enemy targeting now selects one `Building *` through a shared priority query instead of duplicating first/closest raw-record scans and re-resolving the winner from its map cell. `BuildingStorage` owns `Building *` and its runtime APIs accept object references without record re-resolution; normal housing runtime reads definition/state through `HousingModule`. The generic overlay callback record, undo transaction's saved primary-figure slot, generic plague bytes, generic access flags, worker counts, raw destruction entry points, save DTOs, and untouched legacy subsystems remain explicit compatibility boundaries.
- [~] Replace repeated type/string scans with typed runtime lists; `building_runtime` owns stable housing, labor, production, granary, warehouse, combined-storage, and plague-treatment indexes serving filtered semantic call sites through one `BuildingRuntimeList` value rather than combinable Booleans. Doctor target discovery and closest-target selection now share the definition-owned plague classification and typed list instead of repeatedly scanning housing, docks, warehouses, granaries, and then the whole city. The housing advisor takes one ordered snapshot of the housing list and groups occupied owners by `HousingProfileDef`, so merged or alternate building types sharing a profile remain in the same row without representative-type scans. Water access, native-map, storage/distribution, and plague-service code use registry-derived typed ranges; combat, trade, and other global gameplay queries remain to be classified rather than replaced mechanically.
- [~] Move cleanup to owner lifecycle events instead of downstream repair scans; housing ownership and immigrant publication, conditional clearing, reference-only clearing, and removal live in `HousingModule`, storage reservations retain direct `Figure *` owners, and `FigureStore` reverse-indexes home, destination, immigrant, and typed last-destination relationships so normal figure/building removal, house merges, plague transitions, and owner-specific tower-sentry removal do not scan every object. Broad scans remain only at save-load compatibility/validation seams and genuinely global spatial/gameplay queries.
- [x] Remove `event_data attr` as a behavior selector and duplicated identity bridge; root `type` is canonical, with only explicit `<identity aliases="...">` accepted for historical scenario inputs.
- [~] Remove normal-runtime id lookups where object references are available; distribution, depot, invasion, rioter, and plague-service selections carry validated `Building *` values, storage state hydrates a nonserialized runtime object, industry and fifteen exact-id scan sites use direct runtime lookup, and figure relation setters maintain the reverse ownership ledger. Serialized ids, cross-save hydration, and identity-bearing history fields remain deliberate compatibility data.
- [x] Rewrite `src/building/figure.cpp`/`.h` into header-ledgered, owner-bound spawn/service modules. Barracks owns recruitment, military helpers own tower/academy spawning, MessHall owns fort suppliers, `NativeBuildingSpawner` owns huts/meetings/missionaries/crops, and dead temple helpers are deleted. The generator is reduced to ordered semantic dispatch for definitions that do not yet declare generic spawn groups.

## Routing Cost Map Scalability

Source plan: `docs/routing_cost_map_scalability_plan.md`

Requirement: centralize route planning and make cost-map generation lazy, reusable, and policy-owned.

- [~] Add route-specific performance counters; Part6 adds current-path reuse and failed-route backoff pruning metrics, while local cost-map/cache metrics remain to grow with the route-world work.
- [x] Replace public surface routing with `RoutePolicy`.
- [x] Add `Route::Planner::canReach()` facade.
- [x] Move citizen/wall/noncitizen reachability calls behind route facade code.
- [~] Add route-owned destination/policy intent stamps.
- [~] Reuse existing figure routes until intent or next-step validity changes.
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
- [x] Promote route intent, failed-route backoff, and legacy planner backend internals into `route.h` ledgers instead of anonymous route behavior islands.
- [~] Add the first `Building` road-access boundary for committed cached points, storage-destination spawn queries, and same-network local-workforce access-area prechecks.
- [x] Finish splitting `PathingMode` policy creation from route planning.
- [~] Add building-owned road-access caches.
- [~] Convert road-access callers to cached spans or equivalent; local workforce house selection now routes through ledgered `building_local_workforce::RouteAccessSelector`, using `Building::access_area_touches_same_road_network(...)` as an object-owned network gate while `Route::DistanceQuery` still scans candidate roads to preserve shortest reachable behavior.
- [~] Convert local workforce scans to dirty/runtime-list driven refresh; local workforce now owns paired-file `RuntimeBuildingLists` and `RouteAccessSelector` ledgers for live house labor-source candidates and route-access selection, with acquisition selection iterating the runtime list, populated-source filtering owned by the runtime-list ledger, workforce allocation records behind a ledgered allocation table, concrete local route-access context dependencies constructor-bound to the module-owned runtime list/allocation table, and the module's allocation/list/load-preserve globals collapsed behind a single file-owned runtime state.
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
- [x] Expose normalized progress/logical offsets to rendering while preserving legacy output. Tile and cross-country progress convert through a 16.16-style normalized fraction before camera projection, then round at the legacy screen boundary.
- [x] Version and migrate save progress values to 128-unit tile progress. Save version `0xc1` writes the new grain and converts pre-`0xc1` movement progress while preserving action-timer bytes whose meaning is not spatial progress.
- [x] Convert cross-country movement math to 32-bit-safe units. Coordinates, destinations, and deltas are 32-bit 128-unit tile values; pre-`0xc1` signed 16-bit values convert with 64-bit intermediate arithmetic.
- [x] Make figure rendering consume logical coordinates through camera transforms. `OrthographicCamera` owns tile-progress and cross-country projection and keeps the final legacy integer rounding boundary explicit.
- [x] Convert uniform distance fields to weighted Dijkstra. All-target cost maps use the ordered cost queue, and point-to-point searches use the corresponding weighted A* heuristic over the same accumulated costs.
- [x] Derive route cost from inverse movement speed. Route edge cost is the ceiling of 128 progress units divided by effective movement speed, so the existing highway speed multiplier affects path choice through cost rather than a separate distance constant.
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
- [x] Route `city_figure.cpp` through the figure-owned draw-request facade.
- [x] Add debug counters/logging for resolved versus unresolved figure draw requests. Counter snapshots/reset are renderer-visible, unresolved requests are correctness errors, and the city draw path no longer submits `Image::from_id` as a fallback.
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
- [x] Move figure draw-request resolution/submission out of `city_figure.cpp`; city drawing now asks `Figure` for a `FigureGraphicDrawRequest`, and the request owns layer submission plus map-flag number overlay drawing.
- [x] Replace the map-flag number overlay field triplet with a header-ledgered `FigureMapFlagNumberOverlay` draw component on `FigureGraphicDrawRequest`.
- [x] Move draw-request layer capacity checks and `FigureGraphicsLayer` adaptation onto `FigureGraphicDrawRequest`, deleting the local `add_draw_layer` helper pair.
- [x] Route simple XML-owned service walkers with flat legacy `image_group` graphics through `FigureGraphics` draw requests instead of the city-level `Image::from_id` fallback.
- [x] Move cart/resource/animal/standard and ship-family presentation into figure graphics layers. Lion-tamer animals, Roman fort-standard banner/badge stacks, both prefect water-bucket states, complete editor map flags, hippodrome horse/cart race teams, every legacy missile-launcher pose schedule, lighthouse-supplier carts, and production/storage/docker resource carts are strict FigureType graphics assembled or selected by `FigureGraphics`. Resource carts bind owner-specific `FigureGraphicsState`; action controllers publish only hidden/empty/resource at the original visibility ticks, XML owns load fallback, offsets, ordering, and docker body suppression, and `cart_image_id` remains only the old-save hydration/synthesis bridge. Generic `<directional>`/`<pose>` data now owns fishing-boat ordinary/fishing rows and trade-ship dock-facing sprites directly from saved action/direction inputs, without altering assignment, routes, dock orientation, or water movement. Graphics-only definitions do not publish runtime profiles, so migrated presentation cannot replace legacy movement/combat dispatch. Auxiliary infantry/archer standards intentionally retain the existing pole-only output until their native banner assets become an authored contract.
- [~] Transition figure graphics to image group payload manager ownership with real file-path references instead of legacy group/image-id references. The rejected Vespasian graphics-module wrappers are removed. All active Vespasian FigureType definitions now use payload paths with zero legacy source attributes, and the 35 shared definitions changed by the Augustus payload work carry the same graphics blocks while preserving Vespasian profiles and behavior. Many paths still target lower-layer extracted `Group_*` assetlists; the remaining authored migration is to replace those with sensible one-file-per-graphic assetlists that own source paths, dimensions, layer roles, offsets, isometric flags, and animation frames.
- [ ] Add Vespasian half-size FigureType XML overrides using existing source art after all Renderer Scaling Seams work and correctly scoped figure payload ownership are complete. The rejected wrapper pass and its behavior-moving module schema are removed; half-size declarations remain blocked on clean authored assetlists.
- [x] Make zero FigureType legacy draw fallbacks a required validation threshold. `city_figure.cpp` contains no `Image::from_id` submission or sprite-offset fallback; a request that cannot resolve is counted and reported as an unresolved renderer correctness error and draws nothing, preventing compatibility output from hiding the bug.
- [~] Delete legacy figure image-id arithmetic and duplicate corpse/direction/cart tables. Cart attachment and corpse timing are complete as described above, with only save-compatible resource-cart bytes retained. General direction work is now proceeding by bounded families: ballista idle, firing, and terminal corpse presentation is complete in strict graphics-only FigureType data. Its three `<state>` layers own the image group and view-relative direction transform, the firing layer consumes the sibling `<missile_launcher>` schedule, graphics-only state definitions resolve through `FigureGraphics`, and `figure_ballista_action` no longer calculates or mutates image ids. Tower sentries, animals, enemy/soldier combat, carts, and other view-relative callers remain separate slices.

## Renderer Scaling Seams

Source plan: `docs/renderer_scaling_seam_plan.md`

Requirement: scaling filters and logical sizes must produce exact seamless city-view geometry across native and remaining atlas paths.

Gate: this entire section blocks Vespasian half-size FigureType XML. The XML slice depends on source pixel dimensions being split from fixed-point logical image dimensions, and on the seam fixes being validated first.

- [x] Add focused terrain render test matrix for scale filters, grid state, zoom, and atlas/native paths; `RendererSeamTest` now generates the matrix, JSON results, and scale-aware real-pixel software fixture cases for city-tile geometry.
- [x] Split the software isometric geometry fixture into explicit scale-aware `--matrix city-tile-geometry` coverage and remove it from the `terrain-water` false-pass path.
- [x] Move city-tile geometry fixture SDL setup, scale-aware canonical tile geometry, rounded shared-edge endpoint assertions, BMP artifact naming/writing, and artifact status reporting behind the header-ledgered fixture object.
- [x] Move seam report JSON serialization, status derivation, expected-skip artifact paths, fixture-result adaptation, result-file artifact preparation, and summary counting behind a header-ledgered report writer.
- [x] Remove grid-rendering tile-size mutation.
- [x] Introduce exact city-tile destination geometry with shared rounded edges. Terrain, water, road, bridge-underlay, highway, and managed runtime footprint submissions select the shared-city-tile destination policy; the shared helper floors near edges and ceils far edges so fractional nearest-filter draws overlap safely while integral-scale legacy geometry stays unchanged. Legacy non-mesh sprite output remains on default geometry.
- [x] Add temporary atlas edge padding while atlas fallback remains. `ATLAS_MAIN` packing reserves one-pixel gutters, duplicates every content edge into them, keeps source rectangles on real content, and disables the obsolete main-atlas source crop while retaining compatibility behavior for other fallback atlases.
- [x] Move terrain, water, and climate images into managed native image resources. Climate load uploads every non-placeholder `ATLAS_MAIN` image and top slice into `ImageManager`-owned keyed textures before the atlas is created; renderer resolution prefers those managed handles and retains the padded atlas only as fallback.
- [x] Split source pixel dimensions from fixed-point logical image dimensions. Managed runtime slices retain source raster width/height separately from 120-unit-per-pixel fixed logical sizes; image-group XML accepts paired integer `logical_width`/`logical_height` units, default payload metadata preserves legacy source-sized output, and render submission scales logical units through the existing camera zoom value. FigureType XML is forbidden from declaring `logical_width`, `logical_height`, `logical_units_per_source_pixel`, or sprite offsets; strict parser coverage requires these asset facts to live on the referenced graphics assetlist entry. The five shared 26,29-anchored walker families now carry that anchor on their 428 referenced animation, action, and death assetlists instead of their Augustus/Vespasian FigureTypes; the four portrait assetlists remain unanchored.
- [x] Add pixel checks for terrain/water seams; passing the matrix is the deployment-worthy threshold for renderer seam changes. `JuliusGraphicsExtractor` preserves climate-specific flat-tile/grass/water snapshots, and `RendererSeamTest` decodes that real 58x30 art, renders all 4,608 Vespasian climate/backend/filter/scale/grid/orientation/scene cases, checks shared-edge coverage/black gaps, grid-only interior stability, and managed-native versus padded-atlas signatures, writes JSON/failure BMP artifacts, and requires zero skips. The Release matrix passes 4,608/4,608.
- [ ] Unlock Vespasian half-size FigureType XML only after every preceding Renderer Scaling Seams item and correctly scoped figure-owned native graphics payload ownership are complete. The 4,608/4,608 terrain/water seam matrix and proportional per-slice logical sizing are complete; the Vespasian XML ownership boundary and half-size declarations remain gated.

## Render Performance And Vulkan Direction

Source plan: `docs/render_performance_plans.md`

Requirement: move rendering toward command lists, snapshots, GPU-owned resources, and an eventual Vulkan backend.

- [x] Add city draw metrics for top, figure, animation, submission, render requests, managed image requests, texture misses, tile rows, and rendered tiles.
- [x] Consolidate several visible-row traversal and draw-adapter compatibility paths.
- [x] Add transitional fixed logical-size fields at the renderer request boundary.
- [x] Convert `FigureGraphicDrawRequest` to `render_logical_size`.
- [x] Centralize render-domain classification and `scale_filter` interpretation in `Render2DPipeline`.
- [x] Extend metrics to native cache and legacy image buckets. Performance samples now separate managed-handle cache hits/misses and main, enemy, and other legacy-atlas submissions in addition to aggregate managed/fallback totals.
- [x] Collapse remaining repeated visible-row traversal overhead. One row-preserving `CityViewRenderCommandBuffer` now resolves screen position, grid offset, `Building *`, and first `Figure *` exactly once per city frame. Footprint, normal row phases, connectable ghosts, deletion passes, elevated figures, and custom overlay layers replay the same immutable row spans; full-pass ordering remains unchanged where deletion and ghost occlusion require it.
- [x] Move status-icon anchors and remaining gatehouse/decorative-gate tile-composite draw mapping into BuildingType/native draw data; all six gate variants share the normal/overlay terrain-underlay renderer, and warehouse, granary, fountain, and farm mothball/stockpile anchors are strict root-graphics XML data with generic image-centered fallback for unanchored types.
- [x] Add city draw command prepass carrying object references. `CityDrawTileCommand` records screen position, grid offset, `Building *`, and first `Figure *` for each visible row before phase dispatch, removing repeated grid-to-object head lookups from normal and overlay top/figure/animation phases.
- [x] Replace broad native graphics signatures with owner-controlled generation counters. Building graphics cache bindings now compare explicit building, composition-owner, and city-view-orientation generations; owned mutation seams invalidate them, and the per-render broad building-record hash is deleted.
- [~] Add renderer-facing command structs and recording mode behind the existing draw API. `renderer_command` records clear, viewport, clip, line/outline/fill, output-scale, render-domain, push/pop state, legacy image, managed image, custom image, and saved-region image operations; the SDL backend can begin/end/replay capture without changing call sites. Legacy image commands own copied image metadata rather than retaining world pointers, recording maintains a separate domain stack without mutating live SDL state, and replay refuses stale resource revisions. Capture/replay pixel-parity coverage remains before production city drawing can switch to recording.
- [~] Batch obvious same-texture UI/terrain/building runs where ordering allows. Off-thread preparation now groups adjacent managed-handle, custom-texture, and saved-texture commands without crossing order/state boundaries; SDL replay consumes the prepared batch spans in order, while a Vulkan consumer can turn each span into true instanced submission. SDL still issues one copy per command, so backend draw-call coalescing remains.
- [x] Add renderer snapshots with explicit revisions. End-of-recording publishes an immutable command packet with monotonic command and resource-residency revisions. Borrowed snapshots remain available for immediate inspection, while acquired snapshot handles retain the packet across later frame publication and replay holds an acquired packet until retirement. Resource mutation advances the residency revision and stale replay fails loudly. Immutable simulation/world snapshots remain a separate Vulkan preparation input.
- [x] Add thread-pool-backed render preparation. A bounded preparation pool (half the hardware concurrency, capped at four workers) consumes sealed command packets through a newest-work mailbox, builds order-preserving same-texture batch spans off-thread, publishes by revision, and is joined cleanly at shutdown; replay waits for the exact prepared revision rather than reading mutable recording storage.
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
- [x] Route with-overlay farm footprint/top drawing through `Building::draw_footprint` and `Building::draw_top`.
- [x] Move the old hidden-overlay farm corner rule behind `BuildingGraphics::draws_overlay_summary_at`.
- [x] Move `native_crops` frame selection into XML `production_progress` options and delete the direct `GROUP_BUILDING_FARM_CROPS` map-image mutation.
- [x] Move Augustus/Vespasian warehouse/granary storage permission flags into XML `storage_permission` graphics layers, keep Julius flagless to match upstream Julius, and delete the old city-draw storage helper branches.
- [x] Move the gatehouse top overlay image/offset/orientation mapping into BuildingType graphics data and `BuildingGraphics::draw_gatehouse_overlay`.
- [x] Peel the native graphics variant byte out of the runtime building record into `BuildingGraphicsState`; `Building` owns the `BuildingGraphics` container, the container binds owner/state/definition, and save/load copies the existing record byte into/out of module state with no save-version gate or load-time special case.
- [x] Move no-overlay mothball/stockpile icon placement and farm field suppression behind `Building::mothball_status_icon_offset(...)` and `BuildingGraphics`.
- [x] Finish storage and tile-composite cleanup; `resource_storage` uses generic `BuildingGraphics`, all six gates use one XML-authoritative terrain-foundation underlay path in normal and overlay rendering, and paved/dirt road surface selection has one canonical helper without overlay `+49` reconstruction.
- [x] Finish network/water preview graphics cleanup; reservoir, aqueduct, and bridge previews now share native `BuildingGraphics` rendering, bridge piece variants come from the bridge strategy, road-surface projection comes from the road/aqueduct strategy, and connectable previews use one garden-wall-to-gate projection path backed by the gate BuildingType graphics definition.
- [~] Delete compatibility branches made obsolete by strategy data; SmartTool tokens, raw first-of-type traversal, fixed runtime formation-size fallback, duplicated network previews, gate synthesis, overlay road arithmetic, literal gatehouse/dock draw dispatch, low/ship-bridge no-op handlers, and mission-post/military-academy/barracks/warehouse/fort-ground/statue/hippodrome image fallbacks are deleted. Warehouse-space emptiness is resolved by its resource-storage graphics strategy, small/medium statue rotation is native `orientation` data, hippodrome owner/middle/end finished and phased art is native `build_rotation` data, and composed farm owners author their overlay-summary policy without a runtime farm classification branch. Broader renderer branches and save-version bridges remain.

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
- [x] Make barracks recruitment and overflow ejection consume `FormationType.recruit_capacity`; Vespasian explicitly authors 16 for its 8x8 formations while the larger-formation visual gates remain.
- [x] Add fort-owned `Formation` object links with stable addresses, composition-child delegation, and live formations pointing to resolved `FormationType` definitions.
- [x] Replace hardcoded formation-size loops with `Formation` object iteration; unbounded enemy rosters grow dynamically, declared formations retain every overflow figure, and only the old-save prefix remains fixed-size compatibility data.
- [x] Split fixed roster save/storage from 16-offset layout tables before enabling more than 16 live slots.
- [x] Move live combat-actor stats and abilities fully into `UnitType`; Roman forts, city combat actors, standard invasion/Caesar families, enemy catapults, wolves, the crime family, the armed mess-hall supplier, and city ballistae own their exact live policy in identical Julius/Augustus/Vespasian XML. Wolf difficulty values are authored while herd state stays unchanged; crime and supplier missions remain their owning runtime behavior. Projectile movement/collision/sounds and numeric fallback remain projectile-owned so saved in-flight arrows, spears, javelins, friendly arrows, bolts, and legacy catapult missiles retain their stable figure ids and load behavior. Faction-wide invasion morale correctly remains formation policy rather than UnitType data.
- [x] Bridge tactical formation layouts into declared runtime objects. `Mods/Julius/FormationLayout` supplies the 13 layered XML definitions, each with a unique stable legacy id, 16 authored compatibility positions, and authored or explicitly reused enemy-army orientation offsets; runtime formations and enemy armies bind `FormationLayoutDef*`, UI and combat use string identities, and only save/scenario bridges synthesize or resolve numeric ids. The duplicate C++ enemy-army spacing table is deleted.
- [x] Delete runtime constants that assume 16 soldiers. `LEGACY_FORMATION_SAVE_SLOT_COUNT` is private to exact old-save serialization, authored layout definitions provide their compatibility positions, and Vespasian's temporary recruit limit is authored XML data.

## Current Regression Notes

Requirement: keep only active manual-test notes here; resolved findings belong in git history or focused docs.

- [ ] Plaza routing refresh and broader area-tool undo/redo still need runtime sweep; the reported garden adjacency/overlap regression was manually confirmed fixed.
- [~] Path blockers preview correctly through the road smart tool but final placement creates no building object. Load promotion binds a surface `Building` record to legacy road terrain; the valid blocker plan then collided during foundation publication. Placement now transactionally retires permitted surface bindings, publishes the new foundation, and restores the prior record/bindings on failure. Awaiting manual confirmation.
- [~] Hippodrome orientation must be selectable with the `R` rotation hotkey. The deployed build kept reporting `1/4`; rotation now captures one stable construction type, invokes type cycling only for actual cycle definitions, and updates preferred/retained geometry from that state. Awaiting manual confirmation of the counter, ghost, and final graphics.
- [~] On load, water beneath bridges initially renders as a solid-color tile and only recomposes later, with recurring fishing-boat navigation warnings. Terrain-backed native graphics now republish foundation/map ownership with `image_id=-1` instead of overwriting composed water with the flat runtime sentinel; bridge migration restores staged graphics for every dynamic segment. The fishing warning was separate: wharf boat access now selects a live exterior water cell from the published rotated foundation instead of a mismatched legacy orientation switch. Awaiting manual confirmation.
- [x] Replace hardcoded smart-tool/token-building dispatch with an XML-backed `SmartToolDef` runtime. Context-aware owner modes now drive clear/road modifiers, hover-water low/ship bridge choice, and reservoir-routed aqueduct segments with deterministic modifier priority and `any` fallback. The real reservoir owns its button/tool and preserves `draggable_reservoir` only as an identity alias; shoreline probing and routed-network algorithms remain intentional procedural boundaries.
- [~] Declarative `<foundation replaces="...">` records existing building/terrain ownership in the immutable placement result. Aqueduct, reservoir-over-aqueduct, road, and highway final publication now consume retained plans transactionally with rollback and scalable undo; road/highway preview alone keeps legacy painting to render the full ghost. Parser contracts pass; manual dragging/commit retest remains.
