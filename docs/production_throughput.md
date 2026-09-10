# Production throughput and cycle size

Barracks now use this same runtime; see [troop buffers, recruitment costs, and work modifiers](barracks_recruitment.md).

Implemented 2026-09-10. `production_per_month` is absolute nominal inventory output per game month. `cart_loads` sets output per completed cycle, defaulting to 1; a load is 100 inventory units. Larger outputs require proportionally more work. This rule applies to all resource producers, including workshops, with no farm-only exception.

The runtime computes `base_work = floor(100 × days_in_month × 2 × required_workers / monthly_rate)`, then `cycle_work = max(1, floor(base_work × cart_loads))`. Two production updates per day add the employed workers. Ignoring integer work and incomplete harvests, cycles/month are `monthly_rate / (100 × cart_loads)` and output/year is `12 × monthly_rate`. Composed farms sum their fields. Transport `cart_capacity` does not change production work.

`batch_size` has been removed from the parser, code, shipped XML, and template. All 25 shipped declarations were 1, and it was not serialized in saves. Its former output fallback is now a default `cart_loads` of 1. Its former input multiplier has been removed: **input amounts remain literal quantities per completed cycle**, regardless of output size. Availability checks, deductions, supply-chain queries, blessings, and the UI all use those same quantities. Changing output size without changing inputs therefore changes the material ratio; it does not change monthly output. Treasury costs remain per cycle. Non-resource effects, such as boat construction, retain their existing timing and per-event inputs. Figure-delivery production still uses its own delivery loop.

Output fractions must represent whole inventory units (for example 1/5 load = 20 units). Existing integer update rounding, incomplete harvests at year-end, shortages, storage, and delivery can change actual delivered totals relative to nominal throughput. No fractional production-progress save field is introduced. Existing saved progress remains loadable and is checked against the new cycle threshold on the next update.

The regression baseline is upstream `95e120d80`, already merged into this repository. Its `src/game/resource.c` defines wheat 160/month, other farm crops 80, pottery/oil/furniture/wine/weapons 40, clay/timber/iron 80, marble 40, gold 20, sand/concrete 120, stone 80, bricks 60, fish metadata 100, and denarii 200. Five wheat fields at 32/month equal 160/month; five other fields at 16 equal 80. The city mint's gold method uses the denarii rate of 200, matching upstream `src/building/industry.c`; a gold mine uses 20. Fish metadata checks do not measure fishing-boat yield.

## Wheat harvests and staffing

Both standard farms require 5 main-building workers plus 1 per field: 10 workers. Julius/Augustus fields emit one-fifth of a load and complete cycles five times as frequently, measured in game months, as Vespasian's one-load fields. The five fields harvest individually. Vespasian retains its existing cycle length; Julius/Augustus recover the work reduction that their smaller harvests were missing.

| Standard five-field wheat farm, central/desert climate | Julius/Augustus | Vespasian |
| --- | ---: | ---: |
| Workers | 10 | 10 |
| Loads per field harvest | 0.2 | 1 |
| Nominal field harvests per month | 1.6 | 0.32 |
| Nominal farm inventory units per month | 160 | 160 |
| Nominal farm inventory units per year | 1,920 | 1,920 |
| Residents supported at 6 units/year | 320 | 320 |
| Ticks per game year | 9,600 | 36,500 |
| Nominal cartloads per 1,000 ticks | 2.000 | 0.526 |

Month length cancels out of nominal annual production. Output per tick differs because Vespasian's year is longer; food consumption and annual taxes also follow that calendar. Northern wheat keeps its -50% climate adjustment. Monthly food is rounded down in each house, which can permit a population slightly above the smooth 320-person benchmark.

## Housing payroll implications

For 1,296 Small Shack residents, food demand is 7,776 units/year. Both stacks require `ceil(7,776 / 1,920) = 5` wheat farms: **50 production workers**, plus the same 45 allocated service workers. At wage 30, annual expenses are **285**, tax potential is **544.32**, and the balance is **+259.32** in either stack. Earlier 105-versus-25 and 210-versus-50 staffing comparisons are superseded: they omitted field labor and/or included the now-fixed Julius/Augustus underproduction.

The full housing report uses fully staffed nominal domestic chains, five productive fields per farm, central climate, current housing capacities, and whole-building rounding. Raw materials for workshops are included; extra industrial services, supporting workers' housing, reserves, and transport bottlenecks are not. Foregone export income is addressed separately in the export-value table.
