# BuildingType XML

The loader reads every `<building>` XML root in the selected mod's `BuildingType`, `BuildingTypeMenu`, and `Tiles` folders at startup.

Keep templates/examples in non-`.xml` files so they do not get loaded as live data.

Templates and examples are maintained only in `Mods\Vespasian\BuildingType`.
`Mods\Augustus\BuildingType` and `Mods\Julius\BuildingType` keep live XML data only.

Runtime/save identity is migrating away from stable enum slots. New saves include a `building_type_table` that maps compact save ids to BuildingType text ids, while loaded buildings continue to use compact runtime ids. Old saves without the table migrate through `src\building\building_type_legacy_migration.*`.

Water access type identity is XML-owned as well. The selected mod's `WaterAccessType` folder declares up to eight access types, each with a stable text id and a numeric id from `0` through `7`; runtime water coverage stores those declarations as `uint8_t` masks. Augustus and Vespasian define `well`, `fountain`, `reservoir`, `aqueduct`, and `latrines`; Julius defines the same shared water types except `latrines`.

Historical tuning references:

- [Roman City Facility Ratios](../../../research/roman_city_facility_ratios.md)
  gives starting ratios for service counts, area, and employment.
- [Roman Building and Infrastructure Maintenance Needs](../../../research/roman_building_maintenance_needs.md)
  separates public payroll from private employment and gives maintenance/failure
  levers by building family.
- [Roman City Size and Social Ratios](../../../research/roman_city_size_and_social_ratios.md)
  gives resident-class and city-role assumptions for housing, service demand,
  and labor tuning.
- [Caesar III / Julius Housing Progression Defaults](../../../research/caesar3_julius_housing_progression_defaults.md)
  records the vanilla house footprints, capacities, service gates, graphics
  references, prosperity, and tax curve.
- [Caesar III Housing Balance and Play Analysis](../../../research/caesar3_housing_balance_play_analysis.md)
  explains which services and goods are easy, mandatory, or logistically hard in
  vanilla play.
- [Vespasian Housing Progression Design Notes](../../../research/vespasian_housing_progression_design_notes.md)
  records possible future mechanics such as service capacities, market revenue,
  deeper road access, and classed employment.
- [Gameplay Divergences From Augustus](../../../docs/gameplay_divergences_from_augustus.md)
  tracks player-visible differences between this repo's bundled profiles and
  upstream Augustus, including residential walker spawn policy and Vespasian
  local-workforce tuning.
- [Water Access Runtime](../../../docs/water_access_runtime.md)
  records the current typed-mask provider/consumer simulation, aqueduct/reservoir
  propagation, overlays, and save bridge.

Current supported nodes:

- `<identity ... />`
- `<model ... />`
- `<desirability> ... </desirability>`
- `<foundation ... />`
- `<button ... />`
- `<roadblock ... />`
- `<tile ... />`
- `<temple ... />`
- `<sound ... />`
- `<event_data ... />`
- `<market ... />`
- `<flags ... />`
- `<water_access> ... </water_access>`
- `<graphics> ... </graphics>`
- `<construction> ... </construction>`
- `<labor> ... </labor>`
- `<storages> ... </storages>`
- `<production_methods> ... </production_methods>`
- `<housing ... />`
- `<vacant_lot ... />`
- `<spawn_group ...>`
- `<spawn ... />`

Current supported `<identity>` attributes:

- `name_key="translation.key"` stores the localized building name key for generated UI
- `translation_key="translation.key"` is accepted as an alias

Current supported `<model>` attributes:

- `size="N"`
- `cost="N"`

Model `cost` currently overrides the legacy runtime model after XML load. `size` is parsed into BuildingType data and also bridged into the legacy building-properties footprint fields during registry load, so old placement/building code sees XML-authored sizes until placement authority reads BuildingType definitions directly.

Current supported `<desirability>` child nodes:

- `<value value="N" />`
- `<step value="N" />`
- `<step_size value="N" />`
- `<range value="N" />`

Desirability rules:

- all four child nodes are required when `<desirability>` is present
- `value` and `step_size` may be negative
- `step` and `range` must be non-negative
- model-level `desirability_*` attributes are not supported
- labor counts belong under `<labor><employees count="N" /></labor>`, not under `<model>`

Small building example:

```xml
<building type="barber">
    <identity name_key="main_strings.28.49" />
    <model size="1" cost="25" />
    <desirability>
        <value value="2" />
        <step value="1" />
        <step_size value="-1" />
        <range value="2" />
    </desirability>
    <foundation policy="land" />
    <button group="health" order="40" icon="barber" text_key="main_strings.28.49" />
    <sound id="barber" />
    <event_data attr="barber" />
    <labor>
        <employees count="2" />
        <labor_seeker>
            <method value="houses_spawn_if_below" />
            <amount value="50" />
        </labor_seeker>
    </labor>
</building>
```

Current supported `<foundation>` attributes:

- `policy="land|road|road_or_land|road_or_wall_or_land|wall|water|shoreline|aqueduct|custom"` stores the placement/foundation policy key for the next construction pass
- `open_water="true|false"` requires the footprint to connect to open navigable water after the shoreline check; use this for shoreline buildings that must be reachable by ships

Current supported `<foundation>` child nodes:

- `<terrain value="meadow|rock|tree|water|wall|distant_water" />`
- `<cell x="0" y="0" terrain="land|clear|water|road|road_or_land|road_or_wall_or_land|wall|aqueduct|any" />`
- `<cell rotation="0|1|2|3" x="0" y="0" terrain="..." />`

Foundation terrain rules:

- `<terrain>` may appear zero or more times
- terrain requirements feed the existing placement warning/check path
- use `meadow` for farms, `rock` for quarry/mine placement, `tree` for timber yards, `water` for clay pits, and `distant_water` for sand pits or lighthouses
- `<cell>` may appear zero or more times and declares a per-footprint tile requirement in local building coordinates
- `rotation` is optional; omit it for all rotations or set it to `0..3` for rotation-specific shoreline or composed-footprint cells
- cells not listed by XML inherit the foundation policy: `land/custom/shoreline` require clear land, `road` requires road, `road_or_land` allows clear land, road, highway, and valid highway-under-aqueduct tiles, `road_or_wall_or_land` also allows wall tiles, `wall` requires wall, `water` requires water
- shoreline buildings select their waterside orientation from adjacent water, register their placed footprint with the water map, and save their shoreline orientation from the shared construction placement plan; water footprint cells must be declared explicitly with `<cell ... terrain="water" />`
- `policy="wall"` declares a wall-mounted structure: construction requires the wall footprint, replaces the underlying wall terrain with the placed building terrain, and refreshes surrounding wall images through the shared placement path

Current supported `<composed>` attributes:

- `footprint_width="N"` and `footprint_height="N"` declare the full placement footprint used by construction and ghost previews
- `inherit_orientation="true|false"` is optional; use it only when child records must copy the main record orientation byte

Current supported `<composed>` child nodes:

- `<main x="0" y="0" />` optionally declares the main building record offset inside the composed footprint
- `<main><offset rotation="0|1|2|3" x="0" y="0" /></main>` declares rotation-specific main offsets
- `<part type="building_attr" role="..." x="0" y="0" />` declares a child building part offset for all rotations
- `<part type="building_attr" role="..."><offset rotation="0|1|2|3" x="0" y="0" /></part>` declares rotation-specific child offsets

Composition rules:

- construction validation, ghost previews, and live placement all consume the same composed part offsets
- part `type` references another BuildingType `attr`; recursive composed parts are rejected
- `role` is optional runtime metadata for specialized repair paths, such as farm fields

Current supported `<button>` attributes:

- `group="..."` stores the target build submenu key
- `order="N"` stores generated button ordering
- `icon="..."` stores the generated button icon graphics group key
- `icon_image="..."` optionally stores the image id inside that group; if omitted, runtime uses the group's default image
- `text_key="..."` optionally overrides the button text key; otherwise generated UI can use `<identity name_key="...">`

`<menu>` is accepted as a temporary alias for `<button>` only during this migration slice. Prefer `<button>` in new XML.

Roads, highways, roadblocks, and bridges are smart-tool special cases. They must not declare `<button>` nodes because the tool-mode sidebar owns those buttons directly.

Current supported `<cycle>` attributes:

- `group="key"` declares that this BuildingType participates in a construction selection cycle with other BuildingTypes using the same group key
- `order="N"` sorts members inside the cycle; construction skips members disallowed by the scenario
- `steps="N"` declares how many construction-cycle key presses are needed before advancing to the next member; use this for graphics whose rotation key also changes orientation before changing type

Construction cycles are loaded entirely from BuildingType XML. Do not add hardcoded type-name arrays for new cycle groups.

Current supported `<tool>` attributes:

- `kind="clear_land|clear_trees|repair_land|road|highway|wall|roadblock|aqueduct|draggable_reservoir|draggable_building"` declares construction-tool behavior used by construction, ghost rendering, and special tool previews without inferring behavior from the BuildingType name
- `drag_terrain="land|land_or_road"` is required for `kind="draggable_building"` and declares whether drag placement may transform road tiles into gate variants
- `rotation="none|path|hedge|variant|pair"` is required for `kind="draggable_building"` and declares how the placed building stores orientation or variant state

Current supported `<roadblock>` attributes:

- `kind="standard|storage|bridge"` stores the roadblock category used by roadblock routing and permissions
- `bridge_type="low|ship"` is required when `kind="bridge"` and declares the bridge placement/sprite variant without relying on the building type name
- `passage="wall_gate|center_road"` is optional for `kind="standard"` and declares multi-tile pass-through terrain behavior such as wall gates or center-road arches

Current supported `<tile>` attributes:

- `kind="..."` marks a BuildingType as tile-backed data and stores the tile kind key from XML
- `refresh="none|garden|plaza"` is optional and declares which dirty-region image refresh family to run; omit it only for tile-backed tools that never dirty-refresh through the tile system
- `placement="none|garden|plaza"` is optional and declares area-drag placement effects for tile-backed tools; omit it for tiles that are placed through ordinary building/tool construction
- `overgrown="true|false"` marks garden placement with the overgrown tile state used by overgrown gardens

Tile-backed BuildingTypes use the normal root-level `<graphics>` block. Do not add a tile-specific graphics node. For plaza-like tiles, the default graphics target supplies single-tile images and a `<variant role="tile_large">` target supplies two-by-two plaza images. Other tile-backed tools may declare named graphics roles for their live rendering path, such as highway `tile_base` and `tile_barrier`.
Area-drag tile tools such as gardens and plazas declare both their foundation policy and tile `placement`, so shared ghost validation and final construction agree on which map cells are legal.
Construction refreshes each placed tile-backed part through its declared tile refresh behavior, so new tile-backed BuildingTypes should not need construction-side branches.
Single-tile roadblock tiles use their declared `foundation policy="road"` for placement and are treated as passable road tools during shared foundation validation, so walkers standing on the road do not create a separate construction branch.

Current supported `<temple>` attributes:

- `religion="path"` references a definition under the selected mod's `Religions` folder

Temple rules:

- `<temple ... />` marks the BuildingType as religion-backed and links it directly to the referenced Religion module
- god identity, temple tier, religion capacity, and grand-temple panel presentation belong to the referenced Religion module, not to BuildingType names
- Religion modules reference God definitions from the selected mod's `Gods` folder
- Pantheon is represented as the grand temple for all gods: `<god path="all" />` plus `tier="grand"`
- religion and temple behavior must read this tag-backed module data, not infer temple status from BuildingType text ids or file names

Current supported `Gods` XML:

```xml
<god legacy="ceres|neptune|mercury|mars|venus" />
```

Current supported `Religions` XML:

```xml
<religion>
    <god path="ceres" />
    <tier value="shrine|small|large|grand|oracle" />
    <capacity amount="N" />
    <presentation
        sound="wavs/temple_farm.wav"
        name_key="TR_BUILDING_GRAND_TEMPLE_CERES_DESC"
        bonus_key="TR_BUILDING_GRAND_TEMPLE_CERES_BONUS_DESC"
        quote_key="TR_BUILDING_CERES_TEMPLE_QUOTE"
        banner_group="UI\Ceres_L_Banner"
        banner_image="Ceres L Banner"
        content_y_offset="0"
        height_blocks="40"
        module_key="ceres" />
</religion>
```

`<presentation>` is required for `tier="grand"` Religion modules. `module_key` selects the two-module option bucket and currently supports `ceres`, `neptune`, `mercury`, `mars`, `venus`, and `pantheon`.

Current supported `<sound>` attributes:

- `id="..."`, `value="..."`, or `city="..."` selects the city ambient sound key
- `mute_on_enemies="true|false"` suppresses the sound while enemies are active
- `always_play="true|false"` keeps the sound audible even when normal worker/water gates would mute it

Raw-material producer sound ids currently include `clay_pit`, `iron_mine`, `timber_yard`, and `marble_quarry`. Gold mine, stone quarry, and sand pit do not declare sounds because the legacy properties do not assign city sounds to them.

Current supported `<event_data>` attributes:

- `attr="..."` stores the scenario-event/query building attribute key

Current supported `<market>` attributes:

- `max_distance="N"` stores the market supplier search and placement-preview range
- `max_food_stock="N"` stores the fallback food stock threshold used after wanted goods are already stocked

Current supported `<flags>` attributes:

- `fire_proof="0|1"`
- `draw_desirability_range="0|1"`
- `venus_gt_bonus="0|1"`

Current supported root-level `<water_access>` child nodes:

- `<provides type="well|fountain|reservoir|aqueduct|latrines" range="N" origin="footprint|nodes" />`
- `<requires mode="any|all" where="footprint|nodes"> ... </requires>`
- `<access type="well|fountain|reservoir|aqueduct|latrines" where="footprint|nodes" />`
- `<source type="water_source_any|water_source_fresh_only" />`
- `<node role="provide|require|both" x="N" y="N" />`

Root-level `<water_access>` rules:

- at least one `<provides>` or `<requires>` rule is required
- `<provides>` may appear more than once so one building can emit multiple access types
- `origin` is optional and defaults to `footprint`; use `nodes` for reservoir aqueduct connection points
- `<requires>` may appear more than once; all requirement rules must pass
- `mode="any"` means at least one term inside the rule must pass
- `mode="all"` means every term inside the rule must pass
- `where` is optional and defaults to `footprint`; child `<access>` terms may override the parent
- water access type names come from the selected mod's `WaterAccessType` XML folder
- `<node>` is optional and may appear more than once
- `role` is optional and defaults to `both`; use `provide` for `origin="nodes"` emission points and `require` for `where="nodes"` checks
- `x` and `y` are local tile coordinates relative to the building's top-left footprint tile
- node coordinates may sit outside the footprint
- `kind="aqueduct_connection"` is accepted as a legacy alias but no longer required

Water access examples:

```xml
<water_access>
    <provides type="reservoir" range="10" origin="footprint" />
    <provides type="aqueduct" range="0" origin="nodes" />
    <requires mode="any">
        <source type="water_source_any" />
        <access type="aqueduct" where="nodes" />
    </requires>
    <node role="provide" x="1" y="-1" />
    <node role="provide" x="3" y="1" />
    <node role="provide" x="1" y="3" />
    <node role="provide" x="-1" y="1" />
    <node role="require" x="1" y="0" />
    <node role="require" x="2" y="1" />
    <node role="require" x="1" y="2" />
    <node role="require" x="0" y="1" />
</water_access>
```

```xml
<water_access>
    <requires mode="any">
        <access type="fountain" />
        <access type="well" />
    </requires>
</water_access>
```

Runtime water behavior:

- `WaterAccessType` XML declares the text ids and numeric bits; BuildingType XML only references those ids.
- Providers are evaluated by `water_access_runtime` into map-wide access/provider masks.
- Consumers check their requirement rules against those masks; `any` rules are the right shape for "well or fountain" requirements.
- Reservoirs and aqueduct tiles participate in the same fixed-point propagation pass. The pass evaluates providers from the previous mask into a fresh next mask, so range-0 node providers do not satisfy themselves and adjacent dry aqueducts do not create access from nothing.
- Legacy fields such as `has_water_access`, `has_well_access`, and `has_latrines_access` are compatibility mirrors projected from the typed masks. New graphics, placement, and gameplay checks should prefer BuildingType water rules or `water_access_runtime_*` accessors.
- Placement/context overlays should query typed providers/requirements. Do not add new `WATER_ACCESS_RUNTIME_TYPE_*` values or provider-type switch branches.

Current supported `<graphics>` child nodes:

- `<default> ... </default>`
- `<variant> ... </variant>`
- `<variant role="..."> ... </variant>`
- `<options selection="stable_variant"> ... </options>`
- `<option image="..." />`
- `<option path="..." image="..." />`
- `<condition type="has_workers" />`
- `<condition type="water_access" />`
- `<condition type="figure_slot_occupied" slot="primary|secondary|quaternary" />`
- `<condition type="resource_positive" resource="wine" />`
- `<condition type="climate" value="central|northern|desert" />`
- `<condition type="monument_upgrade" value="1" />`
- `<condition type="festival_games" value="1|2|3" />`
- `<condition type="desirability" operator="lt|lte|eq|gt|gte" threshold="N" />`
- `<condition type="days1_positive|days1_not_positive|days2_positive|days1_or_days2_positive" />`

`<path value="...">` rules:

- path is relative to the winning `Graphics` folder
- use backslash-delimited logical keys
- do not include the `Graphics\` prefix
- do not include the `.xml` suffix
- example: `Mods\Augustus\Graphics\Health_Culture\Theatre.xml` becomes `Health_Culture\Theatre`

Graphics target examples:

```xml
<graphics>
    <default>
        <path value="Aesthetics\House_Shack" />
        <options selection="stable_variant">
            <option image="Image_0000" />
            <option image="Image_0001" />
        </options>
    </default>
</graphics>
```

Structured `<graphics>` rules:

- `<default>` is required
- `<variant>` entries are checked in XML order
- `<variant role="...">` declares named graphics used by tile/runtime renderers and is not considered by normal building rendering when the role is tile-only metadata
- all `<condition>` nodes inside one `<variant>` must match
- the first matching variant wins
- the `<default>` target is used when no variant matches
- `<path>` and optional `<image>` must live inside `<default>` or `<variant>`
- `<default>` and `<variant>` may contain `<options selection="stable_variant">` instead of one `<image>`, or may contain only options when every option provides `path`
- target selection and option selection are separate: conditions choose the target first, then `building.variant` chooses among that target's options
- each `<option>` selects one equivalent visual using saved `building.variant % option_count`
- an `<option>` inherits the enclosing target `<path>` unless it declares its own `path`
- every resolved option path/image is validated at BuildingType load time
- target-level `<image value="..."/>` is invalid when `<options>` are present; put image ids on the `<option>` nodes instead
- put water refresh, provider radius, network nodes, and requirement rules under the root `<water_access>` block, not under `<graphics>`

Current supported graphics conditions:

- `type="has_workers"` means `num_workers > 0`
- `type="water_access"` means `has_water_access`
- `type="figure_slot_occupied" slot="primary|secondary|quaternary"` means the named tracked legacy figure slot is occupied
- `type="resource_positive" resource="wine"` means the building has at least one unit of that resource
- `type="climate" value="central|northern|desert"` compares the active scenario climate
- `type="monument_upgrade" value="N"` means the completed monument has upgrade/module `N`
- `type="festival_games" value="1|2|3"` compares the active Colosseum games mode: `1` is naumachia, `2` is imperial games, and `3` is executions
- `type="desirability" operator="lt|lte|eq|gt|gte" threshold="N"` compares the building desirability against `N`
- `type="days1_positive|days1_not_positive|days2_positive|days1_or_days2_positive"` checks the entertainment visit-day counters used by entertainers

Current supported `<construction>` attributes and child nodes:

- missing `<construction>` or `mode="instant"` means placement uses finished graphics immediately
- `mode="instant"` may contain direct `<requirement>` nodes for placement-time resource costs; money cost stays in `<model cost="N" />`
- `mode="phased"` starts the building at `MONUMENT_START`
- `road_update_radius="N"` updates nearby roads when the phased monument is placed
- `free_when_broke_limit="N"` allows placement while out of money until total city count for that BuildingType reaches `N`
- `max_count="N"` blocks placement once the city already has `N` active/created/mothballed buildings of that BuildingType
- `max_count_unless_config="multiple_barracks"` disables `max_count` while that gameplay config is enabled
- `requires_building="building_attr"` blocks placement until the referenced BuildingType exists in the city
- `<phase index="N"> ... </phase>` defines one construction phase
- `<phase><graphics> ... </graphics></phase>` uses direct `<path>` and optional `<image>` target nodes; phase graphics do not currently support `<default>`, `<variant>`, or `<options>`
- `<requirement type="architects|stone|timber|concrete|marble|bricks|gold|iron" amount="N" />` declares phase delivery requirements
- `<construction mode="instant"><requirement type="stone|timber|concrete|marble|bricks|gold|iron" amount="N" /></construction>` declares placement-time resource costs

Phased construction rules:

- phase indexes are authored in ascending one-based order
- root-level `<graphics>` describes the completed building
- phase-level `<graphics>` describes only the unfinished phase
- a phased definition with `N` phase nodes has `N + 1` monument states; advancing past the last phase marks the monument finished

Current supported `<labor>` child nodes:

- `<employees count="N" />`
- `<labor_seeker> ... </labor_seeker>`

Current supported `<labor_seeker>` child nodes:

- `<method value="houses_spawn_if_below|houses_generate_if_below|workforce" />`
- `<amount value="N" />`

`<method>` is required whenever a `<labor_seeker>` node exists. Omit the entire
`<labor_seeker>` node for buildings that use global-access or otherwise
legacy-owned labor and do not send labor seekers.

`method="houses_spawn_if_below"` preserves the normal vanilla labor-seeker path:
the walker counts nearby housing into `houses_covered`, then city-wide labor
allocation fills `num_workers`. Use an explicit `<amount>` when the coverage
threshold should be different from the employee count. `<amount>` is required
for this method.

`method="houses_generate_if_below"` preserves the legacy direct-generation
exception used by vanilla entertainment buildings. `<amount>` is required for
this method.

`method="workforce"` uses local workforce acquisition. The building is excluded
from city-wide labor category allocation while global labor is disabled, and its
labor seeker targets nearby unemployed house residents instead of counting house
coverage. Global labor overrides workforce and uses city-wide labor allocation.
Production methods also consume this labor policy: when global labor is disabled,
native production checks local workforce access for `method="workforce"` buildings
instead of the legacy decaying `houses_covered` value. `<amount>` is optional for
this method and defaults to the building's `<employees count="N" />` value when
omitted.

Current supported `<storages>` child nodes:

- `<storage path="..." />`

Current supported `<production_methods>` child nodes:

- `<production_method path="..." />`

Current supported `<housing>` attributes:

- `path="..."` references a `HousingType` definition
- `capacity="N"` is required and stores the complete building's residential capacity
- `evolve_to="..."`, `devolve_to="..."`, `merge_to="..."`, and `split_to="..."` are optional BuildingType text-id transitions

Current supported `<vacant_lot>` attributes:

- `fill_to="..."` references the BuildingType that an empty vacant lot becomes when residents first enter

Housing rules:

- Footprint remains on BuildingType via `<model size="N" />`
- Residential capacity is a whole-building BuildingType value; do not derive it from tile count
- Residential requirements, tax multiplier, prosperity, and resident class live in the referenced HousingType
- Any non-empty transition target must resolve to an existing BuildingType during load
- Vacant lots are declared as their own BuildingType with normal level-0 `<housing ... />` data and a `<vacant_lot fill_to="house_small_tent" />` node; do not assign the vacant-house tool directly to `house_small_tent`
- Vacant lots are direct-tool special cases and should not declare generated buttons
- Native housing BuildingTypes use their string ids directly; the compatibility layer maps those ids to legacy `house_level` values only where old runtime fields still need a level
- Vespasian, Augustus, and Julius native house chains include every legacy level through `house_luxury_palace`; 1x1 levels define explicit `_2x2` merged BuildingTypes so save/load and evolution can choose by string id plus footprint.

Shared definition path rules:

- path is relative to the winning `StorageType`, `ProductionMethod`, or `HousingType` folder
- do not include the folder prefix
- do not include the `.xml` suffix
- example: `Mods\Vespasian\ProductionMethod\pottery_workshop_basic.xml` becomes `pottery_workshop_basic`

Current supported `<spawn_group>` attributes:

- `road_access="normal"`
- `delay_bands="100:3,75:7,50:15,25:29,1:44"` as a comma-separated list of `worker_percentage:delay` pairs
- `existing_figure="actor|barber|bathhouse_worker|charioteer|doctor|engineer|gladiator|librarian|lion_tamer|prefect|priest|school_child|surgeon|tax_collector|teacher|work_camp_architect|work_camp_worker"`
- `guard_timing="before_road_access|after_labor_seeker"`

`existing_figure` may also be a comma-separated list, such as `actor,gladiator` for amphitheaters or `gladiator,lion_tamer` for arenas. The list is checked against the tracked legacy primary figure slot as one group guard, so alternate performer types block one another without clearing the slot just because the first listed type does not match.

`delay_bands` sanity rules:

- worker percentage must be an integer from `1` to `100`
- delay must be a non-negative integer
- pairs must be written in strictly descending worker-percentage order
- delay values are authored in legacy 50-tick-day units and scale at runtime to the active calendar day length

Current supported `<spawn>` modes:

- `mode="profiled_figure"` for generic BuildingType-owned figure spawns
- `mode="service_roamer"` as the legacy XML spelling for the same generic figure-spawn path
- `mode="temple_supplier"`
- `mode="temple_destination_priest"`
- `mode="temple_mars_mess_hall_priest"`
- `mode="temple_neptune_chariot"`
- `mode="grand_temple_mars_recruit"`

Current supported `<spawn>` attributes:

- `spawn_figure="..."` using the same identifiers; required for generic figure-spawn modes
- `action_state="roaming|engineer_created|prefect_created|tax_collector_created|entertainer_roaming|entertainer_school_created|work_camp_worker_created|work_camp_architect_created"`; required for generic figure spawns that do not use `profile`
- `direction="top|bottom"`
- `figure_slot="primary|secondary|quaternary|none"`
- `spawn_count="N"` for one policy spawning the same figure several times on one trigger
- `init_roaming="true|false"`
- `graphic_timing="none|on_spawn_entry|before_delay_check|before_successful_spawn"`
- `require_water_access="true|false"`
- `mark_problem_if_no_water="true|false"`
- `condition="always|days1_positive|days1_not_positive|days2_positive|days1_or_days2_positive"`
- `block_on_success="true|false"`
- `profile="..."` for native FigureType spawns
- `chance_per_million="N"` for a constant probability gate from `0` to `1000000`
- `chance_source="city_unemployment_percent|house_unemployed_workers"` for data-driven probability gates
- `chance_per_million_bands="20:31250,19:20833,..."` as a descending list of `minimum_source_value:chance_per_million` pairs
- `chance_divisor="N"` to use `chance_source / N`, clamped to 100%

Chance gates are checked after road, water, and condition gates but before figure creation. A `<spawn>` may use only one chance policy: constant `chance_per_million`, source bands, or source divisor.

Current engine behavior:

- Repo-owned BuildingType graphics use only the structured `<graphics>` schema.
- A `spawn_group` owns the shared delay/guard phase, then runs its child `<spawn>` policies in order.
- Any BuildingType with spawn groups uses the runtime spawn path, including housing.
- Temple-specific spawn modes preserve existing religion module behavior while moving temple spawn selection into BuildingType XML. Standard temple priest roamers still use the generic figure-spawn path.
- Delay evaluation now uses the explicit `delay_bands` data from XML rather than a hardcoded named profile.
- Ordered policies can coordinate: a policy that succeeds with `block_on_success="true"` stops later sibling policies in the same group.
- Use `block_on_success="true"` when a building should spawn either A or B on the same trigger.
- For alternate performer venues, put all mutually exclusive performer types in the parent `existing_figure` list and keep their child policies in the same `spawn_group`.
- Use `spawn_count="N"` when one successful policy should create several copies of the same figure at once.
- Today a multi-spawn policy only writes one legacy tracked figure slot; extra spawned figures still exist, but they are not separately tracked by XML-defined slots yet.
- Use `<image value="..."/>` when a graphics group contains several named members and the building must lock to one of them.
- Use `<options selection="stable_variant">` when several image entries are equivalent visual variants. Runtime selection is stable per building and uses `building.variant % option_count`.
- New buildings seed native graphics options from `map_random_get(grid_offset)`. Loaded saves from `0xb6` or earlier also reseed because older `building.variant` values did not mean native graphics options; newer saves preserve and clamp the saved value.
- Graphics-only vertical-slice definitions are valid; runtime-owned production and storage references can be layered onto those same BuildingType files as they migrate.
- BuildingType native storage and production references are resolved at load time; unresolved paths are hard load failures.
- Put shared derived state such as water access under the root `<water_access>` block so graphics and spawn behavior read the same runtime facts.
- Put provider-side water coverage and connection-node data under the same `<water_access>` block so the native water runtime stays data-driven.
- Put BuildingType-authored employee defaults under `<labor><employees ... /></labor>` so the XML and live model table stay in sync.
- Buildings with a validated runtime `BuildingType` graphics block usually use the native runtime renderer path as the authoritative live path; current data-only vertical slices remain on legacy live rendering until their runtime rollout lands.

Residential spawn examples:

```xml
<spawn_group road_access="normal" delay_bands="100:0">
    <spawn mode="profiled_figure" spawn_figure="beggar" profile="unemployment_wanderer" direction="bottom" figure_slot="quaternary" chance_source="house_unemployed_workers" chance_divisor="24" />
</spawn_group>
```

```xml
<spawn_group road_access="normal" delay_bands="100:0">
    <spawn mode="profiled_figure" spawn_figure="patrician" profile="house_roamer" direction="bottom" figure_slot="quaternary" chance_per_million="24390" />
</spawn_group>
```

Residential walkers use `figure_slot="quaternary"` so the house's primary slot remains available for legacy homeless/undo behavior. FigureType profile references in BuildingType spawn XML are validated after FigureType XML load.

Current raw-material producer notes:

- Julius, Augustus, and Vespasian define BuildingType XML for `clay_pit`, `marble_quarry`, `iron_mine`, and `timber_yard`.
- Augustus and Vespasian additionally define XML for `gold_mine`, `stone_quarry`, and `sand_pit`; Julius does not because those resources/buildings are Augustus-era content.
- The four Julius-era raw producers intentionally use Julius extracted graphics payloads when loaded by Augustus or Vespasian. Augustus/Vespasian-specific gold, stone, and sand producers use Augustus extracted payloads.
- These buildings are placeable build-menu entries under `raw_materials`. Their XML uses `foundation policy="custom"` plus a `<terrain>` child for the legacy terrain gates: rock for marble/iron/gold/stone, tree adjacency/coverage for timber, water adjacency for clay, and distant/open-water adjacency for sand.
- Raw-material producers attach native one-output `ProductionMethod` XML and matching output `StorageType` XML. Gold mine production uses `<treasury_cost amount="600" />` to preserve the legacy per-cycle finance charge.
- The raw-producer-specific terrain cases were removed from `src/building/construction.cpp::set_required_terrain`; the function now reads these requirements from BuildingType XML when present.

See `_template.xml.example` here in `Mods\Vespasian\BuildingType` for a copy/paste starter.
