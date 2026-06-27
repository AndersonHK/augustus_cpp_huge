# Codex Augustus long-term working memory

Snapshot: 2026-06-27

## Project identity
This branch is still best understood as a simulation-rhythm fork of Augustus.

Primary design goals remain:
- make months feel longer
- make the city look busier with more visible walkers
- rebalance production and building costs to fit the slower cadence
- adjust demographics coherently, not cosmetically
- improve the rendering pipeline so higher-resolution UI/fonts/assets can exist without layout collapsing into atlas-native sizing assumptions

## Long-term engineering doctrine
- Keep Windows/MSBuild at repo root as the practical dev workflow.
- Prefer incremental C-to-C++ migration through narrow, class-based chokepoints.
- Keep save compatibility by growing runtime wrappers around legacy serialized structs instead of replacing serialized truth all at once.
- Do not broaden a rewrite just because a subsystem is old; center the work on the best control point.
- Prefer object-owned metadata for runtime concepts. For example, `PathingMode` instances should carry pathing requirements like `requires_road` rather than leaving those requirements in separate helper functions or scattered switch lists.
- Prefer concept-owned behavior as well as metadata. For example, `BuildingAnimation` owns frame advancement/gating, and water access type/rule definitions own the facts that older code used to re-encode through helper branches.
- Prefer owner-bound runtime modules over loose policy calls. The external API should read like `building.production().tick()` or `building.culture().should_animate()`, with the module already bound to its owner, immutable definition pointer, and mutable state. Avoid exposing `definition->tick(building)` to normal callers.
- Treat XML folders carefully: some are complete module definitions, some are partial ingredients, and some are vocabularies. `WaterAccessType` is a vocabulary, while building-specific water rules currently still live in BuildingType until a future `WaterAccess`/`BuildingWaterAccess` definition folder peels them out.
- For small enum-like data that must survive XML mods and saves, prefer stable text ids at content/save boundaries and compact runtime ids/masks inside the simulation. Water access uses text ids plus numeric ids `0..7`, stored as `uint8_t` masks.
- During record-to-object migration, `id` is special identity bridge data. Do not treat identity like ordinary mutable public state. Peeled fields should eventually live in runtime structs/modules, and save/load should reconstruct save records from runtime state plus module state.
- Natural tree-like terrain means `TERRAIN_TREE | TERRAIN_SHRUB`; timber-yard adjacency, tree-only clearing, and force-placement tree clearing should stay aligned on that mask.
- Comment functions consistently when touching code. Prefer concise contract comments that explain ownership, invariants, save/load behavior, validation, or surprising legacy interactions; avoid comments that merely restate assignments.
- Update the relevant markdown whenever behavior, XML contracts, save formats, new runtime classes, or major chokepoints change, unless the user explicitly says not to. Add cross-references so future sessions can find the information from the four core Codex files without crowding those files with every detail.
- Always update save versioning and migration gates when changing any data that is stored in save files; do this even when the loader can technically tolerate the new shape.

## Renderer doctrine
- Native window pixels, logical UI size, user UI scale, and world zoom are separate concepts and should stay separate.
- All screen-space drawing should trend toward explicit logical destination geometry rather than "native asset size == layout size".
- Backend renderer policy should stay concentrated in `Render2DPipeline`.
- Shared widget primitives should stay concentrated in `UiPrimitives`.
- Shared widget behavior should live on widget classes, with `SharedUiRuntime` acting as the facade/orchestrator rather than as a logic monolith.
- `window.cpp` should remain a draw-pass orchestrator unless there is a deliberate decision to move more composition into it.

## Asset / startup failure doctrine
- Graphics/content precedence is:
  - active mod stack graphics from top to bottom
  - root Augustus assets
  - Caesar 3/original atlas-backed content
- When a graphics bug is caused by biased canonical extracted data, prefer fixing the extractor and exported XML/PNG semantics before adding more runtime offset compensation.
- Preserve authored XML animation `x/y` offsets exactly unless there is direct proof they are wrong; they are part of the content contract for Augustus overlay animations.
- Missing optional overrides should usually warn/fallback.
- Broken critical assets or invalid definitions should fail at load, not halfway through gameplay.
- Startup errors should retain a precise user-facing reason whenever practical.

## Runtime reporting doctrine
- Use `Info` for expected compatibility or diagnostic notes, `Warning` for probably unintended but safe recovery, `Error` for definitely unintended survivable failures, and `Fatal error` only when the process must stop.
- Reserve the word "Error" in user-facing logs for actual error/fatal reports. Context scopes are neutral location breadcrumbs and should be labeled "Context", not "Error context", when printed.
- `Info` reports should omit context scopes. Warning/error/fatal reports should log the message, detail, and active context scopes as one log entry. Avoid emitting a report line followed by separate scope-count/scope-name lines.

## Save file doctrine
- Vespasian-owned save files use `.svv`. This replaced the temporary `.savf` extension so the fork has a standard three-letter extension while remaining distinct from legacy Caesar/Julius/Augustus `.sav` and expanded `.svx` saves.
- Existing `.savf` saves can be renamed to `.svv`; the on-disk payload format did not change with the extension rename.
- Canonical save layout and ownership notes live in `docs/save_data_organization.md`; check that before changing any `*_save_state`, `*_load_state`, or `init_savegame_data` piece.
- Save/load runtime bridge notes live in `docs/save_load_runtime_bridges.md`; check that before changing BuildingType save ids, monument construction loading, road service history, local workforce allocations, or runtime wrapper rebinding.
- Water access runtime notes live in `docs/water_access_runtime.md`; check that before changing water provider propagation, requirement semantics, placement previews, aqueduct/reservoir network behavior, or water-related BuildingType XML.
- Player-visible behavior differences from upstream Augustus belong in `docs/gameplay_divergences_from_augustus.md`; update it when a migration intentionally changes bundled Augustus, Julius, or Vespasian gameplay.

## Text / UTF doctrine
- Do not attempt a blanket UTF-native storage migration during unrelated renderer or widget work.
- Use boundary-first modernization instead:
  - `std::string` / `std::string_view` in new C++ runtime/parsing boundaries where practical
  - keep legacy internal `uint8_t *` storage until a dedicated text migration phase
- Future UTF/styled-text work should be modeled around:
  - glyph availability
  - fallback behavior
  - codepoint/grapheme-aware measurement
  - text-run/style metadata such as bold/italic, not widget-side hacks

## Load-bearing chokepoints now established
- `Render2DPipeline`
  - request-based 2D backend policy
- `UiPrimitives`
  - low-level shared UI drawing primitives
- widget hierarchy (`UiWidget`, button/panel/scrollbar widgets)
  - shared UI object behavior/composition
- `SharedUiRuntime`
  - shared UI facade/orchestration
- `Building` / `building_runtime`
  - C++ runtime object over saved building records; city rendering now enters native building graphics through `Building::draw(BuildingDrawPass::...)`, while raw records are persistence/legacy-boundary data
- owner-bound building module direction
  - low-hanging module plans live in `docs/bound_runtime_module_extraction_plan.md`; start with water access, culture/entertainment, religion, housing, production, storage, and formation facades before physically peeling save fields
- graphics definition hierarchy
  - `GraphicsDefinition`, `BuildingGraphics`, `FigureGraphics`, and `ResourceGraphics` are the intended split; figure-owned native graphics work should follow `docs/figure_owned_native_graphics_plan.md`
- `BuildingAnimation`
  - building animation frame selection, legacy cursor quirks, and shared native/legacy animation gating
- `WaterAccessType` / `water_access_runtime`
  - XML-defined water access types, mask propagation, provider/consumer rule evaluation, aqueduct wet-state projection, and compatibility mirrors
- `Image` / `ImageGroupEntryRef`
  - manager-owned renderable images plus semantic image-group entry references for UI/resource/building callers
- `ResourceGraphics`
  - resource-owned image-group references for carts, storage stacks, panel icons, empire icons, and editor icons
- `building_type_registry` / `housing_type_registry`
  - native BuildingType and HousingType XML loading, compatibility validation, legacy house-level bridge, and registry-to-legacy fan-out
- `figure_type_registry` / `figure_runtime`
  - native FigureType XML loading and walker runtime migration direction
- `map_road_service_history`
  - pathing-only road recency storage for smart service walkers
- `docs/demographics_runtime.md`
  - citywide age census, birth/mortality tables, and house-population reconciliation contract

## Practical priorities
1. Continue the shared UI runtime rollout where it improves control of common widgets without forcing a whole-window rewrite.
2. Keep load-time validation/failure reporting growing alongside new XML/runtime data systems.
3. Resume gameplay/system tuning in attributable phases once the targeted runtime/control-point work is stable.
4. Treat future world-renderer/zoom/quad-map work as a later phase built on the current backend, not something to mix into every UI pass.

## Guardrails
- Build only when useful for the current task. When building this project, use `Release|x64`; do not rely on Debug-only compiler behavior.
- Keep CRLF consistent on touched files.
- Use `#pragma once` in project-owned headers.
- Preserve existing comments when editing files.
- Add or refresh comments for touched functions whenever behavior, ownership, validation, save/load, or legacy interaction is not obvious from the code itself.
- Refresh relevant markdown in the same run when changing XML, save/load, runtime classes, pathing behavior, or system contracts.
- Do not spread implicit enum/int conversions through migrated code.
- Do not deepen dependence on byte-oriented text assumptions in new runtime/widget code.
- Do not collapse renderer policy back into many ad hoc draw helpers once the chokepoints exist.

## Short mnemonic
Build is stable; renderer backend exists; shared UI runtime now exists; native BuildingType/HousingType, FigureType, WaterAccessType, BuildingAnimation, FigureGraphics, ResourceGraphics, UnitType, and FormationType runtimes are active or mid-migration; asset fallback and retained startup failures are part of the architecture; keep moving through explicit chokepoints and owner-bound modules, not broad rewrites.
