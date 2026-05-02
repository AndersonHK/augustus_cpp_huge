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

## XML Contract

Native figures are profile-based:

- `<profiles default="...">`
- `<profile id="...">`
- `<native class="roaming_service|engineer_service|prefect_service|entertainment_venue_seeker|entertainment_service" />`
- `<owner slot="none|primary|secondary|quaternary" building="any|..." state="any|in_use|in_use_or_mothballed" />`
- `<movement terrain_usage="any|roads|roads_highway|prefer_roads|prefer_roads_highway" roam_ticks="N" max_roam_length="N" return_mode="return_to_owner_road|die_at_limit|none" />`
- `<pathing mode="vanilla_roaming|smart_service|nearest_unemployed|venue_seeker" effect="..." />`
- `<graphics image_group="..." max_image_offset="N" />` at figure level

BuildingType `<spawn>` nodes choose a profile with `profile="..."`. This is the narrow handoff from building policy into figure behavior: after creation, the figure's bound profile owns pathing and road-history effect selection.

BuildingType `<spawn_group existing_figure="...">` is the group-level guard for legacy tracked slots. Single-type groups behave like the old hardcoded checks; comma-list groups such as `actor,gladiator` are for venues where several profile-specific spawns share one legacy `figure_id` slot and must block one another.

`PathingMode` objects own mode requirements. Current native pathing modes set `requires_road`, so they require `terrain_usage="roads"` or `terrain_usage="roads_highway"`. Off-road-capable modes such as `any`, `prefer_roads`, and `prefer_roads_highway` are rejected during XML load because the roaming loop can only choose among adjacent road/path tiles.

`smart_service` is only valid for road-only walkers with a non-none effect. `nearest_unemployed` is a road-only policy used by labor seeker profiles. `venue_seeker` uses profile venue targets to pick a destination venue.

## Runtime Contract

The legacy roaming loop remains the source of tile movement. Native runtime code only overrides the chosen direction after vanilla logic has found a valid forward direction.

Roaming access follows the figure profile's movement type. `roads` walkers consider roads and access ramps; `roads_highway` walkers also consider highways. Building road-access checks remain separate and still answer whether the building itself touches ordinary road access.

`smart_service` applies only when there is more than one valid outgoing road. It chooses the candidate road tile with the lowest visit stamp for the profile's effect. Equal stamps fall back to the vanilla-preferred direction so old behavior stays stable where history gives no preference.

Priests use explicit profiles such as `ceres_service`, `mars_service`, and `pantheon_service`. Pantheon records the Pantheon effect and all five individual god effects on each road tile, matching the previous owner-derived behavior without a special XML effect.

Entertainment training buildings spawn `venue_seeker` profiles. Venue ranking uses the legacy weighted score, `2 * show_days + route_distance`, where `route_distance` comes from the same routing grid the walker will follow. Venue service walkers share the generic `entertainment_service` native class; separate profiles such as `theater_service` and `arena_service` carry the exact `<pathing mode="smart_service" effect="...">` value. Mixed venues such as amphitheaters and arenas rely on the parent BuildingType `existing_figure` list to keep alternate service walkers mutually exclusive while the FigureType profile owns the actual service effect.

Labor seekers use explicit `acquisition` and `validation` profiles. The trip flag remains in `collecting_item_id` for save compatibility, but new spawns bind the profile directly and the runtime owns target selection where practical.

Temporary tuning decision: Vespasian FigureType walkers should use a `max_roam_length` roughly 50% larger than Augustus until walker range tuning is revisited.

## Road Service History

Road service history is pathing telemetry only. It does not provide service, reset building risk, affect coverage overlays, or change building state.

Each effect has a full road grid of `uint32_t` visit stamps. Zero means "never visited" and is also the default for old saves and newly placed roads. The stamp uses game time plus one, preserving zero as the stale sentinel.

Effect ids are save-compatible and append-only. New entertainment effects were appended after religion:

- `entertainment_theater`
- `entertainment_amphitheater_actor`
- `entertainment_amphitheater_gladiator`
- `entertainment_arena_gladiator`
- `entertainment_arena_lion`
- `entertainment_colosseum_gladiator`
- `entertainment_colosseum_lion`
- `entertainment_hippodrome`

`SAVE_GAME_CURRENT_VERSION` is `0xb3`. `SAVE_GAME_LAST_NO_ENTERTAINMENT_ROAD_SERVICE_HISTORY` is `0xb2`; saves at or below that version load existing grids and leave appended entertainment grids zeroed.

## Related Context

Start new sessions with the four core Codex files, then read this file for walker runtime work and `Mods/Vespasian/FigureType/_README.md` for XML details. Use `docs/tile_scale_and_walker_timescale.md` and `docs/preindustrial_walking_service_ranges.md` when tuning `max_roam_length`.
