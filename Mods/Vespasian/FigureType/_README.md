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
- `<graphics>` at figure level.
- `<figure type="..." graphics_only="true">` publishes presentation without a runtime profile. It must contain `<graphics>`, must not contain `<profiles>`, and is appropriate when legacy movement or combat still owns action dispatch.
- `<missile_launcher cursor="attack_image_offset|missile_wait_ticks" frame_divisor="N" frames="comma-separated frames" after_frame="N" />` owns launcher-pose selection. Frame values are `-1..255`; `-1` is an explicit no-launcher sentinel. The final `after_frame` applies after the authored leading sequence and preserves the old long-tail behavior without 128-entry C++ tables.
- `<resource_cart empty_image_group="..." resource_source="resource_id|collecting_item_on_action" resource_action="..." load_mode="fixed_one|carried_loads|carried_or_one" state_source="action|runtime_state" suppress_body_when_hidden="true|false" lift_food_at_loads="N" lift_food_y_adjust="N" offsets_x="8 values" offsets_y="8 values" hide_on_corpse="true|false" />` owns one cart-overlay family. Threshold and shift must both be authored and are enabled or disabled as a pair. Action-derived policies select presentation directly from the figure action. Runtime-state policies bind an owner-specific `FigureGraphicsState`; controllers publish only hidden, empty, or resource, while XML selects the exact cart image, loads, offsets, ordering, and body suppression. `cart_image_id` is synthesized and hydrated only as the existing save bridge.
- `<directional image_group="..." default_base_offset="N" view_adjustments="N" frame_divisor="N" frame_stride="N">` owns a direction-major legacy sprite family without controller image arithmetic. Optional unique `<pose role="..." action="..." base_offset="N" />` children replace the default atlas row for named actions. Fishing boats use the `fishing_boat_fishing` pose; trade ships use the default pose and retain their runtime-selected dock-facing direction.
- `<graphics><default><path value="Walkers\..." /></default></graphics>` is the preferred authored shape for native FigureType graphics. Add `<image value="..." />` when the target needs an explicit image id or pattern during migration.
- `<action state="..." min_wait_ticks="...">` and `<corpse frame_count="...">` use the same nested `<path>` and optional `<image>` children as `<default>`. Corpse source and available frame count are FigureType data; the universal death-lifecycle clock selects frames at the preserved wait-tick boundaries `0,2,4,6,8,32,72,152` without a per-figure controller or lookup table.
- `<overlay role="..." image_group="..." direction="static|figure" frame="static|figure" direction_stride="N" resource_base_offset="N" resource_stride="N" actions="any|comma-separated action names" offsets_x="8 values" offsets_y="8 values" hide_on_corpse="true|false" />` declares a named legacy-atlas layer. Roles must be unique within one FigureType. An empty `actions="any"` policy is visible in every action; a named list restricts visibility. `resource_base_offset + resource_stride * resource_id` selects resource-major atlas blocks. The two offset arrays own attachment geometry for all eight normalized directions, so overlays do not use `cart_image_id` or a shared C++ cart-offset table.
- `<standard moving_frame_divisor="N">` with unique `<flag unit="..." image_group="..." moving_base="..." halted_frame="..." />` children is valid only for `fort_standard`. It owns the animated unit banner and stacked legion badge layers; unsupported auxiliary formations retain their existing pole-only fallback.
- `<state role="..." action="..." image_group="..." base_offset="N" direction="movement|attack" view_adjustments="N" frame="static|image_offset|missile_launcher" frame_divisor="N" direction_stride="N" />` declares a complete action-state sprite layer. Roles and action states must be unique. `frame="missile_launcher"` requires a sibling `<missile_launcher>` definition and uses its resolved frame as the state-layer cursor. Prefect water-bucket states retain their legacy direction/atlas policy, while ballista idle, firing, and terminal corpse states combine one view-relative direction transform with the authored firing schedule without controller image arithmetic.
- `<map_flag resource_min="N" resource_max_exclusive="N" base_direction="N" view_adjustments="N" frame_divisor="N" frame_stride="N" number_x="N" number_y="N">` owns editor-map marker presentation. Its non-overlapping `<marker resource_min="N" resource_max_exclusive="N" image_group="..." image_offset="N" number_base="N" />` ranges must cover that complete authored resource range and choose the stacked category icon plus optional one-based label. The controller only publishes each marker's scenario position.
- `<hippodrome_race resource_min="N" resource_max_exclusive="N" horse_view_adjustments="N" cart_view_adjustments="N" frame_stride="N" cart_direction_offset="N" cart_offsets_x="8 values" cart_offsets_y="8 values">` owns race-team horse/cart layers and their directional attachment geometry. Non-overlapping `<team>` ranges must cover the authored resource range, while exactly four `<offsets orientation="top|right|bottom|left" max_wait_ticks="7 values" x="7 values" y="7 values" />` schedules own the pause animation's world offsets and end with `2147483647`.
- Flat `image_group="..."`, `image_asset="..."`, `path_pattern="..."`, `image_pattern="..."`, `corpse_*`, `action_*`, and cart graphics attributes still parse as legacy migration inputs, but new Vespasian-authored graphics should use child target nodes.
- Optional graphics attributes such as `max_image_offset`, `base_image_offset`, `static_frame_count`, `sprite_offset_x`, and `sprite_offset_y` remain on `<graphics>` until those policies are also split into child nodes.

The current extracted legacy walker groups often lack the meaningful metadata a
hand-authored Vespasian asset should have, so migration files may still carry an
explicit `<image>` child to recover the intended entry. New authored graphics
should instead use meaningful file/group names with one default image entry per
XML whenever possible.

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

Pathing modes are implemented as `PathingMode` objects with their own requirements. Current native pathing profiles require road-only movement because those mode objects set `requires_road`: use `terrain_usage="roads"` or `terrain_usage="roads_highway"`. Off-road-capable modes such as `any`, `prefer_roads`, and `prefer_roads_highway` are rejected because the native pathing contracts are road-route, road-roaming, or road-following policies.

Priests use explicit profiles such as `ceres_service`, `mars_service`, and `pantheon_service`; there is no owner-derived religion effect in XML. Pantheon still records Pantheon plus all five god histories at runtime.

Entertainment training buildings spawn `venue_seeker` profiles. Venues spawn generic `entertainment_service` native profiles such as `theater_service` or `arena_service`; each profile carries its exact smart-service effect in `<pathing>`.

Markets keep their existing BuildingType spawn flow for now, but the spawned figures rebind into FigureType profiles. `market_trader` uses the generic `roaming_service`; Vespasian uses `smart_service` with `market_goods`, while fallback definitions keep `vanilla_roaming`. `market_supplier` uses `native class="market_supplier"` with `pathing mode="storage_fetch"` to route to storage, reroute only to clearly better storage, create delivery followers, and return. `delivery_boy` uses `native class="delivery_follower"` with `pathing mode="follow_leader"` and follows the saved `leading_figure_id` chain.

Doctor, surgeon, and tax collector profiles keep `native class="legacy_action"` because their callbacks still own plague healing or building entry/exit states. Their XML profile still owns range, graphics, and smart-service pathing; the runtime binds the profile for legacy-action walkers without taking over their action callback.

Roaming access checks follow the profile movement type. A `roads` service profile considers roads and access ramps; a `roads_highway` service profile also considers highways. Use `roads_highway` only when that service should actually spread over highway networks, such as Vespasian `hippodrome_service`.

Residential walker support:

- `patrician` uses profile `house_roamer` with `roaming_service`, `vanilla_roaming`, road-only movement, and `return_mode="return_to_owner_road"`.
- `beggar` uses profile `unemployment_wanderer` with `transient_wanderer`, `stand_still`, `terrain_usage="roads_highway"`, and `return_mode="die_at_limit"`.
- Housing BuildingType XML owns when these figures spawn. Any missing profiled BuildingType spawn reference is a FigureType load failure after all FigureType XML has loaded.
- Residential walkers do not declare a road service `effect`, because they do not provide coverage and should not write road-service history.
- Legacy `<graphics base_image_offset="N" />` offsets the resolved image group base. Beggars also use legacy `static_frame_count="8"` for Julius-style still-frame variation and `corpse_image_group="labor_seeker"` for their corpse row.
- See [Walker Pathing Runtime](../../../docs/walker_pathing_runtime.md) for the fuller migration note and save-load inference concerns.

Related implementation notes:

- [Walker Pathing Runtime](../../../docs/walker_pathing_runtime.md) explains runtime flow, save compatibility, and effect-id rules.
- [Tile Scale and Walker Timescale](../../../docs/tile_scale_and_walker_timescale.md) converts `max_roam_length` into approximate tiles, meters, game days, and wall-clock time.
- [Preindustrial Walking Service Ranges](../../../docs/preindustrial_walking_service_ranges.md) gives historical walking-city guidance for `max_roam_length` tuning.
- `codex_augustus_repo_map_memory.md` indexes the native walker chokepoints for future sessions.
