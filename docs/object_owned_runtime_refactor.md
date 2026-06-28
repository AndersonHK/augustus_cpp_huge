# Object-Owned Runtime Refactor Doctrine

This document records the refactoring direction for Vespasian runtime systems. It applies especially to buildings, figures, storage, graphics, production, labor, and other hot simulation paths.

## Core Principle

Runtime objects should own their relationships and invariants.

The long-term goal is to stop writing code where every tick scans every building or figure, checks a type id or string id, calls a static helper, and then tries to clean up whatever inconsistent state it finds. That pattern came from legacy C data tables and compatibility bridges. It is costly, fragile, and spreads responsibility across the whole codebase.

The preferred shape is:

- Objects create valid runtime state when they are created.
- Save/load creates objects from records once. One saved building record should either hydrate one valid `Building` object and its module state or be discarded with a logged error.
- Objects register themselves with the runtime lists they belong to.
- Objects deregister themselves when they are destroyed, replaced, retargeted, or disabled.
- Objects keep their own data private where practical and expose behavior through narrow methods.
- Callers receive object references or pointers that already represent the intended relationship.
- Save/load bridges translate ids into object relationships once, then normal runtime code uses those relationships directly.
- Save-record structs are compatibility input/output at the bridge. They should not remain the runtime object model once a module has been peeled into object state.

## What To Replace

Avoid adding more of this style:

```cpp
for (Building b = first_building_with_attr("warehouse"); b.id(); b = b.next_of_type()) {
    if (!building_type_matches(b, "some_runtime_case")) {
        continue;
    }
    static_helper_that_repairs_or_falls_back(b);
}
```

Avoid repeated cleanup polling like:

```cpp
if (some_downstream_system_notices_stale_state()) {
    repair_everything_here();
}
```

This is acceptable only as a temporary safety net while the true owner is being migrated. If a cart pusher reserves storage space, the cart pusher should release that reservation when it retargets, delivers, dies, or is removed. Storage should not need to poll every frame to discover that a figure vanished. If storage still has to defend itself, that defense should be treated as technical debt and localized to the storage object, not copied into industry, warehouse, cart, and UI code.

## Runtime Lists

Create runtime lists of typed object pointers for hot paths.

Examples:

- Buildings with output storage.
- Buildings with input storage for a given resource.
- Buildings with local labor demand.
- Buildings with active production methods.
- Figures with active reservations.
- Figures in a specific behavior class.
- Buildings of a specific subtype such as `Barracks`, `Temple`, `Granary`, `Warehouse`, or `HippodromePart`.

Registration should happen when the object enters the relevant state. Deregistration should happen when it leaves that state.

The target is not:

```cpp
scan_all_buildings();
if (building.type == X || building_has_attr(building, "x")) { ... }
```

The target is:

```cpp
for (Building *building : city_runtime.output_storage_buildings()) {
    building->maybe_spawn_output_cart();
}
```

Or, when subtypes matter:

```cpp
for (Warehouse *warehouse : city_runtime.warehouses_accepting(resource)) {
    warehouse->consider_delivery(resource);
}
```

The registry owns the list. The object owns registration into that list. Callers should not recreate membership with ad hoc scans.

## Indexed Building Registry

The existing `Building::first_of_type()` lists are the starting point, but the target registry should grow into a set of live object indexes for categories the simulation asks about constantly. A caller looking for a house, warehouse, granary, dock, storage source, service destination, or output producer should iterate the narrow owner-maintained list for that category, not every building in the city.

Useful indexes include:

- exact runtime type lists, already represented by `first_of_type`;
- storage buildings by role and resource, especially granaries and warehouses;
- houses and local workforce providers/demanders;
- buildings with active production output;
- buildings with active input demand;
- road-access endpoints and path-blocker providers;
- military, formation, and venue-provider buildings.

These indexes should be maintained by lifecycle events. Creation registers a building into the indexes implied by its type/modules. Destruction, deletion, type replacement, mothball state changes, storage policy changes, or resource acceptance changes remove or move it. Ordered id maps or dense id-keyed containers are acceptable when stable iteration by building id matters.

This depends on stricter lifecycle boundaries: buildings should be added only through the runtime instantiation path and removed only through the removal path that updates indexes, relationships, reservations, route destinations, and graphics state. Direct record mutation becomes increasingly dangerous once indexes are authoritative.

The practical rule for new cleanup is: if a file needs "the first warehouse", "all granaries", or "all caravanserais", expose that from the owning module or registry. Do not reimplement a local full-building scan.

## Ownership And Cleanup

Cleanup belongs at the lifecycle event that invalidates the relationship.

Examples:

- A figure retargeting from one destination to another releases reservations held against the old destination.
- A figure being killed releases figure-owned reservations, route claims, or target links.
- A building changing type deregisters from old type lists and registers with new type lists.
- A building being destroyed releases storage reservations, figure ownership, labor demand, production membership, and graphics/connectable memberships.
- A save/load bridge hydrates runtime pointers and runtime lists once after records are loaded.
- A save/load bridge rejects bad saved records at the boundary, instead of letting incomplete records survive as runtime objects.

Do not make every consumer rediscover and repair these cases independently.

## Data Privacy Direction

Legacy structs are still present for save compatibility, but runtime code should not treat them as the public API.

New or migrated code should prefer:

```cpp
building.output_storage().available_loads(resource);
building.production().is_blocked();
figure.release_reservations();
```

Over:

```cpp
building_record->resources[resource]
figure_record->destination_building_id
static_cleanup_for_possible_bad_record_state(...)
```

Public fields are acceptable when the operation is plain state access or assignment. Do not create or keep accessors whose whole behavior is equivalent to:

```cpp
return value;
```

or:

```cpp
value = other_value;
```

That kind of accessor is just a compatibility wrapper and should be deleted as callers move to the owning object. Keep data private only when reads or writes must enforce an invariant, synchronize related state, register/deregister from runtime indexes, release ownership, clamp/normalize values, convert save ids into object references, update dirty flags, or perform any behavior beyond simple assignment. In those cases the method should own that behavior directly and be named for the behavior, not for the field plumbing.

This rule also applies during record-to-object migration. Moving fields under `private` is useful as a temporary compiler aid, but the final split should be deliberate: simple state becomes public object data, invariant-bearing state remains private behind behavior methods.

## Bound Runtime Modules

The long-term module target is not loose policy calls such as:

```cpp
entertainment_definition->tick(building);
```

That shape is too easy to call from arbitrary code with the wrong owner, stale state, or the wrong definition. It recreates the same opacity as legacy helper functions, only with newer names.

Prefer owner-bound runtime modules:

```cpp
building.entertainment().tick();
```

Conceptually:

```cpp
class BuildingEntertainment {
public:
    void tick();

private:
    Building &owner_;
    const EntertainmentDef *definition_;
    EntertainmentState &state_;
};
```

`Building` remains the central routing object. `BuildingType` remains immutable startup-loaded definition data. A runtime module binds exactly one owner object, one current type/module definition pointer, and one mutable state/data object. The module is born with the owning building, dies with the owning building, and is refreshed or rebound through the building lifecycle if the building changes type.

This keeps responsibilities separated while keeping the data that belongs together packaged together:

- `*Def` / type module: immutable XML-loaded rules, policies, costs, graphics, capacities, requirements, and tags.
- `*State` / data module: mutable per-building state that must save/load and tick.
- `Building*` runtime module: behavior facade bound to one `Building`, its current `*Def`, and its current `*State`.
- `Building`: owns module construction, lifecycle, rebinding when type changes, and the public route to module behavior.

The same direction applies to figures:

```cpp
figure.trade().draw_info_panel(context);
figure.cargo().reserve_destination();
figure.route().advance();
```

Internally a module may call private helpers across several files, but callers should enter through the owning object or its bound module. That makes wrong usage look wrong: a random subsystem should not be able to casually tick an entertainment definition against any building record.

Type pointers inside bound modules should remain pointers rather than references where the owning object can change type. The definition object itself is immutable; the binding may change to a different immutable definition during upgrade, evolution, conversion, or compatibility migration.

### Graphics Modules

Graphics should be one of the first runtime-module extractions to follow this pattern.

The long-term renderer should not read legacy image groups, legacy group enums, or legacy atlas arithmetic. Those are compatibility bridges only. Authored graphics should be XML-defined by path plus graphics-module policy, and runtime drawing should enter through owner-bound modules:

```cpp
building.graphics().draw();
figure.graphics().draw();
```

Conceptually:

```cpp
class BuildingGraphics {
public:
    void tick();
    void draw();

private:
    Building &owner_;
    const BuildingGraphicsDef *definition_;
    BuildingGraphicsState &state_;
};
```

The existing definition classes already provide most of the immutable side of this split. `GraphicsDefinition` and its current children (`BuildingGraphics`, `FigureGraphics`, and `ResourceGraphics`) should be treated as definition classes awaiting convention-aligned names, not as the final runtime module names. The intended naming direction is:

- `GraphicsDefinition` -> shared graphics definition base.
- current `BuildingGraphics` -> `BuildingGraphicsDef`.
- current `FigureGraphics` -> `FigureGraphicsDef`.
- current `ResourceGraphics` -> `ResourceGraphicsDef`.
- future owner-bound `BuildingGraphics`, `FigureGraphics`, terrain graphics, and tile graphics classes bind owner, `*GraphicsDef`, and mutable `*GraphicsState`.

Every major class in this split should live in a same-named header/source pair. For example, `BuildingGraphics` belongs in `BuildingGraphics.h/.cpp`; `BuildingGraphicsDef` belongs in `BuildingGraphicsDef.h/.cpp`; and the same convention should hold for composed graphics data modules, state modules, and terrain/tile graphics owners.

The composed graphics data module should be instantiated from graphics XML nodes across buildings, figures, terrain, and tiles, so shared graphics node parsing and policy resolution can replace legacy group-specific draw branches instead of wrapping them.

## Single Object Surfaces

When a runtime concept has started migrating to C++, keep one public object surface instead of preserving parallel legacy and object APIs.

For buildings:

- `src/building/building.h` is the public `Building` class header.
- `src/building/building_object.h` was a compatibility split and should not be recreated.
- C++ files that need building behavior should include `building/building.h` and pass `Building` objects or references.
- `building/building_fwd.h`, raw `building *`, and direct `record()` access are temporary pressure valves for narrow boundaries, not a reason to add new compatibility layers.
- Do not add overloads, wrappers, bridge APIs, or C-linkage helpers solely to keep old raw-record callers alive. Convert the caller boundary when the touched scope can move to C++.

Rendering follows the same rule. Current city rendering still exposes `Building::draw_footprint(...)`, `Building::draw_top(...)`, and `Building::draw_animation(...)` as live migration seams, but new work should not multiply public stage-specific building draw paths. The target is one building-owned draw request or command path; the building type and its modules decide internally which footprint, top, animation, detail, or overlay slices are needed.

## Semantic Modules Over Event Attrs

`<event_data attr="...">` is a compatibility crutch, not a semantic contract. It exists to keep legacy event ids, old save bridges, and unported systems alive while the XML model grows. New runtime behavior should not compare `event_data attr` strings such as `academy`, `dock`, or `warehouse` to decide what a building is.

Building identity should come from higher-level declarations:

- `<kind>` for broad classification such as `housing`, `industry`, `service`, `storage`, `military`, `culture`, or `infrastructure`.
- Dedicated modules for real behavior, such as distribution, storage, production, labor, water access, religion, formation ownership, roadblock permissions, and construction policies.
- Typed runtime registration for systems that need to iterate a category quickly.

If a system still needs `event_data attr` to select behavior, treat that as evidence that the system is missing a module, policy, typed list, or object method. Do not promote those attrs into another layer of string-matched building logic. The migration sequence should be:

1. Identify the behavior hidden behind the attr comparison.
2. Add or extend the correct XML-owned module for that behavior.
3. Resolve the module once at load/startup into typed runtime data.
4. Change callers to ask the object/module for behavior.
5. Leave `event_data attr` only for legacy event/save compatibility until that compatibility path can be removed.

## Save/Load Boundary

Ids are bridge data, not runtime ownership.

Allowed:

- Save writes stable ids.
- Load reads stable ids.
- Load resolves ids into pointers/references and typed runtime lists.
- Debug logs print ids.

During the record-to-object migration, public field syntax is acceptable only when it is truly object-owned or safely record-backed. `Building.id`, `Building.storage_id`, and similar transitional fields should not be detached mirrors that can drift away from the save-backed record. Until save/load serializes live object state directly, assignment-capable migration fields must either write through to the record/module state immediately or remain private behind a behavior method.

Disallowed for normal runtime:

- Repeated lookup by id in draw, tick, labor, production, storage, or figure action hot paths.
- String id lookup in runtime behavior.
- Static helpers that rebuild object relationships every frame because ownership was not represented.

## Performance Doctrine

The performance win is not only fewer instructions. It is fewer responsibilities per function.

Polling and fallback-heavy code has three costs:

- CPU cost from scanning and repeated lookups.
- Debugging cost because many functions can silently repair or mutate the same state.
- Design cost because the real owner of a relationship becomes unclear.

When choosing between adding a defensive fallback and moving ownership to the correct object, prefer moving ownership. If a temporary fallback is necessary to keep the game running, keep it narrow, document it, and remove it when the owner lifecycle is migrated.

## Quality Bar

Refactor slices should generally reduce redundancy and line count.

Good migration signs:

- Fewer static helper branches.
- Fewer type/string comparisons in hot paths.
- Fewer duplicate C and C++ entry points.
- More direct object calls.
- Runtime lists replace repeated full-city scans.
- Cleanup is attached to create/destroy/retarget events.
- XML/data declarations define what exists; code does not fallback to hardcoded ghosts of deleted data.

Bad migration signs:

- New compatibility wrappers next to old wrappers.
- More fallback layers that hide invalid data.
- Every subsystem adds its own stale-state repair pass.
- Runtime behavior still starts from ids or text ids and resolves objects repeatedly.

## Practical Rule

When a bug appears because an object did not clean up after itself, fix the owner lifecycle first. Defensive cleanup in downstream systems is only a temporary guardrail, not the architecture.
