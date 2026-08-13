# Deep Refactor Requirements

This is the stable requirements companion for `deep_refactor_implementation_progress.md`. Keep design rules here so the progress tracker can stay short and checklist-oriented.

## Data Ownership

- XML declarations are the source of truth for buildings, figures, units, formations, resources, graphics, pathing, and movement surfaces.
- Preserve mod identity: Julius XML/data should match Julius GH behavior, Augustus should match Augustus GH behavior, and Vespasian is the enhanced data mod for intentional behavior changes such as making actor colonies required for plays.
- Startup should resolve XML keys into immutable typed definition objects once, then hand those definitions to runtime for the rest of the process.
- Save/load should be the only layer that knows save records. It should hydrate records into live objects, serialize live objects and module state back into save records, and apply old-save migrations without owning game-loop behavior.
- Runtime should consume resolved definitions and live objects. It should not reset or recreate type registries, know legacy records exist, or translate records into objects.
- Root XML `type` values are canonical identity. Rare historical names may be accepted through explicit identity aliases at import boundaries, but new behavior must live in modules such as storage, labor, production, religion, military, water access, construction, graphics, or pathing rather than string identity checks.
- The runtime bit/mask layer must be generated from data-owned declarations. Bits are acceleration structures, not authored design.

## Object Ownership

- Data-owning types should own behavior and invariants that naturally belong to them.
- Building-facing code should prefer `Building`, `BuildingType`, and module methods over raw `building *` helpers.
- Figure-facing code should prefer `Figure`, `FigureType`, `FigureGraphics`, `FigureRoute`, and `PathingMode` methods over free helper functions.
- Cross-cutting building/figure behavior should migrate toward owner-bound runtime modules, such as `building.entertainment().tick()`, where the module binds owner object, immutable definition pointer, and mutable state/data instead of exposing loose `definition->tick(owner)` calls.
- New bound modules should first wrap existing record-backed state, then move physical state storage only after callers route through the module facade.
- XML folders may be complete module definitions, partial definition ingredients, or vocabularies. Do not treat a type registry such as `WaterAccessType` as the complete building module when building-specific provider/requirement policy still lives in `BuildingType`.
- Simple assignment-only object state should be public data, not hidden behind one-line `get/set` accessors. Keep a field private only when changing or reading it must enforce behavior such as clamping, lifecycle registration, reservation cleanup, id-to-pointer conversion, dirty marking, cache invalidation, or multi-field invariants.
- During migration, public field syntax is acceptable only for data that is truly object-owned or safely bridged to the current backing state. Identity fields such as `Building.id` are special stable bridge keys, not ordinary mutable state. For peeled fields, the target is a current runtime struct plus module state; the save bridge reconstructs the save record from those pieces. If required module data is missing, finish the save with the safest partial/default data available, then report the error with enough context to diagnose the missing module state.
- Building spawn declarations should name the figure/profile to create, while the FigureType profile owns native behavior, movement, pathing, graphics, and service effects. Avoid parallel behavior taxonomies where a building spawn `mode` and a figure `profile` can disagree about what the same walker is supposed to do.
- Formation logic belongs on formation/unit objects; forts should own a formation instance instead of callers rebuilding formation meaning from hardcoded tables.
- Cleanup should happen at lifecycle events: create, destroy, retarget, unload, save-load hydration, type change, or relationship removal.
- Before adding a helper, class, or file, search for existing similarly named owners and stripped legacy files. Repurpose the closest existing owner when it fits the behavior; create a new file only after proving there is no suitable module to restore, extend, or move the method onto.
- New or substantially rewritten `.cpp` files should have a paired header that lists the file's owned public and private class methods, so the header reads as a map of the implementation. Major classes should live in same-named header/source pairs, such as `BuildingGraphics.h/.cpp` for `BuildingGraphics`. Treat headers as navigation ledgers for future rewrites: they should make bookkeeping easy, reveal redundant internal behavior, show where external calls might replace private helpers, and make useful helpers easy to promote deliberately. Avoid meaningful anonymous helper islands when the behavior belongs to the file's class or module; make those helpers private methods or private helper classes instead.

## Routing And Movement

- Route planning should be centralized through the route/pathing objects. Avoid local pathfinding, distance-grid, or route-validation algorithms outside the route system.
- Pathing mode and movement-surface rules should be XML-owned and key-driven, so mods can define roads, ramps, highways, gardens, trains, bicycles, or any other transport surface without C++ naming branches.
- Route cost should derive from inverse movement speed. Do not author a separate route-cost model that can drift from actual movement speed.
- Route and cost-map regeneration should be lazy: reuse existing routes until the destination, policy, terrain epoch, roadblock permission, or next step is truly invalid.

## Rendering And Graphics

- Buildings and figures should own native graphics requests through their XML-backed type definitions.
- Runtime draw code should ask objects for draw requests, not reconstruct special-case image groups in city draw loops.
- Logical size and source pixel size must be separate. New higher-resolution assets should preserve logical footprint while contributing more source pixels.
- Atlas sampling is temporary compatibility debt; native path-keyed image resources are the target.

## Performance And Cleanup

- Prefer deletions and consolidation over compatibility growth. New public wrappers should earn their existence.
- Repeated full-city scans should migrate toward typed runtime lists or dirty/epoch-driven caches.
- Accessors that only wrap one line of object state should be deleted when callers can take the owning object.
- String and enum bridges should shrink over time, especially for localization, resource names, building type checks, and graphics lookups.

## Validation

- Add small headless tests for startup/parsing and registry resolution so XML regressions fail from PowerShell before a manual runtime test.
- Use Release x64 builds for deploy checkpoints, but add narrower parser/runtime tests whenever the failure mode does not require SDL city rendering.
- Keep progress checkboxes in the tracker tied to source evidence, build output, docs updates, or manual regression feedback.
