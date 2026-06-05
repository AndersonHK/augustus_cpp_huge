# Codex Augustus handoff memory

Snapshot: 2026-05-11
Workspace: C:\Users\imper\Documents\GitHub\augustus_cpp_huge

## Recommended next-chat posture
- Yes: start a new chat for the next feature.
- Reason: the current arc now spans VS/MSBuild repair, mixed C/C++ migration, asset-pack precedence, load-failure doctrine, renderer backend refactor, shared UI runtime rollout, BuildingType graphics migration, typed water access, and animation ownership cleanup.
- Session bootstrap:
  - read the four core Codex memory files first
  - then read task-specific docs such as `docs/graphics_extraction_pipeline.md`, `docs/walker_pathing_runtime.md`, `docs/water_access_runtime.md`, `Mods/Vespasian/FigureType/_README.md`, `Mods/Vespasian/BuildingType/_README.md`, `Mods/Vespasian/HousingType/_README.md`, `docs/save_load_runtime_bridges.md`, `docs/building_type_legacy_reference_ledger.md`, or the renderer/widget sections in `codex_augustus_repo_map_memory.md`
  - when adding a new system doc, link it from the most relevant core memory file and from nearby subsystem READMEs so it is findable without crowding this file
- The next chat should begin from the current renderer baseline:
  - request-based 2D backend active
  - render domains active
  - shared UI object chain active for common widget primitives
  - graphics asset fallback chain active

## Current stable architecture
- Root build entrypoint remains:
- `Vespasian.sln`
- `Vespasian.vcxproj`
- Build only when useful for the current task. When building, use `Release|x64` with the project defaults such as `/O2`; MSBuild may need to be invoked directly from `D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`.
- Keep CRLF on touched files.
- Preserve existing comments when editing files.
- Use `#pragma once` for project-owned headers instead of classic include guards.
- Prefer object-owned mode metadata such as `PathingMode::requires_road` over scattered helper predicates or switch/case lists for attributes that belong to a runtime concept.
- When code behavior, XML contracts, save/load layout, or runtime classes change, update the relevant markdown and add concise function comments for non-obvious touched logic unless the user says not to.

## Recently added gameplay runtime context
- Native walker definitions now use FigureType XML for the currently ported service walkers.
- Native BuildingType XML now owns full bundled house chains for Vespasian, Augustus, and Julius; HousingType XML owns resident class and shared residential model data while BuildingType owns footprint, graphics, and transitions.
- Water access is XML-owned through `Mods/<Mod>/WaterAccessType/*.xml` plus each BuildingType's `<water_access>` rules. Runtime state is a `uint8_t` mask, and provider/consumer logic now flows through typed rules instead of hardcoded well/fountain/reservoir/aqueduct/latrine branches.
- Building graphics animation ownership has been split out to `src/building/animations.h/.cpp`. Live native graphics, legacy overlay animation calls, and placement ghost previews all ask `BuildingAnimation` for frame selection so runtime drawing does not duplicate animation state policy.
- Main implementation notes live in:
  - `docs/walker_pathing_runtime.md`
  - `docs/water_access_runtime.md`
  - `docs/_codex_building_graphics_runtime_working_memory.md`
  - `Mods/Vespasian/FigureType/_README.md`
  - `Mods/Vespasian/BuildingType/_README.md`
  - `Mods/Vespasian/HousingType/_README.md`
  - `docs/save_load_runtime_bridges.md`
  - `docs/building_type_legacy_reference_ledger.md`
- Code chokepoints:
  - `src/building/building_type.cpp`
  - `src/building/building_type_registry.cpp`
  - `src/building/building_type_registry_xml.cpp`
  - `src/building/building_type_id_bridge.cpp`
  - `src/building/housing_type.cpp`
  - `src/building/housing_type_registry.cpp`
  - `src/building/house.cpp`
  - `src/figure/figure_type_registry.cpp`
  - `src/figure/PathingMode.h/.cpp`
  - `src/figure/figure_runtime.cpp`
  - `src/figure/movement.cpp`
  - `src/building/building_runtime.cpp`
  - `src/building/building_runtime_graphics.cpp`
  - `src/building/animations.cpp`
  - `src/building/water_access_runtime.cpp`
  - `src/building/water_access_type.cpp`
  - `src/building/water_access_type_id_bridge.cpp`
  - `src/map/road_service_history.cpp`
- `PathingMode` was extracted from the large FigureType registry file into its own class/files. Pathing mode declarations now own their XML id and validation requirements, so new mode metadata should be added there instead of through scattered helper predicates in the registry.
- `map_road_service_history` is pathing telemetry only. It is saved separately and should not be treated as building coverage or risk state.
- Mixed entertainment spawns are guarded at the BuildingType `spawn_group` level with comma-list `existing_figure` values, so amphitheater and arena performer alternates share one legacy slot safely.
- Roaming access is figure-specific: `roads_highway` profiles can spread over highways, while off-road-capable native pathing profiles fail XML validation.

## Current graphics checkpoint
- Canonical graphics extraction is now load-bearing for the building runtime:
  - `src/core/legacy_image_extractor.cpp`
  - `src/assets/augustus_asset_extractor.cpp`
- The durable current extractor handoff is `docs/graphics_extraction_pipeline.md`. Read it before changing generated graphics, BuildingType graphics targets, or `AugustusGraphicsExtractor.exe`.
- Julius and Augustus extraction are parallel problems with shared helper code, not one monolithic extractor:
  - Julius is stable and atlas-table driven.
  - Augustus is dynamic and packaged under the game folder `assets\Graphics`.
  - Runtime extraction runs Julius first from `src/core/image.c`, then Augustus bootstrap.
  - The standalone `AugustusGraphicsExtractor.exe` can run from the repo and accepts `--extract-julius-first`, `--game-root`, `--source-graphics`, `--output`, and `--julius-graphics`.
- A clean generated Release stack should contain `Mods\Julius\Graphics` and `Mods\Augustus\Graphics`, but no `Mods\Vespasian\Graphics`.
- Current clean sample extraction baseline:
  - Julius: 231 XML, 8933 PNG, 8465 logical images
  - Augustus: 3200 XML, 4088 PNG, 3259 logical images
  - BuildingType graphics refs: 505 checked across the Release mod stack, 0 missing
- The recent native building graphics glitch was fixed at extraction time, not by changing final draw math:
  - Julius footprint exports now trim bottom transparent padding and preserve logical placement through XML metadata
  - Augustus active building variants now inherit local `group="this"` footprint/top parts instead of collapsing to footprint-only
- Julius `Aesthetics\House_Tent` intentionally exposes `Image_0000..Image_0005`; `Image_0001..Image_0005` are compatibility aliases into `Aesthetics\House_Tent_Variants`, because the legacy table splits those tent variants through unnamed group id 18.
- Native building footprint rendering still routes through `src/widget/city_draw.cpp`, and native-owned buildings only submit their whole-building footprint on the owning draw tile.
- The temporary runtime footprint crop/offset compensation was removed from `src/assets/image_group_payload_materialize.cpp`; extractor output is now the sole source of truth for the corrected placement.
- Native building animations now tick through `city_draw_runtime_building_animation()` -> `building_runtime_advance_graphic_animation()` -> `building_runtime::advance_graphic_animation()` -> `BuildingAnimation::runtime_track_offset()`. `graphic_animation()` only reads the selected frame slice.
- Placement ghosts use the same generic BuildingType renderer for XML-owned buildings. They save/restore the map sprite animation byte because the ghost cursor reuses the hovered grid offset without owning real map state.

## Renderer / display status
- The renderer now separates:
  - real window pixels
  - logical UI size
  - user UI scale percentage
  - world/city pixel scale
- `src/platform/render_2d_pipeline.cpp`
  - owns request-based 2D scaling/filter policy
  - maps UI/pixel/tooltip/snapshot render domains
- `src/platform/renderer.cpp`
  - consumes explicit `render_2d_request`
  - preserves/restores render target, viewport, clip, blend, and scale across tooltip/snapshot paths
- `src/graphics/window.cpp`
  - is the pass orchestrator for full-window drawing
  - is not the widget-composition chokepoint
  - keeps reasserting UI render scale around window/tooltip/warning flow

## Shared UI runtime status
- Shared UI now has a layered object model:
  - `src/graphics/ui_primitives.h/.cpp`
  - `src/graphics/ui_widget.h`
  - `src/graphics/button_widget.h`
  - `src/graphics/bordered_button_widget.h/.cpp`
  - `src/graphics/image_button_widget.h/.cpp`
  - `src/graphics/panel_widget.h/.cpp`
  - `src/graphics/label_widget.h/.cpp`
  - `src/graphics/top_menu_panel_widget.h/.cpp`
  - `src/graphics/scrollbar_widget.h/.cpp`
  - `src/graphics/ui_runtime.h/.cpp`
  - `src/graphics/ui_runtime_api.h`
- Current flow is:
  - primitives own low-level request-based drawing
  - widget classes own widget behavior/data
  - `SharedUiRuntime` orchestrates widget objects and keeps the C facade narrow
- New widget/runtime diagnostics should go through `src/core/crash_context.h`, using:
  - `ErrorContextScope` as the preferred C++ scope name
  - `error_context_report_warning/error_context_report_error/error_context_report_fatal_error_dialog` for reports
- `CrashContextScope` and direct `log.h` calls remain compatibility paths for older code, not the preferred pattern for new UI/runtime work
- Legacy shared widget files now call the facade instead of drawing directly:
  - `src/graphics/button.c`
  - `src/graphics/panel.c`
  - `src/graphics/image_button.c`
  - `src/graphics/scrollbar.c`
  - `src/graphics/graphics.cpp`
- Important boundary decision:
  - `window.cpp` remains responsible for draw-pass flow
  - reusable widget composition lives in the widget/primitives chain, not in per-window rewrites

## Graphics pack / asset loading status
- Active graphics XML precedence is now:
  - active mod stack from top to bottom through `mod_manager_get_graphics_path_at()`
  - root Augustus assets folder (`assets/Graphics`)
  - Caesar 3/original atlas-backed content for legacy atlas/image fallback
- Canonical path-keyed loaders are now the preferred direction for authored graphics, including UI groups that have extracted keys such as `UI\Top_Menu`; flat `assets_get_image_id("UI", ...)` lookups are compatibility fallback only
- The startup path no longer hard-fails before that fallback chain is attempted.
- Missing optional overrides should warn/fallback.
- Critical failures should stop startup with a retained reason only after the fallback chain is actually attempted.

## Critical startup failure doctrine now wired
- `src/assets/assets.cpp`
  - retains startup failure reasons for critical asset load failures
- `src/core/image.c`
  - propagates asset-init failure upward
- `src/game/game.c`
  - retains a user-facing init failure message
- `src/platform/augustus.cpp`
  - shows the retained startup error if startup fails
- Good examples of intended critical failures:
  - selected-language font load failure
  - malformed or invalid XML/defines input
  - structurally incomplete runtime definitions that would predictably explode later

## Config / filter status
- Config persistence now lives in `Vespasian.ini`, with legacy fallback reads from `augustus.ini`.
- `CONFIG_UI_SCALE_FILTER` exists and currently supports:
  - auto
  - nearest
  - linear
- The setting is already respected by the request-based 2D pipeline and related SDL texture/filter handling.
- `CONFIG_DEBUG` exists as `debug=0/1` in `Vespasian.ini`; it currently gates transient zoom percentage warnings.
- City zoom start/reset/display bounds are UI-scale-relative while the stored city scale remains raw renderer/world scale.

## Recent migration landmarks that matter
- Renderer/platform:
  - `src/platform/augustus.cpp`
  - `src/platform/screen.cpp`
  - `src/platform/renderer.cpp`
  - `src/platform/render_2d_pipeline.cpp`
- Shared UI runtime:
  - `src/graphics/ui_runtime.cpp`
- Other recent `.cpp` migrations relevant to current architecture:
  - `src/graphics/image.cpp`
  - `src/graphics/text.cpp`
  - `src/graphics/font.cpp`
  - `src/graphics/tooltip.cpp`
  - `src/graphics/window.cpp`
  - `src/core/config.cpp`
  - `src/game/speed.cpp`

## UTF / text direction
- Do not broad-brush migrate the engine to UTF-native storage yet.
- Chosen direction is boundary-first:
  - new C++ runtime/parsing/text-adjacent code may prefer `std::string` / `std::string_view`
  - legacy C-facing storage remains on existing `uint8_t *` paths until a dedicated text migration phase
- New widget/runtime code should not assume `1 byte == 1 glyph`.

## Best next-phase target after this checkpoint
- Continue the shared UI runtime rollout without widening into unrelated windows.
- Good next candidates:
  - shared menu/list/grid framing helpers that still compose through older leaf calls
  - richer button/panel composition policies
  - better explicit logical metrics for top-menu and other shared UI strips
- Leave world/zoom/quad-map work for a later renderer phase.
