# Runtime Spawn Building Object Migration

Scope:
- `src/building/building_runtime_spawn.cpp`
- `docs/agent_logs/building_object_migration/runtime_spawn.md`

Files changed:
- `src/building/building_runtime_spawn.cpp`
- `docs/agent_logs/building_object_migration/runtime_spawn.md`

Public signatures changed:
- None.

Building methods used:
- `id()`
- `worker_count()`
- `has_workers()`
- `distance_from_entry()`
- `has_primary_figure()`
- `has_secondary_figure()`
- `set_primary_figure_id()`
- `barracks_priority()`
- `road_network_id()`
- `has_road_access()`
- `entertainment_days1()`
- `entertainment_days2()`
- `has_water_access()`
- `is_in_use()`

Architecture walkback:
- User directed the migration away from compatibility overloads/bridges.
- `building_object.h` has been deleted; `Building` now lives in `building/building.h`.
- Do not recreate or include `building_object.h`.
- Removed the local object-facing adapter block from `building_runtime_spawn.cpp`.
- Service/storage destination calls now use the available `Building` signatures directly.

Hard override:
- Golden path is a single C++ `Building` object path.
- Do not add overloads, wrappers, bridge APIs, or compatibility helpers.
- Duplicate object/raw API pairs should be merged or removed, not preserved.
- If callers break, migrate them to the object path or leave them visible as migration blockers.
- No new helper APIs unless they directly replace and remove more legacy code.

Draw API clarification:
- Do not add public stage-specific building draw APIs such as `draw_footprint` or `draw_top`.
- Public drawing should cut toward one route, e.g. `building.draw(...)`.
- The draw method should delegate to rendering logic owned by the building type, using the building's properties/state.
- Type rules should decide footprint, top, animation, and details.
- `building_runtime_spawn.cpp` has no draw API references, so this clarification required no code changes in this scope.

Graphics module direction:
- Buildings hold data and interfaces.
- Building types hold modules.
- Modules hold logic.
- Graphics is a module: it owns graphics data, rules, images, and calls animation/matching/rendering logic.
- Do not preserve static `city_draw`-style building rendering calls.
- Runtime-spawn does not render buildings directly; it only triggers `set_building_graphic()` from spawn timing policy.
- Removed unused static-rendering includes from `building_runtime_spawn.cpp`: `assets/image_group_payload.h`, `building/building_runtime_graphics.h`, `building/image.h`, `map/building_tiles.h`, `map/sprite.h`, and `map/terrain.h`.

Graphics/static-call hotspots found outside this scope:
- `src/building/animations.cpp`: `building_image_get`.
- `src/building/building.cpp`: `map_building_tiles_remove`.
- `src/building/building_runtime_graphics.cpp`: `building_image_get`, `map_building_tiles_add`.
- `src/building/connectable.cpp`: `building_image_get`.
- `src/building/construction*.cpp`: `building_image_get`, `map_building_tiles_*`.
- `src/building/destruction.cpp`: `building_image_get`, `map_building_tiles_*`.
- `src/building/figure.cpp`, `house.cpp`, `industry.cpp`, `lighthouse.cpp`, `monument.cpp`, `production.cpp`, `water_access_runtime.cpp`: direct `building_image_*` and/or `map_building_tiles_*` calls.
- Non-building hotspots include `src/core/image.cpp`, `src/editor/tool.c`, `src/game/file_editor.c`, and `src/game/undo.cpp`.

Remaining `legacy_record()` access:
- Local workforce, distribution-demand update, barracks, and armoury calls still require `legacy_record()` because those APIs have not been migrated in this write scope.
- The service/storage destination calls no longer use local adapters.
- Do not add local wrappers around these remaining calls. They are intentional visible migration blockers until those APIs are converted to `Building`.

Build:
- `MSBuild .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`
- First run failed with one in-scope diagnostic: `building_runtime_spawn.cpp(362): building_get identifier not found`.
- Replaced that direct lookup with `Building::from_id`.
- Second run still failed globally, but no diagnostics were reported for `building_runtime_spawn.cpp`.
- After the architecture walkback, reran the same Release x64 MSBuild command.
- Build still fails globally. The visible failure pattern is outside this file: `Building` redefinition/undefined-type issues, old `extern "C"` linkage contradictions around folded `building.h`, and other migrated files still passing `building *` to hard `Building` APIs.
- No `building_runtime_spawn.cpp` diagnostics were visible in the rerun.

Blockers:
- True `Building` signatures are not yet exposed by local workforce, distribution demand, barracks, or armoury APIs.
- Current remaining build blockers are outside this scope, mostly converted C++ files that still call legacy `building_*` functions after `building.h` stopped being a safe broad include.
