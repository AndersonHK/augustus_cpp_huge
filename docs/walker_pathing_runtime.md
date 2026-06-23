# Walker Pathing Runtime

This note maps the native walker pathing work so future sessions can find the runtime, XML, save data, and compatibility rules without rediscovering them.

## Entry Points

- `Mods/Vespasian/FigureType/_README.md` documents the XML contract.
- `src/figure/figure_type_registry.cpp` loads and validates native FigureType XML.
- `src/figure/PathingMode.h/.cpp` owns pathing mode metadata such as road, service-effect, and venue-target requirements.
- `src/figure/figure_runtime.cpp` owns native profile binding, lifecycle rebinding, the C facade, and smart direction choice.
- `src/figure/figure_runtime_native.h/.cpp` owns native controller classes, venue ranking, and controller factory dispatch.
- `src/building/building_runtime.cpp` creates profiled native figures for BuildingType spawns.
- `src/building/local_workforce.h/.cpp` owns local workforce targeting and save data.
- `src/map/routing_distance.h/.cpp` wraps routing-grid distance checks for walker destination selection.
- `src/map/road_service_history.h/.cpp` owns per-road, per-effect visit stamps.
- `src/game/file_io.cpp` saves and loads road service history.
- `src/game/save_version.h` records save-version boundaries.
- `docs/save_data_organization.md` is the canonical map for `.svv` save-piece order and save-backed runtime ownership.
- `docs/save_load_runtime_bridges.md` follows the post-read fan-out for road service history, local workforce allocations, FigureType runtime rebinding, and BuildingType-owned spawns.

## XML Contract

Native figures are profile-based:

- `<profiles default="...">`
- `<profile id="...">`
- `<native class="roaming_service|engineer_service|prefect_service|market_supplier|delivery_follower|entertainment_venue_seeker|entertainment_service|transient_wanderer" />`
- `<owner slot="none|primary|secondary|quaternary" building="any|..." state="any|in_use|in_use_or_mothballed" />`
- `<movement terrain_usage="any|roads|roads_highway|prefer_roads|prefer_roads_highway" roam_ticks="N" max_roam_length="N" return_mode="return_to_owner_road|die_at_limit|none" />`
- `<pathing mode="vanilla_roaming|smart_service|nearest_unemployed|venue_seeker|storage_fetch|follow_leader|stand_still|transient_wander" effect="..." />`
- `<graphics image_group="..." max_image_offset="N" base_image_offset="N" static_frame_count="N" corpse_image_group="..." corpse_base_image_offset="N" />` at figure level. Optional `base_image_offset` defaults to zero, optional `static_frame_count` chooses one still frame by `figure id % N`, and optional corpse attributes let profiles use a different corpse row.

BuildingType `<spawn>` nodes choose a profile with `profile="..."`. This is the narrow handoff from building policy into figure behavior: after creation, the figure's bound profile owns pathing and road-history effect selection.

BuildingType `<spawn_group existing_figure="...">` is the group-level guard for legacy tracked slots. Single-type groups behave like the old hardcoded checks; comma-list groups such as `actor,gladiator` are for venues where several profile-specific spawns share one legacy `figure_id` slot and must block one another.

`PathingMode` objects own mode requirements. Current native pathing modes set `requires_road`, so they require `terrain_usage="roads"` or `terrain_usage="roads_highway"`. Off-road-capable modes such as `any`, `prefer_roads`, and `prefer_roads_highway` are rejected during XML load because the native policies are road-route, road-roaming, or road-following contracts.

`smart_service` is only valid for road-only walkers with a non-none effect. `nearest_unemployed` is a road-only policy used by labor seeker profiles. `venue_seeker` uses profile venue targets to pick a destination venue.

## Runtime Contract

The legacy roaming loop remains the source of tile movement. Native runtime code only overrides the chosen direction after vanilla logic has found a valid forward direction.

Roaming access follows the figure profile's movement type. `roads` walkers consider roads and access ramps; `roads_highway` walkers also consider highways. Building road-access checks remain separate and still answer whether the building itself touches ordinary road access.

`smart_service` applies only when there is more than one valid outgoing road. It chooses the candidate road tile with the lowest visit stamp for the profile's effect. Equal stamps fall back to the vanilla-preferred direction so old behavior stays stable where history gives no preference. Once a smart-service walker commits to a target road, it immediately reserves that target in road service history so another same-service walker choosing later in the same tick does not lockstep onto the same tile.

Priests use explicit profiles such as `ceres_service`, `mars_service`, and `pantheon_service`. Pantheon records the Pantheon effect and all five individual god effects on each road tile, matching the previous owner-derived behavior without a special XML effect.

Entertainment training buildings spawn `venue_seeker` profiles. `show_duration` values are active calendar days: a venue showing 53 days left should expire after 53 active days, not 53 legacy half-days. Savegames at or before `SAVE_GAME_LAST_LEGACY_ENTERTAINMENT_SHOW_HALF_DAYS` migrate stored venue counters from the old half-day units by doubling them on load. Venue ranking uses the legacy weighted score, `2 * show_days + route_distance`, where `route_distance` comes from the same routing grid the walker will follow. Venue service walkers share the generic `entertainment_service` native class; separate profiles such as `theater_service` and `arena_service` carry the exact `<pathing mode="smart_service" effect="...">` value. Mixed venues such as amphitheaters and arenas rely on the parent BuildingType `existing_figure` list to keep alternate service walkers mutually exclusive while the FigureType profile owns the actual service effect.

Labor seekers use explicit `acquisition` and `validation` profiles. The trip flag remains in `collecting_item_id` for save compatibility, but new spawns bind the profile directly and the runtime owns target selection where practical.

Market walkers now have explicit FigureType policies while the legacy market BuildingType spawn source remains in place. `market_trader` is a `roaming_service` profile; Vespasian records the `market_goods` smart-service effect, while Julius/Augustus fallback XML uses `vanilla_roaming`. `market_supplier` uses `native class="market_supplier"` and `pathing mode="storage_fetch"` to preserve storage selection, halfway rerouting, pickup, delivery follower creation, and return-to-market behavior using existing save fields. `delivery_boy` uses `native class="delivery_follower"` and `pathing mode="follow_leader"` to follow `leading_figure_id` chains and deposit carried resources when the leader finishes.

## Residential Walker Pathing

Beggar and patrician spawning is now authored in house `BuildingType` XML, while active figure behavior is FigureType-owned:

- `FIGURE_PATRICIAN` is created by patrician-class housing through profile `house_roamer`, stays bound to its source house through `building_id`, walks on roads, returns through the native roamer action, and uses `GROUP_FIGURE_PATRICIAN`.
- `FIGURE_BEGGAR` is created by plebeian housing when unemployment pressure is high through profile `unemployment_wanderer`, stores the source house in `building_id`, uses road/highway terrain, stands still on its spawn road, uses the beggar row inside `GROUP_FIGURE_HOMELESS`, and dies when its transient lifetime budget expires.

House spawns use the BuildingType runtime spawn path with `delay_bands="100:0"` and `figure_slot="quaternary"`, giving each house one active residential walker slot without colliding with the house primary slot used by legacy homeless/undo behavior. The old city-wide beggar counter and one-patrician-per-generation throttle are gone; spawn pressure is now per-house probability data.

Profiled BuildingType spawn references are validated after FigureType XML load. Any missing `spawn_figure` plus `profile` pair is a load failure instead of a later creation crash.

Current residential spawn policies:

- Augustus/Julius beggars use `chance_source="city_unemployment_percent"` and a precomputed `chance_per_million_bands` table. The table targets the old expected active beggar ratios using a 16-day stationary lifetime and one daily house check.
- Vespasian beggars use `chance_source="house_unemployed_workers"` and per-house `chance_divisor` values derived from housing capacity. This intentionally makes local unemployment pressure matter and lets wealthier plebeian houses have much lower odds.
- Patricians use constant `chance_per_million="24390"` while inactive, matching the old per-house 40-day cadence statistically without the old global throttle.

Existing saves are rebound in `figure_runtime.cpp::infer_profile_id()`: `FIGURE_PATRICIAN` recovers `house_roamer`, and `FIGURE_BEGGAR` recovers `unemployment_wanderer`. Loaded beggars with the old zero action state continue their stationary lifetime. Saves from the short-lived roaming implementation can also load `FIGURE_ACTION_125_ROAMING`; the `stand_still` policy clears that route state and leaves the figure on its current tile.

Three generic pathing contracts matter for residential walkers:

- `vanilla_roaming` with `roaming_service`: owner-bound road roaming for patricians. It requires a live owner building, uses no road-service effect, and returns to the owner's access road when its roam budget expires.
- `stand_still` with `transient_wanderer`: short-lived road or road/highway idling for ambient figures that do not provide service and do not need to return. It expires at the movement/lifetime budget and does not write road service history.
- `transient_wander` with `transient_wanderer`: reserved for short-lived ambient figures that should move. Beggars do not use it in this slice; slow periodic repositioning should be implemented as a later generic policy rather than as beggar-specific movement.

Current profile examples:

```xml
<figure type="patrician">
    <profiles default="house_roamer">
        <profile id="house_roamer">
            <native class="roaming_service" />
            <owner slot="none" building="any" state="in_use" />
            <movement terrain_usage="roads" roam_ticks="1" max_roam_length="128" return_mode="return_to_owner_road" />
            <pathing mode="vanilla_roaming" />
        </profile>
    </profiles>
    <graphics image_group="patrician" max_image_offset="12" />
</figure>
```

```xml
<figure type="beggar">
    <profiles default="unemployment_wanderer">
        <profile id="unemployment_wanderer">
            <native class="transient_wanderer" />
            <owner slot="none" building="any" state="any" />
            <movement terrain_usage="roads_highway" roam_ticks="1" max_roam_length="800" return_mode="die_at_limit" />
            <pathing mode="stand_still" />
        </profile>
    </profiles>
    <graphics image_group="beggar" base_image_offset="104" max_image_offset="1" static_frame_count="8" corpse_image_group="labor_seeker" corpse_base_image_offset="96" />
</figure>
```

Save compatibility does not need a new save table for this conversion. Existing figure records already persist `type`, `building_id`, action state, route state, `wait_ticks`, and roam fields. The native transient controller accepts old beggars with action state zero, and it accepts the temporary roaming action state by discarding route state under `stand_still` while preserving the saved lifetime counter.

Temporary tuning decision: Vespasian FigureType walkers should use a `max_roam_length` roughly 50% larger than Augustus until walker range tuning is revisited.

## Road Service History

Road service history is pathing telemetry only. It does not provide service, reset building risk, affect coverage overlays, or change building state.

Each effect has a full road grid of `uint32_t` visit generations. Zero means "never visited" and is also the default for old saves and newly placed roads. Positive values only represent relative recency: each service-history write receives the next generation so simultaneous smart walkers do not collapse many choices into equal timestamps. If the generation space is exhausted, nonzero values are normalized back down while preserving their ordering.

Effect ids are save-compatible and append-only. New entertainment effects were appended after religion:

- `entertainment_theater`
- `entertainment_amphitheater_actor`
- `entertainment_amphitheater_gladiator`
- `entertainment_arena_gladiator`
- `entertainment_arena_lion`
- `entertainment_colosseum_gladiator`
- `entertainment_colosseum_lion`
- `entertainment_hippodrome`
- `market_goods`
- `doctor`
- `surgeon`
- `tax_collector`

`SAVE_GAME_LAST_NO_ENTERTAINMENT_ROAD_SERVICE_HISTORY` marks the compatibility boundary for the appended entertainment history grids. `SAVE_GAME_LAST_NO_MARKET_ROAD_SERVICE_HISTORY` marks the compatibility boundary for the appended market history grid. `SAVE_GAME_LAST_NO_MEDICINE_TAX_ROAD_SERVICE_HISTORY` marks the compatibility boundary for the appended doctor, surgeon, and tax collector grids. See `docs/save_data_organization.md` and `src/game/save_version.h` for the current save version and full `.svv` piece layout.
`docs/save_load_runtime_bridges.md` documents how the loaded history grids, local workforce allocation vector, and FigureType profile inference are rebuilt after the file pieces are read.

## Related Context

Start new sessions with the four core Codex files, then read this file for walker runtime work and `Mods/Vespasian/FigureType/_README.md` for XML details. Use `docs/tile_scale_and_walker_timescale.md` and `docs/preindustrial_walking_service_ranges.md` when tuning `max_roam_length`. Use `docs/gameplay_divergences_from_augustus.md` when a walker migration intentionally changes bundled Augustus, Julius, or Vespasian gameplay compared with upstream Augustus.
