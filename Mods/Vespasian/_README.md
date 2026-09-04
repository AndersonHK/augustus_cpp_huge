# Mod Root XML

Every mod lives in `Mods/<ModName>/`. The root contains two special XML files:

- `mod.xml` describes the mod and its dependencies. It is required for every mod named in the active mod list.
- `defines.xml` overrides global gameplay tables and scalar values. It is optional in an individual mod layer.

Keep documentation and templates in files whose names do not end in `.xml`, so the runtime cannot mistake them for live data. This directory ships `_mod.xml.example` and `_defines.xml.example` as copy/paste references. Rename a copied template to the exact live filename only inside the new mod's root.

Definition-specific documentation lives beside each definition family. For example, see [BuildingType/_README.md](BuildingType/_README.md) and [BuildingType/_template.xml.example](BuildingType/_template.xml.example).

## Minimal mod layout

```text
Mods/
  MyMod/
    mod.xml
    defines.xml                 optional
    BuildingType/               optional sparse overrides
    FigureType/                 optional sparse overrides
    Graphics/                   optional authored runtime assets
    ...
```

The active mod stack is ordered from base to most-derived. Vespasian normally loads as:

```text
Julius -> Augustus -> Vespasian -> MyMod
```

Later layers may add definitions or replace inherited definitions with the same normalized identity. A dependency declaration validates ordering; it does not automatically insert or reorder mods in the active mod list.

## `mod.xml`

The root element is `<mod>`. All four child fields are required exactly once, including an empty `<dependencies />` for a base mod.

| Element | Required attributes | Rules |
| --- | --- | --- |
| `<name>` | `value` | Non-empty. Must exactly match the mod folder name and its active mod-list entry, including case. |
| `<description>` | `value` | Non-empty human-readable summary. |
| `<version>` | `value` | Non-empty version label. Semantic-version formatting is recommended but not enforced. |
| `<dependencies>` | none | Required container. Use a self-closing element when there are no dependencies. |
| `<dependencies><mod>` | `name` | Optional and repeatable. Each dependency must name an earlier mod in the active stack. |

Names may contain spaces, but cannot be empty, `.` or `..`, and cannot contain control characters or `/ \ : < > " | ? *`.

Dependency names are compared case-insensitively for duplicate, self-dependency, and ordering checks. The metadata `<name>` itself is checked exactly against the folder and active mod-list entry. Declare every prerequisite the mod directly relies upon, and place those prerequisites before the mod in the active list.

Unknown elements, missing `value` attributes, empty values, duplicate metadata fields, duplicate dependencies, self-dependencies, missing dependency folders, and dependencies ordered after the dependent mod reject startup.

See `_mod.xml.example` for a complete starter.

## `defines.xml`

The root element is `<defines>`. Every direct child is optional in an individual layer. Omitting a child inherits the effective value from an earlier layer.

The loader reads `defines.xml` from each active mod in stack order. Scalar nodes replace the earlier scalar when present. Named tables merge by `id`; a later table with the same ID replaces the whole earlier table. Tables do not merge individual rows or values.

The runtime currently selects the `default` calendar, mortality table, and birth table. Other non-empty IDs can be loaded and layered, but are not selected by current gameplay. After all layers merge, all three `default` tables must exist.

### Scalar nodes

| Element | Required attributes when present | Accepted value |
| --- | --- | --- |
| `<combat>` | `default_building_hit_points` | Strict positive integer. Used when a building has no more specific hit-point value. |
| `<presentation>` | `legacy_figure_logical_units_per_source_pixel` | Strict positive integer. Scales legacy figure source pixels into logical render units. |

If no layer supplies these scalars, the compiled fallbacks are `10` hit points and `120` logical units per source pixel. The bundled Julius layer supplies both, so derived mods normally inherit Julius or override it.

### Calendar

```xml
<calendar id="default" ticks_per_day="100">
    <month_days values="31,28,31,30,31,30,31,31,30,31,30,31" />
</calendar>
```

- `id` is required and must be non-empty.
- `ticks_per_day` is required and must be a strict positive integer.
- `<month_days>` is required inside every authored calendar.
- `month_days.values` must contain exactly 12 comma-separated positive integers, January through December.
- Calendar IDs must be unique within one `defines.xml` file.

Changing the calendar changes tick/date conversions and therefore has save-compatibility and balance consequences. Treat it as a gameplay-format change, not merely presentation data.

### Birth table

```xml
<birth_table id="default">
    <age_decennia values="0,3,16,9,2,0,0,0,0,0" />
</birth_table>
```

- `id` is required and must be non-empty.
- Exactly one `<age_decennia>` child is required.
- `values` must contain exactly 10 comma-separated non-negative integers.
- Entries correspond to ages 0-9, 10-19, through 90-99.
- Birth-table IDs must be unique within one `defines.xml` file.

The loader does not currently enforce an upper bound on the values. They are consumed as percentages, so values from 0 through 100 are the normal authoring range.

### Mortality table

```xml
<mortality_table id="default">
    <health bucket="0" values="20,10,5,10,20,30,50,85,100,100" />
    <!-- Buckets 1 through 10 are also required. -->
</mortality_table>
```

- `id` is required and must be non-empty.
- Every health bucket from `0` through `10` is required exactly once.
- Each `<health>` requires `bucket` and `values`.
- `bucket` must be a strict integer in the inclusive range 0-10.
- `values` must contain exactly 10 comma-separated non-negative integers for the same age decennia as the birth table.
- Mortality-table IDs must be unique within one `defines.xml` file.

The loader does not currently enforce an upper bound on mortality values. They are consumed as percentages, so values from 0 through 100 are the normal authoring range.

Because named tables replace whole inherited entries, changing one mortality row requires copying and authoring all 11 rows. An incomplete replacement rejects startup; it never falls back row-by-row.

See `_defines.xml.example` for every currently supported node and attribute in one valid document.

## Authoring workflow

1. Copy `_mod.xml.example` to `Mods/<ModName>/mod.xml` and replace every placeholder.
2. Add the mod after all declared dependencies in the active `config/mod-list` file.
3. Copy `_defines.xml.example` only if the mod changes global defines. Delete blocks that should remain inherited.
4. Add sparse definition folders only for content the mod introduces or changes. Do not copy unchanged inherited definitions.
5. Run the real executable startup gate for the new top-level mod and inspect `vespasian-log.txt` for XML warnings or errors.

Example startup validation:

```text
Vespasian.exe --startup-test --no-audio --mod MyMod "C:\path\to\Caesar 3"
```

The mod must already be present in the active mod list, and its name must match the folder and `mod.xml` exactly.

## Keeping the contract current

When a root XML field is added, renamed, removed, or changes validation behavior, update these together:

1. The runtime parser and its negative/positive tests.
2. The corresponding `_*.xml.example` template.
3. This README's field table, required/optional rules, layering behavior, and compatibility notes.
4. The live Vespasian XML when the new contract changes shipped behavior.

Templates should remain exhaustive but safe to copy. Live mod files should remain minimal and should not repeat inherited definitions merely to mirror the template.
