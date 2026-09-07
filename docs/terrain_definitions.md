# Terrain definitions and archive inspection

Each mod may provide `Terrain-Types/<text_id>.xml`. The winning definition owns one address-stable `Terrain` object. Foundations, scenario actions and tile compositions contain references to these objects. Tiles share immutable compositions; no runtime integer ID, fixed-width terrain mask, or per-tile string lookup is used. Compositions and definitions are rebuilt when the mod stack reloads.

```xml
<terrain text_id="shallow_water" graphics_priority="1">
    <traversal land="false" sea="false" enemy="false" herd="false" earthquake="false" water="true" />
    <placement blocking="true" clearable="true" editable="true" paintable="true" />
    <includes terrain="water" />
    <water_image shape="open" group="Terrain_Maps\Water_Shore" images="Image_0000 Image_0001 Image_0002 Image_0003" />
</terrain>
```

`text_id` matches the file path without its extension. Names contain lowercase ASCII letters, digits, underscores, hyphens, periods or path separators. `none` and `highway` are reserved authoring collection names. There is no `number_id`. The traversal and placement fields are explicit booleans; lower mods supply inherited fields through the existing XML overlay mechanism. Duplicate identities in a layer, missing references and include cycles fail startup with the source path.

`includes` describes a prerequisite overlay: adding shallow water also adds water; removing water removes dependent shallow water. Traversal properties feed the native land, enemy, herd and earthquake policy collections. Sea barriers invalidate cached water routes even when a tile remains water. `editable` includes the terrain in legacy editor clearing operations, while `paintable` exposes it to the scenario terrain selector. Existing native systems bind common roles such as road and water once; new water overlays should explicitly include `water`.

Water-image rules bind named native image entries during graphics loading. Image-providing terrain definitions declare distinct `graphics_priority` values (default zero); the highest matching priority wins when overlays coexist. `open`, `north`, `east`, `south`, and `west` describe the existing clean shore contexts; other shapes retain their normal water image unless explicitly authored. The extracted source groups stay in the game installation. Terrain XML declares image selection; it does not duplicate graphics or animation groups.

## Foundation queries

Herd route planning and movement both enforce `traversal herd`, including roads beneath otherwise blocked buildings. Herd formations can additionally declare `<herd ... building_clearance="4" />` (0–16, default 0). This is a Chebyshev buffer around occupied building cells, independent of desirability. Julius sheep and zebras declare four tiles; aggressive wolves declare zero. A herd already inside a new building's buffer may move sideways or outward, but cannot approach more closely. Nearby construction prompts destination reconsideration; formation destination checks use the same actual member offsets as movement.

```xml
<proximity terrain="rock" min_distance="1" max_distance="1" exclude_map_flags="true" warning_key="TR_CITY_WARNING_ROCK_NEEDED" />
<proximity terrain="tree|shrub" min_distance="1" max_distance="1" match="any" />
<proximity name="water_source" placement="false" terrain="water" min_distance="0" max_distance="1" metric="chebyshev" />
```

Distances are measured from actual rotated foundation cells, including sparse footprints. `match` is `all` by default or `any`; `metric` is `chebyshev` by default or `manhattan`. `min_count` defaults to one. `navigable_water` and `sea` authoring queries use boat navigation; `sea` also requires connectivity to the river entrance. The normal cell `requires`, `permits`, `adds` and `removes` attributes accept terrain names, bound during startup.

An operational query has a name and `placement="false"`. A water-access source binds that query through `<source type="foundation_requirement" requirement="water_source" />`. The reservoir uses this to retain dry placement while requiring a nearby source or an aqueduct connection for supply. Existing special `site_requires` branches have been replaced by proximity data.

## Saves

Native SVV version **207** and scenario version **27** contain an uncompressed UTF-8 terrain ledger alongside the archive pieces:

```text
terrain-ledger	1
terrain	1	road
terrain	2	water
set	0
set	1	1
set	2	1	2
end-terrain-ledger
```

The separators shown above are actual tabs. Terrain records map archive numbers to stable names. Set records list explicit members; map cells, foundation rollback deltas and terrain-bearing scenario parameters serialize set numbers. The grid itself remains a compact archive payload. The whole save is not plain text, but the ledger can be located with a simple byte regex without decompressing the grid.

```powershell
python tools/inspect_terrain_ledger.py "path/to/city.svv"
```

The inspector prints definitions and sets as JSON with resolved names. Archive numbers are local to each save and may change on the next save. Loading binds names once, then resolves cells through the ledger; saving emits no class statistics. Missing-mod terrain aliases are applied only during import and logged as repairs. Unknown terrain names without an alias are removed with a warning; a canonical resave contains only active definitions. Malformed or unresolved archive set references fail rather than being reinterpreted as bits.

Older supported SAV/SVV layouts translate fixed terrain bits in `TerrainArchive.cpp`, including the upstream shallow-water bit. Full semantic conversion of newer foreign Augustus SVX archives remains the separately tracked SB04+ work; recognizing its terrain bit alone does not make those divergent archives loadable.
