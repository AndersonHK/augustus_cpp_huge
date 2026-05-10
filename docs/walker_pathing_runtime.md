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
- `<native class="roaming_service|engineer_service|prefect_service|market_supplier|delivery_follower|entertainment_venue_seeker|entertainment_service" />`
- `<owner slot="none|primary|secondary|quaternary" building="any|..." state="any|in_use|in_use_or_mothballed" />`
- `<movement terrain_usage="any|roads|roads_highway|prefer_roads|prefer_roads_highway" roam_ticks="N" max_roam_length="N" return_mode="return_to_owner_road|die_at_limit|none" />`
- `<pathing mode="vanilla_roaming|smart_service|nearest_unemployed|venue_seeker|storage_fetch|follow_leader" effect="..." />`
- `<graphics image_group="..." max_image_offset="N" />` at figure level

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

## Planned Residential Walker Pathing

Beggar and patrician spawning is now keyed from `HousingType` resident class, but the figures themselves still use legacy action handlers:

- `FIGURE_PATRICIAN` is created by patrician-class housing, stays bound to its source house through `building_id`, walks on roads, uses `max_roam_length = 128`, returns through the normal roamer action, and uses `GROUP_FIGURE_PATRICIAN`.
- `FIGURE_BEGGAR` is created by plebeian housing when unemployment pressure is high, stores the source house in `building_id`, uses road/highway terrain, cycles homeless/beggar graphics, and dies after a fixed `wait_ticks > 800` lifetime. The current handler does not use a full native movement controller, so making beggars visibly roam is a gameplay-visible change rather than a pure data migration.

The native conversion should start by adding FigureType ids for `patrician` and `beggar`, plus graphics keys for `patrician` and the homeless/beggar image group. Old-save rebinding should infer profiles for existing `FIGURE_PATRICIAN` and `FIGURE_BEGGAR` records in `figure_runtime.cpp::infer_profile_id()` before their legacy action handlers are retired.

Two pathing contracts are useful here:

- `resident_roaming`: owner-bound road roaming for walkers spawned from a house. It requires a live owner building, uses no road-service effect, and returns to the owner's access road when its roam budget expires. This is the natural patrician mode.
- `transient_wander`: short-lived road or road/highway wandering for ambient figures that do not provide service and do not need to return. It expires at the movement/lifetime budget and should not write road service history. This is the natural beggar mode if we intentionally make beggars move through the road network.

Those could be implemented either as new `PathingMode` objects with the existing `roaming_service` native class, or as clearer native classes such as `resident_roamer` and `ambient_roamer`. The lower-risk first slice is to reuse `RoamingServiceFigure` where it already matches:

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
            <native class="roaming_service" />
            <owner slot="none" building="any" state="any" />
            <movement terrain_usage="roads_highway" roam_ticks="1" max_roam_length="800" return_mode="die_at_limit" />
            <pathing mode="vanilla_roaming" />
        </profile>
    </profiles>
    <graphics image_group="beggar" max_image_offset="64" />
</figure>
```

If exact legacy beggar behavior must be preserved first, add a separate `timed_idle` or `ambient_lifetime` pathing mode instead of forcing movement through `vanilla_roaming`. That mode would own only lifetime, corpse handling, and animation, leaving position unchanged until a later gameplay pass decides how beggars should roam.

Spawner migration should remain in `src/building/figure.cpp` initially because the conditions are city-pressure policies, not simple per-building spawn timers: beggars depend on unemployment and the global beggar counter, while patricians use a one-spawn-per-generation throttle. The immediate change is to replace `figure_create(FIGURE_PATRICIAN, ...)` and `figure_create(FIGURE_BEGGAR, ...)` with `figure_runtime_create_profiled(...)` using the profiles above, with legacy `figure_create` fallback until XML exists in all active mods. A later HousingType policy could move these city-pressure knobs into data, but the FigureType profile should own movement once the figure exists.

Save compatibility does not need a new save table if the conversion only adds XML definitions and profile inference. Existing figure records already persist `type`, `building_id`, action state, route state, `wait_ticks`, and roam fields. The risk is action-state mismatch: legacy beggars may load with a state that `RoamingServiceFigure` does not handle. Either infer them to a compatibility profile whose native controller accepts the old state, or leave unrecognized loaded beggars on the legacy handler until they naturally expire.

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

`SAVE_GAME_LAST_NO_ENTERTAINMENT_ROAD_SERVICE_HISTORY` marks the compatibility boundary for the appended entertainment history grids. `SAVE_GAME_LAST_NO_MARKET_ROAD_SERVICE_HISTORY` marks the compatibility boundary for the appended market history grid. See `docs/save_data_organization.md` and `src/game/save_version.h` for the current save version and full `.svv` piece layout.
`docs/save_load_runtime_bridges.md` documents how the loaded history grids, local workforce allocation vector, and FigureType profile inference are rebuilt after the file pieces are read.

## Related Context

Start new sessions with the four core Codex files, then read this file for walker runtime work and `Mods/Vespasian/FigureType/_README.md` for XML details. Use `docs/tile_scale_and_walker_timescale.md` and `docs/preindustrial_walking_service_ranges.md` when tuning `max_roam_length`.
