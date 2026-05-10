# FigureType XML

The loader reads every `*.xml` file in the winning `FigureType` folders using this precedence:

1. active mod
2. `Mods\Augustus\FigureType`
3. `Mods\Julius\FigureType`

Later fallback definitions are allowed, but the first resolved figure type wins. Templates/examples belong in non-`.xml` files so they do not load as live data.

Current supported shape:

- `<figure type="...">`
- `<profiles default="...">`
- `<profile id="...">`
- `<native class="..." />`
- `<owner ... />`
- `<movement ... />`
- `<pathing ... />`
- `<venue_targets ...>` with `<venue ... />` children for entertainer venue seekers; `show_duration` is stored and displayed as active calendar days, and runtime ranking uses `2 * show_days + route_distance`
- `<graphics image_group="..." max_image_offset="N" base_image_offset="N" static_frame_count="N" corpse_image_group="..." corpse_base_image_offset="N" />` at figure level. Optional `base_image_offset` defaults to zero, optional `static_frame_count` chooses one still frame by `figure id % N`, and optional corpse attributes let profiles use a different corpse row.

Buildings select a native profile with `profile="..."` on their `<spawn>`. The building only chooses the profile; the figure profile owns the native class, owner contract, movement, pathing mode, and road-history effect.

For range tuning, pair
[Preindustrial Walking Service Ranges](../../../docs/preindustrial_walking_service_ranges.md)
with [Tile Scale and Walker Timescale](../../../docs/tile_scale_and_walker_timescale.md).
For deciding which services should be very local, ordinary neighborhood range,
or broader patrol/maintenance range, see
[Roman City Facility Ratios](../../../research/roman_city_facility_ratios.md)
and [Roman Building and Infrastructure Maintenance Needs](../../../research/roman_building_maintenance_needs.md).

Current supported figure ids include `labor_seeker`, `engineer`, `prefect`, `priest`, `patrician`, `beggar`, `market_trader`, `market_supplier`, `delivery_boy`, `teacher`, `librarian`, `barber`, `bathhouse_worker`, `school_child`, `actor`, `gladiator`, `lion_tamer`, and `charioteer`.

Current supported native classes are `roaming_service`, `engineer_service`, `prefect_service`, `market_supplier`, `delivery_follower`, `entertainment_venue_seeker`, `entertainment_service`, and `transient_wanderer`.

Current supported `<pathing>` values:

- `mode="vanilla_roaming"`
- `mode="smart_service" effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor|religion_ceres|religion_neptune|religion_mercury|religion_mars|religion_venus|religion_pantheon|entertainment_theater|entertainment_amphitheater_actor|entertainment_amphitheater_gladiator|entertainment_arena_gladiator|entertainment_arena_lion|entertainment_colosseum_gladiator|entertainment_colosseum_lion|entertainment_hippodrome|market_goods"`
- `mode="nearest_unemployed"`
- `mode="venue_seeker"`
- `mode="storage_fetch"`
- `mode="follow_leader"`
- `mode="stand_still"`
- `mode="transient_wander"`

Pathing modes are implemented as `PathingMode` objects with their own requirements. Current native pathing profiles require road-only movement because those mode objects set `requires_road`: use `terrain_usage="roads"` or `terrain_usage="roads_highway"`. Off-road-capable modes such as `any`, `prefer_roads`, and `prefer_roads_highway` are rejected because the native pathing contracts are road-route, road-roaming, or road-following policies.

Priests use explicit profiles such as `ceres_service`, `mars_service`, and `pantheon_service`; there is no owner-derived religion effect in XML. Pantheon still records Pantheon plus all five god histories at runtime.

Entertainment training buildings spawn `venue_seeker` profiles. Venues spawn generic `entertainment_service` native profiles such as `theater_service` or `arena_service`; each profile carries its exact smart-service effect in `<pathing>`.

Markets keep their existing BuildingType spawn flow for now, but the spawned figures rebind into FigureType profiles. `market_trader` uses the generic `roaming_service`; Vespasian uses `smart_service` with `market_goods`, while fallback definitions keep `vanilla_roaming`. `market_supplier` uses `native class="market_supplier"` with `pathing mode="storage_fetch"` to route to storage, reroute only to clearly better storage, create delivery followers, and return. `delivery_boy` uses `native class="delivery_follower"` with `pathing mode="follow_leader"` and follows the saved `leading_figure_id` chain.

Roaming access checks follow the profile movement type. A `roads` service profile considers roads and access ramps; a `roads_highway` service profile also considers highways. Use `roads_highway` only when that service should actually spread over highway networks, such as Vespasian `hippodrome_service`.

Residential walker support:

- `patrician` uses profile `house_roamer` with `roaming_service`, `vanilla_roaming`, road-only movement, and `return_mode="return_to_owner_road"`.
- `beggar` uses profile `unemployment_wanderer` with `transient_wanderer`, `stand_still`, `terrain_usage="roads_highway"`, and `return_mode="die_at_limit"`.
- Housing BuildingType XML owns when these figures spawn. Any missing profiled BuildingType spawn reference is a FigureType load failure after all FigureType XML has loaded.
- Residential walkers do not declare a road service `effect`, because they do not provide coverage and should not write road-service history.
- `<graphics base_image_offset="N" />` offsets the resolved image group base. Beggars also use `static_frame_count="8"` for Julius-style still-frame variation and `corpse_image_group="labor_seeker"` for their corpse row.
- See [Walker Pathing Runtime](../../../docs/walker_pathing_runtime.md) for the fuller migration note and save-load inference concerns.

Related implementation notes:

- [Walker Pathing Runtime](../../../docs/walker_pathing_runtime.md) explains runtime flow, save compatibility, and effect-id rules.
- [Tile Scale and Walker Timescale](../../../docs/tile_scale_and_walker_timescale.md) converts `max_roam_length` into approximate tiles, meters, game days, and wall-clock time.
- [Preindustrial Walking Service Ranges](../../../docs/preindustrial_walking_service_ranges.md) gives historical walking-city guidance for `max_roam_length` tuning.
- `codex_augustus_repo_map_memory.md` indexes the native walker chokepoints for future sessions.
