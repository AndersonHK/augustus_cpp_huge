# Building Object Migration - Graphics Module Route

## Scope
- Lane: central graphics route only.
- Writable files used: `src/building/building.h`, `src/building/building.cpp`, `src/building/building_fwd.h`, `src/building/animations.h`, `src/building/animations.cpp`, `src/building/building_type.h`, `src/building/building_type.cpp`, `src/building/building_runtime_graphics.h`.
- Hard rule followed: no compatibility overloads, wrappers, bridge APIs, or duplicate raw/object API pairs. `building_object.h` is not used.

## Files Changed
- `src/building/building_fwd.h`
  - Removed public `BuildingDrawPass`.
- `src/building/building.h`
  - Changed the public draw route from `Building::draw(BuildingDrawPass, ...)` to singular `Building::draw(...)`.
- `src/building/building.cpp`
  - Changed `Building::draw(...)` to delegate to `BuildingType::draw(...)`.
- `src/building/building_type.h`
  - Added `BuildingType::draw(Building, ...)` as the type-owned route into the graphics module.
- `src/building/building_type.cpp`
  - Implemented `BuildingType::draw(...)` by delegating to its owned `GraphicsDefinition`.
- `src/building/animations.h`
  - Changed `GraphicsDefinition::render(...)` to a passless graphics-module render.
- `src/building/animations.cpp`
  - Removed the public pass switch from graphics rendering.
  - `GraphicsDefinition::render(...)` now internally draws footprint, top, and animation according to runtime graphics state.
- `src/building/building_runtime_graphics.h`
  - Removed unused public declarations for stage-specific runtime graphic slice accessors.

## Signature Changes
- Removed: `Building::draw(BuildingDrawPass pass, int x, int y, int grid_offset, color_t color_mask, float scale)`.
- Added: `Building::draw(int x, int y, int grid_offset, color_t color_mask, float scale)`.
- Added: `BuildingType::draw(Building building, int x, int y, int grid_offset, color_t color_mask, float scale) const`.
- Changed: `GraphicsDefinition::render(Building building, BuildingDrawPass pass, ...)` to `GraphicsDefinition::render(Building building, ...)`.
- Removed public enum: `BuildingDrawPass`.

## Building / BuildingType Methods Used Or Added
- Added `BuildingType::draw(...)` so the route is `Building` object -> `BuildingType` -> graphics module.
- Used existing `Building::type()` from `Building::draw(...)`.
- Used existing graphics-module/runtime methods inside `GraphicsDefinition::render(...)`: `runtime->graphic_footprint()`, `runtime->graphic_top()`, `runtime->owns_graphic_animation()`, `runtime->advance_graphic_animation(...)`, and `runtime->graphic_animation(...)`.
- No new compatibility helper APIs were added.

## Legacy Record Access
- `GraphicsDefinition::render(...)` still passes `building.legacy_record()` only to `log_missing_runtime_stage_slice(...)` for diagnostics.
- Existing `runtime_for_building(Building)` still uses `legacy_record()` to find the main legacy record and get/create the runtime instance. This remains because runtime storage is still keyed by legacy record.

## Remaining Static / Stage Hotspots
- Out of writable scope: `src/widget/city_without_overlay.cpp` still calls `BuildingDrawPass::Footprint`, `Top`, and `Animation`.
- Out of writable scope: `src/widget/city_with_overlay.cpp` still calls `BuildingDrawPass::Footprint`, `Top`, and `Animation`.
- Out of writable scope: `src/building/building_runtime.cpp` still defines unused stage-specific C helpers:
  - `building_runtime_get_graphic_footprint_slice(building *b)`
  - `building_runtime_get_graphic_top_slice(building *b)`
  - `building_runtime_owns_graphics(building *b)`

## Build / Checks
- `git diff --check -- src/building/building_fwd.h src/building/building.h src/building/building.cpp src/building/animations.h src/building/animations.cpp src/building/building_type.h src/building/building_type.cpp src/building/building_runtime_graphics.h docs/agent_logs/building_object_migration/graphics_module_route.md`
  - Result: no whitespace errors; Git reported LF-to-CRLF warnings for touched source files.
- `MSBuild .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:quiet /clp:ErrorsOnly`
  - First run after the cut failed with in-scope stale signatures because parallel edits reintroduced `BuildingDrawPass` declarations in `building.h`, `building.cpp`, `building_type.h`, `building_type.cpp`, and `animations.cpp`.
  - Reapplied the passless central signatures.
  - Latest run fails before widget compilation in unrelated migration areas:
    - `src/city/ratings.cpp`: `int32_t` to `selected_rating`.
    - `src/city/sentiment.cpp` and `src/city/finance.cpp`: `const building *` to `Building`.
    - `src/city/resource.cpp`: enum/resource type conversions and raw `building *` passed to granary/warehouse object APIs.
  - Latest run did not report errors in this lane's central graphics files.

## Blockers
- Full compile cannot pass until current global migration errors are resolved.
- Once the build reaches widgets, `src/widget/city_without_overlay.cpp` and `src/widget/city_with_overlay.cpp` still need migration from staged `BuildingDrawPass` calls to the singular `Building::draw(...)` route or their rendering flow must be folded into the graphics module path.
- Active parallel edits repeatedly reintroduced stale `BuildingDrawPass` signatures while this lane was editing; final source scan in this lane should be treated as the current truth.
