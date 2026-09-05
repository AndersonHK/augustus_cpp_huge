# Building Object Migration - Carver

## Scope

- Production and industry group only:
  - `src/building/industry.cpp`
  - `src/building/industry.h`
  - `src/building/production.cpp`
  - `src/building/production.h`
  - `src/building/production_runtime.cpp`
  - `src/building/production_runtime.h`
  - `src/building/production_runtime_api.h`
  - `src/building/production_method.cpp`
  - `src/building/production_method.h`

## Files Changed

- `src/building/production.h`
- `src/building/production.cpp`
- `src/building/production_method.h`
- `src/building/production_method.cpp`
- `src/building/production_runtime.cpp`
- `src/building/production_runtime.h`
- `src/building/production_runtime_api.h`
- `src/building/industry.cpp`

## Public Signatures Changed

- Internal C++ production runtime signatures moved from raw `::building *` to `Building`:
  - `production_runtime_impl::get_or_create`
  - `production_runtime_impl::get_or_create_primary`
  - `production_runtime_impl::get_method_count`
- `Production` now stores a `Building` object and exposes `const Building &building() const` plus `building_id()`.
- `ProductionMethod` methods now accept `const Building &` instead of `const ::building &`:
  - `max_progress_for`
  - `has_required_inputs`
  - `labor_access_for`
  - `can_start_cycle`
- The extern "C" `production_runtime_api.h` signatures remain `building *` as a legacy bridge for existing C callers.

Update: the internal runtime functions take `Building` by value instead of `const Building &` so the C bridge can pass the wrapped legacy pointer into existing runtime construction without const-casting.

## Building Methods Used

- `id()`
- `legacy_record()`
- `type_id()`
- `type_definition()`
- `x()`
- `y()`
- `road_network_id()`
- `is_in_use()`
- `is_mothballed()`
- `has_workers()`
- `worker_count()`
- `has_water_access()`
- `resource_amount(resource_type)`
- `add_resource(resource_type, int)`

## Building Methods Added

- None in this scope so far.

## Coordination Notes

- Header rule for future touches: keep raw C declarations inside `extern "C"` blocks, and place any `Building` overloads under `#ifdef __cplusplus` outside the C block. The current duplicate-linkage failures in other lanes are consistent with object overloads being declared inside C linkage.
- Coordination update says central `Building::count()` and type-predicate helpers are being added locally. This does not change the production lane patch directly, but it may replace some future `building_count`/`building_is_*` bridge usage in other migration groups.

## Remaining Legacy Record Access

- `Production` still uses `legacy_record()` for mutable industry state and strike fields:
  - `data.industry.progress`
  - `data.industry.has_raw_materials`
  - `data.industry.curse_days_left`
  - `data.industry.blessing_days_left`
  - `data.industry.age_months`
  - `data.industry.average_production_per_month`
  - `data.industry.production_current_month`
  - `strike_duration_days`
  - `figure_id4`
  - `output_resource_id`
- `ProductionMethod` still uses `legacy_record()` for fields/helpers that the object API does not cover yet:
  - `houses_covered` for legacy labor coverage fallback
  - `strike_duration_days` and `data.industry.progress` for production eligibility
  - `building_local_workforce_*` C helpers that still require `const building *`
- These accesses are now confined to `production.cpp` and `production_method.cpp`; no raw-record helper is declared in `production.h`.

## Build Commands And Results

- `& '<MSBuild path>' .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`
  - First run failed on two local `production_runtime.cpp` constness errors caused by passing `Building` as `const &`.
  - After switching internal runtime functions to take `Building` by value, rerun failed only on broader repo missing `building_*` declarations in other files such as `house_population.cpp`, `house.cpp`, `destruction.cpp`, `warehouse.cpp`, `monument.cpp`, and similar. No remaining errors were reported for this production/industry scope.
  - Final rerun after moving the raw-record helper out of `production.h` still failed only outside this scope. Additional external blockers include duplicate extern "C" overload declarations in headers such as `storage.h`, `distribution.h`, `granary.h`, and `warehouse.h`.

## Blockers

- The global Release x64 build is still blocked by other migration groups that need either object-method rewrites or `.cpp`-local `building.h` bridge includes.
- Some object overloads appear to have been added under `extern "C"` in other headers (`storage.h`, `distribution.h`, `granary.h`, `warehouse.h`), which MSVC rejects.
- The extern C runtime API remains a raw pointer boundary because callers outside this C++ runtime scope still use the legacy C interface.
