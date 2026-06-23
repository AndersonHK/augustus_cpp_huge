# Object-Owned Runtime Refactor Doctrine

This document records the refactoring direction for Vespasian runtime systems. It applies especially to buildings, figures, storage, graphics, production, labor, and other hot simulation paths.

## Core Principle

Runtime objects should own their relationships and invariants.

The long-term goal is to stop writing code where every tick scans every building or figure, checks a type id or string id, calls a static helper, and then tries to clean up whatever inconsistent state it finds. That pattern came from legacy C data tables and compatibility bridges. It is costly, fragile, and spreads responsibility across the whole codebase.

The preferred shape is:

- Objects create valid runtime state when they are created.
- Objects register themselves with the runtime lists they belong to.
- Objects deregister themselves when they are destroyed, replaced, retargeted, or disabled.
- Objects keep their own data private where practical and expose behavior through narrow methods.
- Callers receive object references or pointers that already represent the intended relationship.
- Save/load bridges translate ids into object relationships once, then normal runtime code uses those relationships directly.

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

## Ownership And Cleanup

Cleanup belongs at the lifecycle event that invalidates the relationship.

Examples:

- A figure retargeting from one destination to another releases reservations held against the old destination.
- A figure being killed releases figure-owned reservations, route claims, or target links.
- A building changing type deregisters from old type lists and registers with new type lists.
- A building being destroyed releases storage reservations, figure ownership, labor demand, production membership, and graphics/connectable memberships.
- A save/load bridge hydrates runtime pointers and runtime lists once after records are loaded.

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

Public fields may remain temporarily during migration, but the direction is toward private data with explicit methods that preserve invariants.

## Save/Load Boundary

Ids are bridge data, not runtime ownership.

Allowed:

- Save writes stable ids.
- Load reads stable ids.
- Load resolves ids into pointers/references and typed runtime lists.
- Debug logs print ids.

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
