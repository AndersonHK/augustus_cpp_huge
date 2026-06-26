# Lagrange Migration Log

## 2026-06-05 Building Header Hard Migration

- `src/building/building.h` is the sole public C++ building object/class header.
- `src/building/building_object.h` was a compatibility split and has been deleted; do not recreate it or include it.
- Files that need `Building` must be C++ implementation/header paths that include `building/building.h` directly.
- Do not add compatibility overloads to legacy C headers just to preserve C linkage. Migrate signatures and implementations toward `Building` instead.
- `building/building_fwd.h` and legacy `building *` APIs are temporary pressure points only; each touched building-facing file should prefer `Building` and narrow `legacy_record()` use to fields that do not have object methods yet.

Walkback list:

- Replace any previous `building/building_object.h` include with `building/building.h`.
- Remove compatibility-only C++ overloads from legacy headers when the surrounding call sites can be migrated to `Building`.
- Do not keep parallel object/class headers or split class declarations out of `building.h`.
- Continue converting building-dependent `.c` files to `.cpp` instead of insulating them from `building.h`.

## 2026-06-05 Hard Object Path Rule

- Do not add overloads, wrappers, bridge APIs, or compatibility helpers that preserve the old raw `building *` surface beside a new `Building` surface.
- Building-facing code should converge on one object path: call methods on `Building`, and move behavior onto the object/type path when that lets legacy code be deleted.
- Headers in a migrated scope should expose `Building` directly with `building/building.h`; callers that need those headers must become C++.
- `legacy_record()` is only a temporary local escape hatch for saved fields not yet exposed as `Building` methods. It must not become a new public compatibility layer.

Duplicate work being cut now:

- The house module will not keep parallel raw-pointer and `Building` APIs.
- Any C callers that need house object helpers are being converted to C++ callers rather than receiving wrapper functions.
- Prior compatibility-split assumptions around `building_object.h` remain walked back; `building.h` is the only building object header.

## 2026-06-05 Singular Draw Route

- Do not create stage-specific public building draw APIs such as `draw_footprint`, `draw_top`, or `draw_animation`.
- The public object rendering path should be singular: `building.draw(...)` or the closest equivalent.
- `Building::draw(...)` should delegate to rendering logic owned by the building's type, using the live building object's properties and state.
- Type rules decide footprint/top/animation/details internally; caller code should not duplicate that stage-routing logic.
