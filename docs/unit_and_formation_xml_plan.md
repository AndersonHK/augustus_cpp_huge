# Unit And Formation XML Plan

## Intent

Move military unit and formation structure into mod data so forts, soldiers, and battle formations are no longer governed by hardcoded 4x4 assumptions. Vespasian's target formation size should be 8x8, for 64 soldiers, while Julius and Augustus can preserve their current 4x4 behavior through their own XML.

The visual/runtime constraint is the fort mustering ground. It is currently sized around the legacy 4x4 soldier layout, so larger formations need adaptive slot spacing until Vespasian's figure logical sizes shrink. The scaled figure XML overrides are therefore a practical prerequisite for full 8x8 formations that look correct without expanding the mustering-ground footprint.

The model should follow the same direction as native buildings: data declares the semantic object, runtime code resolves it into typed definitions, and gameplay asks the owning object what it contains instead of branching on a legacy building or figure id.

## New Data Folders

Add two new mod folders:

- `Mods/<mod>/UnitType`
- `Mods/<mod>/FormationType`

`UnitType` defines combat-capable unit archetypes. These are figure-backed units with stats, abilities, graphics references, and movement/pathing policy. The scope is all combat actors, not only Roman fort soldiers: legionaries, javelin soldiers, cavalry, enemy barbarians, native fighters, wolves/animals, and future special combat figures should all migrate through the same unit definition model.

`FormationType` defines a collection of unit slots arranged into a tactical shape. A fort references one formation type and owns the units created from it. Enemy and herd definitions use the same object model and explicitly declare their spawn role instead of relying on engine identity, figure-number ranges, or a parallel hardcoded formation table.

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
<formation key="vespasian_legionary_century" recruit_capacity="64">
  <grid width="8" height="8" />
  <slot fill="all" unit="legionary" />
</formation>
```

`recruit_capacity` is an optional positive balance limit no larger than the grid capacity. When omitted, recruitment may fill the complete formation. Vespasian authors 64 for legionary and auxiliary centuries and 36 for its 6x6 cavalry turma. The fixed 16-slot legacy save prefix is an import/export concern and does not limit live recruitment.

Enemy and herd behavior is explicit on the formation:

```xml
<formation key="enemy43_spear_formation">
  <grid width="4" height="4" />
  <spawn role="enemy" />
  <slot fill="all" unit="enemy43_spear" />
</formation>

<formation key="wolf_herd">
  <grid width="4" height="4" />
  <spawn role="herd" climate="northern" count="8" />
  <slot fill="all" unit="wolf" />
</formation>
```

`role="enemy"` may not declare herd-only climate/count metadata. `role="herd"` requires a recognized scenario-climate identity and a positive initial count no larger than the formation capacity. A climate may resolve zero, one, or many active herd definitions; many are selected deterministically by herd-point location. Each active enemy figure and herd figure currently owns exactly one spawn definition so legacy saves can rebind a live formation unambiguously from its saved figure identity. Upper mod layers may replace or suppress individual formation keys, so this rule does not depend on whether the running game is called Julius, Augustus, or Vespasian.

Spawn definitions are currently homogeneous, full-grid formations. That needs to become a validated invariant before this migration is considered closed: creation publishes the primary unit figure, so accepting mixed or partial spawn slots would claim composition that runtime cannot reproduce.

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
    <formation key="vespasian_legionary_century" />
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
9. Compute one formation pitch from the live mustering-ground foundation and resolved FormationType grid, use it for both fort and deployed layouts, and move figures through generic exact fixed-point destinations without teleporting between route and local movement.
10. Bridge old saves by mapping legacy fort/soldier records and vanilla formation enum values into the formation declared by the fort type.
11. Delete legacy constants that assume 16 soldiers once save migration and runtime behavior are covered.

### Current Prerequisite Status

- [x] Legacy runtime formation logic has formation-owned capacity helpers for active slot iteration, open/full checks, and overflow counts.
- [x] Capacity, roster clearing, and open-slot assignment helpers now live on the `formation` object instead of free functions, so callers ask the formation directly while the save/storage contract is still fixed.
- [x] Active figure-id iteration, reverse iteration, first active figure lookup, and active-figure predicates now live on `formation`; normal `src/figure` runtime callers no longer turn raw `figures[]` slots directly into `Figure` objects outside fixed save/layout storage.
- [x] `UnitType` and `FormationType` registries load before `BuildingType`, expose typed lookups, and fail startup on missing or invalid XML.
- [x] Julius authors inherited 4x4 formations for legionary, javelin, mounted, infantry, and archer forts; Vespasian references semantically named 8x8/64 legionary and auxiliary centuries plus a 6x6/36 cavalry turma.
- [x] Fort BuildingType XML declares `<military><formation key="..." /></military>`, resolves it through `FormationType`, and construction/counting derives the legacy primary soldier figure type from the resolved formation where possible.
- [x] Runtime `formation` objects bind to their owning fort's resolved `FormationType` at creation/load refresh time, and live figure slots can grow to the declared formation capacity.
- [x] The old C++ fort-type-to-soldier table was removed from building counts; fort counting now enumerates BuildingType definitions with military data and derives the soldier figure from the resolved formation.
- [x] UnitType XML now declares the transitional barracks recruit category and weapon requirement; formation recruitment and barracks weapon consumption ask the resolved `FormationType`/`UnitType` instead of remapping fort soldier figure enums.
- [x] All live combat actors now own their exact legacy-live combat policy in identical Julius/Augustus/Vespasian UnitType XML: Roman forts, seven city actors, standard invasion/Caesar families 43-57, enemy catapults, wolves, the five-member crime family, the armed mess-hall supplier, and city ballistae. Combat, difficulty overrides, launcher damage, missile dispatch, formation modifiers, health accounting, and tower/watch fire consume resolved definitions; duplicate figure ownership and invalid projectile/difficulty metadata fail parser tests. Wolf, crime, and supplier compatibility state remains in its owning runtime/save systems. Projectile figures remain deliberately separate so movement, collision, sounds, and saved in-flight fallback retain stable ids and behavior.
- [x] Fort-bound legion initialization, declared recruit selection, overflow ejection, and non-combat movement setup now route through `formation` methods instead of duplicating slot scans at legion call sites.
- [x] Herd animal movement and enemy formation-wide combat/city predicates now route through `formation` methods, further concentrating legacy figure-id iteration behind the runtime formation object.
- [x] Julius FormationType data explicitly declares all legacy enemy archetypes with `spawn role="enemy"` and sheep, wolf, and zebra with `spawn role="herd"`, climate, and initial count. Later mods inherit or override this data without engine-specific runtime branches.
- [x] Herd indexes are collections per climate rather than single hardcoded entries. Zero active definitions is valid and creates no herd; one selects directly; many select deterministically from herd-point coordinates.
- [x] Enemy/herd creation requires a resolved spawn definition, load refresh rebinds by the uniquely owned primary figure type, and normal live formation behavior terminates loudly when its immutable definition is missing.
- [ ] Validate at final registry publication—not only parse/resolve—that enemy and herd figure ownership is unique, each supported live spawn type has a definition, and distinct herd figures can coexist in one climate.
- [ ] Enforce homogeneous/full-grid spawn slot composition, or extend spawning and save identity so mixed/partial definitions are honestly representable.
- [x] Formation layout callers now ask `formation` for live grid offsets derived from the resolved `FormationType` footprint; the lower helper preserves legacy 16-position output without modulo wrapping slot indices.
- [x] Save/load now routes the fixed 16 roster slot serialization through `formation` methods, naming the legacy storage bridge without changing the old save prefix.
- [x] Formation save data now keeps the legacy 16-slot prefix for old-version compatibility and appends an extended roster section for larger XML-declared formations.
- [x] Formation live storage is now a dynamically sized roster owned by `formation`, with direct external `figures[]` access removed from runtime callers.
- [x] Barracks recruitment and recruit-overflow ejection use `FormationType.recruit_capacity`; Vespasian explicitly authors 64 for its century-scale infantry formations and 36 for its cavalry turma without deriving runtime balance from legacy save storage.
- [x] Tactical layouts use mathematical `FormationLayoutDef` geometry at every capacity, with no small-formation tables or square fallback. Stable ids remain save/scenario bridges; enemy army spacing remains separately authored. See `docs/formation_runtime.md`.
- [x] Fort runtime objects now hold a validated direct `Formation*`; composition children resolve through their owner, while the saved `formation_id` remains only the compatibility bridge. Formation storage has stable addresses, and creation/load hydration rejects inactive, non-legion, unresolved-type, and conflicting-id links.
- [x] Add adaptive fixed-point slot spacing for larger formations inside the current fort mustering-ground footprint. A 4x4 grid preserves its exact tile positions; 6x6 and 8x8 grids span the same live foundation, and that pitch remains identical after deployment.
- [x] Route formation travel through the generic movable-object exact-destination path: physically converge from the current sub-tile position, use ordinary routing for the coarse tile segment, walk the final sub-tile residual, then rotate. This is shared movement support suitable for soldiers, walkers, animals, and projectiles rather than a formation-only stationing system.
- [x] Keep the military standard outside the live soldier roster and recruit capacity. Its direct relationship to the formation is independent from the legacy scenario-query sentinel that uses the standard figure type to mean all player troops.
- [x] Migrate combat figure archetypes into `UnitType`; standard invasion and Caesar families 43-57, enemy catapults, wolves, protesters/criminals/rioters/robbers/looters, the only armed supplier, and city ballistae are complete. Ordinary supplier variants are noncombat walkers. Wolf herd behavior, crime lifecycle/city state, and supplier missions remain their owning runtime behavior. Standalone projectiles are not units: their handlers and legacy numeric fallback stay outside UnitType to preserve saved in-flight figures, including the otherwise unused catapult missile.
- [x] Move legacy tactical-layout numeric translation into save/scenario compatibility bridges. Runtime formations and enemy armies bind resolved `FormationLayoutDef` objects, UI/combat use string identities, invalid saved ids normalize once to `column`, and `FormationType` remains focused on unit composition.

### Next Gates

- Larger Vespasian formations should not be visually validated until figure logical-size ownership is complete enough to author scaled figures.
- Adaptive spacing reads the current fortress mustering-ground `Foundation`; it does not assume that a larger formation occupies a larger world footprint, and the resulting pitch is used by deployed layouts as well as fort stationing.
- The bridge preserves the legacy 16-slot prefix only as save compatibility. Runtime storage owns the complete dynamic roster, including Vespasian's authored 64/36 capacities.
- Unit coverage should include every combat actor family, not only Roman fort soldiers.

### Remaining Fixed Formation Constants

The remaining values of 16 are compatibility boundaries, not runtime formation iteration limits:

- `LEGACY_FORMATION_SAVE_SLOT_COUNT = 16` is private to `formation.cpp` and used only by the fixed prefix in old formation save records. Barracks recruitment is independently authored through `FormationType.recruit_capacity`.
- Tactical member positions are generated mathematically from XML geometry at every capacity; no fixed-size position tables remain.
- The 16-entry loops in `write_legacy_figure_slots()` and `read_legacy_figure_slots()` serialize that old prefix and must remain exact for save compatibility. Normal legion behavior iterates the `formation` object's declared roster instead.
- `EXTENDED_FORMATION_ROSTER_SAVE_SLOTS = 256` is the bounded capacity of the appended keyed-era roster payload, not a gameplay formation size.

Legion healing no longer scans every city figure looking for soldiers; it iterates each live legion's owned roster. Explosion-cloud counts, eight-direction wrap scans, building-render footprints, and empire-map highlight samples are unrelated uses of 16 and are outside this migration.

The stable tactical-layout numbers now live only in the explicitly named `formation_layout_legacy` save/scenario namespace. Runtime layout identity and mathematical geometry belong to layered `FormationLayoutDef` objects.

## Runtime Boundaries

The first slice should not rewrite all combat AI. It should establish ownership and data shape:

- `UnitType` owns combat stats and ability declarations.
- `FormationType` owns slot layout, unit composition, string identity, combat modifiers, and optional explicit enemy/herd spawn behavior.
- `Formation` owns live formation state and points to its `FormationType`.
- `Fort` owns a pointer/id link to its live formation.
- `Figure` owns movement/action state, exact fixed-point current/destination coordinates, and a link back to its unit slot when it is part of a formation. Routing operates on the destination's containing tile while the same movement operation owns physical convergence at both sub-tile ends.

Later slices can move attack selection, ranged projectile creation, morale, formation movement, and battlefield commands into unit/formation methods.

## Risks

- Save migration needs careful mapping from existing soldier ids to formation slots.
- UI panels that show soldier counts currently assume a small fixed unit count.
- Spawn definitions must not accept mixed/partial slot composition while runtime creates only the primary unit figure; either validate the homogeneous contract or implement true composed spawning and stable save identity first.
- The climate vocabulary still maps the scenario's three legacy climate identities. That mapping is a domain/save bridge, not permission to select definitions by executable or mod name.
- Performance needs attention when Vespasian formations grow from 16 to 64 figures.
- Any graphics ownership work for figures should align with the figure-owned native graphics plan so units can select figure graphics through XML.
