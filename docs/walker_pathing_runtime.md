# Walker Pathing Runtime

This note maps the native walker pathing work so future sessions can find the
runtime, XML, save data, and compatibility rules without rediscovering them.

## Entry Points

- `Mods/Vespasian/FigureType/_README.md` documents the XML contract.
- `src/figure/figure_type_registry.cpp` loads and validates native FigureType XML.
- `src/figure/figure_runtime.cpp` owns native controllers and smart direction choice.
- `src/figure/movement.cpp` keeps the legacy roaming movement loop and calls the native pathing hook.
- `src/figuretype/maintenance.cpp` keeps only Worker behavior plus retired Engineer/Prefect action-table guards.
- `src/map/road_service_history.h/.cpp` owns per-road, per-effect visit stamps.
- `src/building/local_workforce.h/.cpp` owns local workforce labor-seeker targeting and save data.
- `src/game/file_io.cpp` saves and loads road service history.
- `src/game/save_version.h` records the save-version boundary for road service history.

## XML Contract

Native service walkers use:

- `<movement terrain_usage="roads" roam_ticks="N" max_roam_length="N" return_mode="..." />`
- `<pathing mode="vanilla_roaming" />`
- `<pathing mode="smart_service" effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor|religion_ceres|religion_neptune|religion_mercury|religion_mars|religion_venus|religion_pantheon|religion_owner" />`
- `<pathing mode="nearest_unemployed" />`

`smart_service` is only valid for road-only walkers with a non-none effect. Loader
validation should fail early through crash-context reporting if either condition
is missing.
`nearest_unemployed` is a road-only policy that targets the closest reachable
house with locally unemployed residents unless the figure already has an explicit
destination.

## Runtime Contract

The legacy roaming loop remains the source of tile movement. Native runtime code
only gets a chance to override the chosen direction after vanilla logic has found
a valid forward direction.

`smart_service` applies only when there is more than one valid outgoing road. It
chooses the candidate road tile with the lowest visit stamp for that effect.
Equal stamps fall back to the vanilla-preferred direction so old behavior stays
stable where history gives no preference.

Priests use `religion_owner` instead of a fixed saved effect id. The runtime
derives the concrete service effect from the owning temple type. Pantheon priests
record the Pantheon effect and all five individual god effects on each road tile,
so Pantheon coverage can inform later god-specific pathing without merging all
temple walkers into one generic religion bucket.

Labor seekers also derive service meaning from their owner. BuildingType
`method="none"` is an explicit no-seeker policy for labor-using buildings that
should not create labor seekers. BuildingType `method="houses_spawn_if_below"`
and `method="houses_generate_if_below"` keep the vanilla roaming/coverage
callback. BuildingType `method="workforce"` uses a
targeted local workforce trip through the generic `nearest_unemployed` policy:
acquisition seekers path from the workplace road to the closest reachable house
with unemployed local workers, and validation seekers periodically revisit an
assigned worker-source house by handing the figure an explicit target. The
workplace only creates the seeker; the figure runtime owns movement and target
selection when the seeker runs. Partially employed workplaces keep acquisition
as the first priority, but if they cannot launch another acquisition seeker they
may validate existing worker sources. Both searches cap candidates at the loaded labor seeker's
`max_roam_length`; houses beyond that road-routing distance are ignored. If a
validation seeker cannot path to its assigned source, that source allocation is
released. Local workforce allocations are labor-access records only; actual
`num_workers` still comes from the city-wide labor pool.

Smart service history records any tile the citizen routing layer treats as road,
including traversable granary interior road tiles that may not carry the raw
`TERRAIN_ROAD` flag. This keeps smart walkers from repeatedly preferring
untracked granary pass-through tiles as permanently stale roads.

Prefect emergency behavior is separate from smart roaming. Fire and enemy
response states keep using targeted movement and should not be redirected by
road recency.

Engineer and Prefect legacy switch bodies were retired after the native
controllers covered their normal, emergency, attack, and corpse states. The
action table still has guard callbacks, but those callbacks should not be part
of normal gameplay.

## Road Service History

Road service history is pathing telemetry only. It does not provide service,
reset building risk, affect coverage overlays, or change building state.

Each effect has a full road grid of `uint32_t` visit stamps. Zero means "never
visited" and is also the default for old saves and newly placed roads. The stamp
uses game time plus one, preserving zero as the stale sentinel.

Current effect ids are stored in `road_service_effect`. Treat those numeric ids
as a save compatibility contract:

- Add new effects only at the end.
- Do not reorder existing ids.
- Do not reuse removed ids for a different meaning.
- Keep removed meanings as reserved/deprecated slots.
- If a service meaning changes enough that old recency would mislead pathing,
  either migrate that id explicitly or clear just that effect on load.

Religion effects were appended after the original maintenance/culture effects:
`religion_ceres`, `religion_neptune`, `religion_mercury`, `religion_mars`,
`religion_venus`, and `religion_pantheon`. `religion_owner` is XML-only resolver
metadata and is not serialized as its own road-service effect id.

The current save format writes grids in ordinal enum order. If frequent effect
schema changes become likely, move the payload to explicit `(effect_id, grid)`
records before doing removals or reordering.

Loading a save from before road service history existed is expected compatibility
behavior. It should clear the history and emit a concise `Info` report with no
context scope. Invalid or unsupported road service history is recoverable but
unintended, so it remains a `Warning` while still resetting the history to zero
and appending neutral context in the same log entry.

## Local Workforce Save Data

Local workforce allocations are stored as a separate dynamic save piece. The
payload writes a format version, record count, then
`{ workplace_id, house_id, workers }` records. Saves older than
`SAVE_GAME_LAST_NO_LOCAL_WORKFORCE` start with an empty allocation table and
rebuild house counters from current residents.

Loading also revalidates saved allocation records against the current
BuildingType XML. Dead buildings, houses that are no longer houses, workplaces
that are no longer `method="workforce"`, and allocations above the current
workplace employee requirement are removed.

During play, building deletion/destruction removes any allocation where the
building is either the workplace or worker-source house, clears runtime house
counters, and kills in-flight workforce labor seekers tied to that building.

House `local_workforce_assigned` and `local_workforce_unemployed` counters are
runtime caches. They are rebuilt from allocation records on load and refreshed
lazily when workforce seekers query a house. Resident-count decreases reconcile
allocations immediately; labor-participation decreases are corrected lazily when
a seeker next queries or validates the affected worker source.

Local workforce buildings remain in city-wide category allocation. Their saved
assignments only replace housing coverage as the building's labor-access score,
so taking workers from a house does not reduce the city labor pool by itself.
Loading an old save without local allocation records therefore leaves workforce
buildings without local labor access until new workforce seekers assign
residents, after which normal city labor allocation supplies `num_workers`.

## Related Context

Start new sessions with the four core Codex files, then read this file for walker
runtime work and `Mods/Vespasian/FigureType/_README.md` for XML details.
Renderer or overlay work that visualizes recency should also read
`../codex_augustus_repo_map_memory.md` for renderer/widget chokepoints before
touching `src/widget/city_with_overlay.cpp`.
