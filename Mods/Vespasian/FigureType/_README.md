# FigureType XML

The loader reads every `*.xml` file in the winning `FigureType` folders using this precedence:

1. active mod
2. `Mods\Augustus\FigureType`
3. `Mods\Julius\FigureType`

Later fallback definitions are allowed, but the first resolved figure type wins.
Templates/examples belong in non-`.xml` files so they do not load as live data.

Current supported nodes:

- `<figure type="...">`
- `<native class="..." />`
- `<owner ... />`
- `<movement ... />`
- `<pathing ... />`
- `<graphics ... />`

Current supported figure ids:

- `labor_seeker`
- `engineer`
- `prefect`
- `priest`
- `teacher`
- `librarian`
- `barber`
- `bathhouse_worker`
- `school_child`

Current supported native classes:

- `roaming_service`
- `engineer_service`
- `prefect_service`

Current supported `<owner>` attributes:

- `slot="none|primary|secondary|quaternary"`
- `building="any|school|..."` using the building attr names
- `state="any|in_use|in_use_or_mothballed"`

Current supported `<movement>` attributes:

- `terrain_usage="any|roads|roads_highway|prefer_roads|prefer_roads_highway"`
- `roam_ticks="N"`
- `max_roam_length="N"`
- `return_mode="return_to_owner_road|die_at_limit"`

Current supported `<pathing>` attributes:

- `mode="vanilla_roaming|smart_service|nearest_unemployed"`
- `effect="damage_risk|fire_risk|barber|bathhouse|school|academy|library|labor|religion_ceres|religion_neptune|religion_mercury|religion_mars|religion_venus|religion_pantheon|religion_owner"`

Current supported `<graphics>` attributes:

- `image_group="labor_seeker|engineer|prefect|priest|teacher_librarian|barber|bathhouse_worker|school_child"`
- `max_image_offset="N"`

Current engine behavior:

- FigureType v1 is native-subset only; all other figure types stay on legacy behavior.
- The runtime writes state back into the legacy `figure` struct.
- If a native figure hits an unsupported action state, the runtime can decline and the legacy action path will still run.
- `vanilla_roaming` preserves the existing random roaming policy. If an `effect` is provided, visits are still recorded for pathing telemetry, but the effect does not alter the vanilla direction choice.
- `smart_service` chooses the least recently serviced road branch at intersections and requires road movement plus an effect.
- `nearest_unemployed` targets the closest reachable house with local unemployed residents unless the figure was created with an explicit destination.
- `religion_owner` is an owner-derived religion effect resolver for priests. The runtime derives the concrete god effect from the owning temple; Pantheon priests record all five god effects plus the Pantheon effect.
- Labor seekers derive their service behavior from the owning BuildingType labor seeker policy. `method="none"` has no seeker behavior, `method="houses_spawn_if_below"` and `method="houses_generate_if_below"` use housing coverage, and `method="workforce"` uses `nearest_unemployed` to target the closest reachable unemployed house within the labor seeker's `max_roam_length` and uses explicit-target validation trips to release unreachable assigned sources.
- Smart service visit history is pathing telemetry only; building coverage and risk resets still use the normal service callbacks.
- Road service history is saved separately from figures. Old saves start with zeroed history and a crash-context warning.

Related implementation notes:

- `docs/walker_pathing_runtime.md` explains runtime flow, save compatibility, and effect-id rules.
- `docs/tile_scale_and_walker_timescale.md` converts `max_roam_length` into approximate tiles, meters, game days, and wall-clock time.
- `docs/preindustrial_walking_service_ranges.md` gives historical walking-city guidance for `max_roam_length` tuning.
- `codex_augustus_repo_map_memory.md` indexes the native walker chokepoints for future sessions.
