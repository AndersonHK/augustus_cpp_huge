# Mod-Owned Compatibility Bridge Plan

## Intent

Move legacy id translation out of hardcoded runtime/save code and into mod-owned compatibility declarations. Runtime should deal in objects and string ids; save/load and migration bridges should translate old numeric ids into loaded definitions through explicit tables.

This is low priority until the current refactors are stable, but it is important for long-term mod portability and for future ports of Pharaoh, Poseidon, and Emperor as data mods.

Root [`mod.xml`](mod_metadata.md) manifests now establish each mod's identity and dependencies. The migration ledger will be owned through that identity, but its element and file names remain deliberately undecided.

## Target Shape

- Julius owns Caesar 3 legacy ids and their canonical string ids.
- Augustus owns its own legacy ids and string ids.
- Vespasian owns string ids for new content and optional migration declarations for removed or merged content.
- Other mods own their own building, figure, and terrain migration declarations rather than extending a global Vespasian table.
- Startup loads XML definitions, assigns stable numeric runtime ids for the current mod stack, and publishes lookup tables.
- Save/load bridge code converts old numeric ids to string ids, then resolves those strings through loaded definitions.
- Runtime never branches on the old numeric ids once objects are instantiated.

## Example Uses

- A resource bridge could declare that old `wine` becomes `beer`, or `grain` becomes `rice`.
- A building bridge could declare that several old pavilion building ids migrate into one Vespasian `pavilion` type with graphical variants.
- A mod could map the string id `tavern` to `elven_tavern`, or map several overgrown-garden ids to one ordinary `garden` definition.
- Any bridge that translates a legacy enum or raw save id should store the numeric id as data. The old enum name is allowed only as an XML comment for human readers.
- A formation bridge could map vanilla numeric formation ids to `FormationType` string ids.
- A future Pharaoh/Poseidon/Emperor mod could declare how legacy buildings, figures, resources, gods, units, overlays, and save records translate into the shared runtime object model.

## Boundary Rules

- Compatibility bridge XML belongs to startup/save-load boundaries, not normal runtime.
- Bridge data should be strict and loud: missing target string ids fail migration with useful context.
- Runtime receives fully instantiated `Building`, `Figure`, `Formation`, `Resource`, and related objects.
- Save/load bridge code may know old records and numeric ids; runtime should not.
- Definitions are static after startup. Bridge tables should not recreate or mutate definitions during gameplay.

## Illustrative Data Shape (Not Yet a Schema)

```xml
<compatibility_bridge source="augustus">
  <building from_id="123" to="pavilion"> <!-- BUILDING_PAVILION_BLUE -->
    <variant key="blue" />
  </building>
  <resource from_id="9" to="beer" /> <!-- RESOURCE_WINE -->
  <formation from_id="0" to="legacy_legion_4x4" /> <!-- FORMATION_COLUMN -->
</compatibility_bridge>
```

The example above records the intended information only; it does not reserve any element or attribute name. The exact schema and ledger name should wait until the save/load DLL boundary is clearer. The important contract is that legacy enum conversions use numeric ids as the authoritative value, while comments provide the old names. String-to-string migrations are still valid for already string-owned mod ids, but they should not be confused with legacy enum bridges.

Each source key resolves to zero or one target string id. Any number of distinct source keys may converge on that target, but a source key cannot fan out to several targets. Zero-target declarations need explicit, category-specific safety semantics before implementation.

## Suggested Slices

1. Inventory current hardcoded legacy-id bridges for buildings, resources, figures, formations, overlays, and text/image references.
2. Add a read-only startup registry for bridge declarations, but keep existing hardcoded bridges authoritative.
3. Move one low-risk bridge table, such as formation enum to `FormationType`, into XML with strict startup validation.
4. Move building/resource migration bridges into XML after current BuildingType and Resource runtime ids are fully string-owned.
5. Move save-load bridge code into its DLL boundary and make bridge XML one of its explicit startup inputs.
6. Remove hardcoded compatibility tables once XML parity and old-save migration tests pass.
