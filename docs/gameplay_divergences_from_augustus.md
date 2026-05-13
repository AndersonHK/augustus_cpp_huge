# Gameplay Divergences From Augustus

This file tracks deliberate or observed gameplay differences between this repo and upstream Augustus. It is meant to grow: add entries when a difference changes player-visible simulation behavior, not when a system is merely implemented with different code.

Labels:

- `Project-wide`: shared runtime behavior can affect every bundled mod in this engine.
- `Bundled Augustus`: this repo's `Mods/Augustus` behavior differs from upstream Augustus.
- `Vespasian-only`: `Mods/Vespasian` differs from upstream Augustus or from the bundled Augustus compatibility profile.

## Residential Walkers

### Beggar Spawning

Labels: `Project-wide`, `Bundled Augustus`, `Vespasian-only`

Upstream Augustus/Julius beggar spawning was hardcoded in the house figure-generation path with city-level thresholds, a shared counter, and legacy house delay timing. This repo now treats beggars as ordinary house `BuildingType` spawns using `FIGURE_BEGGAR` profile `unemployment_wanderer`, `delay_bands="100:0"`, and `figure_slot="quaternary"`.

For the bundled Augustus and Julius profiles, the XML uses `chance_source="city_unemployment_percent"` plus `chance_per_million_bands` to preserve the old expected active beggar ratios statistically. Exact spawn order differs because each house rolls independently and owns one active residential walker slot.

Vespasian intentionally diverges further: its house XML uses `chance_source="house_unemployed_workers"` plus a per-house `chance_divisor`. Beggar pressure is local to each household's unemployed workers instead of being derived only from citywide unemployment. Wealthier plebeian houses have much lower odds because their divisors are capacity-scaled.

Primary references:

- `Mods/Vespasian/BuildingType/house_*.xml`
- `Mods/Augustus/BuildingType/house_*.xml`
- `Mods/Julius/BuildingType/house_*.xml`
- `src/building/building_runtime_spawn.cpp`
- `docs/walker_pathing_runtime.md`

### Patrician Spawning

Labels: `Project-wide`, `Bundled Augustus`

Upstream patrician spawning used hardcoded residential generation logic, including the legacy citywide one-patrician-per-generation throttle. This repo now spawns patricians through house `BuildingType` XML using profile `house_roamer`, `delay_bands="100:0"`, `figure_slot="quaternary"`, and `chance_per_million="24390"` while no active patrician is already owned by the house.

The intended average cadence matches the old per-house 40-delay check statistically, but the citywide throttle is gone. This means the bundled Augustus profile is no longer sequence-identical to upstream Augustus even though the authored per-house probability is designed to stay close to the old cadence.

Primary references:

- `Mods/Vespasian/BuildingType/house_small_villa.xml` through `house_luxury_palace.xml`
- `Mods/Augustus/BuildingType/house_small_villa.xml` through `house_luxury_palace.xml`
- `Mods/Julius/BuildingType/house_small_villa.xml` through `house_luxury_palace.xml`
- `src/building/building_runtime_spawn.cpp`
- `docs/walker_pathing_runtime.md`

## Time And Walker Range

### Vespasian Calendar Cadence

Label: `Vespasian-only`

Vespasian uses a slower calendar rhythm: `Mods/Vespasian/defines.xml` sets `ticks_per_day="100"` and uses month lengths close to the real calendar. Bundled Augustus keeps `ticks_per_day="50"` and twelve 16-day months. Any rule expressed in days, months, years, or scaled legacy day ticks can therefore have different player-visible pacing in Vespasian.

Primary references:

- `Mods/Vespasian/defines.xml`
- `Mods/Augustus/defines.xml`
- `docs/tile_scale_and_walker_timescale.md`

### Vespasian FigureType Range Tuning

Label: `Vespasian-only`

Current Vespasian FigureType XML uses longer `max_roam_length` values than bundled Augustus for several migrated walkers. The active rule of thumb is roughly 50% longer range or lifetime until walker range tuning is revisited.

Observed examples:

- `beggar`: Vespasian `1200`, Augustus/Julius `800`
- `patrician`: Vespasian `192`, Augustus/Julius `128`
- `market_trader`: Vespasian `576`, Augustus/Julius `384`
- `engineer` and `prefect`: Vespasian `960`, Augustus/Julius `640`
- `labor_seeker`: Vespasian `576`, Augustus/Julius `384`
- `charioteer`: Vespasian `4800`/`768`, Augustus/Julius `3200`/`512`

Primary references:

- `Mods/Vespasian/FigureType/*.xml`
- `Mods/Augustus/FigureType/*.xml`
- `Mods/Julius/FigureType/*.xml`
- `docs/walker_pathing_runtime.md`

## Labor And Staffing

### Broader Vespasian Local Workforce Coverage

Label: `Vespasian-only`

When global labor is disabled, `method="workforce"` makes a building acquire nearby unemployed house residents instead of relying on the legacy citywide labor coverage model. Bundled Augustus currently uses this method for a smaller set of Augustus-era industries, temples, and entertainment/production buildings. Vespasian applies it to many more public services, raw-material producers, workshops, military/support buildings, and civic buildings.

This changes staffing pressure because Vespasian buildings can compete for local unemployed workers across a wider set of building families. The exact building list is data-owned in XML rather than hardcoded here.

Primary references:

- `Mods/Vespasian/BuildingType/*.xml`
- `Mods/Augustus/BuildingType/*.xml`
- `src/building/local_workforce.cpp`
- `src/building/building_runtime_spawn.cpp`
- `Mods/Vespasian/BuildingType/_README.md`

## Audit Queue

These are likely places to check before claiming the divergence list is complete:

- Building costs, labor counts, and footprint changes in `Mods/Vespasian/BuildingType`.
- Production rates and resource costs in `Mods/Vespasian/ProductionMethod`.
- Housing capacity, prosperity, resident class, tax multipliers, and evolution gates in `Mods/Vespasian/HousingType`.
- Service and walker lifetime values after each new FigureType migration.
- Any future upstream Augustus commits that add slow beggar repositioning or other residential walker changes.
