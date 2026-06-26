# Unit And Formation XML Plan

## Intent

Move military unit and formation structure into mod data so forts, soldiers, and battle formations are no longer governed by hardcoded 4x4 assumptions. Vespasian's target formation size should be 8x8, for 64 soldiers, while Julius and Augustus can preserve their current 4x4 behavior through their own XML.

The model should follow the same direction as native buildings: data declares the semantic object, runtime code resolves it into typed definitions, and gameplay asks the owning object what it contains instead of branching on a legacy building or figure id.

## New Data Folders

Add two new mod folders:

- `Mods/<mod>/UnitType`
- `Mods/<mod>/FormationType`

`UnitType` defines combat-capable unit archetypes. These are figure-backed units with stats, abilities, graphics references, and movement/pathing policy.

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

At runtime, a Fort object owns a `FormationInstance`. The instance owns or references the spawned unit figures, tracks missing/dead slots, and exposes formation-level methods:

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
5. Teach fort construction/loading to create a formation instance from the resolved definition.
6. Replace hardcoded formation size loops with `FormationInstance` iteration.
7. Move soldier stats and ranged/melee behavior into `UnitType`.
8. Bridge old saves by mapping legacy fort/soldier records into the formation declared by the fort type.
9. Delete legacy constants that assume 16 soldiers once save migration and runtime behavior are covered.

## Runtime Boundaries

The first slice should not rewrite all combat AI. It should establish ownership and data shape:

- `UnitType` owns combat stats and ability declarations.
- `FormationType` owns slot layout and unit composition.
- `Fort` owns a formation instance.
- `Figure` owns movement/action state and links back to its unit slot when it is part of a formation.

Later slices can move attack selection, ranged projectile creation, morale, formation movement, and battlefield commands into unit/formation methods.

## Risks

- Save migration needs careful mapping from existing soldier ids to formation slots.
- UI panels that show soldier counts currently assume a small fixed unit count.
- Enemy formations may have their own hardcoded layouts and should migrate to the same definitions instead of gaining a parallel system.
- Performance needs attention when Vespasian formations grow from 16 to 64 figures.
- Any graphics ownership work for figures should align with the figure-owned native graphics plan so units can select figure graphics through XML.
