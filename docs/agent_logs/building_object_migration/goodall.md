# Goodall Building Object Migration Log

## Scope
- Service/storage-destination group:
  - `src/building/market.cpp`, `src/building/market.h`
  - `src/building/caravanserai.cpp`, `src/building/caravanserai.h`
  - `src/building/lighthouse.cpp`, `src/building/lighthouse.h`
  - `src/building/mess_hall.cpp`, `src/building/mess_hall.h`
  - `src/building/tavern.cpp`, `src/building/tavern.h`
  - `src/building/temple.cpp`, `src/building/temple.h`

## Goodall Files Changed
- `src/building/market.h`
- `src/building/caravanserai.h`
- `src/building/lighthouse.h`
- `src/building/mess_hall.h`
- `src/building/tavern.h`
- `src/building/temple.h`
- `src/building/market.cpp`
- `src/building/caravanserai.cpp`
- `src/building/lighthouse.cpp`
- `src/building/mess_hall.cpp`
- `src/building/tavern.cpp`
- `src/building/temple.cpp`

## Related Central/Header-Hygiene Changes Observed
- `src/building/building.h`
- `src/building/building.cpp`
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_graphics.cpp`
- `src/building/animations.h`
- `src/building/animations.cpp`
- `src/building/house.h`
- `src/building/house_evolution.h`
- `src/building/house_evolution.cpp`
- `src/building/house_population.h`
- `src/building/house_population.cpp`
- `src/building/production.h`
- `src/building/production.cpp`
- `src/building/production_method.h`
- `src/building/production_method.cpp`
- `src/building/production_runtime.cpp`
- `src/building/water_access_runtime.cpp`
- `src/building/building_object.h` deleted after class declaration was folded into `building.h`.

## Public Signatures Changed
- Architecture change from user: stop the compatibility-overload approach. The following overload work must be walked back and replaced with hard-migrated `Building` signatures that include `building.h` directly:
  - `building_market_get_max_food_stock(Building)`
  - `building_market_get_max_goods_stock(Building)`
  - `building_market_get_needed_inventory(Building, resource_storage_info[RESOURCE_MAX])`
  - `building_market_fetch_inventory(Building, resource_storage_info[RESOURCE_MAX])`
  - `building_market_get_storage_destination(Building)`
  - `building_caravanserai_enough_foods(Building)`
  - `building_caravanserai_get_storage_destination(Building)`
  - `building_lighthouse_enough_timber(Building)`
  - `building_lighthouse_get_storage_destination(Building)`
  - `building_mess_hall_get_storage_destination(Building)`
  - `building_tavern_get_storage_destination(Building)`
  - `building_temple_get_storage_destination(Building)`
  - `building_temple_mars_food_to_deliver(Building, Building)`
  - `building_temple_mars_food_to_deliver(Building, int)`
- Legacy `building *` wrapper declarations and implementations added for compatibility are no longer desired and should be removed in this scope.
- New direction: `building.h` is the sole building object/class header; scoped APIs should use the building object/class directly or let non-migrated callers fail until converted.
- Hard override from user: no overloads, wrappers, bridge APIs, or compatibility helpers. Duplicate object/raw API pairs should be cut, merged, or deleted. Golden path is a single `Building` object path.
- Draw-path clarification from user: do not add public stage-specific draw APIs like `draw_footprint` or `draw_top`; the object path should be singular, for example `building.draw(...)`, and rendering details should be delegated through the building type rules.
- Architecture update from main thread: Buildings hold data and interfaces; building types hold modules; modules hold logic. Graphics is a module owning graphics data/rules/images and calling animation/matching/rendering logic. Do not preserve static city_draw-style building rendering calls or bridge them with duplicate APIs.
- Current service headers declare only `Building` signatures; no `building *` service declarations remain in this scope.
- `building_object.h` was removed per user architecture update. `Building` now lives in `building.h`, and scoped `.cpp` files include `building/building.h` directly for implementation.
- Service headers avoid adding helper compatibility APIs; they expose only the single object path. C callers that include them should fail until migrated.
- Header hygiene follow-up: removed unsafe `building.h` includes from C++ headers that stored or referenced `Building` (`building_runtime`, `animations`, `house`, `production`, etc.) and moved full object inclusion into `.cpp` files. Where headers previously stored `Building` by value, they now store the record/type-definition pair and materialize `Building` in `.cpp`.

## Building Methods Added Or Used
- Added no new `Building` methods in this pass. `fetch_inventory_id` / `set_fetch_inventory_id` were already present in the folded `Building` surface and used here to remove direct `data.market.fetch_inventory_id` writes.
- Used in this scope: `Building::id()`, `Building::type_id()`, `Building::resource_amount(resource_type)`, `Building::accepts_good(resource_type)`, `Building::set_fetch_inventory_id(resource_type)`, `Building::from_id(unsigned int)`, `Building::first_of_type(building_type)`, `Building::is_venus_temple_type(building_type)`, `Building::is_ceres_temple_type(building_type)`, `Building::road_network_id()`, `Building::is_in_use()`, `Building::x()`, `Building::y()`, `Building::size()`, and `Building::set_resource_amount(resource_type, int)`.
- Used in lighthouse graphics refresh after the module-architecture update: `Building::type()` and `BuildingType::has_phased_construction()` replaced the static `building_type_registry_has_phased_construction(...)` decision.
- Removed `Building::draw_runtime_footprint` and `Building::draw_runtime_top` declarations/definitions from the folded class surface to honor the single draw-route rule.
- Remaining central hotspot observed outside Goodall scope: `BuildingDrawPass` and `Building::draw(BuildingDrawPass, ...)` still exist in `building.h`; that is a public stage-specific draw route and should be folded into the singular `building.draw(...)` path by the graphics-module lane.

## Remaining `legacy_record()` Access
- `src/building/lighthouse.cpp`: `set_lighthouse_graphic(Building)` still extracts `legacy_record()` to call existing lower-level rendering/runtime C functions: `building_runtime_apply_graphic(Building*)` equivalent does not exist yet, and `building_image_get` still takes the saved record. This is not a duplicate service API pair; it is a remaining rendering subsystem boundary.
- No `legacy_record()` remains in market/caravanserai/mess_hall/tavern/temple service-destination logic after switching to object distribution APIs and `set_fetch_inventory_id`.
- Graphics/static-call hotspot in this scope: lighthouse timber consumption now asks `lighthouse.type().has_phased_construction()` for the type decision, but it still refreshes graphics through `building_runtime_apply_graphic`, `building_image_get`, and `map_building_tiles_add`. This should be converted later to `Building` object -> `BuildingType` -> graphics module, not wrapped.

## Build Commands And Results
- `& '<MSBuild path>' .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`
  - Result: failed. Early errors included `Building` redefinition from headers that still included unsafe `building.h`, callers outside this scope passing raw `building *` to newly hard-migrated object APIs, and several unrelated migration lanes (`granary`, `storage`, `production_runtime`, `destruction`, `house`) still needing object conversion.
- Same MSBuild command after header hygiene.
  - Result: failed, but the `Building` redefinition and deleted `building_object.h` failures are gone. Current front is raw-to-object caller migration and missing direct includes in non-Goodall files, especially `armoury`, `construction_routed`, `data_transfer`, `destruction`, `dock`, `figure`, `warehouse`, and storage/granary call sites.
- Same MSBuild command after moving unsafe object includes out of headers and adding `.cpp` includes for runtime/production/house/animation code.
  - Result: failed. Goodall service implementation files no longer appear in the error front. Remaining errors are non-Goodall caller/object migrations such as `armoury`, `construction_routed`, `data_transfer`, `destruction`, `dock`, `figure`, `warehouse`, and storage/granary call sites.

## Blockers
- Need to migrate callers that still include these service headers from C or still pass raw `building *`.
- Need to verify with MSBuild after caller migration catches up.

## Goodall Latest Verification
- Reordered scoped `.cpp` includes so `building/building.h` is included before the local forward-only service header. This keeps by-value `Building` definitions in the implementation files on the complete type.
- Updated scoped service headers to wrap object-only declarations in `extern "C++"` so they remain valid when included from older `.cpp` files that still have broad `extern "C"` include blocks.
- Replaced lighthouse's static `building_type_registry_has_phased_construction(...)` decision with `lighthouse.type().has_phased_construction()`.
- Scoped scans:
  - No scoped service header includes `building/building.h`, `building_record.h`, or exposes `building *`.
  - No scoped implementation uses `building *`, `->`, or `legacy_record()` except `lighthouse.cpp` line 54 for the remaining graphics/runtime C boundary.
  - No scoped file references `building_object.h`, `building_type_registry_has_phased_construction`, or `BuildingDrawPass`.
  - `git diff --check -- <goodall scope files>` reports no whitespace errors, only CRLF normalization warnings.
- Latest build command: `MSBuild .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:quiet /clp:ErrorsOnly`.
- Latest build result: failed globally. Remaining errors are now dominated by unsafe `building.h` includes in other headers causing `BuildingDrawPass` / `Building` redefinitions, and other lanes still passing raw `building *` into hard-migrated warehouse/storage/distribution APIs.
- Follow-up build command: `MSBuild .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:quiet /clp:ErrorsOnly`.
- Follow-up build result after header hygiene: failed globally, but the unsafe `building.h` redefinition class is gone. Remaining representative errors are raw callers of object-only APIs in `dock.cpp`, `warehouse.cpp`, `figure.cpp`, `building_runtime_spawn.cpp`, `construction_building.cpp`, and central files that still need object conversion (`monument.cpp`, `building_type.cpp`, `animations.cpp`, `building.cpp`, `production.*`). Goodall service files were not the direct source of the new failures.
