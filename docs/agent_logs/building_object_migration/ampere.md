# Building Object Migration - Ampere

## Scope

- Military/access group only:
  - `src/building/barracks.cpp`
  - `src/building/barracks.h`
  - `src/building/armoury.cpp`
  - `src/building/armoury.h`
  - `src/building/roadblock.cpp`
  - `src/building/roadblock.h`
  - `src/building/maintenance.cpp`
  - `src/building/maintenance.h`
  - `src/building/local_workforce.cpp`
  - `src/building/local_workforce.h`
  - `src/figure/formation_legion.cpp`
  - `src/figure/formation_legion.h`
  - `src/figuretype/wall.cpp`
  - `src/figuretype/wall.h`

## Files Changed

- Created this log.
- Hard-migrated Ampere lane headers/implementations:
  - `src/building/barracks.cpp`
  - `src/building/barracks.h`
  - `src/building/armoury.cpp`
  - `src/building/armoury.h`
  - `src/building/roadblock.cpp`
  - `src/building/roadblock.h`
  - `src/figuretype/wall.cpp`
- Updated object call sites that broke on the removed `building` struct / removed wrappers:
  - `src/building/building.cpp`
  - `src/building/building_runtime_spawn.cpp`
  - `src/building/construction_building.cpp`
  - `src/building/construction_routed.cpp`
  - `src/building/figure.cpp`
  - `src/city/festival.cpp`
  - `src/city/games.cpp`
  - `src/city/gods.cpp`
  - `src/editor/tool.cpp`
  - `src/figure/movement.cpp`
  - `src/figuretype/cartpusher.cpp`
  - `src/figuretype/crime.cpp`
  - `src/figuretype/workcamp.cpp`
  - `src/map/road_access.cpp`
  - `src/window/building/military.cpp`
  - `src/window/building/utility.cpp`
  - `src/window/building_info.cpp`
- Converted C files that needed the C++ building object:
  - `src/city/festival.c` -> `src/city/festival.cpp`
  - `src/city/games.c` -> `src/city/games.cpp`
  - `src/city/gods.c` -> `src/city/gods.cpp`
  - `src/editor/tool.c` -> `src/editor/tool.cpp`
- Updated project entries:
  - `Vespasian.vcxproj`
  - `Vespasian.vcxproj.filters`

## Public Signatures Changed

- Removed raw barracks/armoury/roadblock free-function declarations from lane headers instead of preserving wrappers.
- Added/reused the object path on `Building` for barracks, armoury, and roadblock behavior:
  - `Building::barracks_for_weapon(...)`
  - `Building::armoury_is_needed() const`
  - `Building::create_barracks_soldier(...)`
  - `Building::unmanned_tower_for_barracks(...) const`
  - `Building::create_tower_sentry(...)`
  - `Building::is_roadblock_type(...)`
  - `Building::is_roadblock() const`
  - `Building::toggle_roadblock_permission(...)`
  - `Building::has_roadblock_permission(...) const`
  - `Building::accept_no_roadblock_permissions()`
  - `Building::accept_all_roadblock_permissions()`

## Architecture Change From User

- Stop the compatibility-overload approach immediately.
- `building.h` is the sole building object/class header.
- Do not add or preserve `building_object.h` usage.
- Do not maintain parallel C/C++ APIs just to play nicely with C linkage.
- Files that need the building object must become C++ files and include `building.h` directly.
- In this lane, remove C++ overloads added only for compatibility and migrate signatures/implementations to the object path instead.
- Hard override: do not add overloads, wrappers, bridge APIs, compatibility helpers, or duplicate raw/object APIs.
- Golden path is one object path. For example, callers should move toward object calls such as `building.draw()`, with the object delegating to type/rendering behavior.
- New helper APIs are allowed only when they directly replace and remove more legacy code.
- Military/access work must migrate implementations and callers to the object path, then cut the old redundant path instead of preserving both.
- Clarification: do not create stage-specific public building draw APIs such as `draw_footprint` or `draw_top`.
- Public rendering path should be singular, e.g. `Building::draw(...)`, with type/rendering logic deciding how footprint/top/animation/details are handled behind that route.
- Direction update: buildings hold data and interfaces; building types hold modules; modules hold logic.
- Graphics is a module. It owns graphics data/rules/images and calls animation/matching/rendering logic.
- Building rendering should move toward `Building` object method -> `BuildingType` -> graphics module.
- Do not preserve static `city_draw`-style building rendering calls.

## Work To Walk Back

- This log previously planned object internals with legacy raw-pointer wrappers where outside callers remained C. That plan is obsolete.
- Any future Ampere edits must avoid adding `building_object.h` includes or compatibility overloads in legacy headers.
- Existing lane headers still expose raw `building *` in several APIs; these now need hard migration to `Building` signatures where the lane owns the API, with dependent callers converted as needed.
- `src/building/building_object.h` has been deleted and removed from the project. The `Building` class declaration now lives in `src/building/building.h`.
- Replace any stale `building/building_object.h` include with `building/building.h`; do not recreate the deleted header.
- Cut duplicate military/access raw-pointer APIs instead of adding object overloads beside them.
- Current duplicate work to cut in this lane: barracks functions that still accept `building *`, armoury raw-field access, roadblock raw-field access, local workforce raw-record internals where object methods can replace them, and any `legacy_record()` calls into those functions from C++ callers.
- Stage-specific `Building::draw_runtime_footprint` and `Building::draw_runtime_top` existed in the tree. They must be walked back into one `Building::draw(...)` path rather than preserved as public object methods.
- I briefly added an unimplemented `BuildingDrawPass` enum plus `Building::draw(BuildingDrawPass, ...)` declaration in `src/building/building.h`; removed it after the explicit module-ownership direction because it was still stage-shaped and did not route through `BuildingType` graphics module ownership.

## Remaining Graphics / Static-Call Hotspots

- `src/widget/city_draw.cpp` and `src/widget/city_draw.h` still expose static `city_draw_runtime_building_footprint` / `city_draw_runtime_building_top` building render helpers outside this lane.
- `src/building/building_runtime_graphics.h` still exposes `building_runtime_get_graphic_footprint_slice(building *)` and `building_runtime_get_graphic_top_slice(building *)`, which are static/raw-record graphics accessors that should fold into the BuildingType graphics module path.
- `src/building/building_runtime_graphics.cpp` and `src/building/building_runtime.cpp` still hold runtime graphic slice logic. The future graphics pass should move that logic behind the graphics module rather than preserving free functions.
- In Ampere's lane, `src/figuretype/wall.cpp` still uses raw `building_get(...)` records for tower/watchman behavior and figure image assignment. It is not a building render pipeline, but it is a military/access raw-record hotspot to migrate toward `Building` methods.

## Building Methods Used Or Added

- Added/used lane object methods listed in Public Signatures Changed.
- Used existing object methods during wider compile fixes: `Building::from_id`, `Building::id`, `Building::type_id`, `Building::resource_amount`, `Building::add_resource`, `Building::set_resource_amount`, `Building::legacy_record`.

## Remaining `legacy_record()` Access

- `src/figuretype/workcamp.cpp`: `Building::legacy_record()` is still used for `building_monument_resource_in_delivery(...)`, which remains a monument-delivery API taking the saved record. It should migrate when the monument-delivery module is converted to `Building`.

## Build Commands And Results

- `MSBuild Vespasian.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`: passed `AugustusGraphicsExtractor`, then exposed C/C++ building-object breakages; `/m` sometimes races on shared `vc143.pdb`.
- `MSBuild Vespasian.sln /m:1 /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`: same real compile surface, but can still race with other workers' builds in the shared intermediate directory.
- Isolated validation command now used to avoid other agents' PDB locks:
  - `MSBuild Vespasian.vcxproj /m:1 /p:Configuration=Release /p:Platform=x64 /p:MultiProcessorCompilation=false /p:IntDir=out\x64\Release_ampere\ /p:OutDir=x64\Release_ampere\ /v:minimal /nologo`
- Latest isolated build result: build reaches `src/figuretype/trader.cpp` and `src/figuretype/cartpusher.cpp`; remaining failures are raw `building *` passed to migrated `Building &`/`const Building &` storage, granary, and warehouse APIs plus enum casts from legacy `unsigned char` resource fields.

## Blockers Or Conflicts

- Parallel workers are active. Do not revert Carver or Euclid edits; re-read central `Building` API before adding methods.
- Other MSBuild processes are active in the shared workspace, so Ampere validation uses separate `IntDir`/`OutDir` rather than killing unrelated builds.

## 2026-06-05 Continuation

### Scope Update

- Continued under the hard migration rule: no compatibility wrappers, overload bridges, or duplicate raw/object APIs.
- Treated C/C++ linkage fixes as declaration hygiene for existing legacy C APIs only. `Building` object-taking APIs were kept under explicit C++ linkage, including when headers are pulled through legacy `extern "C"` include blocks.
- Converted remaining warning files that called building APIs from C to C++ instead of preserving implicit C declarations:
  - `src/game/cheats.c` -> `src/game/cheats.cpp`
  - `src/game/file_editor.c` -> `src/game/file_editor.cpp`

### Files Changed In This Continuation

- Project entries:
  - `Vespasian.vcxproj`
  - `Vespasian.vcxproj.filters`
- Converted to C++ for direct `building/building.h` use:
  - `src/game/cheats.cpp`
  - `src/game/file_editor.cpp`
- Mixed legacy/object headers split so legacy C declarations keep C linkage and `Building` declarations keep C++ linkage:
  - `src/building/distribution.h`
  - `src/building/dock.h`
  - `src/building/granary.h`
  - `src/building/house.h`
  - `src/building/house_evolution.h`
  - `src/building/house_population.h`
  - `src/building/storage.h`
  - `src/building/warehouse.h`
  - `src/city/health.h`
  - `src/figure/formation.h`
  - `src/figure/formation_legion.h`
- Legacy C declaration headers normalized with `extern "C"` so converted C++ files and remaining C files agree on one ABI:
  - `src/building/clone.h`
  - `src/building/connectable.h`
  - `src/building/construction_routed.h`
  - `src/building/count.h`
  - `src/building/data_transfer.h`
  - `src/building/entertainment.h`
  - `src/building/government.h`
  - `src/building/house_service.h`
  - `src/building/list.h`
  - `src/building/maintenance.h`
  - `src/building/rotation.h`
  - `src/building/variant.h`
  - `src/city/buildings.h`
  - `src/city/culture.h`
  - `src/city/data.h`
  - `src/city/data_private.h`
  - `src/city/emperor.h`
  - `src/city/entertainment.h`
  - `src/city/festival.h`
  - `src/city/finance.h`
  - `src/city/games.h`
  - `src/city/gods.h`
  - `src/city/map.h`
  - `src/city/message.h`
  - `src/city/migration.h`
  - `src/city/military.h`
  - `src/city/mission.h`
  - `src/city/race_bet.h`
  - `src/city/ratings.h`
  - `src/city/resource.h`
  - `src/city/sentiment.h`
  - `src/city/trade_policy.h`
  - `src/city/victory.h`
  - `src/core/array.h`
  - `src/core/random.h`
  - `src/core/string.h`
  - `src/editor/tool.h`
  - `src/editor/tool_restriction.h`
  - `src/empire/city.h`
  - `src/empire/editor.h`
  - `src/empire/empire.h`
  - `src/empire/object.h`
  - `src/empire/trade_route.h`
  - `src/figure/combat.h`
  - `src/figure/enemy_army.h`
  - `src/figure/formation_enemy.h`
  - `src/figure/formation_herd.h`
  - `src/figure/formation_layout.h`
  - `src/figure/image.h`
  - `src/figure/name.h`
  - `src/figure/phrase.h`
  - `src/figure/roamer_preview.h`
  - `src/figure/route.h`
  - `src/figure/trader.h`
  - `src/figure/visited_buildings.h`
  - `src/figuretype/animal.h`
  - `src/figuretype/editor.h`
  - `src/figuretype/missile.h`
  - `src/figuretype/native.h`
  - `src/figuretype/wall.h`
  - `src/game/animation.h`
  - `src/game/campaign.h`
  - `src/game/difficulty.h`
  - `src/game/file.h`
  - `src/game/save_version.h`
  - `src/game/state.h`
  - `src/game/tutorial.h`
  - `src/game/undo.h`
  - `src/graphics/weather.h`
  - `src/map/aqueduct.h`
  - `src/map/bookmark.h`
  - `src/map/bridge.h`
  - `src/map/building.h`
  - `src/map/data.h`
  - `src/map/desirability.h`
  - `src/map/elevation.h`
  - `src/map/figure.h`
  - `src/map/image_context.h`
  - `src/map/natives.h`
  - `src/map/property.h`
  - `src/map/random.h`
  - `src/map/ring.h`
  - `src/map/road_access.h`
  - `src/map/road_aqueduct.h`
  - `src/map/road_network.h`
  - `src/map/routing.h`
  - `src/map/routing_data.h`
  - `src/map/routing_terrain.h`
  - `src/map/soldier_strength.h`
  - `src/map/sprite.h`
  - `src/map/terrain.h`
  - `src/map/water.h`
  - `src/platform/file_manager.h`
  - `src/scenario/allowed_building.h`
  - `src/scenario/criteria.h`
  - `src/scenario/custom_media.h`
  - `src/scenario/custom_messages.h`
  - `src/scenario/custom_variable.h`
  - `src/scenario/data.h`
  - `src/scenario/demand_change.h`
  - `src/scenario/distant_battle.h`
  - `src/scenario/earthquake.h`
  - `src/scenario/editor.h`
  - `src/scenario/editor_events.h`
  - `src/scenario/editor_map.h`
  - `src/scenario/emperor_change.h`
  - `src/scenario/empire.h`
  - `src/scenario/event/action_handler.h`
  - `src/scenario/event/action_types.h`
  - `src/scenario/event/condition_handler.h`
  - `src/scenario/event/condition_types.h`
  - `src/scenario/event/controller.h`
  - `src/scenario/event/data.h`
  - `src/scenario/event/event.h`
  - `src/scenario/event/formula.h`
  - `src/scenario/gladiator_revolt.h`
  - `src/scenario/invasion.h`
  - `src/scenario/map.h`
  - `src/scenario/price_change.h`
  - `src/scenario/property.h`
  - `src/scenario/random_event.h`
  - `src/scenario/request.h`
  - `src/scenario/scenario.h`
  - `src/sound/effect.h`
  - `src/sound/music.h`
  - `src/sound/speech.h`
  - `src/widget/city_overlay_education.h`
  - `src/widget/city_overlay_entertainment.h`
  - `src/widget/city_overlay_health.h`
  - `src/widget/minimap.h`
  - `src/widget/sidebar/military.h`
  - `src/window/building_info.h`
  - `src/window/console.h`
  - `src/window/editor/attributes.h`
  - `src/window/editor/scenario_action_edit.h`
  - `src/window/editor/scenario_condition_edit.h`
  - `src/window/editor/scenario_events.h`
  - `src/window/editor/select_scenario_action_type.h`
  - `src/window/editor/select_scenario_condition_type.h`
  - `src/window/editor/select_special_attribute_mapping.h`
  - `src/window/plain_message_dialog.h`

### Public Signatures Changed

- No compatibility overloads, bridge wrappers, or duplicate raw/object signatures were added.
- No callable function parameter list was changed in the header-linkage pass; linkage declarations were normalized.
- `src/game/cheats.cpp` now includes `building/building.h` and uses explicit enum casts for parsed console arguments.
- `src/game/file_editor.cpp` now includes `building/building.h` for `building_clear_all()`.

### Building Methods Added Or Used

- New central methods already added/used in this migration and kept on the object path:
  - `Building::formation_id() const`
  - `Building::set_formation_id(int formation_id)`
  - `Building::set_orientation(int orientation)`
- Existing object methods used by this continuation or earlier Ampere fixes include:
  - `Building::from_id`
  - `Building::id`
  - `Building::type_id`
  - `Building::legacy_record`
  - `Building::resource_amount`
  - `Building::add_resource`
  - `Building::set_resource_amount`

### Remaining `legacy_record()` Access

- `src/building/building_runtime_spawn.cpp`: still passes records into `building_local_workforce_*` functions; local workforce remains a raw-record API and should be converted in a dedicated pass.
- `src/figuretype/workcamp.cpp`: still calls `building_monument_resource_in_delivery(...)`, which takes the saved record.
- `src/map/image.cpp`: still uses records for farm progress and `building_runtime_apply_graphic_if_native(...)`; this is a graphics/static runtime hotspot to move behind the BuildingType graphics module.
- Broader repo scan still shows additional `legacy_record()` in building runtime, house, production, trader, and overlay code outside the narrow military/access lane.

### Remaining Graphics / Static-Call Hotspots

- Current recursive source scan found no stale `building_object.h`, `BuildingDrawPass`, `draw_runtime_footprint`, `draw_runtime_top`, `city_draw_runtime_building_footprint`, or `city_draw_runtime_building_top` references.
- Current named static graphics hotspots:
  - `src/building/building_runtime.cpp`: `building_runtime_get_graphic_footprint_slice(building *)`
  - `src/building/building_runtime.cpp`: `building_runtime_get_graphic_top_slice(building *)`
- `src/map/image.cpp` still routes through `building_runtime_apply_graphic_if_native(...)` and raw records while updating map imagery.

### Build Result

- Command:
  - `MSBuild Vespasian.vcxproj /m:1 /p:Configuration=Release /p:Platform=x64 /p:MultiProcessorCompilation=false /p:IntDir=out\x64\Release_ampere\ /p:OutDir=x64\Release_ampere\ /v:minimal /nologo`
- Result:
  - Passed.
  - Output: `x64\Release_ampere\Vespasian.exe`
  - The earlier 952-linker unresolved set was resolved by normalizing C/C++ declarations and keeping `Building` APIs under C++ linkage.
  - The follow-up build after converting `src/game/cheats.c` and `src/game/file_editor.c` to C++ also passed without the previous implicit-building-call warnings.
