# FigureType XML

The loader reads every `*.xml` file from every active mod layer in `mod-list` order, from the lowest dependency through the selected mod. A later definition with the same stable figure type replaces the earlier winner; an upper mod inherits every definition it does not replace. FigureType folders are optional, so a sparse dependent mod may omit the folder entirely. Templates/examples belong in non-`.xml` files so they do not load as live data.

Current supported shape:

- `<figure type="...">`
- `<profiles default="...">`
- `<profile id="...">`
- `<native class="..." />`
- `<owner ... />`
- `<movement ... />`
- `<pathing ... />`
- `<venue_targets ...>` with `<venue ... />` children for entertainer venue seekers; `show_duration` is stored and displayed as active calendar days, and runtime ranking uses `2 * show_days + route_distance`
- `<graphics>` at figure level.
- `<figure type="..." graphics_only="true">` publishes presentation without a runtime profile. It must contain `<graphics>`, must not contain `<profiles>`, and is appropriate when legacy movement or combat still owns action dispatch.
- Figure graphics use one strict logical-asset reference:

  ```xml
  <graphics>
      <default>
          <path value="Walkers\architect" />
      </default>
  </graphics>
  ```

- The path names an asset XML without a `.xml` suffix. It never names a PNG, an absolute legacy image id, or a synthetic `Sequences` namespace.
- FigureType selects the conceptual asset only. Named entries, animations, corpse states, direction rows, and composite layers belong together in that asset XML and are selected by the native runtime through generic graphics functions.
- Runtime-extracted logical assets are the default source. Authored bridge assets are permitted only when an old, non-inferable schedule or composition cannot be expressed by the extracted metadata. Such a bridge belongs in the lowest relevant mod and contains references and semantics only, never extracted proprietary payloads.
- Legacy FigureType graphics policies and source attributes are migration errors. Do not add `image_group`, `image_asset`, `runtime_selected_*`, `path_pattern`, `image_pattern`, `hippodrome_race`, or similar figure-specific graphics schema.

Buildings select a native profile with `profile="..."` on their `<spawn>`. The building only chooses the profile; the figure profile owns the native class, owner contract, movement, pathing mode, and road-history effect.

For range tuning, pair
[Preindustrial Walking Service Ranges](../../../docs/preindustrial_walking_service_ranges.md)
with [Tile Scale and Walker Timescale](../../../docs/tile_scale_and_walker_timescale.md).
For deciding which services should be very local, ordinary neighborhood range,
or broader patrol/maintenance range, see
[Roman City Facility Ratios](../../../research/roman_city_facility_ratios.md)
and [Roman Building and Infrastructure Maintenance Needs](../../../research/roman_building_maintenance_needs.md).

Current supported figure ids include `cart_pusher`, `warehouseman`, `docker`, `trade_ship`, `fishing_boat`, `labor_seeker`, `engineer`, `prefect`, `priest`, `doctor`, `surgeon`, `tax_collector`, `missionary`, `patrician`, `beggar`, `market_trader`, `market_supplier`, `delivery_boy`, `teacher`, `librarian`, `barber`, `bathhouse_worker`, `school_child`, `actor`, `gladiator`, `lion_tamer`, `charioteer`, `fort_standard`, `map_flag`, and `hippodrome_horses`.

Current supported native classes are `roaming_service`, `engineer_service`, `prefect_service`, `market_supplier`, `delivery_follower`, `entertainment_venue_seeker`, `entertainment_service`, and `transient_wanderer`.

Current supported `<pathing>` values:

- `mode="vanilla_roaming"`
- `mode="smart_service" effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor|religion_ceres|religion_neptune|religion_mercury|religion_mars|religion_venus|religion_pantheon|entertainment_theater|entertainment_amphitheater_actor|entertainment_amphitheater_gladiator|entertainment_arena_gladiator|entertainment_arena_lion|entertainment_colosseum_gladiator|entertainment_colosseum_lion|entertainment_hippodrome|market_goods|doctor|surgeon|tax_collector"`
- `mode="nearest_unemployed"`
- `mode="venue_seeker"`
- `mode="storage_fetch"`
- `mode="follow_leader"`
- `mode="stand_still"`
- `mode="transient_wander"`

Pathing modes are implemented as `PathingMode` objects with their own requirements. Current native pathing profiles require road-only movement because those mode objects set `requires_road`: use `terrain="roads"` or `terrain="roads_highway"` on `<pathing>`. Off-road-capable modes such as `any`, `prefer_roads`, and `prefer_roads_highway` are rejected because the native pathing contracts are road-route, road-roaming, or road-following policies.

Priests use explicit profiles such as `ceres_service`, `mars_service`, and `pantheon_service`; there is no owner-derived religion effect in XML. Pantheon still records Pantheon plus all five god histories at runtime.

Entertainment training buildings spawn `venue_seeker` profiles. Venues spawn generic `entertainment_service` native profiles such as `theater_service` or `arena_service`; each profile carries its exact smart-service effect in `<pathing>`.

Markets keep their existing BuildingType spawn flow for now, but the spawned figures rebind into FigureType profiles. `market_trader` uses the generic `roaming_service`; Vespasian uses `smart_service` with `market_goods`, while fallback definitions keep `vanilla_roaming`. `market_supplier` uses `native class="market_supplier"` with `pathing mode="storage_fetch"` to route to storage, reroute only to clearly better storage, create delivery followers, and return. `delivery_boy` uses `native class="delivery_follower"` with `pathing mode="follow_leader"` and follows the saved `leading_figure_id` chain.

Doctor, surgeon, and tax collector profiles keep `native class="legacy_action"` because their callbacks still own plague healing or building entry/exit states. Their XML profile still owns range, graphics, and smart-service pathing; the runtime binds the profile for legacy-action walkers without taking over their action callback.

Roaming access checks follow the profile movement type. A `roads` service profile considers roads and access ramps; a `roads_highway` service profile also considers highways. Use `roads_highway` only when that service should actually spread over highway networks, such as Vespasian `hippodrome_service`.

Residential walker support:

- `patrician` uses profile `house_roamer` with `roaming_service`, `vanilla_roaming`, road-only movement, and `return_mode="return_to_owner_road"`.
- `beggar` uses profile `unemployment_wanderer` with `transient_wanderer`, `stand_still`, `<pathing terrain="roads_highway">`, and `return_mode="die_at_limit"`.
- Housing BuildingType XML owns when these figures spawn. Any missing profiled BuildingType spawn reference is a FigureType load failure after all FigureType XML has loaded.
- Residential walkers do not declare a road service `effect`, because they do not provide coverage and should not write road-service history.
- Residential presentation follows the same logical-asset contract as every other figure type; state and corpse variants belong in the referenced conceptual asset.
- See [Walker Pathing Runtime](../../../docs/walker_pathing_runtime.md) for the fuller migration note and save-load inference concerns.

Related implementation notes:

- [Walker Pathing Runtime](../../../docs/walker_pathing_runtime.md) explains runtime flow, save compatibility, and effect-id rules.
- [Tile Scale and Walker Timescale](../../../docs/tile_scale_and_walker_timescale.md) converts `max_roam_length` into approximate tiles, meters, game days, and wall-clock time.
- [Preindustrial Walking Service Ranges](../../../docs/preindustrial_walking_service_ranges.md) gives historical walking-city guidance for `max_roam_length` tuning.
- `codex_augustus_repo_map_memory.md` indexes the native walker chokepoints for future sessions.
