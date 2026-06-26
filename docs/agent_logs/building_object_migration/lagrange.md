# Lagrange Building Object Migration Log

## 2026-06-05 Architecture Direction

- Buildings hold live data and public interfaces.
- Building types hold modules.
- Modules hold logic.
- Graphics is a building-type module: it owns graphics data, rules, images, matching, animation, and rendering delegation.
- Do not add compatibility wrappers, overload bridges, or duplicate helper APIs.
- Do not preserve static city-draw-style building rendering calls as the migrated surface.
- Cut toward one route: `Building` object method -> `BuildingType` -> graphics module.

## House Lane Changes

- Converted `src/building/house.h` to expose one `Building`-based API instead of parallel raw `building *` and object APIs.
- Converted `src/building/house_evolution.h` and `src/building/house_population.h` to expose `Building` where these APIs operate on a house; implementations include `building/building.h` directly.
- Began moving `src/building/house.cpp` exported helpers from raw saved-record signatures to `Building` signatures, using `legacy_record()` only as a local implementation escape hatch for fields that the object does not expose yet.
- Confirmed `src/building/building_object.h` is gone and stale `building_object.h` references are absent in `src`.
- Moved house/evolution/population implementation includes for migrated C++ headers outside `extern "C"` blocks.
- Rewrote house helper call sites to pass `Building(...)` handles instead of raw saved records.
- Renamed `src/city/buildings.c`, `src/city/health.c`, and `src/city/migration.c` to `.cpp` because they include migrated house C++ headers; updated `Vespasian.vcxproj` and `.filters`.
- Converted `house_population_get_capacity` to take `Building` and use `legacy_record()` only inside the implementation.
- Converted `city_health_get_house_health_level` to a C++-only `Building` API; C callers still see unrelated health functions, but the building-specific health query no longer has a raw public declaration.
- Renamed `src/widget/city_overlay_health.c` to `.cpp` and updated the project entries so its house-health calls use the object API.
- Removed an unused `building/monument.h` include from `src/city/migration.cpp` after its promotion to C++ exposed unrelated buffer declaration requirements.
- Cut the public stage-specific `BuildingDrawPass` rendering surface: removed the enum, `Building::draw(pass, ...)`, `BuildingType::draw(pass, ...)`, `GraphicsDefinition::render(pass, ...)`, and migrated existing object draw callers in `src/widget/city_without_overlay.cpp` and `src/widget/city_with_overlay.cpp` to the singular `building.draw(...)` route.

## Duplicate Work Cut

- Did not add raw-pointer/object overload pairs for the house helpers.
- Did not add compatibility bridges for C callers; callers that need object house APIs must move to C++.
- Removed the stage-specific draw pass overload that was added in parallel; the remaining object path is singular `Building::draw(...)`.

## Remaining House-Lane Hotspots

- `src/building/house.cpp:add_house_tiles` still calls `building_runtime_assign_graphic_variant`, `building_runtime_apply_graphic_if_native`, `map_building_tiles_add`, and `building_image_get` through raw saved records.
- `src/building/house.cpp:create_vacant_lot` still assigns visual tiles through `map_building_tiles_add(... building_image_get(...))`.
- `src/building/house.cpp:building_house_change_to_vacant_lot` still uses `map_image_set(... building_image_get(...))`.
- These should migrate to a singular `Building` graphics/render/update method that delegates to the `BuildingType` graphics module instead of preserving static helper calls.
- `src/widget/city_without_overlay.cpp` and `src/widget/city_with_overlay.cpp` still run the legacy footprint/top/animation city draw loop around the singular object draw route. A deeper graphics-lane pass should collapse that loop so the renderer asks each building to draw once and the type graphics module decides the internal slices.
- `src/widget/city_overlay_health.cpp` still receives `const building *` from the legacy overlay callback table and constructs `Building` with a local `const_cast`; the real cleanup is migrating the overlay callback interface itself to `Building`.
- Focused compiles now surface an existing broader warning in `src/building/granary.h`: `building_granary_get_granary_needing_food` has C linkage while returning `Building`. That should be hard-migrated rather than bridged.

## Verification Notes

- Stale search found no remaining `.c` file including `building/house.h`, `building/house_population.h`, or `building/house_evolution.h`.
- Focused Release x64 `ClCompile` with `TrackFileAccess=false` passes for `src/building/house.cpp`, `src/building/house_evolution.cpp`, `src/building/house_population.cpp`, `src/city/buildings.cpp`, `src/city/health.cpp`, `src/city/migration.cpp`, and `src/widget/city_overlay_health.cpp`.
- Focused Release x64 `ClCompile` with `TrackFileAccess=false` also passes for `src/building/building.cpp`, `src/building/building_type.cpp`, `src/building/animations.cpp`, `src/widget/city_without_overlay.cpp`, and `src/widget/city_with_overlay.cpp` after cutting the pass overload.
- A stale scan found no remaining `BuildingDrawPass` references after cutting the pass overloads.
- Full-solution `MSBuild` was not rerun in this pass; the broader tree remains heavily dirty and mid-migration.
- Previously observed broader blockers include missing direct `building/building.h` includes in object-using `.cpp` files, static legacy calls such as `building_type_is_roadblock`, and raw-record calls into already-migrated distribution/dock/granary/warehouse APIs.
