# Declarative building-info windows

## Goal

Building-info windows should be selected, composed, and laid out by the active mod while the engine exposes typed data bindings and actions. The migration must remove the current requirement that every building category has matching bespoke draw, mouse, and tooltip branches.

The immediate warehouse repair introduces `BuildingInfoScreenSelection`. It resolves one presentation-facing screen id before dispatch so storage takes precedence over its more general owner-controlled-foundation capability. Background drawing, foreground drawing, and input now consume the same selection. These ids are the compatibility seam for future XML document or template ids.

## Runtime layers

1. **Screen selection model**
   - Resolve a screen from building capabilities and current navigation state.
   - Keep selection independent from layout coordinates and widget implementation.
   - Allow a mod-owned BuildingType/UI mapping to override the engine default.

2. **Declarative document registry**
   - Discover window documents from the active mod stack instead of adding a compiled path constant per window.
   - Support reusable templates/components, explicit imports, and mod-stack replacement.
   - Validate widget types, bindings, actions, and template parameters during startup with retained failure context.

3. **Dynamic DOM instance**
   - Instantiate typed element objects from a `DeclarativeWindowDefinition`.
   - Give elements stable keys, parent/child ownership, visibility/enabled state, and resolved geometry.
   - Reconcile changed bindings and repeated collections without rebuilding unrelated widgets or losing focus/scroll state.

4. **Bindings and actions**
   - C++ view models expose typed values and observable collections such as stored resources, permissions, figures, and production rows.
   - XML owns which capabilities appear, their order, presentation, and action wiring.
   - Actions remain a validated engine vocabulary; XML must not call arbitrary native functions.

5. **Reusable widgets**
   - DOM elements compose the existing `UiWidget`/`SharedUiRuntime` classes.
   - Drawing primitives remain renderer-level building blocks and do not acquire window state or gameplay behavior.

## Dynamic element vocabulary

The next declarative-window schema slice should add:

- nested `element`/`component` nodes rather than a permanently flat widget list;
- `template` plus typed parameters for reusable rows and button groups;
- `repeat` over an observable collection, with an explicit stable item key;
- `if`/`enabled_when` expressions over registered bindings;
- layout containers for row, column, grid, overlay, and scroll content;
- event bubbling from element action to the owning window controller;
- tooltip and focus bindings on the same element that owns pointer input.

Resource filters are a good first dynamic collection: each resource produces one keyed row from a shared template, while the selected mod chooses row geometry, visible controls, and labels.

## Migration sequence

1. Keep `BuildingInfoScreenSelection` authoritative and move remaining duplicated special-screen precedence into it.
2. Generalize declarative-window discovery and add component/template parsing without changing existing main-menu and mission-briefing documents.
3. Add DOM element instances, typed binding/action registries, and keyed repeat reconciliation.
4. Migrate the warehouse resource-filter screen as the first building-info document, backed by a storage view model and collection.
5. Migrate roadblock permissions as a reusable component embedded by warehouses, granaries, bridges, and standalone roadblocks.
6. Move per-building screen selection/mapping into mod XML, retaining engine defaults only as a transitional compatibility layer.
7. Remove bespoke draw/mouse/tooltip branches as each window becomes declarative.

## Invariants

- One resolved screen owns drawing, input, focus, and tooltips for a frame.
- A broader capability cannot intercept a more specific screen's input.
- Dynamic elements retain stable identity across data changes.
- Mod XML controls composition but cannot bypass typed gameplay APIs.
- Existing widgets and primitives are reused; declarative migration does not create parallel rendering controls.
