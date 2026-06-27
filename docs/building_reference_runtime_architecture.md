# Building Reference Runtime Architecture

This document is the canonical target for migrating runtime building relationships away from numeric building ids and `Building::from_id`. The goal is not to cache ids more carefully. The goal is to make the runtime object graph express actual object relationships.

For the broader refactor doctrine around object-owned registration, deregistration, and lifecycle cleanup, read [Object-Owned Runtime Refactor Doctrine](object_owned_runtime_refactor.md) alongside this file. Building ids, static scans, and defensive cleanup fallbacks are all symptoms of the same legacy ownership problem.

## Target State

Runtime code works with `Building&` for required building parameters and cached pointers for retargetable runtime relationships.

If a code path is building-specific, it receives a valid building reference before it starts. It does not enter the path with an id, a nullable pseudo-building, or a "maybe no building" object and then branch repeatedly on whether the building exists.

Absence is handled by control flow at the boundary:

```cpp
if (!map_has_building(grid_offset)) {
    draw_terrain_tile(grid_offset, x, y);
    return;
}

Building &building = map_building_at(grid_offset);
draw_building_tile(building, grid_offset, x, y);
```

Building-specific functions should look like this:

```cpp
void draw_building_tile(Building &building, int grid_offset, int x, int y);
void send_cart_to_storage(Figure &cart, Building &destination);
void link_building_parts(Building &first, Building &second);
```

Object fields that model runtime relationships should usually be pointers, not C++ references:

```cpp
class Building {
public:
    BuildingType *type = nullptr;
};

class Figure {
public:
    Building *building = nullptr;
    Building *destination_building = nullptr;
    Figure *target_figure = nullptr;
};
```

The reason is simple: many runtime relationships can be cleared or retargeted. C++ references cannot be reseated after construction. Pointers can be cached once and retargeted cheaply without returning to id lookups.

Use references for function parameters when the caller has already proven presence:

```cpp
void update_building(Building &building);
void draw_building(const Building &building);
```

They should not look like this:

```cpp
void draw_building_tile(int building_id, int grid_offset, int x, int y);
void send_cart_to_storage(Figure &cart, int destination_building_id);
void link_building_parts(unsigned int first_id, unsigned int second_id);
```

Nor like this:

```cpp
void draw_building_tile(Building building)
{
    if (!building.id()) {
        draw_something_else();
    }
}
```

If there is no building, the whole building-specific operation should be skipped or routed into a terrain/no-target path before a `Building&` is requested.

## Building Object Header Boundary

`src/building/building.h` is the single public C++ header for the `Building` object. Do not reintroduce `src/building/building_object.h` or split the class declaration into a compatibility header. Files that need building behavior should become C++ callers and include `building/building.h` directly.

`building/building_fwd.h` is useful only when a header truly needs a forward declaration. It is not a substitute object API, and it should not be used to justify new raw-record wrappers. If a touched function exists only to translate an id, `building *`, or compatibility record into the object path, prefer deleting that function and updating callers to pass `Building&`, `Building *`, or the owning typed runtime object.

The same boundary applies to building rendering. The current implementation still has public footprint/top/animation methods because the city renderer is not fully command-list/object-owned yet. Treat those as existing migration seams, not as the pattern for new systems. The long-term public shape is a single building-owned draw request/command path, with stage decisions contained by `Building`, `BuildingType`, and graphics modules.

## Runtime And Save Boundary

Numeric building ids are allowed only as persistence, debug, or compatibility transport. They are not runtime ownership.

Allowed id uses:

- Save files serializing a building reference.
- Load code resolving serialized ids into runtime references.
- Debug logs that print a stable diagnostic id.
- Temporary migration adapters, clearly marked and removed as their callers are converted.

Disallowed id uses:

- Runtime fields such as owner, destination, selected building, hovered building, source storage, destination storage, previous part, next part, immigrant target, home fort, or figure origin.
- Hot draw code resolving a building from an id every frame.
- APIs that accept an id only to immediately call `Building::from_id` or `building_get`.
- Logic that passes `0` as "no building" into a building-specific function.

The save/load path should be explicit:

1. Save writes ids for building references.
2. Load clears the old runtime graph.
3. Load creates the new building objects.
4. Load hydrates all saved building references.
5. Normal runtime resumes with references, not ids.

After load finishes, ordinary gameplay/rendering/window/figure code should not need to know that serialized building ids existed.

## Pointer Storage

Required relationships should be expressed as `Building&` at the API surface. Stored relationships should be pointers when they can change, be cleared, or be hydrated after load.

Preferred field shape:

```cpp
Building *owner = nullptr;
Building *destination = nullptr;
BuildingType *type = nullptr;
```

This is not a return to id-based runtime. The pointer is the cached runtime relationship. It should not lazily resolve an id in a getter.

The important rule is what this type does not expose. It should not become an id-handle API:

```cpp
if (relation.has_value()) { ... }
if (relation.id() == 0) { ... }
Building *maybe = Building::from_id(id);
```

If the section of logic can run without a building, guard it before entering that section. Once inside building-specific logic, dereference the pointer once and pass `Building&` onward.

The `BuildingType` relationship should also be cached as a pointer on `Building`. Building types are stable after startup, so runtime code should not repeatedly look them up by string or id. If a building changes type through a controlled runtime operation, that operation must update the cached `BuildingType *`.

When code needs to know what type a building is, it can compare against the held type directly:

```cpp
if (building.type == &warehouse_type) {
    // warehouse behavior
}
```

If the logic needs behavior, ask the type object:

```cpp
if (building.type->is_warehouse()) {
    // warehouse behavior
}
```

Do not add a new helper for every comparison. Prefer the existing object, field, or method unless the repeated logic is genuinely domain behavior.

## XML Subtypes And Typed Runtime Lists

Building subtypes should be declared by XML and materialized by the registry as typed runtime lists.

The goal is to replace broad city scans that look like this:

```cpp
for (Building building = first_building_with_attr("barracks"); building.id(); building = building.next_of_type()) {
    // barracks work
}
```

or like this:

```cpp
for (Building building : all_buildings) {
    if (!building.type->has_attribute("barracks")) {
        continue;
    }
    // barracks work
}
```

with typed selection over only the buildings that can run that logic:

```cpp
for (int index = 0; index < registry.subtype_count<Barracks>(); ++index) {
    Barracks &barracks = registry.pick_from_subtype<Barracks>(index);
    barracks.run_tick();
}
```

The caller asks for a class, not a string. The registry returns an object of that class, not a generic `Building` that must be classified again. The API should be templated or otherwise statically typed:

```cpp
template <typename BuildingSubclass>
BuildingSubclass &pick_from_subtype(int index);

template <typename FigureSubclass>
FigureSubclass &pick_figure_from_subtype(int index);
```

The same rule applies to figure-type code: runtime code should iterate typed `Figure` subclasses from their registry lists rather than scanning every figure and testing ids or attributes after the fact.

Subtype membership is a registry/runtime responsibility:

- XML declares which concrete subclass a building or figure type belongs to.
- Startup resolves XML declarations into immutable type definitions once; save/load hydrates live instances against those already-resolved definitions.
- Creating, deleting, hydrating, or type-changing a building instance updates the subtype lists.
- Runtime loops iterate the already-maintained typed list.
- Save/load still serializes ids as bridge data, then hydrates typed runtime lists before normal gameplay resumes.

This should make `next_of_type()` and `first_building_with_attr(...)` unnecessary in normal runtime code. If a loop wants barracks, theaters, temples, warehouses, farms, traders, or any other concrete runtime class, it should iterate that class list directly. This is both clearer and friendlier to performance work: it removes repeated full-map scans, string/attribute checks, and late behavior selection.

## Cleanup And Lifetime

Buildings participate in a dense object graph: tiles point to buildings, figures point to origins and destinations, storage orders point to source/destination buildings, UI state points to selected/hovered buildings, and multi-part buildings point to neighbors.

The owner of a building's lifetime must cleanly detach that building from every runtime object that references it.

The preferred destruction path is explicit and graph-aware:

```cpp
city.destroy_building(building);
```

That operation should:

- Detach the building from map tiles.
- Clear or reroute figures that reference it.
- Remove storage/distribution/depot/market references.
- Detach previous/next part links.
- Clear UI selections, hover state, tooltips, and building windows that reference it.
- Remove overlay/render/runtime caches that hold building references.
- Then destroy the building.

The `Building` destructor is still useful as a safety net and invariant checker, but dense graph cleanup should not rely on surprising destructor side effects alone. Destruction should be visible and ordered because related objects may need to make decisions while the building is still fully inspectable.

Assertions are appropriate in destructors and pointer cleanup paths:

- A destroyed building should not remain in map tiles.
- A destroyed building should not remain selected or hovered.
- A destroyed building should not remain a live figure destination.
- A building-specific function should assert or fail loudly if it is handed a null pointer after the caller claimed presence.

## Raw `struct building` Access

Runtime code should not work directly through the saved `struct building` record.

The raw record is a storage implementation detail. Normal gameplay, figure, UI, render, and simulation code should receive and pass `Building` objects. A good migration changes the public entry point itself, as in replacing a raw `building *` migrant API with a `Building&` migrant API and updating the callers, rather than creating a C-compatible duplicate beside it.

Normal code should ask `Building` for behavior or data:

```cpp
building.worker_count();
building.type->required_workers();
building.grid_offset();
building.is_deleted();
building.storage();
```

not:

```cpp
building_get(id)->num_workers;
building_get(id)->type;
building_get(id)->grid_offset;
building_get(id)->is_deleted;
```

Direct raw-record access is permitted in:

- Save/load serialization.
- The narrow internals that build and own the `Building` objects.
- Temporary migration adapters with clear removal scope.

Every direct `building_get(...)`, raw `building *`, or raw `building` field access outside those places should be treated as a migration smell. If converting a file makes the code longer, harder to read, or split between old and new APIs, the migration is probably preserving the wrong boundary. The preferred result is fewer lines, fewer pass-through helpers, and one object-oriented path.

## Performance Rationale

The current id-based model creates avoidable work in hot paths:

- Draw code repeatedly calls `map_building_at`, `Building::from_id`, `building_get`, and then type/definition checks for the same visible tile.
- Figure and storage code frequently converts owner/destination ids back into objects before doing actual work.
- Repeated `id == 0` checks split building logic into defensive branches that should have been skipped at a higher level.
- The same runtime relationship is rediscovered many times instead of being held directly.

The reference model improves performance by making the common path direct:

- Building-specific functions operate on `Building&` immediately.
- The branch for "no building" happens once at the boundary.
- Draw row payloads carry building references for occupied tiles rather than rehydrating ids per phase.
- Figure and storage updates avoid repeated array lookups for owner/destination relationships.
- Building type behavior uses the cached `BuildingType *` instead of resolving type ids or strings in hot paths.
- Subtype-specific loops iterate maintained typed lists instead of scanning every building or figure and filtering by type/attribute afterward.
- Native/runtime graphics invalidation can belong to the building object, reducing per-frame signature scans.

This change will not by itself remove all draw-call overhead. It is still foundational because command lists, render caches, and dirty-state systems work better when they are fed stable object references rather than ids that must be resolved on every pass.

Performance caution: pointers are not a license to hide repeated work inside relation getters. A relation dereference should be cheap and should not do a lookup by id. If a getter resolves an id through `building_get`, the migration has only moved the old cost behind a prettier name.

Pointer dereference and reference access are effectively the same cost. Both are dramatically cheaper than string matching, map lookups, or repeated id-to-object rehydration. The goal is not pointer cleverness; the goal is to make the hot path already have the object it needs.

## Deduplication Goal

This migration is also a line-count and vocabulary cleanup pass.

Do not preserve parallel versions of the same logic:

- C API plus C++ API that do the same thing.
- `Building` accessor plus `BuildingType` accessor plus free function for the same fact.
- Local helper that only wraps `building_get(id)` or `building.type->...`.
- String/id classifier helpers when the held `BuildingType *` can be compared or asked directly.
- Full-map scans that immediately discard most buildings by type, attribute, or string id.
- Compatibility overloads that keep raw `building *`, `building_type`, or id callers alive after the owning phase can pass objects.

Prefer deleting the old path and updating callers. If the answer is already on `Building`, use it. If the answer is type behavior, use `building.type->...`. If all the code needs is identity, compare the held type pointer. Add a helper only when it removes real duplicated domain behavior, not when it hides an unfinished migration.

## Reliability Rationale

Ids make invalid state easy to ignore.

Common id-model failure modes:

- `0` is passed deeper than intended and triggers fallback behavior in the wrong layer.
- A stale id points to a reused slot after deletion/load.
- Save/load migration forgets to update one consumer of a building id.
- UI or figure state holds an id after the building has been destroyed.
- Code accidentally mixes main-building ids, part ids, rubble ids, storage ids, and building ids.

Reference-based runtime design makes these failures sharper and earlier:

- A building path cannot be entered without a building reference.
- A missing building must be handled as a branch in the caller's control flow.
- Destroying a building has a defined cleanup path for all references to it.
- Retargeting is explicit.
- Save/load is the only place where id translation is expected.

This is stricter, and that is the point. The migration should prefer compile errors and loud assertions over compatibility shims that preserve ambiguous id behavior.

## Migration Order

This should be compiler-driven. The first real change should be replacing id fields and id-returning APIs with building references, then following the compiler errors outward until all callers are object-first.

1. Replace runtime building ids with pointers/references.
   Start with the core runtime fields that mean "this object refers to that building": figure owner/destination, map tile occupancy, selected/hovered building state, building part links, storage owner, depot order source/destination, and building-info context.

2. Add boundary guards.
   Where code may legitimately have no building, split the no-building path before requesting a `Building&`. Use helpers like `map_has_building(grid_offset)` and `tile_has_building(tile)` for control flow. Do not pass a nullable object into building logic.

3. Convert map/grid APIs.
   `map_building_at` should return `Building&` after presence has been proven. If a caller has not proven presence, it should call a predicate first. Runtime grid storage may use pointers internally, but the public gameplay API should pass `Building&` once presence is known.

4. Convert figure relationships.
   `figure::building_id`, `figure::destination_building_id`, `figure::immigrant_building_id`, `last_destinatation_id` when it means a building, formation home fort, formation destination, and visited-building state should become `Building *` fields or graph-managed relation slots. Figure-to-figure targets should likewise become `Figure *` fields where they can be cleared or retargeted.

5. Convert building relationships.
   `prev_part_building_id`, `next_part_building_id`, rubble-origin references, reconstruction references, storage owner references, and runtime building lists should become pointers or graph-managed relation slots.

6. Cache building type pointers.
   `Building` should hold its `BuildingType *` after startup/load/type-change. Runtime behavior should not call string-to-id helpers or id classifier helpers to rediscover the type. Type-changing operations must update the pointer.

7. Add XML-declared subtype lists.
   XML should declare concrete building and figure subclasses. The registry should maintain typed lists for those subclasses and expose typed selection such as `pick_from_subtype<Barracks>(index)`. Convert broad loops that scan all buildings or walk `next_of_type()` chains into typed-list loops where the loop's intent is a concrete subtype.

8. Convert storage, orders, and distribution.
   `building_storage::building_id`, fetch-inventory destinations, depot order source/destination, market/mess hall/tavern/temple/lighthouse/caravanserai destination APIs, and warehouse/granary minimum-selection helpers should return or accept `Building&`.

9. Convert UI and window contexts.
   Selected building, hovered building, current building-info context, depot windows, distribution windows, overlay tooltip context, and building-specific buttons should carry references through the active window lifetime. If the building is destroyed, the destroy path closes or clears the relevant UI state.

10. Convert render paths.
   City draw row traversal should produce tile/building context once. Building draw functions should accept `Building&`. Preserve the existing isometric row phase ordering while changing the carried data.

11. Remove `Building::from_id`.
   Once normal runtime code has no legitimate callers, make id hydration private to save/load or delete the helper entirely. Any remaining use should be treated as a blocker unless it is in a documented serialization adapter.

12. Remove direct raw-record runtime access.
    After object references are flowing, replace direct `building_get(...)` field reads with `Building` methods. Keep raw record access only in save/load and object-table internals.

13. Remove `next_of_type()` and attribute-scan runtime loops.
    Once subtype lists exist, delete normal-runtime loops whose shape is "iterate many buildings, then check whether this one is the desired subtype." Those loops should ask the registry for the typed list and operate on the returned subclass.

14. Delete duplicate accessors and bridges.
    Once callers use `Building`, `BuildingType`, and cached pointers directly, delete redundant free functions, C wrappers, type-id classifiers, and pass-through accessors.

## Draw-Order Constraint

Do not combine this migration with draw-order experiments.

The current city draw row system depends on phase order across an entire visible row. A migration may change the data passed to callbacks, but it must not change the ordering shape from:

```cpp
for each row:
    callback1(all tiles in row)
    callback2(all tiles in row)
    callback3(all tiles in row)
```

to:

```cpp
for each row:
    for each tile:
        callback1(tile)
        callback2(tile)
        callback3(tile)
```

The latter is a behavioral change and can break isometric layering. Preserve row phase order first; optimize or batch only after visual correctness is proven.

## Agent Rules

When migrating a file:

- Prefer changing the function signature upstream over converting an id locally.
- If a function starts by calling `Building::from_id`, ask why it was not passed `Building&`.
- If a function receives `building_type` only to classify behavior, ask why it was not passed `Building&` or `BuildingType&`.
- If a loop scans every building only to check type, attribute, or string id, ask why it is not iterating an XML-declared typed subtype list.
- If code checks `id == 0` inside building-specific logic, move that branch to the caller.
- If a struct stores a building id for runtime use, replace that field with a pointer/reference before cleaning up call sites.
- If a save/load function stores ids, keep it as a boundary and document the hydration step.
- If raw `building *` access is needed temporarily, keep it narrow and do not expose it through public gameplay APIs.
- Do not add compatibility aliases that preserve old id semantics under a new name.
- Do not make a nullable building wrapper the default answer to absence.
- Do not add pass-through helpers when a direct `building.type->...` call or pointer comparison is clearer.
- Do not keep both old and new functions doing the same job.
- Do not replace a type-filtered full-map scan with another type-filtered full-map scan under a nicer name.
- Do not change isometric draw ordering while converting data flow.

The migration is complete only when ordinary runtime code no longer needs to ask "what building id is this?" before doing building work. It should already have the building.
