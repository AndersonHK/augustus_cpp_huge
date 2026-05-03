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
- `<graphics ... />` at figure level

Buildings select a native profile with `profile="..."` on their `<spawn>`. The building only chooses the profile; the figure profile owns the native class, owner contract, movement, pathing mode, and road-history effect.

Current supported figure ids include `labor_seeker`, `engineer`, `prefect`, `priest`, `teacher`, `librarian`, `barber`, `bathhouse_worker`, `school_child`, `actor`, `gladiator`, `lion_tamer`, and `charioteer`.

Current supported native classes are `roaming_service`, `engineer_service`, `prefect_service`, `entertainment_venue_seeker`, and `entertainment_service`.

Current supported `<pathing>` values:

- `mode="vanilla_roaming"`
- `mode="smart_service" effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor|religion_ceres|religion_neptune|religion_mercury|religion_mars|religion_venus|religion_pantheon|entertainment_theater|entertainment_amphitheater_actor|entertainment_amphitheater_gladiator|entertainment_arena_gladiator|entertainment_arena_lion|entertainment_colosseum_gladiator|entertainment_colosseum_lion|entertainment_hippodrome"`
- `mode="nearest_unemployed"`
- `mode="venue_seeker"`

Pathing modes are implemented as `PathingMode` objects with their own requirements. Current native pathing profiles require road-only movement because those mode objects set `requires_road`: use `terrain_usage="roads"` or `terrain_usage="roads_highway"`. Off-road-capable modes such as `any`, `prefer_roads`, and `prefer_roads_highway` are rejected because the roaming loop chooses among adjacent road tiles, not arbitrary terrain.

Priests use explicit profiles such as `ceres_service`, `mars_service`, and `pantheon_service`; there is no owner-derived religion effect in XML. Pantheon still records Pantheon plus all five god histories at runtime.

Entertainment training buildings spawn `venue_seeker` profiles. Venues spawn generic `entertainment_service` native profiles such as `theater_service` or `arena_service`; each profile carries its exact smart-service effect in `<pathing>`.

Roaming access checks follow the profile movement type. A `roads` service profile considers roads and access ramps; a `roads_highway` service profile also considers highways. Use `roads_highway` only when that service should actually spread over highway networks, such as Vespasian `hippodrome_service`.

Related implementation notes:

- `docs/walker_pathing_runtime.md` explains runtime flow, save compatibility, and effect-id rules.
- `docs/tile_scale_and_walker_timescale.md` converts `max_roam_length` into approximate tiles, meters, game days, and wall-clock time.
- `docs/preindustrial_walking_service_ranges.md` gives historical walking-city guidance for `max_roam_length` tuning.
- `codex_augustus_repo_map_memory.md` indexes the native walker chokepoints for future sessions.
