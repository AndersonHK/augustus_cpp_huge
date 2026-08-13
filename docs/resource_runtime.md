# Resource Runtime

This note records the current resource ownership model after moving resource defaults out of code and into mod XML.

## Ownership

- `Mods/<Mod>/Resources/*.xml` owns the selected mod's complete resource set.
- `src/game/resource.cpp` parses those XML files into `resource_data` defaults.
- `resource_data` owns resource text id, numeric slot, locale key, flags, default trade prices, and XML attribute name.
- `ResourceGraphics : GraphicsDefinition` in `src/game/ResourceGraphics.h/.cpp` owns resource presentation icons only: panel icons, empire icons, and editor icons.
- `BuildingGraphics : GraphicsDefinition` owns the resource storage-stack image refs parsed from resource `<storage>` nodes, because those images draw warehouse/building storage.
- `FigureGraphics : GraphicsDefinition` owns the resource cart/load image refs parsed from resource `<cart>` nodes, because those images draw cartpusher-style figure overlays.
- FigureType `<resource_cart>` policies own per-figure selection and attachment. The lighthouse supplier policy selects `collecting_item_id` only while returning and keeps that gameplay/resource field independent from the preserved legacy `cart_image_id` save slot.
- Resources do not own production throughput, producers, industries, or warning templates. Producer lookup belongs to building/production code through `building_output_resource(...)` and `building_producer_for_resource(...)`, while warning selection belongs to gameplay triggers.
- `ProductionMethod` owns base monthly throughput through the `<output production_per_month="...">` attribute. Scenario/save production-rate overrides mutate production methods, not resources.
- `src/game/resource_id_bridge.cpp` owns save-local resource id tables and the legacy raw-id migration maps for old saves.

## XML Contract

Each resource file has one root node:

```xml
<resource id="gold" slot="12" name_key="TR_RESOURCE_GOLD">
  <model xml_attr="gold" flags="storable" />
  <trade buy="350" sell="250" />
  <graphics>
    <cart load="single" path="Industry\Gold_Cart_NE" image="Gold_Cart_NE" />
    <storage load="1" path="Industry\Warehouse_Gold_01" image="Warehouse_Gold_01" />
    <panel_icon path="UI\Panelling_Gold_01" image="Panelling_Gold_01" />
  </graphics>
</resource>
```

Required root attributes:

- `slot`: the stable `resource_type` slot.
- `id`: stable text id used by XML references and the save-local resource table.
- `name_key`: translation key for the resource name.

Optional model attributes:

- `xml_attr`: scenario/model XML attribute spelling.
- `flags`: comma-separated `food`, `storable`, `inventory`, and/or `special`.

Graphics nodes use `ImageGroupEntryRef` data: `path` is the logical image group path and `image` is the optional entry id. Resource XML still keeps cart, storage, and icon declarations together for authoring convenience, but startup routes the refs to their owner classes. UI callers should ask `resource_graphics(resource).panel_icon().draw(x, y)` instead of reaching through resource structs or raw image ids.

Production throughput belongs to `Mods/<Mod>/ProductionMethod/*.xml`:

```xml
<production_method>
    <kind value="workshop" />
    <output resource="gold" production_per_month="20" />
    <batch_size value="1" />
</production_method>
```

The output resource identifies the produced good. `production_per_month` is the method's base monthly throughput before climate bonuses or scenario/save overrides. Resource supply-chain queries now derive their raw-material/good pairs from loaded ProductionMethod inputs and outputs instead of from a hardcoded resource table.

`cart_loads` is the produced amount for a completed cycle, including fractional field output such as `numerator="1" denominator="5"`. `cart_capacity` is optional and controls how many full loads an output cart may reserve from storage; if omitted, carts default to the integer produced-load count, or one load for fractional producers.

## Save Bridge

New live saves write a resource save table through `resource_id_bridge_save_table_save_state()`. The table stores save-local numeric ids plus resource text ids, then resolves those text ids against the active mod's `Resources` XML during load. Current saves use the XML `slot` as the save-local id so existing resource arrays remain stable while identity is text-backed.

Old saves without the table still use legacy raw numeric resource ids. Those legacy maps live only in `src/game/resource_id_bridge.cpp`; runtime resource data remains XML-owned.

## Gameplay Warnings And Producers

Warnings are not part of the resource or industry definitions.

When construction needs to warn about an unavailable input, the gameplay trigger already has the resolved resource and building definitions available. It asks the building/production registry which building outputs the missing resource, then selects the city warning template locally:

- missing resource -> resource-needed warning
- missing producer building -> build-producer warning
- missing trade route/import status -> trade warning

`city_warning_show(...)` is deliberately presentation-only: callers pass a `warning_type` token plus the final localized text to display. It does not accept numeric warning ids, resource ids, building ids, or localization ids.

The generic construction templates are Julius-owned project keys so Augustus and Vespasian inherit them through the normal mod stack:

- `TR_CITY_WARNING_TEMPLATE_MISSING_RESOURCE`: replace `<resource>` at runtime.
- `TR_CITY_WARNING_TEMPLATE_MISSING_PRODUCER`: replace `<building-type>` at runtime.

The Julius localization extractor preserves existing `project_keys` in `Mods/Julius/Localization/*.json` before merging immutable extracted Caesar/Julius text, so hand-authored template keys survive regeneration.

## Mod Stack Notes

The selected mod must provide a complete `Resources` folder. Vespasian inherits gameplay/content expectations from Augustus and Julius, but `resource_init()` reads the selected mod's resource folder directly.

When validating resource names or graphics against the local game install, check `D:\Games\GOG Games\Caesar 3\Mods` as well as the repo. Augustus localization is available there, and Julius localization can be extracted at runtime rather than existing as a checked-in `Localization` folder.

## City Mint

Denarii production now has XML-owned producer metadata through the City Mint definitions in Augustus and Vespasian:

- `Mods/Augustus/BuildingType/city_mint.xml`
- `Mods/Augustus/ProductionMethod/city_mint_basic.xml`
- `Mods/Augustus/ProductionMethod/city_mint_gold_basic.xml`
- `Mods/Vespasian/BuildingType/city_mint.xml`
- `Mods/Vespasian/ProductionMethod/city_mint_basic.xml`
- `Mods/Vespasian/ProductionMethod/city_mint_gold_basic.xml`

The runtime mint behavior still has specialized monument handling, but producer discovery no longer relies on a resource-owned industry field or a hardcoded resource-to-industry table.

## Wharf

Fish production now has native BuildingType/ProductionMethod metadata in Julius, Augustus, and Vespasian:

- `Mods/<Mod>/BuildingType/wharf.xml`
- `Mods/<Mod>/ProductionMethod/fish_wharf_basic.xml`

The fishing-boat runtime still owns the actual catch loop. The wharf production method supplies semantic producer discovery and monthly-throughput metadata so fish no longer needs a resource-owned production value.

## Future Farm Field Ownership

Current farms still use the composed-building model: the main farm owns the building-level labor/storage/export behavior, and its field buildings contribute their production methods into the farm's aggregate production math.

The next farm slice should move fields to separately placed tile-tool buildings. A farm then acquires ownership of adjacent fields at runtime, from zero up to an XML-authored maximum. The aggregation rule stays the same but must stop assuming a fixed field count: group owned fields by their static `ProductionMethod`, then multiply that method's throughput and output by the number of owned fields using that same method. This keeps the code readable, supports future farms with arbitrary field counts, and avoids scattering one-off "five fields" assumptions through production, labor, and storage.
