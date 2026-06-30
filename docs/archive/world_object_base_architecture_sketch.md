# World Object Base Architecture Sketch

Archived design note. This is not an adopted plan.

This note records a possible future architecture discussed during the rubble and composed-building refactor. It is intentionally archived because the idea may be useful later, but it is not yet as certain or as direction-setting as the `BuildingType` and `Building` runtime object migration.

## Problem

`Building` is currently a god-class. Much of the engine expects every map object of interest to be a `Building`, and most tools, windows, views, figures, and save/load paths know how to talk to buildings specifically.

That makes rubble awkward.

If rubble is modeled as a building module, it can reuse a lot of existing engine paths, but it remains conceptually wrong: rubble is not a working building with labor, production, storage, housing, religion, and all the rest. It is a placed world object with position, footprint, graphics, decay or repair state, and origin metadata.

If rubble is instead modeled as a fully independent top-level runtime object immediately, the blast radius is large. Every system that currently asks for `Building` would need to decide whether it wants a building, rubble, or some common base object. That implies new runtime storage, map lookup APIs, selection logic, rendering paths, tool behavior, save/load bridges, and UI handling.

## Possible Shape

One possible solution is to extract a small shared base class for placed map objects.

```cpp
class WorldObject {
public:
    int id;
    tile2i tile;
    tile2i size;

    WorldObject *main = nullptr;

    BuildingGraphics &graphics();
    const BuildingGraphics &graphics() const;
};
```

Then `Building` and `Rubble` could become separate top-level object types over the same physical/runtime core:

```cpp
class Building : public WorldObject {
public:
    const BuildingType *type;
    // labor, storage, housing, religion, production, and building modules
};

class Rubble : public WorldObject {
public:
    RubbleState state;
    const RubbleDef *definition;
};
```

The base must stay deliberately small. If `WorldObject` starts owning labor, storage, fire behavior, desirability rules, window behavior, or production policy, it merely becomes the new god-class.

## Migration Order If Adopted

If this direction is adopted later, the least disruptive order would likely be:

1. Extract the common placed-object core from `Building` internally.
2. Move only identity, position, size, footprint/main relationship, graphics access, and lifecycle hooks into that core.
3. Keep existing callers using `Building` while the core is delegated through `Building`.
4. Introduce `Rubble` as a real sibling only after shared placement, graphics, and selection behavior are already available.
5. Change APIs from `Building` to `WorldObject` only when the caller truly cares about placement, footprint, graphics, selection, clearing, or removal.
6. Keep building-specific APIs as `Building` APIs when they need labor, housing, production, storage, religion, or other building modules.

The useful litmus test would be:

- Position, size, graphics, footprint, selection, clear, or removal probably belongs on the shared placed-object surface.
- Labor, storage, housing, production, religion, and building policy should stay building-specific.
- Decay, origin, and repair should belong to rubble-specific state.

## Save And Runtime Implications

The save/load bridge would eventually translate full legacy records into:

- shared placed-object identity and footprint data,
- `Building` module state for real buildings,
- `Rubble` state for rubble objects.

Runtime code should still avoid keeping save structs as the object model. Legacy records remain import/export data at the bridge.

This architecture would also imply a future split in map lookup and iteration APIs:

```cpp
WorldObject *map_object_at(tile2i tile);
Building *map_building_at(tile2i tile);
Rubble *map_rubble_at(tile2i tile);
```

And either one owning `WorldObjectRuntime` with typed views, or separate runtimes coordinated by a shared placement/indexing layer.

## Current Status

This is only an archived possibility. The immediate rubble work should not assume this base class exists unless the architecture is deliberately revived.

The main reason to keep this note is to preserve the distinction between:

- rubble as a fake building for compatibility,
- rubble as a building module for a staged migration,
- rubble as a real placed world object in a broader future architecture.

