# Building Graphics Migration Inventory And Plan

> Completed migration record. All acceptance items below passed; remaining renderer work is tracked in the active renderer and figure-graphics documents.

## Active validation checklist

Status legend: `[x]` passing/completed, `[ ]` pending, `[!]` currently failing or known regression, `[-]` deliberately deferred.

- [x] Building draw calls enter the pass-specific `Building` -> `BuildingGraphics` -> `BuildingGraphicsDef` path.
- [x] Release x64 solution builds with warnings treated as errors.
- [x] Deployed Julius, Augustus, and Vespasian startup definition validation passes.
- [x] Runtime assets are inherited from the freshly extracted installed-game graphics trees; no proprietary extraction output was added to the repository.
- [x] Newly built roadblocks start with every walker permission disabled.
- [x] Repair composed farm fields whose saved surface cells remained bound to the parent record and therefore rendered `Industry\Farm_House` instead of `Industry\Farm_Crops`.
- [x] Allow a declared composition child foundation to replace a stale surface binding owned by its attached composition parent while retaining collision rejection for unrelated buildings.
- [x] Log parent-owned child cells as one save-bridge repair warning and add a post-hydration invariant proving every binding child cell is owned by that child.
- [x] Add a graphics-selection contract proving every farm field resolves five `Industry\Farm_Crops` options while its owner resolves `Industry\Farm_House`.
- [x] Reproduce and pass the `Engineer demographics test farm bug.svv` headless load/render and clean round-trip test after the repair.
- [-] Automated contracts cover all six farm models and five field-progress choices; final in-game visual confirmation is reserved for the user's manual test.
- [x] Re-run Julius-only, Julius+Augustus, and Vespasian startup/save gates after the farm fix.
- [x] Run the required save cohort for 3000 ticks at 1000% speed with clean post-repair roundtrips, zero runtime errors, and the post-warmup performance gate.
- [x] Remove temporary diagnostics and retain only generally useful invariants/error context.
- [x] Run `git diff --check`; verify `.gitignore` is untouched and no extracted/generated graphics are newly tracked or staged.

## Current Status

- Release x64 solution build is currently green:
  `MSBuild .\Vespasian.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`
  produced `x64\Release\Vespasian.exe`.
- Final acceptance evidence is recorded in `temp/full_save_soak_3000.log`: 58/58 soaks passed at 3000 ticks, the slowest post-warmup result was 1323.9 TPS, and no unresolved/incomplete graphics request occurred.
- The exact deployed executable matches the tested Release binary and passes Julius, Augustus, and Vespasian startup; the deployed farm regression save passed at 7158.3 TPS in its initial targeted run.
- `building_object.h` is gone. The C++ `Building` class lives in `src/building/building.h`.
- `building.h` still has no include guard. Current header hygiene relies on full `building.h` being included only by `.cpp` files or by C++ headers that are already intentionally object-facing.
- `Building::draw_footprint(...)`, `draw_top(...)`, and `draw_animation(...)` now exist and delegate:
  `Building` -> `BuildingType::graphics()` -> `GraphicsDefinition` pass-specific draw methods.
- `BuildingType` owns `GraphicsDefinition graphics_`, so the intended ownership shape is partly present:
  `Building` holds data/interface, `BuildingType` holds modules, `GraphicsDefinition` is the graphics module data/logic object.
- `GraphicsDefinition` now exposes pass-specific draw methods in `src/building/animations.cpp`.
- The city renderer still has separate footprint/top/animation passes:
  `city_view_foreach_valid_map_tile(draw_footprint)` and row-wise `draw_figures, draw_top, draw_animation`.
- The prior singular `Building::draw(...)` problem has been removed; callers now enter the matching pass method.
- `src/widget/city_draw.cpp` now only contains the native tile footprint helper. The old static building footprint/top wrapper layer is gone.
- `src/widget/city_draw_overlay.cpp/.h` are deleted.
- `src/building/building_runtime_graphics.cpp` still owns cached runtime slices and animation frame materialization. It is close to the graphics module, but it is still a runtime cache implementation rather than the public draw API.
- The raw-record graphics slice functions have been removed from `src/building/building_runtime.cpp`.
- `src/widget/city_building_ghost.cpp` now wraps `data.ghost_building` as a temporary `Building` and draws through the graphics module with preview-specific `force_draw_tile` context.
- `src/widget/city_without_overlay.cpp` and `src/widget/city_with_overlay.cpp` still contain building-specific graphics rules and ornament logic:
  warehouse flags, granary stores, depot cart, dock workers, plague/fumigation, hippodrome extras, fort flags, gatehouse orientation, workshop raw-material overlays, senate rating flags, mothball/stockpile icons.
- `src/building/image.cpp` remains the large legacy image-id decision switch. Native graphics also still write sentinel image ids through `building_runtime::set_building_graphic`.

## Interface Decision

The current singular no-pass `Building::draw(...)` should be replaced before more graphics migration work happens.

Use explicit pass methods on `Building`, backed by explicit pass methods on the graphics module:

```cpp
struct BuildingDrawContext {
    int x = 0;
    int y = 0;
    int grid_offset = 0;
    color_t color_mask = 0;
    float scale = 1.0f;
};

class Building {
public:
    int draw_footprint(const BuildingDrawContext &ctx);
    int draw_top(const BuildingDrawContext &ctx);
    int draw_animation(const BuildingDrawContext &ctx);
};

class BuildingType {
public:
    const GraphicsDefinition &graphics() const;
};

class GraphicsDefinition {
public:
    int draw_footprint(Building building, const BuildingDrawContext &ctx) const;
    int draw_top(Building building, const BuildingDrawContext &ctx) const;
    int draw_animation(Building building, const BuildingDrawContext &ctx) const;
};
```

Return value remains the existing convention for now:

- `1`: native graphics handled this pass, caller should not do legacy fallback for this pass.
- `0`: native graphics did not handle this pass, caller should keep its legacy fallback for this pass.

Reasoning:

- This matches the engine we actually have.
- It prevents native graphics from being drawn multiple times.
- It keeps the public interface honest while still preserving the ownership path:
  `Building` -> `BuildingType` -> `GraphicsDefinition`.
- It gives agents exact call sites to rewrite without inventing wrappers.

Do not add raw-pointer overloads, compatibility bridges, or static city-draw building helpers.

## Module Layout Decision

`GraphicsDefinition` currently lives in `building/animations.h` and `animations.cpp`. That name is now too narrow.

Near-term:

- Keep the type name `GraphicsDefinition` for churn control.
- Pass rendering logic now enters `GraphicsDefinition`; remaining city draw work is ornament/special-case cleanup.
- Keep `BuildingAnimation` as the frame-selection policy, but make it a collaborator of the graphics module, not a draw API.

Later cleanup:

- Split graphics module declarations/implementation into `building/building_graphics.h/.cpp`.
- Leave `building/animations.h/.cpp` for `BuildingAnimation` only.

Do not do this split until the pass interface is stable and the city/ghost call sites are converted.

## Immediate Rewrite Order

1. Stabilize the draw contract.
   - Done: `Building::draw(...)` was replaced with `draw_footprint`, `draw_top`, and `draw_animation`.
   - Done: `BuildingType::draw(...)` is not the public draw route; callers use `Building` then `BuildingType::graphics()`.
   - Done: `GraphicsDefinition::render(...)` was replaced with pass-specific methods.
   - Current Release x64 build is green.

2. Rewrite normal city renderers to the pass contract.
   - `src/widget/city_without_overlay.cpp`
   - `src/widget/city_with_overlay.cpp`
   - Each city pass calls exactly one matching object method.
   - No static building rendering helpers.
   - Do not move all ornament logic yet; first stop duplicated native full-draw calls.

3. Convert ghost preview to the same graphics module contract.
   - `src/widget/city_building_ghost.cpp`
   - Done: reuse a temporary `Building` object wrapping `data.ghost_building`.
   - Done: `BuildingDrawContext::force_draw_tile` is the preview-specific context.
   - Done: direct calls to `runtime.graphic_footprint`, `runtime.graphic_top`, and `runtime.graphic_animation` were removed from widget code.

4. Retire raw runtime graphics accessors.
   - Done: removed:
     `building_runtime_get_graphic_footprint_slice`,
     `building_runtime_get_graphic_top_slice`,
     `building_runtime_owns_graphics`.
   - Runtime may still cache slices, but callers should enter through `Building`/`GraphicsDefinition`.

5. Pull building-specific ornament logic into graphics.
   - Start with repeated normal/overlay cases:
     warehouse flags, granary stores, depot cart, dock workers.
   - Then move special cases:
     plague/fumigation, hippodrome, fort/gatehouse, workshop raw materials, senate flags, mothball/stockpile.
   - For UI/status overlays, decide explicitly whether they are graphics-module decorations or widget overlays. Do not casually move UI concepts into graphics.

6. Shrink legacy image id authority.
   - Audit `building_image_get` callers.
   - Keep legacy fallback for unmigrated types only.
   - Native XML/payload buildings should not require widget-side `map_image_at` decisions except sentinel compatibility.

## Agent Plan After Interface Is Defined

Limit to at most three agents. Do not spawn them until step 1 above is implemented and documented.

Agent 1: Normal City Renderers

- Files:
  `src/widget/city_without_overlay.cpp`,
  `src/widget/city_with_overlay.cpp`,
  `src/widget/city_draw.cpp`,
  `src/widget/city_draw.h`.
- Task:
  Replace current singular draw calls with pass-specific `Building` methods.
  Do not touch ghost preview or central graphics interfaces.
  Do not add helpers.

Agent 2: Graphics Module And Runtime Boundary

- Files:
  `src/building/building.h`,
  `src/building/building.cpp`,
  `src/building/building_type.h`,
  `src/building/building_type.cpp`,
  `src/building/animations.h`,
  `src/building/animations.cpp`,
  `src/building/building_runtime.h`,
  `src/building/building_runtime.cpp`,
  `src/building/building_runtime_graphics.cpp`,
  `src/building/building_runtime_graphics.h`.
- Task:
  Own the pass-specific graphics module API and delete raw runtime graphics entry points once callers are gone.
  Do not touch city renderer ornament logic except as needed to compile.

Agent 3: Ghost Preview

- Files:
  `src/widget/city_building_ghost.cpp`,
  `src/widget/city_building_ghost.h` if needed.
- Task:
  Convert preview drawing to the same graphics module contract after Agent 2 defines it.
  Do not edit normal city renderers or central building APIs unless explicitly coordinated.

## Coordination Rules

- No agent gets open-ended repo-wide authority.
- No compatibility overloads.
- No new `building_object` path.
- No new static city-draw building graphics helpers.
- New `Building` methods must either:
  - expose real building data needed by multiple migrated callers, or
  - remove a larger amount of direct saved-record code.
- Every agent logs changed files and remaining legacy graphics hotspots in `docs/agent_logs/building_object_migration/`.
- After every agent batch, run:
  `MSBuild .\Vespasian.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`.

## Open Design Questions

1. Should pass methods be separate public methods as above, or one `draw(const BuildingDrawContext &ctx)` with `ctx.pass`?
   - Recommendation: separate methods, because the renderer has separate passes and this avoids pretending the call is one operation.

2. Should `GraphicsDefinition` be renamed now?
   - Recommendation: no. First stabilize behavior, then split/rename to `building_graphics`.

3. Which ornaments belong to graphics and which remain widget overlays?
   - Recommendation: move authored building visuals and resource/state-dependent decorations into graphics. Keep selection, hover, problem-overlay columns, and pure UI indicators in widgets until a deliberate overlay module exists.
