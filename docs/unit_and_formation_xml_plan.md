# Unit And Formation XML Plan

## Intent

Move military unit and formation structure into mod data so forts, soldiers, and battle formations are no longer governed by hardcoded 4x4 assumptions. Vespasian's target formation size should be 8x8, for 64 soldiers, while Julius and Augustus can preserve their current 4x4 behavior through their own XML.

The visual/runtime constraint is the fort mustering ground. It is currently sized around the legacy 4x4 soldier layout, so larger formations need adaptive slot spacing until Vespasian's figure logical sizes shrink. The planned half-size figure XML overrides are therefore a practical prerequisite for full 8x8 formations that look correct without expanding the mustering-ground footprint.

The model should follow the same direction as native buildings: data declares the semantic object, runtime code resolves it into typed definitions, and gameplay asks the owning object what it contains instead of branching on a legacy building or figure id.

## New Data Folders

Add two new mod folders:

- `Mods/<mod>/UnitType`
- `Mods/<mod>/FormationType`

`UnitType` defines combat-capable unit archetypes. These are figure-backed units with stats, abilities, graphics references, and movement/pathing policy. The scope is all combat actors, not only Roman fort soldiers: legionaries, javelin soldiers, cavalry, enemy barbarians, native fighters, wolves/animals, and future special combat figures should all migrate through the same unit definition model.

`FormationType` defines a collection of unit slots arranged into a tactical shape. A fort references one formation type and owns the units created from that formation.

## Unit Type Shape

A target unit XML could look like:

```xml
<unit key="legionary">
  <figure type="soldier" />
  <combat>
    <health value="100" />
    <attack value="12" />
    <defense value="8" />
    <morale value="70" />
    <armor value="4" />
  </combat>
  <movement pathing="soldier_land" />
  <abilities>
    <melee />
  </abilities>
  <graphics figure_type="legionary" />
</unit>
```

Ranged or special units should be expressed as abilities, not separate hardcoded figure branches:

```xml
<unit key="archer">
  <figure type="soldier" />
  <combat>
    <health value="70" />
    <attack value="6" />
    <defense value="3" />
    <morale value="60" />
  </combat>
  <movement pathing="soldier_land" />
  <abilities>
    <melee />
    <ranged range="8" projectile="arrow" cooldown="40" damage="10" />
  </abilities>
  <graphics figure_type="archer" />
</unit>
```

The combat system should consume resolved `UnitType` definitions and ability objects. Code should not ask "is this javelin soldier?" when it can ask whether the unit has a ranged ability.

## Formation Type Shape

A target formation XML could look like:

```xml
<formation key="vespasian_legion_cohort">
  <grid width="8" height="8" />
  <slot fill="all" unit="legionary" />
</formation>
```

Mixed formations should support explicit rows, rectangular regions, or slot lists:

```xml
<formation key="mixed_cohort">
  <grid width="8" height="8" />
  <slot row="0" unit="archer" />
  <slot row="1" unit="archer" />
  <slot rows="2..7" unit="legionary" />
</formation>
```

The runtime result should be a resolved list of unit slots with local formation coordinates, unit type refs, and optional role metadata. Formation code should not assume 16 slots or a square shape, even if the legacy formations do.

## Fort Ownership

Forts should declare the formation they hold:

```xml
<building type="fort_legionaries">
  <military>
    <formation key="vespasian_legion_cohort" />
  </military>
</building>
```

At runtime, a Fort object owns a pointer/link to a live `Formation` object. The `Formation` object owns or references the spawned unit figures, tracks missing/dead slots, and owns a pointer to its static `FormationType`. Runtime code should ask the `Formation` object for behavior and state instead of branching on the old formation enum.

- `capacity()`
- `alive_count()`
- `unit_at(slot)`
- `spawn_missing_units()`
- `remove_unit(figure_id)`
- `for_each_unit(...)`
- `combat_strength()`

This also creates the right place for composed state: the fort owns the formation, the formation owns the unit slots, and each figure can point back to its unit slot or formation instance when needed.

## Migration Strategy

1. Add `UnitType` and `FormationType` registries with strict XML loading.
2. Author Julius/Augustus legacy-compatible formations as 4x4 definitions.
3. Author Vespasian fort formations as 8x8 definitions.
4. Add a `military/formation` field to fort BuildingType XML.
5. Teach fort construction/loading to create a `Formation` object from the fort's resolved `FormationType`.
6. Replace hardcoded formation size loops with `Formation` object iteration.
7. Move soldier stats and ranged/melee behavior into `UnitType`.
8. Extend `UnitType` coverage to enemies, barbarians, animals, and any other combat figures instead of maintaining a Roman-only data lane.
9. Make formation slot spacing data-driven or computed from the fort's current mustering-ground footprint, then switch Vespasian to half-size figure logical dimensions before relying on dense 8x8 visuals.
10. Bridge old saves by mapping legacy fort/soldier records and vanilla formation enum values into the formation declared by the fort type.
11. Delete legacy constants that assume 16 soldiers once save migration and runtime behavior are covered.

### Current Prerequisite Status

- [x] Legacy runtime formation logic has formation-owned capacity helpers for active slot iteration, open/full checks, and overflow counts.
- [x] Capacity, roster clearing, and open-slot assignment helpers now live on the `formation` object instead of free functions, so callers ask the formation directly while the save/storage contract is still fixed.
- [x] Active figure-id iteration, reverse iteration, first active figure lookup, and active-figure predicates now live on `formation`; normal `src/figure` runtime callers no longer turn raw `figures[]` slots directly into `Figure` objects outside fixed save/layout storage.
- [x] `UnitType` and `FormationType` registries load before `BuildingType`, expose typed lookups, and fail startup on missing or invalid XML.
- [x] Julius and Augustus author legacy 4x4 formations for legionary, javelin, mounted, infantry, and archer forts; Vespasian authors 8x8 cohort formations for the same fort unit families.
- [x] Fort BuildingType XML declares `<military><formation key="..." /></military>`, resolves it through `FormationType`, and construction/counting derives the legacy primary soldier figure type from the resolved formation where possible.
- [x] Runtime `formation` objects bind to their owning fort's resolved `FormationType` at creation/load refresh time, and live figure slots can grow to the declared formation capacity.
- [x] The old C++ fort-type-to-soldier table was removed from building counts; fort counting now enumerates BuildingType definitions with military data and derives the soldier figure from the resolved formation.
- [x] UnitType XML now declares the transitional barracks recruit category and weapon requirement; formation recruitment and barracks weapon consumption ask the resolved `FormationType`/`UnitType` instead of remapping fort soldier figure enums.
- [x] Fort-bound legion initialization, declared recruit selection, overflow ejection, and non-combat movement setup now route through `formation` methods instead of duplicating slot scans at legion call sites.
- [x] Herd animal movement and enemy formation-wide combat/city predicates now route through `formation` methods, further concentrating legacy figure-id iteration behind the runtime formation object.
- [x] Formation layout callers now ask `formation` for live grid offsets derived from the resolved `FormationType` footprint; the lower helper preserves legacy 16-position output without modulo wrapping slot indices.
- [x] Save/load now routes the fixed 16 roster slot serialization through `formation` methods, naming the legacy storage bridge without changing the old save prefix.
- [x] Formation save data now keeps the legacy 16-slot prefix for old-version compatibility and appends an extended roster section for larger XML-declared formations.
- [x] Formation live storage is now a dynamically sized roster owned by `formation`, with direct external `figures[]` access removed from runtime callers.
- [ ] Layout still carries a 16-position legacy compatibility table for existing formation layouts; larger formations need adaptive spacing or data-driven slot coordinates before >16 live slots can render correctly.
- [ ] Add real fort-owned `Formation` object links so forts hold pointers/ids to live formations, and each live formation holds its `FormationType` pointer plus per-slot unit state.
- [ ] Add adaptive slot spacing for larger formations inside the current fort mustering-ground footprint.
- [ ] Migrate enemy, barbarian, wolf/animal, and other combat figure archetypes into `UnitType`.
- [ ] Move legacy vanilla formation numeric-id translation into the save/load compatibility bridge; bridge entries should store the old numeric id as data, keep old enum names only as same-line XML comments, and build string-id to runtime-id tables from loaded `FormationType` definitions.

### Next Gates

- Larger Vespasian formations should not be visually validated until figure logical-size ownership is complete enough to author half-size figures.
- Adaptive spacing must fit the current fortress mustering-ground footprint; do not assume a larger formation can simply occupy a larger world footprint.
- The bridge should preserve the legacy 16-slot prefix only as save compatibility. Runtime iteration should continue using the dynamic formation roster.
- Unit coverage should include every combat actor family, not only Roman fort soldiers.

## Runtime Boundaries

The first slice should not rewrite all combat AI. It should establish ownership and data shape:

- `UnitType` owns combat stats and ability declarations.
- `FormationType` owns slot layout, unit composition, string id, and numeric runtime id assigned at startup.
- `Formation` owns live formation state and points to its `FormationType`.
- `Fort` owns a pointer/id link to its live formation.
- `Figure` owns movement/action state and links back to its unit slot when it is part of a formation.

Later slices can move attack selection, ranged projectile creation, morale, formation movement, and battlefield commands into unit/formation methods.

## Risks

- Save migration needs careful mapping from existing soldier ids to formation slots.
- UI panels that show soldier counts currently assume a small fixed unit count.
- Enemy formations may have their own hardcoded layouts and should migrate to the same definitions instead of gaining a parallel system.
- Performance needs attention when Vespasian formations grow from 16 to 64 figures.
- Any graphics ownership work for figures should align with the figure-owned native graphics plan so units can select figure graphics through XML.
