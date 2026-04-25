# Tile Scale and Walker Timescale

This note translates historical walking-city research into the game's tile and
tick vocabulary. It is deliberately approximate: Caesar/Augustus buildings are
symbolic isometric compounds, not measured architectural plans. The goal is to
make `max_roam_length` tuning compare against plausible walking distances
without mistaking that field for a literal tile count.

## Working Tile Scale

Use **15 meters per tile side** as the working estimate for walker tuning, with
**12-20 meters per tile** as the honest uncertainty band.

Why 15 m/tile:

- Pompeii is a useful Roman urban anchor. Jeremy Hartnett summarizes Pompeii's
  streets as averaging about 5-6 m facade-to-facade, with common bands around
  4-6 m and about 8 m for larger avenues. A one-tile game road should be read
  as an arterial street corridor, not a narrow lane. That pushes the estimate
  above a literal 5-8 m roadbed.
- Medieval burgage plots are a useful London/Paris-adjacent urban morphology
  anchor. Scottish burgh evidence gives frontages commonly around 5.5-6.4 m in
  Perth, about 6.3-7.6 m in Dunfermline, and about 8.5-11.6 m in St Andrews.
  A 2x2 evolved house block should be read as four houses with paths between
  them, not one mansion-sized building. That pulls the estimate below 20-25 m.
- The supplied game image reinforces the compromise: one-tile roads are visually
  broad enough to be arterials, while house tiles are small residential modules.
  A 10 m tile makes houses plausible but undersells arterial roads and civic
  footprints; 20 m makes arterial roads and larger buildings plausible but makes
  ordinary house modules too generous. 15 m is the practical middle.

This estimate should not be used for exact map cartography. It is a tuning
scale: close enough to ask whether a walker is covering 500 m, 1 km, or 2 km.

## Engine Meaning of `max_roam_length`

`max_roam_length` is not tiles. Native roaming controllers increment
`f->roam_length` once per figure action tick, compare it against the XML
`max_roam_length`, then call movement with `movement.roam_ticks`.

The movement loop uses fifteenths of a tile:

- `roam_ticks=1` advances 1/15 tile per game tick on normal road.
- `roam_ticks=2` advances 2/15 tile per game tick on normal road.
- Highway terrain doubles movement ticks, so it doubles physical tiles crossed
  for the same roam budget.

Useful formulae:

```text
normal road tiles before limit = max_roam_length * roam_ticks / 15
all-highway tiles before limit = max_roam_length * roam_ticks * 2 / 15
meters before limit = tiles before limit * tile_side_meters
Vespasian game days before limit = max_roam_length / 100
wall-clock seconds at 100% speed = max_roam_length * 0.016
```

Vespasian currently uses `ticks_per_day="100"`. `speed.cpp` currently maps
100% game speed to 16 ms per game tick, so a 100-tick Vespasian game day is
about 1.6 seconds of wall-clock time at 100% speed.

## Movement Profiles

Assuming 15 m/tile and 100% game speed:

| Movement profile | Effective movement | Ticks per tile | Wall-clock per tile | Physical meaning |
| --- | ---: | ---: | ---: | --- |
| Normal adult/service walker | 1/15 tile/tick | 15.0 | 0.24 sec | Baseline for most `roam_ticks=1` FigureType walkers. |
| Fast child/service walker | 2/15 tile/tick | 7.5 | 0.12 sec | Used by `school_child`; same distance needs half the roam budget. |
| Normal walker on highway | 2/15 tile/tick | 7.5 | 0.12 sec | Highway doubles normal walker distance inside the same budget. |
| Fast walker on highway | 4/15 tile/tick | 3.75 | 0.06 sec | Upper bound if a fast walker stays on highway. |
| Prefect emergency move | about 1.2/15 tile/tick | 12.5 | 0.20 sec | Enemy-response movement adds 20% tick progress before normal movement. |

This wall-clock movement is intentionally very compressed. A walker crossing a
15 m tile in 0.24 seconds is not a real pedestrian speed; it is the visual rate
of an abstract city simulation.

## Current Vespasian Walker Ranges

These are the live XML values observed in `Mods/Vespasian/FigureType` on
2026-04-25, using 15 m/tile. Real walking minutes use 4.8 km/h, or 80 m/min,
for adult walkers; the school-child row uses 3.6 km/h, or 60 m/min.

| Walker group | `roam_ticks` | `max_roam_length` | Normal road tiles | Normal road meters | All-highway meters | Real walk time for normal-road distance | Vespasian days before limit | Wall-clock at 100% |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Barber, bathhouse, librarian, priest, teacher | 1 | 384 | 25.6 | 384 m | 768 m | 4.8 adult min | 3.84 days | 6.1 sec |
| Labor seeker | 1 | 576 | 38.4 | 576 m | 1,152 m | 7.2 adult min | 5.76 days | 9.2 sec |
| Engineer, prefect | 1 | 640 | 42.7 | 640 m | 1,280 m | 8.0 adult min | 6.40 days | 10.2 sec |
| School child | 2 | 192 | 25.6 | 384 m | 768 m | 6.4 child min | 1.92 days | 3.1 sec |

The same table under the 12-20 m/tile uncertainty band:

| Range tier | Normal road tiles | 12 m/tile | 15 m/tile | 20 m/tile |
| --- | ---: | ---: | ---: | ---: |
| `192`, `roam_ticks=2` | 25.6 | 307 m | 384 m | 512 m |
| `384`, `roam_ticks=1` | 25.6 | 307 m | 384 m | 512 m |
| `576`, `roam_ticks=1` | 38.4 | 461 m | 576 m | 768 m |
| `640`, `roam_ticks=1` | 42.7 | 512 m | 640 m | 853 m |

## Comparison to Real Walking Tolerance

The preindustrial walking research in
`docs/preindustrial_walking_service_ranges.md` suggests this design curve:

- 5-10 real minutes one-way: daily essentials, children, frequent household
  errands.
- 10-20 real minutes one-way: ordinary neighborhood services.
- 20-30 real minutes one-way: upper bound for routine adult services and
  workplace access in a foot-only city.
- 30-45 real minutes one-way: exceptional civic, high-value, or public-safety
  trips.

At the 15 m/tile scale, the current Vespasian numbers are local:

- The `384` adult tier is roughly 384 m, or about 4.8 real walking minutes.
  That is a good daily-essential and dense-neighborhood range.
- The `576` labor tier is roughly 576 m, or about 7.2 real walking minutes.
  That is a local-workforce radius, not a long commute radius.
- The `640` engineer/prefect tier is roughly 640 m, or about 8.0 real walking
  minutes. It is "long" only relative to current service walkers.
- An all-highway route doubles those distances, pushing labor seekers near 1.15
  km and engineers/prefects near 1.28 km, which enters ordinary adult
  neighborhood-service territory but still stays below a 20-30 minute commute.

If the design goal is a 20-30 minute ordinary adult commute at 15 m/tile, the
target physical range is about 107-160 normal road tiles, or `max_roam_length`
about 1,600-2,400 for `roam_ticks=1`. That would make walkers stay active for
16-24 Vespasian game days before turning back, so it would change service
cadence as well as distance. In other words, one field is currently carrying at
least three meanings: physical reach, calendar duration, and on-screen visual
persistence.

## Routing Caveat

There is an important local-workforce caveat. Candidate search uses
`map_routing_distance(...) <= max_roam_length`, and the routing distance grid is
a road-step style breadth-first distance. That makes the candidate radius much
larger than the physical tiles a `roam_ticks=1` labor seeker can visibly walk
before its roam budget expires. For example, `max_roam_length=576` can admit a
house hundreds of route steps away, while the visible walker budget covers only
about 38.4 normal road tiles before the limit.

That mismatch is worth treating as a separate tuning or runtime decision:

- If `max_roam_length` remains a time budget, candidate search should probably
  compare against `max_roam_length * roam_ticks / 15` in road tiles, with any
  intended highway or policy bonus applied explicitly.
- If candidate search intentionally wants a larger abstract access radius, the
  XML contract should say that local workforce reach and visible roam lifetime
  are different quantities.

## Tuning Takeaways

- Keep `384` as a short dense-neighborhood service tier if the goal is strong
  pressure to distribute services.
- Treat `576` as a local labor tier, not a long commute tier, at 15 m/tile.
- Treat `640` as a broad local patrol tier; it is still only about 8 real
  walking minutes on ordinary roads.
- To model true pre-rail adult commute tolerance, add either a larger distance
  parameter or decouple access radius from visible walker lifetime. Raising
  `max_roam_length` alone will also make walkers remain alive for many more
  game days.
- When comparing against real history, prefer physical distance first, then
  decide the desired game-calendar and wall-clock feel separately.

## Sources

- Jeremy Hartnett, *The Roman Street: Urban Life and Society in Pompeii,
  Herculaneum, and Rome*, Cambridge University Press, street-width discussion:
  https://dokumen.pub/the-roman-street-urban-life-and-society-in-pompeii-herculaneum-and-rome-9781107105706-9781316226438-1107105706.html
- Pompeii streets summary, including 2.5-4.5 m street-width range:
  https://www.pompeii.org.uk/s.php/tour-the-streets-of-pompeii-pompeii-ruins-en-223-s.htm
- Russel Coleman, "The archaeology of burgage plots in Scottish medieval
  towns: a review", *Proceedings of the Society of Antiquaries of Scotland* 134,
  2004, plot-width discussion:
  https://journals.socantscot.org/index.php/psas/article/download/9608/9575
- Britannica, "Insula", for Roman urban density and multi-story tenement
  context:
  https://www.britannica.com/technology/insula
- Naismith walking baseline, 5 km/h on flat ground:
  https://www.hikeclock.com/learn/naismith-rule
