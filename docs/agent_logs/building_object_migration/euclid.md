# Building Object Migration - Euclid

## Scope
- Lane: storage/logistics group.
- Files in scope: `src/building/granary.cpp`, `src/building/granary.h`, `src/building/warehouse.cpp`, `src/building/warehouse.h`, `src/building/storage.cpp`, `src/building/storage.h`, `src/building/distribution.cpp`, `src/building/distribution.h`, `src/building/dock.cpp`, `src/building/dock.h`.
- Coordination rule: do not edit other agents' logs or files outside this lane unless a central `Building` API addition is necessary and documented here.
- Architecture change from user: stop compatibility-overload migration immediately. `building.h` is the sole building object/class header. Do not preserve `building_object.h` usage. Storage/logistics headers should migrate to hard `Building` signatures and include `building.h` directly; files that need that API must be C++.
- Follow-up update: `src/building/building_object.h` has been deleted and removed from the project by coordination. The `Building` class declaration is now folded into `src/building/building.h`; do not recreate or include `building_object.h`.
- Hard override from user: stop adding overloads, wrappers, bridge APIs, or compatibility helpers. The migration should cut duplicate lines and converge on the C++ `Building` object path. In this lane, if a `Building` declaration merely mirrors a raw `building *` declaration, remove the duplicate shape and migrate the API/implementation/callers to one object path.
- Rendering clarification from user: do not create stage-specific public object APIs such as `draw_footprint` or `draw_top`. The public route should be singular, e.g. `building.draw(...)`, and delegate to type-owned rendering logic using the building's state/properties. Euclid storage/logistics has not added draw APIs.
- Module-boundary update from main thread: buildings hold data/interfaces, building types hold modules, and graphics is a module. No compatibility wrappers, overload bridges, duplicate helpers, or static city_draw-style building rendering calls should be preserved. Convert graphics access toward `Building` -> `BuildingType` -> graphics module when this lane owns it.

## Files Changed
- `docs/agent_logs/building_object_migration/euclid.md`: created this progress log.
- `src/building/building_object.h`: added reusable object accessors needed by storage/logistics migration.
- `src/building/building.cpp`: implemented the new `Building` accessors.
- `src/building/storage.h`: added guarded C++ `Building` overloads while keeping raw-pointer legacy declarations.
- `src/building/granary.h`: added guarded C++ `Building` overloads and direct `game/resource.h` include.
- `src/building/warehouse.h`: added guarded C++ `Building` overloads while keeping the C ABI declarations inside `extern "C"`.
- `src/building/distribution.h`: added guarded C++ `Building` overloads for distributor helpers.
- `src/building/dock.h`: added guarded C++ `Building` overloads for dock helpers.
- `src/building/storage.h`, `src/building/granary.h`, `src/building/warehouse.h`, `src/building/distribution.h`, `src/building/dock.h`: corrected the C++ overload blocks and `building_object.h` includes to use explicit `extern "C++"` linkage, because several legacy `.cpp` files include these headers from inside caller-owned `extern "C"` blocks.
- Walkback in progress: remove the obsolete overload blocks and raw-pointer compatibility declarations in this lane. Replace them with hard `Building` signatures and direct `building/building.h` includes.
- Duplicate work being cut: remove paired raw-pointer plus `Building` declarations in storage/logistics headers; keep one `Building` signature for building-facing operations.
- `src/building/granary.h`: cut the legacy raw-pointer declaration block and the mirrored overload block; replaced with one `Building` API set.
- `src/building/warehouse.h`: cut the `extern "C"` raw-pointer declaration block and the mirrored overload block; replaced with one `Building` API set.
- `src/building/distribution.h`, `src/building/dock.h`, `src/building/storage.h`: now expose only `Building` signatures for building-facing operations in this lane.
- `src/building/distribution.cpp`: cut raw-pointer adapter functions and the local `as_building` bridge. Remaining implementation uses `Building` directly.
- `src/building/storage.cpp`: cut raw-pointer exported definitions and local adapter bridges for storage amount/state/permissions/tooltip/resource-add paths. Remaining building-facing definitions now take `Building`.
- `src/building/granary.cpp`: stock/add/remove/count functions are being moved from raw records to `Building` methods and object iteration.

## Public Signatures Changed
- Added C++ overloads in `storage.h`, `granary.h`, `warehouse.h`, `distribution.h`, and `dock.h` that accept `Building` references. Legacy raw-pointer declarations remain so unmigrated callers can continue compiling.
- Linkage correction: all `Building` overload declarations are now guarded by `#ifdef __cplusplus` and wrapped in `extern "C++"` so they are not swallowed by enclosing legacy `extern "C"` include blocks.
- Walkback required: the compatibility overload strategy above is obsolete. Remove those overload blocks from mixed legacy headers and replace raw-pointer public signatures in this lane with `Building` signatures instead of keeping parallel APIs.
- `building_object.h` walkback: central class declaration now lives in `building.h`; any scoped references to `building_object.h` must be replaced with `building/building.h`.
- New hard rule: do not add replacement bridge declarations after deleting the duplicate declarations. Callers must migrate to `Building`; raw record access should only remain as narrow implementation detail where no object method exists yet.
- Public signature cut: raw `building *`/`const building *` signatures for granary, warehouse, storage, distribution, and dock building-facing APIs are being removed instead of preserved as wrappers.

## Building Methods Used Or Added
- Existing methods inspected for planned use: `id()`, `legacy_record()`, `type_id()`, `grid_offset()`, `x()`, `y()`, `size()`, `road_network_id()`, `distance_from_entry()`, `is_in_use()`, `has_plague()`, `has_cached_road_access()`, `has_house_size()`, `has_required_workers()`, `has_road_access()`, `is_working()`, `resource_amount()`, `add_resource()`, `set_resource_amount()`, accepted-good helpers, `storage_id()`, dock helpers, and permission/data helpers exposed through storage APIs.
- Added `next()` to walk linked building parts through the existing `building_next` relationship. Reason: warehouses use the main building plus eight linked storage spaces, and this is a general relationship rather than a warehouse-only field leak.
- Added `set_storage_id(int)`. Reason: storage reassignment currently writes the saved record directly; storage id is already exposed by `storage_id()` and the setter keeps that paired concept reusable.
- Added `warehouse_resource_id()` and `set_warehouse_resource_id(resource_type)`. Reason: warehouse space occupancy is a repeated domain concept across warehouse storage helpers; keeping it on `Building` avoids exposing `subtype.warehouse_resource_id` throughout migrated code. Coordination note: setter implementation was corrected to cast to `short`, matching the legacy field type.
- Central API adjustment required after architecture change: move the `Building` class declaration from `building_object.h` into `building.h` or otherwise make `building.h` the only included object header. Do not keep using `building_object.h`.

## Remaining `legacy_record()` Access
- None introduced yet in this lane. Planned use will remain local for fields that do not yet have a reusable `Building` method, such as storage permission bitsets, storage quantity arrays, and dock queue/trade route bitfields.
- Remaining in-scope legacy/static graphics hotspot: `src/building/warehouse.cpp` has `building_warehouse_space_set_image`, which calls `resource_graphics(...).storage_image(...)` and `map_image_set(...)` directly for individual warehouse bays. This should move toward the type graphics module path; Euclid has not converted it yet.

## Build Commands And Results
- Not run yet in this lane.
- Build feedback from monitoring: MSVC reported C2733 overload/linkage errors for `Building` overloads in mixed legacy headers. Header linkage has been corrected before continuing implementation rewrites.
- Ran `MSBuild .\Vespasian.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`.
- Result: failed, but the previous C2733 header-linkage overload errors are no longer present. Current visible in-scope failures are `warehouse.cpp` references to legacy helpers such as `building_next`, `building_main`, `building_first_of_type`, and `building_get`.

## Blockers
- External callers may still pass raw `building *` into this lane. Under the new hard-migration direction, those callers should be converted to `Building` instead of preserving parallel compatibility wrappers here.

## 2026-06-05 Current Euclid Update

### Scope And Files Changed
- Stayed in the storage/logistics lane plus one central `Building` data accessor.
- Changed `src/building/warehouse.cpp`: migrated implementation signatures and internals from raw `building *` traversal to `Building`; removed `building_next`, `building_main`, `building_first_of_type`, and `building_get` usage from this file.
- Changed `src/building/dock.cpp`: migrated dock iteration/trade-route/ship-routing helpers to `Building`; removed raw `building *`, `building_get`, `building_first_of_type`, and `legacy_record()` usage from this file.
- Changed `src/building/dock.h`: removed the no-longer-needed `building_fwd.h` include and hid `Building` declarations from C translation units without adding raw alternatives.
- Changed `src/building/granary.h`, `src/building/warehouse.h`, `src/building/storage.h`, `src/building/distribution.h`, `src/building/dock.h`: `Building` declarations are now `#ifdef __cplusplus` only so C files can still include shared constants/enums without seeing object signatures. No raw `building *` replacement APIs were added.
- Changed `src/building/building.h` and `src/building/building.cpp`: added `Building::distribution_cartpusher_id(int)` as a reusable building data interface for distribution cartpusher slots.

### Public Signatures Changed
- `warehouse.cpp` definitions now match the object-only declarations in `warehouse.h`, including `Building &`/`const Building &` plus typed `resource_type` parameters for building-facing APIs.
- `dock.cpp` definitions now match object-only declarations for dock-facing helpers that need a building object: `building_dock_count_idle_dockers`, `building_dock_get_ship_request_tile`, `building_dock_can_import_from_ship`, and `building_dock_can_export_to_ship`.
- Header visibility change: C translation units no longer see `Building` signatures, but scalar/non-building declarations remain visible. This is not a compatibility overload path; it prevents C parse failures for files that include these headers only for constants or scalar functions.

### Building Methods Used Or Added
- Newly added: `distribution_cartpusher_id(int)`. Reason: dock idle-worker counting needs the distribution cartpusher slots; this is a repeated building data concept used across dock/depot/distribution code, so exposing it on `Building` removes a direct saved-record read without adding a one-off raw field bridge.
- Newly used in this pass: `id()`, `main()`, `next()`, `next_of_type()`, `first_of_type()`, `from_id()`, `type_id()`, `grid_offset()`, `x()`, `y()`, `road_network_id()`, `distance_from_entry()`, `road_access_x()`, `road_access_y()`, `is_in_use()`, `is_working()`, `has_plague()`, `has_house_size()`, `has_cached_road_access()`, `has_water_access()`, `set_has_water_access()`, `worker_count()`, `max_distance_to()`, `orientation()`, `resource_amount()`, `add_resource()`, `set_resource_amount()`, `warehouse_resource_id()`, `set_warehouse_resource_id()`, `storage_id()`, `set_accepted_good()`, `dock_has_accepted_route_ids()`, `dock_accepted_route_ids()`, `dock_trade_ship_id()`, `dock_orientation()`, and `set_dock_accepted_route_ids()`.
- No stage-specific draw methods were added. The storage/logistics lane did not add any `draw_footprint`/`draw_top` style APIs.

### Remaining `legacy_record()` Access
- `src/building/granary.cpp`: one remaining `legacy_record()` call remains at `building_destroy_by_fire(max_building.legacy_record())`. Reason: `building_destroy_by_fire` is still a raw-record destruction boundary and there is not yet a `Building` object method for destruction-by-fire.
- `src/building/warehouse.cpp`: no `legacy_record()` remains after replacing the bay load calculation with `Building::resource_amount()` iteration.
- `src/building/dock.cpp`: no `legacy_record()` remains after adding and using `distribution_cartpusher_id(int)`.

### Graphics And Static-Call Hotspots
- Remaining in-scope graphics hotspot: `src/building/warehouse.cpp::building_warehouse_space_set_image` still directly calls `resource_graphics(...).storage_image(...)` and `map_image_set(...)` for warehouse bays. This is no longer raw-record based, but it is still not the desired `Building` -> `BuildingType` -> graphics module path.
- No new graphics helper wrappers or duplicate draw-stage APIs were added.

### Build Commands And Results
- Ran `MSBuild .\Vespasian.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`.
  - Result: failed first on external city/resource and enum strictness errors while other agents were actively changing those files.
- Ran `MSBuild .\Vespasian.sln /m /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo` again.
  - Result: failed on C translation units parsing object-only headers. Fixed scoped headers by hiding `Building` declarations from C without adding raw APIs.
- Ran `MSBuild .\Vespasian.sln /m:1 /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`.
  - Result: progressed past storage/logistics compilation. A PDB contention failure occurred with `/m`; `/m:1` avoided it.
- Latest `MSBuild .\Vespasian.sln /m:1 /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo` result:
  - Failed in external files now converted to C++ by another lane: `src/city/festival.cpp`, `src/city/games.cpp`, and `src/city/gods.cpp`.
  - Representative blockers: `festival.cpp` still uses raw `building_get` and raw `building` fields without including/migrating to `Building`; `games.cpp` has C-to-C++ initializer syntax and `int` to `resource_type` call errors; `gods.cpp` has `int` to `god_type` call errors.

### Blockers
- Current build blockers are outside Euclid scope and appear to be active parallel C-to-C++ conversions in the city lane.
- Some C++ files may still include object headers from inside caller-owned `extern "C"` blocks, causing possible C4190 warnings for UDT-returning declarations. The current scoped headers avoid C parse failures, but caller include blocks should still be cleaned up by the owning lane.
