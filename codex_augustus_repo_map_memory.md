# Codex Augustus repository map and implementation memory

Snapshot: 2026-04-06
Workspace: C:\Users\imper\Documents\GitHub\augustus_cpp_huge

## Top-level layout that matters now
- `src/` - main game source tree
- `res/` - resources, icon/resource script, version files, asset tools
- `ext/` - bundled third-party source
- `vendor/` - local SDL2 / SDL2_mixer binary drops for the VS workflow
- `Vespasian.sln` / `Vespasian.vcxproj` - authoritative Windows build entrypoint
- `x64/`, `.vs/`, local output folders - local build state only

## Build/project facts
- There is no active root-CMake workflow to center normal work around.
- The Visual Studio solution/project at repo root is the build path that matters.
- `res/augustus.rc` is part of the project and provides the executable icon/resource wiring.

## Upstream lineage and source references
- Julius is the base project repository: https://github.com/bvschaik/julius
- Augustus is a gameplay fork of Julius: https://github.com/Keriew/augustus
- When checking vanilla behavior, compare against Julius first; when checking Augustus-added behavior, compare against the Augustus fork. This matters for XML migrations because copied mod data can accidentally pull Vespasian/Augustus behavior into Julius or vice versa.
- Tile graphics need the same upstream care: Augustus added a newer tile graphical set on top of Julius, so Augustus/Vespasian tile XML can legitimately refer to both Julius base tile images and Augustus-added tile aliases behind the same logical path.

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
- `src/core/image.c`
  - bridges legacy image ids to asset-backed images

### Canonical extractor / building graphics seams
- `src/core/legacy_image_extractor.cpp`
  - canonical Julius extraction
  - trims bottom transparent padding from isometric footprint PNG exports while preserving logical placement through exported XML height / layer `y`
- `src/assets/augustus_asset_extractor.cpp`
  - canonical Augustus extraction
  - rewrites numeric Julius references through canonical group keys
  - now resolves same-group `group="this"` part inheritance recursively so active Augustus ON variants preserve both footprint and top slices
- `src/assets/image_group_payload_materialize.cpp`
  - runtime composition/materialization seam for extracted graphics
  - no longer carries the temporary runtime footprint crop/offset workaround; extractor output now defines placement semantics
- `src/widget/city_draw.cpp`
  - native building footprint/top/animation draw seam
  - native whole-building footprints now draw on the owning draw tile only

Current graphics precedence:
1. `Mods/<selected mod>/Graphics`
2. root Augustus assets folder `assets/Graphics`
3. Caesar 3/original atlas-backed assets

Doctrine:
- optional missing overrides should warn/fallback
- broken critical assets/data should fail at load with a retained reason

## Config / settings map
- `src/core/config.h`
- `src/core/config.cpp`
  - `Vespasian.ini`
  - legacy fallback reads from `augustus.ini`
  - `CONFIG_UI_SCALE_FILTER`

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
- Key save-backed runtime payloads currently include the `0xb6` owned building save heap, legacy building records for old saves, figure records/routes, building type save tables, road service history, and local workforce allocations.

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

## Current migration reference points
- `src/building/tool_mode.cpp`
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_graphics.cpp`
- `src/building/building_runtime_spawn.cpp`
- `src/building/building_runtime_api.h`
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
  - selected-mod/Augustus/Julius FigureType XML precedence and profile validation
- `src/figure/PathingMode.h/.cpp`
  - pathing mode objects and requirements such as `requires_road`, `requires_service_effect`, and `requires_venue_targets`
- `src/figure/figure_runtime.cpp`
  - profile binding, lifecycle rebinding, C facade, and smart-service direction selection
- `src/figure/figure_runtime_native.h/.cpp`
  - native service, engineer, prefect, and entertainment controller classes
- `src/building/local_workforce.h/.cpp`
  - local workforce labor-seeker targeting, house/workplace allocation table, and save payload
- `src/map/routing_distance.h/.cpp`
  - C++ helper for route-grid destination distance; venue seekers rank by `2 * show_days + route_distance`
- BuildingType native spawns choose a `FigureType` profile with `profile="..."`; figures own the native class, movement/pathing, and road-history effect after creation.
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
- Extend the shared UI runtime rollout to more shared widget composition without rewriting large windows wholesale.
- Keep world-renderer/zoom/quad-map work for a later phase built on top of the current request-based backend.
