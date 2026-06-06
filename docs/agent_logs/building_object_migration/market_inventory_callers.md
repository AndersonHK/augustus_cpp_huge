# Market Inventory Callers Migration Log

## Scope
- `src/figure/figure_runtime_native.cpp`
- `src/figure/phrase.cpp`
- `src/figuretype/supplier.cpp`

## Files Changed
- `src/figure/figure_runtime_native.cpp`
- `src/figure/phrase.cpp`
- `src/figuretype/supplier.cpp`

## Public Signatures Changed
- None. This slice only migrated callers to the existing object-only market service APIs.

## Building Methods Added Or Used
- Added no new `Building` methods.
- Used `Building::from_id(unsigned int)` in `src/figure/phrase.cpp`.
- Used `Building::Building(building *)` and `Building::set_fetch_inventory_id(resource_type)` in the supplier recalculation paths.

## Remaining Legacy Or Static Hotspots
- The scoped files still contain broad legacy raw-record logic unrelated to the market inventory service calls. This slice did not convert those larger walker/state paths.
- No graphics/static rendering calls were changed in this scope.
- `src/figure/figure_runtime_native.cpp` and `src/figuretype/supplier.cpp` contain other in-progress edits outside this caller migration. I did not revert or normalize them.

## Build Commands And Results
- Scoped scan:
  - `src/figure/figure_runtime_native.cpp`: includes `building/building.h` and `building/market.h` before the legacy `extern "C"` block; market supplier recalculation now calls `building_market_get_needed_inventory(Building, ...)`, `building_market_fetch_inventory(Building, ...)`, and `Building::set_fetch_inventory_id(...)`.
  - `src/figure/phrase.cpp`: includes `building/building.h` and calls `building_market_get_max_food_stock(Building::from_id(...))`.
  - `src/figuretype/supplier.cpp`: includes `building/building.h` and `building/market.h` before the legacy `extern "C"` block; market supplier recalculation now calls object-only market service APIs and `Building::set_fetch_inventory_id(...)`.
- `git diff --check -- src/figure/figure_runtime_native.cpp src/figure/phrase.cpp src/figuretype/supplier.cpp docs/agent_logs/building_object_migration/market_inventory_callers.md`
  - Result: only Git CRLF normalization warnings for `src/figure/figure_runtime_native.cpp` and `src/figuretype/supplier.cpp`.
- `& 'D:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:quiet /clp:ErrorsOnly /nologo`
  - Result: failed in broader migration code before a clean project build.
  - No errors from this slice were observed in the visible MSBuild output.
  - Current visible failures outside this slice include object-only/raw-call mismatches in `src/building/dock.cpp`, `src/building/warehouse.cpp`, `src/building/construction_building.cpp`, `src/building/building.cpp`, `src/building/building_runtime_spawn.cpp`, and `src/building/figure.cpp`; missing replacement helpers in `src/building/image.cpp`, `src/building/monument.cpp`, `src/building/building_type.cpp`, and `src/building/figure.cpp`; a `Production::record_` const/raw mismatch in `src/building/production.cpp`; `BuildingType` visibility errors in `src/building/production.h`; and undeclared local record variables in `src/building/animations.cpp`.

## Blockers
- None in this caller slice. The current project build is blocked by other migration lanes outside the delegated scope.
