# Service/Trade Caller Building Migration

## Scope
- Lane: service/trade caller files that include `building/caravanserai.h`, `building/lighthouse.h`, or `building/market.h` outside the Goodall implementation files.
- Hard rule followed: no overload bridges, wrappers, compatibility helpers, or duplicate raw/object API pairs. Callers must compile against the single C++ `Building` object path.
- `building_object.h` is deleted in this architecture and was not recreated or included.

## Files Changed
- `src/figuretype/trader.cpp`
- `src/figuretype/docker.cpp`
- `src/figuretype/service.cpp`
- `src/game/tick.cpp`
- `src/window/trade_prices.cpp`
- `src/window/advisor/trade.cpp`
- `src/window/building/culture.cpp`
- `src/window/building/distribution.cpp`
- `src/window/building/figures.cpp`
- `src/widget/city_building_ghost.cpp`

## Public Signatures Changed
- None in this lane.
- Existing caller project entries were already `.cpp` in `Vespasian.vcxproj` and `Vespasian.vcxproj.filters`, so no project conversion was needed.

## Building Methods Used
- `Building(::building *)` in `src/window/building/distribution.cpp` to pass object values to:
  - `building_caravanserai_enough_foods(Building)`
  - `building_lighthouse_enough_timber(Building)`

## Legacy Record Access Remaining
- No new `legacy_record()` access was added in this lane.
- Raw `building *` usage remains in these caller files where the surrounding subsystem still uses legacy records directly, but no compatibility bridge was added for the service APIs.

## Notes
- Moved C++ service headers out of `extern "C"` include blocks in touched C++ callers so they are consumed as C++ declarations.
- Left out-of-scope files alone: Goodall service implementation/header files, Peirce storage/dock/granary/distribution/warehouse files, Dalton files, `building/figure.cpp`, and `building_runtime_spawn.cpp`.

## Build Commands / Results
- `git diff --check -- <touched service-caller files> docs/agent_logs/building_object_migration/service_callers.md`
  - Result: passed with no whitespace errors. Git reported existing LF-to-CRLF normalization warnings for touched files.
- `& '<MSBuild path>' .\Vespasian.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo`
  - Result: failed before this lane's caller files were meaningfully reached.
  - First blocker: `src/building/building_type.h(482,33): error C2061: syntax error: identifier 'BuildingDrawPass'`.
  - Related blockers: `src/building/building.cpp(307,37)` still defines `Building::draw(BuildingDrawPass, ...)` while the current `Building` declaration exposes `draw(int, int, int, color_t, float)`, and `src/building/building_type.cpp(924,19)` defines a `BuildingType::draw(Building, BuildingDrawPass, ...)` overload not present in `building_type.h`.
  - This is outside the service/trade caller write scope and appears to belong to the central graphics/object route migration.

## Blockers
- Release x64 validation is currently blocked by the central `BuildingDrawPass` / `Building::draw` / `BuildingType::draw` mismatch outside this lane.
