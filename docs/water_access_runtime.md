# Water Access Runtime

Snapshot: 2026-08-13

This document records the current water access architecture after the rule refactor. The short version: water access is now defined by XML water access types and BuildingType rules, while runtime simulation stores compact `uint8_t` masks. Old `has_*_access` building fields still exist as compatibility mirrors, but they are no longer the source of truth for new gameplay or graphics decisions.

## Motive

The old model treated wells, fountains, reservoirs, aqueducts, and latrines as hardcoded special cases. That made every new provider or consumer branch grow its own copy of water logic, and it made ghosts/overlays/graphics disagree whenever one branch was missed.

The new model separates the facts:

- `WaterAccessType` says what access types exist in the active mod.
- BuildingType `<water_access>` rules say what a building provides or requires.
- `water_access_runtime` evaluates those rules into map-wide typed masks.
- legacy fields such as `has_water_access`, `has_well_access`, and `has_latrines_access` are projected from masks for compatibility.

The goal is that a building asks the same question everywhere: "given my BuildingType rules and this tile state, do I have the access I require?"

## Access Type Definitions

Water access type XML lives in:

- `Mods/Augustus/WaterAccessType/*.xml`
- `Mods/Julius/WaterAccessType/*.xml`
- `Mods/Vespasian/WaterAccessType/*.xml`

Each file has:

```xml
<water_access_type text_id="fountain" number_id="1">
</water_access_type>
```

Validation happens while loading the active mod:

- at most eight types are allowed
- `number_id` must be unique and between `0` and `7`
- `text_id` must be unique
- provider/requirement references in BuildingType XML must resolve

Runtime mask convention:

- `mask = 1u << number_id`
- mask value `0` means no access
- `uint8_t` is the storage type for coverage and provider masks

Bundled ids:

| Text id | Numeric id | Mods |
| --- | --- | --- |
| `well` | `0` | Julius, Augustus, Vespasian |
| `fountain` | `1` | Julius, Augustus, Vespasian |
| `reservoir` | `2` | Julius, Augustus, Vespasian |
| `aqueduct` | `3` | Julius, Augustus, Vespasian |
| `latrines` | `4` | Augustus, Vespasian only |

Julius must load without `latrines` and must not reference it.

## BuildingType Water Rules

BuildingType XML stores water facts under a root-level `<water_access>` block. Do not put water semantics under graphics or spawn nodes.

Navigable open-water requirements are declared on that block:

```xml
<water_access requires_open_water="true" />
```

The runtime evaluates this against the selected rotation's Foundation cells that require water. At least one cardinal neighbor outside those authored cells must be reachable from the scenario river entry through navigable water. The Foundation therefore owns shoreline geometry, while `WaterAccessDefinition` owns the semantic connectivity requirement. No additional rows of water are implied. Docks declare this requirement; wharves and shipyards do not unless their own XML opts in.

Provider rules:

```xml
<provides type="reservoir" range="10" origin="footprint" />
<provides type="aqueduct" range="0" origin="nodes" />
```

Requirement rules:

```xml
<requires mode="any">
    <access type="fountain" />
    <access type="well" />
</requires>
```

Node rules:

```xml
<node role="provide" x="1" y="-1" />
<node role="provide" x="3" y="1" />
<node role="provide" x="1" y="3" />
<node role="provide" x="-1" y="1" />
<node role="require" x="1" y="0" />
<node role="require" x="2" y="1" />
<node role="require" x="1" y="2" />
<node role="require" x="0" y="1" />
```

Current rule conventions:

- multiple `<provides>` rules are allowed
- multiple `<requires>` rules are allowed, and all requirement rules must pass
- `mode="any"` means any term inside that rule can satisfy it
- `mode="all"` means every term inside that rule must satisfy it
- `where="footprint"` checks any tile under the footprint
- `where="nodes"` checks `role="require"` nodes
- `origin="footprint"` emits from the building footprint
- `origin="nodes"` emits from `role="provide"` nodes
- `<node>` without `role` is accepted as `role="both"` for legacy definitions
- `<source type="water_source_any" />` and `<source type="water_source_fresh_only" />` currently both use the natural-water terrain check

## Runtime Ownership

Primary files:

- `src/building/water_access_type.h/.cpp`
  owns WaterAccessType objects and active-mod type lookup.
- `src/building/water_access_type_id_bridge.h/.cpp`
  owns save-local water access id tables and old-save raw-id migration.
- `src/building/building_type.h/.cpp`
  stores `WaterAccessDefinition`, open-water connectivity, provide rules, requirement rules, and nodes.
- `src/building/building_type_registry_xml.cpp`
  parses and validates XML references.
- `src/building/water_access_runtime.h/.cpp`
  evaluates provider and consumer rules, projects compatibility state, and exposes C queries to old callers.

Old callers should go through the C facade in `water_access_runtime.h`. New C++ code inside the building runtime can use the stored BuildingType water definitions directly when it already owns the relevant object.

## Retained Runtime Cache

`water_access_runtime_refresh()` runs once after a city is loaded. It seeds provider snapshots, aqueduct topology, per-tile contribution counts, and the typed access/provider masks. Normal simulation never rebuilds those masks from the whole map.

Runtime mutations update the retained state directly:

- building creation, deletion, replacement, mothballing, and labor changes update one provider snapshot
- a provider adds or subtracts only its own footprint/range contribution
- contribution reference counts preserve overlapping providers without rescanning them
- changed access tiles queue only buildings occupying those tiles for compatibility-state projection
- house merge, split, expansion, shrink, and type-change publication projects the new footprint directly from the retained masks
- aqueduct edits seed only the connected network components touching the edited tile
- reservoir labor, source-water, or connector changes seed only that reservoir and its connected components
- repeated dirty notifications coalesce in tile, reservoir, provider, and building queues

The daily update drains these queues. With no mutations it performs no map scan, network traversal, range reconstruction, or building projection.

Provider activity:

- live providers must be `BUILDING_STATE_IN_USE`
- labor-using providers must have workers
- providers with requirements emit only when their own requirements pass
- planned providers are used by placement preview and are evaluated with the same rules

Projection:

- `project_aqueduct_state()` updates the legacy aqueduct wet/dry flag and image state after the mask stabilizes
- `project_terrain_ranges()` writes `TERRAIN_RESERVOIR_RANGE` and `TERRAIN_FOUNTAIN_RANGE` for legacy systems and overlays
- `project_building_state()` writes compatibility mirrors such as house well/fountain/latrine bytes and refreshes water-driven native graphics

## Reservoirs And Aqueducts

Reservoirs are ordinary water-rule buildings now:

- require either natural water source access or `aqueduct` access at their footprint-edge requirement nodes
- provide `reservoir` access from footprint over their range
- provide `aqueduct` access at range `0` from provider nodes just outside their footprint

Aqueduct tiles are also modeled as a BuildingType provider/consumer:

- require `aqueduct` access on their own tile
- provide `aqueduct` access at range `0` to the four cardinal neighbor tiles
- are evaluated in the fixed-point pass so two dry adjacent aqueducts do not make each other wet from nothing

The terrain aqueduct image still has legacy wet/dry projection code. That projection is an output of the retained network cache, not the source of network truth. Adding or removing one aqueduct traverses the affected component; it does not recalculate unrelated networks.

## Ghosts And Overlays

Placement/context paths should ask the water runtime, not hardcoded provider types:

- `water_access_runtime_building_type_provides_access(type)`
- `water_access_runtime_building_type_provides_access_text(type, "reservoir")`
- `water_access_runtime_building_type_requires_access_text(type, "reservoir")`
- `water_access_runtime_begin_preview(type, primary_grid_offset, secondary_grid_offset)`
- `water_access_runtime_tile_has_preview_highlight(grid_offset)`
- `water_access_runtime_should_draw_overlay_at(grid_offset)`

Current config-sensitive behavior:

- empty-house context overlays still respect the existing well/fountain config toggles
- market range display remains gated by the existing market-range config
- reservoir-range and water-structure range overlays are now chosen through typed provider/requirement queries

## Graphics Interaction

Graphics conditions still read building state such as `has_water_access`, but that value is projected from the generic water requirement rules.

Water-driven buildings such as fountains, reservoirs, concrete makers, ponds, large statues, and latrines should use normal BuildingType graphics variants and water rules rather than dedicated rendering branches. When `project_building_state()` changes a water-relevant building's compatibility state, it calls the native graphics refresh path so the generic renderer sees the updated condition.

## Save/Load

New saves include `water_access_type_table`, written immediately after the BuildingType save table and before building records.

The table stores save-local id -> text id. On load, the text ids resolve against the active mod's WaterAccessType XML before current numeric ids are used. Old saves without the table synthesize the legacy shared ids:

| Legacy raw id | Text id |
| --- | --- |
| `1` | `well` |
| `2` | `fountain` |
| `3` | `reservoir` |
| `4` | `aqueduct` |
| `5` | `latrines` |

Current building records still persist compatibility mirrors, but gameplay checks should move toward typed mask accessors and BuildingType requirements. The save bridge exists so future persisted typed-water references can survive mod-defined numeric ids.

## Conventions For Future Work

- Add a WaterAccessType XML file first when adding a new access kind.
- Keep the numeric id between `0` and `7`; do not reuse ids inside one active mod.
- Prefer `any` requirements for "well or fountain" style logic.
- Prefer multiple provider rules when one building emits more than one access type.
- Use nodes when the exact connection points matter; use footprint when any tile under the building should count.
- Register every provider lifecycle or activity mutation with `water_access_runtime`; do not add periodic repair scans.
- Keep aqueduct/reservoir invalidation component-local and keep provider range changes contribution-local.
- Do not add new `WATER_ACCESS_RUNTIME_TYPE_*` defines or provider-type switch branches.
