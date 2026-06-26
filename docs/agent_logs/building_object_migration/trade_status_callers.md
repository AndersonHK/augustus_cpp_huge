# Trade Status Callers Migration Log

## Scope
- Trade status callers that included object-only service headers:
  - `src/city/trade.c` -> `src/city/trade.cpp`
  - `src/empire/trade_prices.c` -> `src/empire/trade_prices.cpp`
- Project rename metadata:
  - `Vespasian.vcxproj`
  - `Vespasian.vcxproj.filters`

## Files Changed
- `src/city/trade.cpp`
- `src/city/trade.h`
- `src/empire/trade_prices.cpp`
- `src/empire/trade_prices.h`
- `Vespasian.vcxproj`
- `Vespasian.vcxproj.filters`

## Public Signatures Changed
- No semantic public API shape changed.
- Existing `city/trade.h` and `empire/trade_prices.h` declarations now use `extern "C"` under `__cplusplus` so the renamed C++ implementations keep the same single C-callable API used by existing C and C++ callers.
- No building compatibility wrappers, overload bridges, or duplicate raw/object service APIs were added.

## Building Methods Added Or Used
- Added no `Building` methods.
- Used `Building::from_id(unsigned int)` to resolve the active caravanserai/lighthouse records for trade policy price factors.
- Used `Building::worker_count()` and `Building::type().required_workers()` instead of reading `building->num_workers` and `model_get_building(building->type)->laborers`.

## Remaining Legacy Or Static Hotspots
- No `building_object.h` usage.
- No `building *` use remains in `trade_prices.cpp`.
- `trade.cpp` still iterates legacy `figure *` records in `trade_caravan_count`; that is outside the building-object service migration and not a graphics/static building path.
- No graphics/static rendering calls are in this scope.

## Build And Checks
- Direct MSVC compile:
  - `cl /nologo /std:c++17 /EHsc /c ... src\city\trade.cpp`
  - `cl /nologo /std:c++17 /EHsc /c ... src\empire\trade_prices.cpp`
  - Result: passed.
- Focused MSBuild:
  - `MSBuild .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /v:quiet /clp:ErrorsOnly`
  - Result: failed globally before this lane became linkable. Errors are dominated by other building-object migration lanes: missing central legacy declarations, raw `building *` callers of object-only APIs in dock/storage/warehouse/etc., and incomplete `Building` use in `building_runtime_spawn.cpp`. There was also a `CL.write.1.tlog` lock from another concurrent build.
- `git diff --check` on this scope reported no whitespace errors, only CRLF normalization warnings.

## Blockers
- Full project verification is blocked by parallel global migration errors outside this scope.
