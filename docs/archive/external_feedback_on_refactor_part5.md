# C++ BuildingType Design Note

Context: `BuildingType` is a runtime-loaded data definition, and `Building` is the runtime instance. The current design uses subclasses such as `HousingBuildingType`, `IndustryBuildingType`, and `TempleBuildingType`, with some `dynamic_cast` dispatch. That has started to create hierarchy pressure because many variations are cross-cutting rather than neatly taxonomic.

My opinion: the inheritance design is understandable, but it is probably the wrong pressure valve for this problem. I would keep `BuildingType` as the generic anchor required by existing code, but gradually reduce subclass responsibility. The more scalable split is not necessarily “one subclass per building family,” but “generic type record plus smaller attached definition/policy records.”

For example, housing, religion, production, workforce, graphics, popup behavior, overlays, and spawn behavior are different axes. Some are simulation definitions; some are runtime state; some are UI/presentation policy. Treating all of them as reasons to subclass `BuildingType` risks turning the hierarchy into a combinatorial taxonomy.

A lighter alternative is:

- `BuildingType` remains the stable generic type handle.
- Simulation facts live in child definition records: `HousingDef`, `ProducerDef`, `ReligionDef`, `WorkforceDef`, etc.
- Mutable instance data lives on `Building` or child state records: `HousingState`, `ProducerState`, etc.
- External variations such as contextual popups, overlays, previews, advisor text, and problem icons are dispatched through subsystem policy IDs or registries, not through `BuildingType` subclasses.

So instead of asking:

```cpp
if (auto* h = dynamic_cast<const HousingBuildingType*>(&type)) {
    ...
}
```

newer code would increasingly ask:

```cpp
if (const auto* housing = type.housing()) {
    ...
}
```

or for UI:

```cpp
PopupSystem::render(building, type.popupPolicy());
```

This does not require a full rewrite. Existing subclasses and compatibility accessors can remain while responsibilities are moved out one cluster at a time. Contextual popups might be a good first extraction target because they are high-variation but relatively isolated from core simulation correctness.

The main design question I would ask Codex to evaluate is not “how do we create runtime classes in C++?” but:

> Which current `BuildingType` subclass responsibilities are true type-definition data, which are runtime state, and which are external subsystem policies that should move out of the class hierarchy?

My bias would be to preserve the current generic `BuildingType`/`Building` pairing, but reduce inheritance-based dispatch and move toward capability/policy composition where it relieves actual pain.

```