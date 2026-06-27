# Deep Refactor Requirements

This is the stable requirements companion for `deep_refactor_implementation_progress.md`. Keep design rules here so the progress tracker can stay short and checklist-oriented.

## Data Ownership

- XML declarations are the source of truth for buildings, figures, units, formations, resources, graphics, pathing, and movement surfaces.
- C++ should resolve XML keys into typed runtime objects once at startup or load time, then pass object references instead of repeatedly matching strings, enum ids, or legacy records.
- Compatibility attrs such as `<event_data attr="...">` are allowed only as migration bridges for old events, saves, or unported systems. New behavior should live in explicit modules such as storage, labor, production, religion, military, water access, construction, graphics, or pathing.
- The runtime bit/mask layer must be generated from data-owned declarations. Bits are acceleration structures, not authored design.

## Object Ownership

- Data-owning types should own behavior and invariants that naturally belong to them.
- Building-facing code should prefer `Building`, `BuildingType`, and module methods over raw `building *` helpers.
- Figure-facing code should prefer `Figure`, `FigureType`, `FigureGraphics`, `FigureRoute`, and `PathingMode` methods over free helper functions.
- Building spawn declarations should name the figure/profile to create, while the FigureType profile owns native behavior, movement, pathing, graphics, and service effects. Avoid parallel behavior taxonomies where a building spawn `mode` and a figure `profile` can disagree about what the same walker is supposed to do.
- Formation logic belongs on formation/unit objects; forts should own a formation instance instead of callers rebuilding formation meaning from hardcoded tables.
- Cleanup should happen at lifecycle events: create, destroy, retarget, unload, save-load hydration, type change, or relationship removal.

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
