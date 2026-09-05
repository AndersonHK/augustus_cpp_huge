# Formation geometry, stationing, and recruitment

`FormationType` owns capacity and unit composition. `FormationLayoutDef` owns mathematical geometry. A live `formation` owns roster indices, commanded origin, and its stable `FormationMemberMovementPlan`. Figures reach exact fixed-point destinations through `Figure::move_ticks_to` and the shared `Route` implementation.

## Geometry contract

Every layout requires one `<geometry>` declaration. Member `<position>` tables and the special path for capacities of 16 or less have been removed. Stable numeric layout ids and the fixed 16-slot save prefix are compatibility boundaries only; neither selects geometry. Existing saves retain their layout identity and physically converge on the generated stations after loading.

Examples:

```xml
<geometry shape="square"/>
<geometry shape="square" centered="true"/>
<geometry shape="ranks" transpose="true"/>
<geometry shape="staggered"/>
<geometry shape="square" centered="true" spacing_percent="75"/>
<geometry shape="scatter"/>
```

Square slots grow in successive shells from their anchor, so every perfect-square prefix is already square. Corner anchoring keeps the mustering-ground footprint nonnegative. Centered anchoring alternates coordinates around zero. Rectangular grids stop expanding along each axis at its declared extent.

Ranked lines grow in rectangular shells with four files per rank of depth, extending alternately along the long axis. Staggered lines grow wider shells and alternate offsets between adjacent files. Population determines how much of this stable sequence is occupied; declared capacity never determines a partial line's depth. Thus 16 soldiers in a 64-slot formation form an eight-by-two ranked line, rather than the former four-by-four square. Recruitment adds stations without relocating incumbents. Transpose exchanges the axes. Scattered groups use deterministic jitter in separate lattice cells, avoiding the duplicate stations in the old mob/herd tables. Tortoise uses a centered square at 75% pitch; wide enemy columns use 200%. Spacing must be between 50% and 200%.

Vespasian overrides `mop_up` with scatter geometry and `restore_when_idle="false"`. Outside the fort, soldiers retain that loose formation when there are no targets. Julius and Augustus inherit the default `restore_when_idle="true"` and retain the legacy return to the previous tactical layout. Returning home still uses mustering-ground geometry. Leaving pursuit releases its target and resumes normal station movement, subject to the existing combat/fire restrictions.

Trained legionaries and infantry have all five tactical controls, including testudo. Untrained heavy infantry have three. Both military UIs share this eligibility rule; a recruit visiting the military academy grants the formation its training flag. Merely constructing an academy does not retroactively train every existing formation.

The live mustering-ground `Foundation` still determines pitch for scaled formations. An 8x8 square spans the same fort footprint as a 4x4 square. Deployed lines may extend beyond that footprint, using the same pitch. Increasing recruitment count does not change pitch or move existing slots. Enemy army *formation-to-formation* offsets are a separate XML contract and retain their authored orientation tables.

## Roster events and idle compaction

A casualty/removal or actual layout change sets `roster_compaction_pending`. Loaded rosters start pending because this state is derived, not serialized. Once all remaining members are idle at their fort or standard, with no combat/missile activity or distant service, survivors are stably packed into the lowest slot indices. Their relative order is preserved. A changed assignment invalidates the station plan once; ordinary movement brings members to the new positions.

The normal roster refresh neither rebuilds assignments nor closes holes while members are moving or fighting. Once compaction consumes the event, later idle ticks leave assignments alone. New recruits take vacant slots without reassigning incumbents. Selecting the already-active shape is a no-op.

A morale retreat publishes the home command and clears the attack target before returning soldiers. Arrival therefore cannot leave a deployed anchor in the next save. This command update does not compact assignments; compaction still waits for idle members and cleared combat/fire state.

## Reachability, reservations, and overflow

The plan first attempts each member's ideal exact endpoint. When terrain blocks it, the shared router supplies reachability and route distance for the nearest free fallback. Selection is deterministic: displacement from the ideal, route distance, then coordinates. All canonical endpoints are reserved, including future recruits' stations; fallback endpoints are exclusive. The search is bounded by the live map, rather than an arbitrary small ring.

Physical placement capacity and roster capacity are distinct:

- When no valid endpoint is available, the soldier remains alive and owned by its formation with explicit `Unplaced` state. It settles where movement stopped. It is not counted as stationed at the fort, deleted, teleported, or stacked onto another reservation. Repeated ticks do not repeat the search.
- A new command, shape change, or casualty compaction rebuilds the plan and retries placement. A recruit does not reshuffle incumbents. A terrain change alone does not currently invalidate settled unplaced members; reissue a movement command to retry. This is an intentional event boundary, not continuous tick reorganization.
- A route requesting a rebuild remains `Moving`; only a genuinely lost route yields `Blocked`. This distinction also applies to non-formation users of exact movement.
- If a mod reduces declared capacity below the loaded roster, the extended roster retains those members. The load bridge warns once when sending excess soldiers back to barracks. Already-returning overflow is valid serialized state. While outside the new footprint they cannot obtain an aliased station. Normal recruitment cannot create this overflow.
- The extended save representation can hold 256 roster indices, matching the existing byte-sized figure index. A relationship claiming more than that remains an unsupported malformed roster; this change does not expand the save format.

Endpoint reservation prevents duplicate destinations. Figure body collision and intermediate travel remain the generalized movement/router's responsibility.

The performance direction is a shared routing thread pool, with immutable query inputs and owner-applied results that cannot outlive the command or topology they were computed for. Formation assignment must remain deterministic and relationship-owned during that migration. This change reuses the generalized router; it does not introduce a separate formation router or worker access to mutable figures/buildings.

## Multiple barracks and commute delay

Each barracks retains its own staffing/food-adjusted recruitment timer and chooses the closest eligible formation according to its priority. A formation is eligible when commanded home, not cursed or in distant service, below recruitment capacity, and without a recruit traveling to its academy or fort.

Publishing a recruit immediately reserves that formation through the owned roster. A second barracks therefore chooses another eligible formation, even in the same building update. Two equally supplied/staffed barracks can deliver two recruits per recruitment cycle to two formations; they do not double the rate into a single formation. The academy/fort commute remains part of recruitment delay. Once that commute ends, eligibility resumes. A settled unplaced member does not permanently stop recruitment.

## Standard presentation

The standard remains a non-physical `FormationDestination`. Its pole is the composite request's base slice, so sprite offsets and flag/icon layer offsets use its declared logical size. Previously all pieces were overlays and the request used the default soldier scale, shifting the flag's baseline.

The rendering target is Vulkan, startup-loaded VRAM assets, shaders, and a 3D orthographic camera. Simulation should update formation/figure data only; rendering should consume it once per frame. The standard's draw request is produced by the rendering path, while station assignment changes simulation state. Remaining legacy graphics refresh calls in figure/building logic are migration debt to remove as those paths are converted, not a pattern for new code.

The reference anchor comes from upstream Augustus at commit `85529e47ceef6831455606a979919ea4db5eb841`: `figure_create` initializes completed tile progress, `figure_military_standard_action` uses the fort-ground tile at home, and `adjust_pixel_offset` adds `(29,15)` before subtracting the pole sprite offset. See [legacy city figure drawing](https://github.com/Keriew/augustus/blob/85529e47ceef6831455606a979919ea4db5eb841/src/widget/city/figure.c) and [legacy standard action](https://github.com/Keriew/augustus/blob/85529e47ceef6831455606a979919ea4db5eb841/src/figuretype/soldier.c). That tile anchor is retained; only the incorrect composite scaling is corrected.

## Validation

`StartupParserTest` covers XML replacement/suppression, geometry at multiple capacities, proportions of partially filled lines, compact partial squares, unique fixed-point stations, obstacle fallback, and exhausted physical capacity with stable unplaced members.

The executable accepts `--formation-test --load-save-test <save>` for a destructive **in-memory-only** regression fixture. It checks saved deployed formations before resetting their movement state, fills one legion to its declared capacity, clicks the actual military sidebar, and converges through normal soldier actions and roster refreshes. It checks six tactical geometries plus Vespasian's persistent loose formation, distinct exact endpoints, stable arrivals, flag scaling and reduced capacity, deferred casualty compaction, idle stability, and two barracks recruiting to different formations while retaining commute reservations. It also runs the dock demolition and production ownership regressions. The fixture requires two populated forts, two road-connected barracks, two docks, and a live visitor to a working dock; Aedile 1 16 and Aedile 1 17 supply these. It does not write the fixture to disk.

The startup save gate explicitly imports old saves into temporary canonical copies, reloads them with zero warnings/errors, advances 3000 ticks with a rendered frame per tick, then saves and reloads again to check the runtime producer. It collects diagnostics across the selected saves and returns failure if any save fails. Profiler samples accompany the existing 1000 simulation-ticks-per-second threshold after 1000 warmup ticks. Import repair warnings remain visible and are not treated as a clean original load. Original saves stay unchanged. See also [save/runtime bridge notes](save_load_runtime_bridges.md).

## Dock deletion regression

Building removal disconnects figure relationships before the building has finished retiring. A trade ship previously searched for another dock inside that callback. With route restrictions excluding the other dock, it could reconnect to the dock being removed, which caused the relationship-disconnection loop to repeat indefinitely.

The relationship callback retains synchronous recovery. It passes the removed endpoint supplied by the event into dock selection as an explicit exclusion, even though the ship's relationship has already been cleared. If no permitted dock is available, the ship immediately enters its leaving state through the nearer river entrance or exit. Selection continues to respect the existing goods and trade-route rules. Queue, berth, and departure transitions share the same routing/relationship updates instead of repeating them in each action branch.

Runtime ownership is push-based: owners notify their actual dependents when relationships or relevant state change. Removal processes the affected relationships once; it must not introduce per-frame dock scans or validity polling. The event must either restore the dependent's invariants immediately or schedule a single explicit recovery transition. Imported inconsistent references belong in load repair, not perpetual runtime guards. Existing trade-policy and berth-availability polling is legacy work to migrate through owner notifications when those producers are refactored; it is not the deletion mechanism or the architectural template for new code.

The runtime test starts the real player-demolition transaction with ships approaching the berth, approaching its queue, anchored, and moored. It checks that demolition returns, all links to the removed dock are gone, each affected ship leaves when the surviving dock rejects its route, and visitors to the other dock retain their destinations. It then advances the affected ships for 3000 ticks and requires them to finish leaving.

## Production owner lifetime regression

The manual-test crash reached `BuildingComposition::for_each_member` through native production. A global cache indexed by reusable building ids retained copied `Building` objects, including pointers to the previous runtime's composition. Reusing a slot for the same production type could therefore reuse a stale production object.

Production objects now belong to their building runtime and reference its actual `Building`. Runtime replacement destroys production with its owner, and definition rebinding clears the owned methods. Production resolves composition context through the owner's reciprocal composition link rather than caching another building copy. The regression creates a shipyard, exercises production, deletes it, reuses its slot, and repeats while requiring the exact current owner reference.
