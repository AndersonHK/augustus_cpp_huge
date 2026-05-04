# BuildingType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates/examples in non-`.xml` files so they do not get loaded as live data.

Templates and examples are maintained only in `Mods\Vespasian\BuildingType`.
`Mods\Augustus\BuildingType` keeps live XML data only.

Runtime/save identity is migrating away from stable enum slots. New saves include a `building_type_table` that maps compact save ids to BuildingType text ids, while loaded buildings continue to use compact runtime ids. Old saves without the table migrate through `src\building\building_type_legacy_migration.*`.

Current supported nodes:

- `<identity ... />`
- `<model ... />`
- `<desirability> ... </desirability>`
- `<foundation ... />`
- `<button ... />`
- `<sound ... />`
- `<event_data ... />`
- `<flags ... />`
- `<water_access> ... </water_access>`
- `<state> ... </state>`
- `<graphics> ... </graphics>`
- `<construction> ... </construction>`
- `<labor> ... </labor>`
- `<storages> ... </storages>`
- `<production_methods> ... </production_methods>`
- `<spawn_group ...>`
- `<spawn ... />`

Current supported `<identity>` attributes:

- `name_key="translation.key"` stores the localized building name key for generated UI
- `translation_key="translation.key"` is accepted as an alias

Current supported `<model>` attributes:

- `size="N"`
- `cost="N"`

Model `cost` currently overrides the legacy runtime model after XML load. `size` is parsed and exposed for the next placement/build-authority pass, but legacy placement still owns footprint size until that pass lands.

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
    <identity name_key="building.barber.name" />
    <model size="1" cost="25" />
    <desirability>
        <value value="2" />
        <step value="1" />
        <step_size value="-1" />
        <range value="2" />
    </desirability>
    <foundation policy="land" />
    <button group="health" order="40" icon="barber" text_key="building.barber.name" />
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

- `policy="land|road|water|shoreline|aqueduct|custom"` stores the placement/foundation policy key for the next construction pass

Current supported `<foundation>` child nodes:

- `<terrain value="meadow|rock|tree|water|wall|distant_water" />`

Foundation terrain rules:

- `<terrain>` may appear zero or more times
- terrain requirements feed the existing placement warning/check path
- use `rock` for quarry/mine placement, `tree` for timber yards, `water` for clay pits, and `distant_water` for sand pits

Current supported `<button>` attributes:

- `group="..."` stores the target build submenu key
- `order="N"` stores generated button ordering
- `icon="..."` stores the generated button icon key
- `text_key="..."` optionally overrides the button text key; otherwise generated UI can use `<identity name_key="...">`

`<menu>` is accepted as a temporary alias for `<button>` only during this migration slice. Prefer `<button>` in new XML.

Current supported `<sound>` attributes:

- `id="..."`, `value="..."`, or `city="..."` selects the city ambient sound key
- `mute_on_enemies="true|false"` suppresses the sound while enemies are active
- `always_play="true|false"` keeps the sound audible even when normal worker/water gates would mute it

Raw-material producer sound ids currently include `clay_pit`, `iron_mine`, `timber_yard`, and `marble_quarry`. Gold mine, stone quarry, and sand pit do not declare sounds because the legacy properties do not assign city sounds to them.

Current supported `<event_data>` attributes:

- `attr="..."` stores the scenario-event/query building attribute key

Current supported `<flags>` attributes:

- `fire_proof="0|1"`
- `draw_desirability_range="0|1"`
- `venus_gt_bonus="0|1"`

Current supported `<state>` child nodes:

- `<water_access mode="reservoir_range" />`

Current supported root-level `<water_access>` child nodes:

- `<type value="well|fountain|reservoir" />`
- `<range value="N" />`
- `<requirement value="none|reservoir_network|water_source_any|water_source_fresh_only" />`
- `<node kind="aqueduct_connection" x="N" y="N" />`

Root-level `<water_access>` rules:

- `<type>` and `<range>` are required
- `<requirement>` is optional and defaults to `none`
- `<node>` is optional and may appear more than once
- `x` and `y` are local tile coordinates relative to the building's top-left footprint tile
- node coordinates may sit outside the footprint
- `kind="aqueduct_connection"` is the only supported node kind in this slice

Current supported `<graphics>` child nodes:

- `<default> ... </default>`
- `<variant> ... </variant>`
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

Structured `<graphics>` rules:

- `<default>` is required
- `<variant>` entries are checked in XML order
- all `<condition>` nodes inside one `<variant>` must match
- the first matching variant wins
- the `<default>` target is used when no variant matches
- `<path>` and optional `<image>` must live inside `<default>` or `<variant>`
- put water refresh rules under `<state>`, not under `<graphics>`
- put provider-side water radius/network data under the root `<water_access>` block, not under `<graphics>`

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
- `<phase index="N"> ... </phase>` defines one construction phase
- `<phase><graphics> ... </graphics></phase>` uses the same `<path>` and `<image>` target nodes as root graphics
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

- `<method value="none|houses_spawn_if_below|houses_generate_if_below|workforce" />`
- `<amount value="N" />`

`<method>` is required. `<amount>` is optional and defaults to the
building's `<employees count="N" />` value when omitted. If a labor seeker has no
`<amount>` node, the enclosing `<labor>` node must also define `<employees>`.

`method="houses_spawn_if_below"` preserves the normal vanilla labor-seeker path:
the walker counts nearby housing into `houses_covered`, then city-wide labor
allocation fills `num_workers`. Use an explicit `<amount>` when the coverage
threshold should be different from the employee count.

`method="houses_generate_if_below"` preserves the legacy direct-generation
exception used by vanilla entertainment buildings.

`method="none"` explicitly marks a labor-using building as having no labor
seeker policy. It is used for global-access or otherwise legacy-owned labor.

`method="workforce"` uses local workforce acquisition. The building is excluded
from city-wide labor category allocation while global labor is disabled, and its
labor seeker targets nearby unemployed house residents instead of counting house
coverage. Global labor overrides workforce and uses city-wide labor allocation.
Production methods also consume this labor policy: when global labor is disabled,
native production checks local workforce access for `method="workforce"` buildings
instead of the legacy decaying `houses_covered` value.

Current supported `<storages>` child nodes:

- `<storage path="..." />`

Current supported `<production_methods>` child nodes:

- `<production_method path="..." />`

Shared definition path rules:

- path is relative to the winning `StorageType` or `ProductionMethod` folder
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

- `mode="service_roamer"`
- `mode="temple_supplier"`
- `mode="temple_destination_priest"`
- `mode="temple_mars_mess_hall_priest"`
- `mode="temple_neptune_chariot"`
- `mode="grand_temple_mars_recruit"`

Current supported `<spawn>` attributes:

- `spawn_figure="..."` using the same identifiers; required for `service_roamer`
- `action_state="roaming|engineer_created|prefect_created|tax_collector_created|entertainer_roaming|entertainer_school_created|work_camp_worker_created|work_camp_architect_created"`; required for `service_roamer`
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

Current engine behavior:

- Repo-owned BuildingType graphics use only the structured `<graphics>` schema.
- A `spawn_group` owns the shared delay/guard phase, then runs its child `<spawn>` policies in order.
- Temple-specific spawn modes preserve existing religion module behavior while moving temple spawn selection into BuildingType XML. Standard temple priest roamers still use `service_roamer`.
- Delay evaluation now uses the explicit `delay_bands` data from XML rather than a hardcoded named profile.
- Ordered policies can coordinate: a policy that succeeds with `block_on_success="true"` stops later sibling policies in the same group.
- Use `block_on_success="true"` when a building should spawn either A or B on the same trigger.
- For alternate performer venues, put all mutually exclusive performer types in the parent `existing_figure` list and keep their child policies in the same `spawn_group`.
- Use `spawn_count="N"` when one successful policy should create several copies of the same figure at once.
- Today a multi-spawn policy only writes one legacy tracked figure slot; extra spawned figures still exist, but they are not separately tracked by XML-defined slots yet.
- Use `<image value="..."/>` only when a graphics group contains several named members and the building must lock to one of them.
- Graphics-only vertical-slice definitions are valid; runtime-owned production and storage references can be layered onto those same BuildingType files as they migrate.
- BuildingType native storage and production references are resolved at load time; unresolved paths are hard load failures.
- Put shared derived state such as water access under `<state>` so graphics and spawn behavior read the same runtime facts.
- Put provider-side water coverage and connection-node data under the root `<water_access>` block so the native water runtime stays data-driven.
- Put BuildingType-authored employee defaults under `<labor><employees ... /></labor>` so the XML and live model table stay in sync.
- Buildings with a validated runtime `BuildingType` graphics block usually use the native runtime renderer path as the authoritative live path; current data-only vertical slices remain on legacy live rendering until their runtime rollout lands.

Current raw-material producer notes:

- Julius, Augustus, and Vespasian define BuildingType XML for `clay_pit`, `marble_quarry`, `iron_mine`, and `timber_yard`.
- Augustus and Vespasian additionally define XML for `gold_mine`, `stone_quarry`, and `sand_pit`; Julius does not because those resources/buildings are Augustus-era content.
- The four Julius-era raw producers intentionally use Julius extracted graphics payloads when loaded by Augustus or Vespasian. Augustus/Vespasian-specific gold, stone, and sand producers use Augustus extracted payloads.
- These buildings are placeable build-menu entries under `raw_materials`. Their XML uses `foundation policy="custom"` plus a `<terrain>` child for the legacy terrain gates: rock for marble/iron/gold/stone, tree adjacency/coverage for timber, water adjacency for clay, and distant/open-water adjacency for sand.
- Raw-material producers attach native one-output `ProductionMethod` XML and matching output `StorageType` XML. Gold mine production uses `<treasury_cost amount="600" />` to preserve the legacy per-cycle finance charge.
- The raw-producer-specific terrain cases were removed from `src/building/construction.cpp::set_required_terrain`; the function now reads these requirements from BuildingType XML when present.

See `_template.xml.example` here in `Mods\Vespasian\BuildingType` for a copy/paste starter.
