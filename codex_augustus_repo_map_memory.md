# Codex Augustus repository map and implementation memory

Snapshot: 2026-06-27
Workspace: <repository checkout>

## Top-level layout that matters now
- `src/` - main game source tree
- `res/` - resources, icon/resource script, version files, asset tools
- `ext/` - bundled third-party source
- `vendor/` - local SDL2 / SDL2_mixer binary drops for the VS workflow
- `Vespasian.sln` / `Vespasian.vcxproj` - authoritative Windows build entrypoint
- `x64/`, `.vs/`, local output folders - local build state only

## Build/project facts
- The active local build workflow is the root Visual Studio/MSBuild project.
- The Visual Studio solution/project at repo root is the build path that matters.
- `res/augustus.rc` is part of the project and provides the executable icon/resource wiring.
- Runtime performance tracker contract: `docs/performance_tracker_runtime.md`; `debug_performance_tracker` enables the `vespasian-performance.log` sidecar next to `vespasian-log.txt`.
- Deep refactor checklist and requirements live in `docs/deep_refactor_implementation_progress.md` and `docs/deep_refactor_requirements.md`.
- Owner-bound runtime module extraction planning lives in `docs/object_owned_runtime_refactor.md` and `docs/bound_runtime_module_extraction_plan.md`.

## Upstream lineage and source references
- Julius is the base project repository: https://github.com/bvschaik/julius
- Augustus is a gameplay fork of Julius: https://github.com/Keriew/augustus
- When checking vanilla behavior, compare against Julius first; when checking Augustus-added behavior, compare against the Augustus fork. This matters for XML migrations because copied mod data can accidentally pull Vespasian/Augustus behavior into Julius or vice versa.
- Tile graphics need the same upstream care: Augustus added a newer tile graphical set on top of Julius, so Augustus/Vespasian tile XML can legitimately refer to both Julius base tile images and Augustus-added tile aliases behind the same logical path.
- `docs/gameplay_divergences_from_augustus.md` is the living ledger for player-visible ways this repo's bundled profiles or Vespasian content intentionally diverge from upstream Augustus behavior.

## Renderer/backend map
### Platform layer
- `src/platform/augustus.cpp`
  - SDL startup
  - startup failure reporting
- `src/platform/screen.cpp`
  - window sizing, native resolution, display-size handling
- `src/platform/render_2d_pipeline.h`
- `src/platform/render_2d_pipeline.cpp`
  - request-based 2D scaling/filter policy
  - render-domain mapping
- `src/platform/renderer.cpp`
  - SDL renderer integration
  - draw-image-request backend
  - tooltip/snapshot/offscreen state preservation

### Graphics layer
- `src/graphics/renderer.h`
  - `render_2d_request`
  - render domains
  - scaling policy enums
- `src/graphics/GraphicsDefinition.h/.cpp`
  - shared graphics definition vocabulary and comparison/orientation concepts
- `src/building/BuildingGraphics.h/.cpp`
  - building-specific graphics selection and native building draw policy
- `src/figure/FigureGraphics.h/.cpp`
  - FigureType graphics policy, native/cached figure graphics bindings, legacy fallback draw helpers, and figure graphics migration target
- `src/game/ResourceGraphics.h/.cpp`
  - resource icon/presentation graphics; resource cart/building draw policy should route through figure/building graphics instead of expanding this class
- `src/graphics/image.cpp`
  - legacy image/glyph wrappers now emit request-based draws
- `src/graphics/font.h`
- `src/graphics/font.cpp`
- `src/graphics/text.cpp`
  - current font/text layer
  - logical font metric groundwork exists through `metric_scale_percentage`
  - shared measured numeric-pair helpers keep `n/m` and `n x m` UI counters out of widget-side fixed-width hacks
- `src/graphics/tooltip.cpp`
  - tooltip draw path now uses the renderer's scoped domain/state flow
- `src/graphics/window.cpp`
  - full-window draw orchestration
  - reasserts UI render scale after tooltip/warning/underlying-window flow

## Shared UI runtime map
- `src/graphics/ui_primitives.h`
- `src/graphics/ui_primitives.cpp`
- `src/graphics/ui_widget.h`
- `src/graphics/button_widget.h`
- `src/graphics/bordered_button_widget.h`
- `src/graphics/bordered_button_widget.cpp`
- `src/graphics/image_button_widget.h`
- `src/graphics/image_button_widget.cpp`
- `src/graphics/panel_widget.h`
- `src/graphics/panel_widget.cpp`
- `src/graphics/label_widget.h`
- `src/graphics/label_widget.cpp`
- `src/graphics/top_menu_panel_widget.h`
- `src/graphics/top_menu_panel_widget.cpp`
- `src/graphics/scrollbar_widget.h`
- `src/graphics/scrollbar_widget.cpp`
- `src/graphics/ui_runtime.h`
- `src/graphics/ui_runtime.cpp`
- `src/graphics/ui_runtime_api.h`

Purpose:
- `UiPrimitives` owns low-level request-based drawing and saved-region helpers
- `UiWidget` is the base widget layer
- concrete widget classes own their own composition/data
- `SharedUiRuntime` orchestrates widget objects and exposes the C facade

Current users:
- `src/graphics/button.c`
- `src/graphics/panel.c`
- `src/graphics/image_button.c`
- `src/graphics/scrollbar.c`
- `src/graphics/graphics.cpp`

Important architectural note:
- `window.cpp` is the full-window/pass orchestration seam
- `ui_runtime.cpp` is the facade/orchestration seam for shared widgets
- widget behavior belongs in the widget classes, not in the runtime itself
- For UI work, read `docs/ui_widgets_and_primitives_working_memory.md` before adding new primitives/widgets; it records the current vocabulary, widget stock, ad-hoc UI tally, and safe deduplication targets.

## Asset loading / graphics-pack map
- `src/game/mod_manager.cpp`
  - selected mod ownership and graphics-path validation
- `src/assets/assets.h`
- `src/assets/assets.cpp`
  - asset bootstrap
  - retained critical failure reason
- `src/assets/xml.cpp`
  - XML-driven asset definitions
- `src/core/image.cpp`
  - bridges legacy image ids to asset-backed images

### Canonical extractor / building graphics seams
- `docs/graphics_extraction_pipeline.md`
  - current extractor handoff, standalone CLI usage, output contract, known residual generated-reference buckets, and clean validation commands
- `src/core/legacy_image_extractor.cpp`
  - canonical Julius extraction
  - exposes `LegacyClimateAtlas`, `JuliusExtractor`, and `GroupImageKey` through the C++ header while retaining narrow C bridges
  - trims bottom transparent padding from isometric footprint PNG exports while preserving logical placement through exported XML height / layer `y`
  - exports split-aware compatibility aliases for legacy visible ranges such as `Aesthetics\House_Tent/Image_0001..Image_0005`
  - exposes native `JuliusExtractor::resolveLegacyGroup()` and `JuliusExtractor::resolveLegacyImage()` so Augustus numeric references resolve to canonical split group/image keys
- `src/assets/augustus_asset_extractor.cpp`
  - canonical Augustus extraction
  - exposes `ExtractorPaths`, `ExtractorOptions`, `ExtractionReport`, `AugustusExtractor`, and `RuntimeGraphicsExtractionService`; runtime C calls forward through the service
  - reads packaged game-folder `assets\Graphics` XML/PNG atlases and exports per-group XML plus per-image PNGs under `Mods\Augustus\Graphics`
  - rewrites numeric Julius references through canonical split-aware group/image keys
  - resolves same-group `group="this"` part inheritance recursively so active Augustus ON variants preserve both footprint and top slices
  - keeps alias groups for source-visible wrapper names instead of skipping collisions silently
- `src/assets/augustus_julius_template_resolver.cpp`
  - class-based `JuliusTemplateCatalog` parses extracted Julius XML templates for Augustus reference translation and next generated `Image_####` id discovery
- `src/assets/graphics_extractor_common.cpp`
  - shared path/key/file helpers and the extractor-owned `XmlReader`/`XmlElement` parser used by the Julius/Augustus extraction split
- `tools/augustus_graphics_extractor/main.cpp`
  - standalone `AugustusGraphicsExtractor.exe` CLI for clean repo-side extraction tests
- `src/assets/image_group_payload_materialize.cpp`
  - runtime composition/materialization seam for extracted graphics
  - no longer carries the temporary runtime footprint crop/offset workaround; extractor output now defines placement semantics
- `src/building/building.h` / `src/building/building.cpp`
  - `Building::draw_footprint(...)`, `draw_top(...)`, and `draw_animation(...)` are the native building footprint/top/animation draw seams
  - native whole-building footprints now draw on the owning draw tile only
- `src/widget/city_draw.cpp`
  - native terrain-tile footprint bridge only; building draw stages now enter through `Building`
- `src/building/building_runtime_graphics.cpp`
  - resolves BuildingType graphics targets, stable options, and cached `RuntimeDrawSlice` bindings
  - delegates animation frame policy to `BuildingAnimation`
- `src/building/animations.h`
- `src/building/animations.cpp`
  - graphics target helper classes plus `BuildingAnimation`, which owns frame cursor normalization, legacy animation gates, reversible/looping/wine-workshop advancement, storage flag animation, and fumigation animation
- `src/widget/city_overlay_other.cpp`
  - converted overlay translation unit that now calls `BuildingAnimation` directly instead of using the removed `building_animation_*` C facade

Current graphics XML precedence:
1. active mod stack from top to bottom through `mod_manager_get_graphics_path_at()`
2. root Augustus assets folder `assets/Graphics`
3. Caesar 3/original atlas-backed assets for legacy atlas/image fallback

Doctrine:
- optional missing overrides should warn/fallback
- broken critical assets/data should fail at load with a retained reason

## Resource runtime map
- `docs/resource_runtime.md`
  - current resource XML contract, graphics ownership, and producer/warning responsibility split
- `Mods/<Mod>/Resources/*.xml`
  - selected-mod complete resource definitions
- `src/game/resource.cpp`
  - parses resource XML into `resource_data` defaults
- `src/game/resource_id_bridge.cpp`
  - save-local resource id table plus legacy raw-id migration maps for old saves
- `src/game/resource_graphics.h`
- `src/game/resource_graphics.cpp`
  - `ResourceGraphics` exposes semantic `ImageGroupEntryRef` handles for carts, warehouse stacks, panel icons, empire icons, and editor icons
- `src/building/industry.cpp`
  - owns production/resource inference through `building_output_resource(...)` and `building_producer_for_resource(...)`
- `src/building/construction_warning.c`
  - gameplay trigger for construction warnings; derives missing-resource and missing-producer warning templates after resources/buildings/production are loaded

Resource motive:
- resources own text id, numeric slot, locale key, and model/trade values
- resources do not own producer buildings, industry menu names, or warning templates
- live saves persist save-local resource ids through text ids; legacy raw numeric resource maps live only in the resource id bridge
- C++ callers should use `resource_graphics(resource).panel_icon().draw(...)` instead of reaching through `resource_data` image fields or raw image ids

## Config / settings map
- `src/core/config.h`
- `src/core/config.cpp`
  - `Vespasian.ini`
  - legacy fallback reads from `augustus.ini`
  - `CONFIG_SCALE_FILTER` / `scale_filter`
  - `CONFIG_DEBUG` / `debug`, currently used to gate zoom percentage warnings
- City zoom stores raw renderer scale, but player-facing start/reset/display percentages are adjusted by UI scale; displayed interactive bounds are `33%` to `300%`, capped by map size.

## Save/load map
- `docs/save_data_organization.md`
  - canonical `.svv` file-piece order, allocation ownership, writer/loader map, dynamic payload notes, and scenario-file appendix
- `docs/save_load_runtime_bridges.md`
  - post-read bridge map for save-local ids, BuildingType runtime ids/text ids, monument construction fan-out, legacy structs, and C++ runtime wrapper rebinding
- `src/game/file_io.cpp`
  - `init_savegame_data()` owns live-save piece order
  - `savegame_save_to_state()` and `savegame_load_from_state()` dispatch subsystem writers/loaders
- `src/game/save_version.h`
  - save and scenario version gates; update when persisted layout or behavior changes
- Key save-backed runtime payloads currently include building records, figure records/routes, building type save tables, road service history, and local workforce allocations.
- The current save also has resource and water access type save tables. They persist save-local ids as text ids, then resolve them against active `Resources` and `WaterAccessType` XML so runtime numeric ids can remain mod-defined.
- Current architectural target: save/load should hydrate current runtime structs plus module-owned state, then reconstruct save records at the bridge. `id` fields are stable identity bridge keys. Ordinary peeled fields should leave the runtime record as their module takes ownership and be appended back only by the save bridge.

## Water access runtime map
- `Mods/<Mod>/WaterAccessType/*.xml`
  - selected-mod declarations of water access text ids and numeric ids `0..7`
- `src/building/water_access_type.h`
- `src/building/water_access_type.cpp`
  - load-time registry, uniqueness/range validation, and text-id to mask lookup
- `src/building/water_access_type_id_bridge.h`
- `src/building/water_access_type_id_bridge.cpp`
  - save-local water access id table and legacy raw-id migration
- `src/building/building_type.h`
- `src/building/building_type.cpp`
- `src/building/building_type_registry_xml.cpp`
  - BuildingType `<water_access>` provider and requirement rule storage/parsing
- `src/building/water_access_runtime.h`
- `src/building/water_access_runtime.cpp`
  - fixed-point provider simulation, typed access masks, aqueduct wet-state projection, terrain range projection, building `has_*_access` mirror projection, and placement-preview highlights
- `src/widget/city_building_ghost.cpp`
- `src/widget/city_water_ghost.cpp`
  - placement/context overlays call the generic water runtime queries instead of hardcoded provider-type helpers

Water access motive:
- providers declare what access types they emit, where from, and at what range
- consumers declare what access types or natural-water source terms they require
- runtime masks are compact, but content and saves remain text-id driven
- aqueducts are ordinary water access providers/consumers evaluated through the same fixed-point rule pass as reservoirs.

## Demographics / defines map
- `src/game/defines.cpp`
  - merged `calendar`, `birth_table`, and `mortality_table` XML definitions
- `Mods/*/defines.xml`
  - active `default` demographic/calendar tables by mod stack
- `src/city/population.cpp`
  - citywide age census, yearly births/deaths, and census recalculation
- `src/building/house_population.cpp`
  - actual per-house resident additions/removals and consistency reconciliation
- `docs/demographics_runtime.md`
  - runtime contract between demographic tables, census counts, and house populations
- `research/roman_city_size_and_social_ratios.md`
  - Roman city-size bands, plebeian/patrician interpretation, elite/common ratios, and dependent-labor caveats
- `research/roman_city_facility_ratios.md`
  - Roman service-building ratios for population, labor, and area tuning
- `research/roman_building_maintenance_needs.md`
  - maintenance labor, water, fuel, sanitation, and failure-mode guidance for service and infrastructure buildings
- `research/caesar3_julius_housing_progression_defaults.md`
  - vanilla housing progression values, service gates, visual descriptions, and design-preservation guidance
- `research/caesar3_housing_balance_play_analysis.md`
  - Caesar-specific housing efficiency, labor cliff, goods friction, and service difficulty notes
- `research/vespasian_housing_progression_design_notes.md`
  - forward-looking Vespasian mechanics for classed labor, demand gates, service capacity, road access, and market revenue
- `research/comparative_citybuilder_design/`
  - weakly linked comparative design notes for SimCity RCI, Anno tiers, Stronghold popularity, and Seven Kingdoms economy patterns

## Current migration reference points
- `src/building/tool_mode.cpp`
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_graphics.cpp`
- `src/building/animations.cpp`
- `src/building/building_runtime_spawn.cpp`
- `src/building/building_runtime_api.h`
- `src/building/building_type.cpp`
- `src/building/building_type_registry.cpp`
- `src/building/building_type_registry_xml.cpp`
- `src/building/building_type_id_bridge.cpp`
- `src/building/water_access_runtime.cpp`
- `src/building/water_access_type.cpp`
- `src/building/water_access_type_id_bridge.cpp`
- `src/building/housing_type.cpp`
- `src/building/housing_type_registry.cpp`
- `src/building/house.cpp`
- `src/building/construction_session.h/.cpp`
- `src/figure/figure_type_registry.cpp`
- `src/figure/figure_runtime.cpp`
- `src/figure/figure_runtime_native.h/.cpp`
- `src/figure/movement.cpp`
- `src/figuretype/maintenance.cpp`
- `src/map/road_service_history.h`
- `src/map/road_service_history.cpp`
- `src/platform/renderer.cpp`
- `src/graphics/ui_runtime.cpp`
- `src/graphics/ui_primitives.cpp`
- `src/widget/top_menu.cpp`

## Native BuildingType / HousingProfile map
- `Mods/Vespasian/BuildingType/_README.md`
  - current XML contract for BuildingType identity, model, foundation, button, sound, event data, flags, water access, state refresh, graphics/options, construction, labor, storage, production, housing, and spawns
- `docs/water_access_runtime.md`
  - current architecture and call chains for WaterAccessType XML, BuildingType water rules, fixed-point propagation, aqueduct/reservoir behavior, overlays, save bridges, and compatibility mirrors
- `Mods/Vespasian/HousingProfile/_README.md`
  - current XML contract for residential requirements, resident class, prosperity, tax multiplier, and legacy house-level compatibility
- `research/caesar3_julius_housing_progression_defaults.md`
  - Caesar III / Julius house capacities, footprints, service gates, desirability thresholds, graphics references, and tuning degrees of freedom
- `research/caesar3_housing_balance_play_analysis.md`
  - patrician non-labor, plebeian labor base, tier-efficiency heuristics, and goods/service friction for HousingProfile/BuildingType tuning
- `docs/building_type_legacy_reference_ledger.md`
  - cleanup queue for remaining building-type enum references and whether each path is migrated, bridged, retained, or still needs a future phase
- `docs/save_load_runtime_bridges.md`
  - save-local BuildingType id table, old raw-id migration, native graphics variant normalization, and post-load runtime wrapper rebuilding
- `docs/gameplay_divergences_from_augustus.md`
  - living gameplay ledger for project-wide, bundled-Augustus, and Vespasian-only differences from upstream Augustus
- Vespasian, Augustus, and Julius now define the full native house chain from `house_small_tent` through `house_luxury_palace`. BuildingType owns footprint, capacity, graphics, transitions, and runtime identity.
- HousingProfile owns shared residential requirement/tax/prosperity data and resident class.
- Legacy `house_level` remains a compatibility value for old save migration, old city-stat arrays, and UI/stat surfaces that still need a level-like key.

Pattern:
- keep new reusable behavior in C++ classes
- expose narrow C-callable seams where old C code still depends on them
- avoid widening the touched surface when a subsystem chokepoint can be introduced instead

## Native walker / FigureType map
- `Mods/Vespasian/FigureType/_README.md`
  - XML contract for native walker definitions
- `docs/walker_pathing_runtime.md`
  - runtime flow, road service history, and save compatibility notes
- `docs/tile_scale_and_walker_timescale.md`
  - approximate tile-side scale, `max_roam_length` to meters/ticks conversion, and real/game timescale comparison
- `docs/preindustrial_walking_service_ranges.md`
  - historical walking-city calibration for walker `max_roam_length` tiers
- `src/figure/figure_type_registry.cpp`
  - selected-mod/Augustus/Julius FigureType XML precedence and profiled BuildingType spawn reference validation
- `src/figure/PathingMode.h/.cpp`
  - pathing mode objects and requirements such as `requires_road`, `requires_service_effect`, and `requires_venue_targets`
- `src/figure/figure_runtime.cpp`
  - profile binding, lifecycle rebinding, C facade, and smart-service direction selection
- `src/figure/figure_runtime_native.h/.cpp`
  - native service, engineer, prefect, and entertainment controller classes
- `src/figure/FigureGraphics.h/.cpp`
  - FigureType graphics ownership and cached native graphics bindings; use with `docs/figure_owned_native_graphics_plan.md`
- `src/building/local_workforce.h/.cpp`
  - local workforce labor-seeker targeting, house/workplace allocation table, and save payload
- `src/map/routing_distance.h/.cpp`
  - C++ helper for route-grid destination distance; venue seekers rank by `2 * show_days + route_distance`
- BuildingType native spawns choose a `FigureType` profile with `profile="..."`; figures own the native class, movement/pathing, and road-history effect after creation. Spawn policies may use constant `chance_per_million`, source `chance_per_million_bands`, or source `chance_divisor` gates.
- Market walkers are now FigureType-bound after legacy market spawning: `market_trader` uses roaming service pathing, `market_supplier` owns storage-fetch routing, and `delivery_boy` owns follow-leader behavior.
- Residential walkers are BuildingType-spawned and FigureType-bound: `patrician` uses `house_roamer` with `roaming_service`, while `beggar` uses `unemployment_wanderer` with `transient_wanderer` and `stand_still`. House XML uses `figure_slot="quaternary"` so each house owns at most one active residential walker.
- Priests use explicit god profiles; entertainment service walkers use generic native behavior with profile-specific smart-service effects.
- Mixed entertainment venues use comma-list BuildingType `existing_figure` guards, such as `actor,gladiator`, so alternate profiled service walkers share one legacy slot without orphaning one another.
- `src/figure/movement.cpp`
  - legacy roaming loop and native pathing hook
  - figure-specific roaming access checks; `roads_highway` profiles consider highways while off-road-capable native pathing profiles are rejected at XML load
- Temporary Vespasian tuning: FigureType `max_roam_length` should be roughly 50% larger than Augustus until walker range tuning is revisited.
- `src/figuretype/maintenance.cpp`
  - Worker maintenance action plus retired Engineer/Prefect action-table guards
- `src/map/road_service_history.cpp`
  - per-effect, per-road-tile recency stamps for smart service pathing

## Text / UTF direction
- Internal strings are still legacy encoded `uint8_t *` in most of the engine.
- Translation/font boundaries already do encoding-aware work.
- The current agreed direction is boundary-first:
  - prefer `std::string` / `std::string_view` in new C++ runtime code where practical
  - do not attempt a full UTF storage migration during unrelated renderer/widget work

## Current "next target" map
- Work down `docs/deep_refactor_implementation_progress.md` against the stable requirements in `docs/deep_refactor_requirements.md`.
- Continue figure-owned native graphics and renderer seam work before authoring Vespasian half-size FigureType XML.
- Continue routing cost-map work through `PathingMode`, `Route`, and future `RouteWorld`/cost-map cache ownership rather than local pathfinding helpers.
- Extend the shared UI runtime rollout to more shared widget composition without rewriting large windows wholesale when UI work is in scope.
