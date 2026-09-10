# Annual housing consumption and recurring budgets

Implemented housing rules, revised 2026-09-09. Vespasian now uses 0.3/0.6 rates. The patrician capacity rework below is a proposal, not an implemented change. This replaces the fixed-per-building consumption assumptions in the earlier planning tables.

## Implementation contract

The four manufactured-goods attributes under `<requirements>` now declare **annual inventory units per actual resident**. Food still uses `food_types` and its existing population-based ration calculation. One trade load contains 100 inventory units.

- Julius and Augustus use exact fractions, calibrated separately for ordinary and merged houses. Small Insula uses `pottery="12/19"`; its merged variant uses `24/76`. Full occupancy therefore consumes exactly 12 or 24 pottery per year. Five residents in an ordinary Small Insula owe 60/19 pottery per year; the fraction carries into subsequent years.
- Vespasian plebeians consume 0.3 of each demanded manufactured good per resident per year. Patricians consume 0.6 pottery, 0.6 oil, and 0.6 furniture. Their wine is **0.3 through Grand Villa, then 0.6 from Small Palace onward**. This is a 70% reduction from the previous 1/2 rates; food is unchanged.
- `wine_sources` is independent of quantity: 1 through Grand Villa, 2 for palaces. Omitting it defaults to 1 when wine is demanded, otherwise 0. Existing source-access requirements are preserved.
- The previous proposed unlock moves and fractional tax multipliers remain planning proposals. This change preserves current unlocks and tax multipliers.

Execution plan: introduce rational rate parsing; convert profiles and merged variants; charge actual residents with persistent fractional carry; scale market targets; preserve legacy scenario/save boundaries; validate shipped rates and save/render behavior; regenerate the comparison below. These changes are implemented in the working tree.

The parser accepts nonnegative integers, exact fractions, and decimals with up to six decimal places. Fractions avoid approximating 12/19 as 0.63. Existing one/two-event monthly cadence and consumption-reduction bonuses remain in force. Market stock targets use the larger requirement of the current and next tier, keeping an eight-event buffer (twelve with the Venus bonus) scaled by current residents and annual demand. Supply must still reach the market and house.

Fractional carry belongs to each house and each good. Merging adds it; splitting apportions it. Save version 212 appends four doubles to the building record; older saves initialize these fractions to zero. Invalid serialized fractions are repaired with a warning. Whole demand that cannot be supplied is not accumulated as debt, matching the previous shortage behavior. Legacy scenario model controls and imported scenario overrides retain their per-event integer contract through an adapter that converts them to annual resident rates; the XML profile contract is the new annual one.

## Benchmark and limits

Each row uses **144 residential tiles**, fully occupied: 36 merged/fixed 2x2 houses, 16 3x3 houses, or 9 4x4 houses. Roads and services occupy additional space. This compares a fixed serviced area, allowing density to reduce service expense per person. It is not a tested 12x12 block layout.

Annual denarii estimates use Hard difficulty, 7% tax, complete tax coverage, wages 30, and 38% plebeian workforce. Patricians supply no workers. Tax potential is `6 × population × multiplier × 0.07`. Payroll is `staff × 30 / 10` annually. The game rounds actual tax, food, and wages, so these are smooth budget estimates, rounded only for display; displayed columns can differ by one denarius when subtracted.

The first budget table is a **land-import-funded supply case**; the second values the same food and goods at export prices. Food averages 6 units/person/year total: wheat for one food type; equal wheat/vegetables for two; equal wheat/vegetables/fruit for three. Per-load buy prices are 28/38/38 food, 180 pottery, 180 oil, 200 furniture, and 215 wine. Corresponding sell prices are 22/30/30 food, 140 pottery, 140 oil, 150 furniture, and 160 wine. These are the authored base prices in both stacks, before scenario changes or trade bonuses. Goods quantities below are annual demand; integer consumption carries any fraction between years. Food cost approximates the existing per-house/per-food rounding.

Shared service assumptions, held fixed until a requirement adds a service:

| Requirement | Allocated service staff | Annual levy |
| --- | ---: | ---: |
| Safety and taxes: 2 prefectures, engineer, forum | 23 | 0 |
| Food distribution: 2 markets, granary | 16 | 0 |
| Warehouse, when food or manufactured goods are required | 6 | 0 |
| Wells | 0 | 0 |
| Fountain water: 5 fountains and supplied water network | 20 | 0 |
| Each god: small temple | 2 | 48 |
| Education 1 / 2 / 3: school; plus library; plus academy | 10 / 30 / 60 | 0 |
| Bathhouse / barber | 10 / 2 | 0 |
| Health 1 / 2: doctor; plus hospital | 5 / 35 | 0 |
| Entertainment >0: theater and actors | 13 | 0 |
| Entertainment >10: add amphitheater and gladiators | +20 | 0 |
| Entertainment >25: add arena and lions | +33 | 0 |
| Entertainment >45: allocate 10% of hippodrome and chariots | +16 | 86.4 |

Maintenance here means actual recurring temple/hippodrome levies. Construction, land purchase, and initial stocking are excluded. No monument bonuses, tourism, exports, military costs, festivals, or administrative salaries are allocated. This benchmark assumes shared entertainment supply and adequate service capacity; it does not prove coverage, enrollment, market throughput, trade quota availability, or reserve staffing. Extra service facilities raise the bill. Source counts are scenario assumptions, not engine-enforced ratios.

Domestic production needs a different resource bill: allocate farm/workshop/raw-material-chain wages, maintenance, transport, and throughput. **Do not subtract both the import value and the domestic production payroll for the same units.** Surplus plebeian labor can produce these goods and exports; an import-funded deficit alone does not establish that a tier is economically unviable. Patrician districts also need workers from elsewhere; this table does not allocate those workers' housing costs a second time.

## Annual manufactured goods per full house

Goods columns are **pottery / oil / furniture / wine**, in annual inventory units of demand. The first ten tiers use their merged 2x2 variant. An unmerged full Large Casa/Small Insula consumes 12 pottery in Julius/Augustus; merged consumes 24. Unmerged Medium Insula consumes 12 each pottery/furniture; merged consumes 24 each. At the new Vespasian rates, unmerged Small Insula demands 5.7 pottery/year and merged demands 22.8; fractional carry avoids truncating this to a permanently lower rate.

| Tier | Footprint tiles | Residents/house | Augustus P/O/F/W | Vespasian P/O/F/W |
| --- | ---: | ---: | --- | --- |
| Small Tent | 4 | 20 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Large Tent | 4 | 28 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Small Shack | 4 | 36 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Large Shack | 4 | 44 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Small Hovel | 4 | 52 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Large Hovel | 4 | 60 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Small Casa | 4 | 68 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Large Casa | 4 | 76 | 24 / 0 / 0 / 0 | 22.8 / 0 / 0 / 0 |
| Small Insula | 4 | 76 | 24 / 0 / 0 / 0 | 22.8 / 0 / 0 / 0 |
| Medium Insula | 4 | 80 | 24 / 0 / 24 / 0 | 24 / 0 / 24 / 0 |
| Large Insula | 4 | 84 | 24 / 24 / 24 / 0 | 25.2 / 25.2 / 25.2 / 0 |
| Grand Insula | 4 | 84 | 24 / 24 / 24 / 0 | 25.2 / 25.2 / 25.2 / 0 |
| Small Villa | 4 | 40 | 24 / 24 / 24 / 24 | 24 / 24 / 24 / 12 |
| Medium Villa | 4 | 42 | 24 / 24 / 24 / 24 | 25.2 / 25.2 / 25.2 / 12.6 |
| Large Villa | 9 | 90 | 24 / 24 / 24 / 24 | 54 / 54 / 54 / 27 |
| Grand Villa | 9 | 100 | 24 / 24 / 24 / 24 | 60 / 60 / 60 / 30 |
| Small Palace | 9 | 106 | 24 / 24 / 24 / 48 | 63.6 / 63.6 / 63.6 / 63.6 |
| Medium Palace | 9 | 112 | 24 / 24 / 24 / 48 | 67.2 / 67.2 / 67.2 / 67.2 |
| Large Palace | 16 | 190 | 24 / 24 / 24 / 48 | 114 / 114 / 114 / 114 |
| Luxury Palace | 16 | 200 | 24 / 24 / 24 / 48 | 120 / 120 / 120 / 120 |

## Annual district tax versus total recurring expenses

Same population and tax potential for both stacks. **Balance = tax − wages − levies − food − manufactured goods.** Positive values are surpluses within this benchmark; negative values need other income or a cheaper supply chain.

| Tier | Residents | Tax potential | Augustus expenses | Augustus balance | Vespasian expenses | Vespasian balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | 720 | 302 | 69 | 233 | 69 | 233 |
| Large Tent | 1,008 | 423 | 69 | 354 | 69 | 354 |
| Small Shack | 1,296 | 544 | 2,312 | -1,768 | 2,312 | -1,768 |
| Large Shack | 1,584 | 665 | 2,850 | -2,185 | 2,850 | -2,185 |
| Small Hovel | 1,872 | 1,572 | 3,394 | -1,821 | 3,394 | -1,821 |
| Large Hovel | 2,160 | 1,814 | 3,917 | -2,102 | 3,917 | -2,102 |
| Small Casa | 2,448 | 2,056 | 4,431 | -2,374 | 4,431 | -2,374 |
| Large Casa | 2,736 | 2,298 | 6,500 | -4,201 | 6,422 | -4,124 |
| Small Insula | 2,736 | 3,447 | 6,560 | -3,112 | 6,482 | -3,035 |
| Medium Insula | 2,880 | 3,629 | 8,545 | -4,916 | 8,545 | -4,916 |
| Large Insula | 3,024 | 3,810 | 10,408 | -6,597 | 10,650 | -6,839 |
| Grand Insula | 3,024 | 5,080 | 11,414 | -6,334 | 11,656 | -6,576 |
| Small Villa | 1,440 | 5,443 | 10,189 | -4,746 | 9,260 | -3,817 |
| Medium Villa | 1,512 | 6,350 | 10,422 | -4,071 | 9,781 | -3,431 |
| Large Villa | 1,440 | 6,653 | 6,649 | 4 | 9,440 | -2,788 |
| Grand Villa | 1,600 | 7,392 | 7,314 | 78 | 10,746 | -3,354 |
| Small Palace | 1,696 | 8,548 | 8,340 | 208 | 12,424 | -3,877 |
| Medium Palace | 1,792 | 9,032 | 8,593 | 438 | 13,125 | -4,093 |
| Large Palace | 1,710 | 10,773 | 6,760 | 4,013 | 12,573 | -1,800 |
| Luxury Palace | 1,800 | 12,096 | 6,947 | 5,149 | 13,178 | -1,082 |

## Annual district tax versus recurring expenses at export values

This alternative uses **sell prices for both food and manufactured goods**. Population, tax, service payroll, and levies match the import table. **Opportunity-cost balance = tax − service wages − levies − export value of consumed food and goods.** The resource value is forgone revenue, not a treasury payment.

Assume the same local production is available in both alternatives and its output could actually be sold. Production payroll is common to producing-for-housing and producing-for-export, so this comparison does not add it again. The separate domestic payroll table measures cash requirements. Adding that entire production bill to this opportunity-cost table would mix the two baselines. Extra production, transport, or export-only costs require an incremental comparison of their own.

If export quotas, buyers, or routes prevent selling the marginal output, the full sell price overstates its opportunity cost. Bonuses and scenario-specific prices also change the result. This table measures the value of resources allocated to housing under available export demand; it is not a complete city treasury budget. It credits taxes only, without monetizing plebeian labor, prosperity, or other housing benefits.

| Tier | Residents | Tax potential | Augustus expenses at export values | Augustus balance | Vespasian expenses at export values | Vespasian balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | 720 | 302 | 69 | 233 | 69 | 233 |
| Large Tent | 1,008 | 423 | 69 | 354 | 69 | 354 |
| Small Shack | 1,296 | 544 | 1,846 | -1,301 | 1,846 | -1,301 |
| Large Shack | 1,584 | 665 | 2,280 | -1,615 | 2,280 | -1,615 |
| Small Hovel | 1,872 | 1,572 | 2,720 | -1,148 | 2,720 | -1,148 |
| Large Hovel | 2,160 | 1,814 | 3,139 | -1,325 | 3,139 | -1,325 |
| Small Casa | 2,448 | 2,056 | 3,549 | -1,493 | 3,549 | -1,493 |
| Large Casa | 2,736 | 2,298 | 5,169 | -2,871 | 5,109 | -2,810 |
| Small Insula | 2,736 | 3,447 | 5,229 | -1,782 | 5,169 | -1,721 |
| Medium Insula | 2,880 | 3,629 | 6,730 | -3,101 | 6,730 | -3,101 |
| Large Insula | 3,024 | 3,810 | 8,196 | -4,386 | 8,382 | -4,571 |
| Grand Insula | 3,024 | 5,080 | 9,021 | -3,940 | 9,206 | -4,126 |
| Small Villa | 1,440 | 5,443 | 7,986 | -2,543 | 7,295 | -1,852 |
| Medium Villa | 1,512 | 6,350 | 8,188 | -1,838 | 7,717 | -1,367 |
| Large Villa | 1,440 | 6,653 | 5,334 | 1,319 | 7,475 | -822 |
| Grand Villa | 1,600 | 7,392 | 5,900 | 1,492 | 8,530 | -1,138 |
| Small Palace | 1,696 | 8,548 | 6,672 | 1,876 | 9,796 | -1,248 |
| Medium Palace | 1,792 | 9,032 | 6,883 | 2,148 | 10,347 | -1,315 |
| Large Palace | 1,710 | 10,773 | 5,489 | 5,284 | 9,922 | 851 |
| Luxury Palace | 1,800 | 12,096 | 5,636 | 6,460 | 10,388 | 1,708 |

## Expense decomposition and density

Wages, levies, and food are shared across these Augustus/Vespasian rows because service unlocks and capacity are unchanged. Service/resident includes wages plus levies. Remaining workers are the 38% plebeian workforce less service staff, **before** domestic production or other jobs. Negative values for patricians indicate workers supplied by other districts.

| Tier | Wages | Levies | Food | Augustus goods | Vespasian goods | Service/resident | Remaining workers |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | 69 | 0.0 | 0 | 0 | 0 | 0.096 | 250.6 |
| Large Tent | 69 | 0.0 | 0 | 0 | 0 | 0.068 | 360.0 |
| Small Shack | 135 | 0.0 | 2,177 | 0 | 0 | 0.104 | 447.5 |
| Large Shack | 141 | 48.0 | 2,661 | 0 | 0 | 0.119 | 554.9 |
| Small Hovel | 201 | 48.0 | 3,145 | 0 | 0 | 0.133 | 644.4 |
| Large Hovel | 240 | 48.0 | 3,629 | 0 | 0 | 0.133 | 740.8 |
| Small Casa | 270 | 48.0 | 4,113 | 0 | 0 | 0.130 | 840.2 |
| Large Casa | 300 | 48.0 | 4,596 | 1,555 | 1,477 | 0.127 | 939.7 |
| Small Insula | 360 | 48.0 | 4,596 | 1,555 | 1,477 | 0.149 | 919.7 |
| Medium Insula | 375 | 48.0 | 4,838 | 3,283 | 3,283 | 0.147 | 969.4 |
| Large Insula | 441 | 48.0 | 5,080 | 4,838 | 5,080 | 0.162 | 1,002.1 |
| Grand Insula | 540 | 48.0 | 5,988 | 4,838 | 5,080 | 0.194 | 969.1 |
| Small Villa | 546 | 96.0 | 2,851 | 6,696 | 5,767 | 0.446 | -182.0 |
| Medium Villa | 636 | 96.0 | 2,994 | 6,696 | 6,056 | 0.484 | -212.0 |
| Large Villa | 726 | 96.0 | 2,851 | 2,976 | 5,767 | 0.571 | -242.0 |
| Grand Villa | 780 | 230.4 | 3,328 | 2,976 | 6,408 | 0.631 | -260.0 |
| Small Palace | 780 | 230.4 | 3,528 | 3,802 | 7,886 | 0.596 | -260.0 |
| Medium Palace | 786 | 278.4 | 3,727 | 3,802 | 8,333 | 0.594 | -262.0 |
| Large Palace | 786 | 278.4 | 3,557 | 2,138 | 7,952 | 0.622 | -262.0 |
| Luxury Palace | 786 | 278.4 | 3,744 | 2,138 | 8,370 | 0.591 | -262.0 |
## Domestic production payroll scenario

Implemented 2026-09-10. `production_per_month` is absolute nominal inventory output per game month. `cart_loads` sets output per completed cycle, defaulting to 1; a load is 100 inventory units. Larger outputs require proportionally more work. This rule applies to all resource producers, including workshops, with no farm-only exception.

The runtime computes `base_work = floor(100 × days_in_month × 2 × required_workers / monthly_rate)`, then `cycle_work = max(1, floor(base_work × cart_loads))`. Two production updates per day add the employed workers. Ignoring integer work and incomplete harvests, cycles/month are `monthly_rate / (100 × cart_loads)` and output/year is `12 × monthly_rate`. Composed farms sum their fields. Transport `cart_capacity` does not change production work.

`batch_size` has been removed from the parser, code, shipped XML, and template. All 25 shipped declarations were 1, and it was not serialized in saves. Its former output fallback is now a default `cart_loads` of 1. Its former input multiplier has been removed: **input amounts remain literal quantities per completed cycle**, regardless of output size. Availability checks, deductions, supply-chain queries, blessings, and the UI all use those same quantities. Changing output size without changing inputs therefore changes the material ratio; it does not change monthly output. Treasury costs remain per cycle. Non-resource effects, such as boat construction, retain their existing timing and per-event inputs. Figure-delivery production still uses its own delivery loop.

Output fractions must represent whole inventory units (for example 1/5 load = 20 units). Existing integer update rounding, incomplete harvests at year-end, shortages, storage, and delivery can change actual delivered totals relative to nominal throughput. No fractional production-progress save field is introduced. Existing saved progress remains loadable and is checked against the new cycle threshold on the next update.

The regression baseline is upstream `95e120d80`, already merged into this repository. Its `src/game/resource.c` defines wheat 160/month, other farm crops 80, pottery/oil/furniture/wine/weapons 40, clay/timber/iron 80, marble 40, gold 20, sand/concrete 120, stone 80, bricks 60, fish metadata 100, and denarii 200. Five wheat fields at 32/month equal 160/month; five other fields at 16 equal 80. The city mint's gold method uses the denarii rate of 200, matching upstream `src/building/industry.c`; a gold mine uses 20. Fish metadata checks do not measure fishing-boat yield.

### Wheat harvests and staffing

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

### Housing payroll implications

For 1,296 Small Shack residents, food demand is 7,776 units/year. Both stacks require `ceil(7,776 / 1,920) = 5` wheat farms: **50 production workers**, plus the same 45 allocated service workers. At wage 30, annual expenses are **285**, tax potential is **544.32**, and the balance is **+259.32** in either stack. Earlier 105-versus-25 and 210-versus-50 staffing comparisons are superseded: they omitted field labor and/or included the now-fixed Julius/Augustus underproduction.

The full housing report uses fully staffed nominal domestic chains, five productive fields per farm, central climate, current housing capacities, and whole-building rounding. Raw materials for workshops are included; extra industrial services, supporting workers' housing, reserves, and transport bottlenecks are not. Foregone export income is addressed separately in the export-value table.


### Annual district tax versus domestic recurring expenses

| Tier | Augustus production staff | Augustus expenses | Augustus balance | Vespasian production staff | Vespasian expenses | Vespasian balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | 0 | 69 | 233 | 0 | 69 | 233 |
| Large Tent | 0 | 69 | 354 | 0 | 69 | 354 |
| Small Shack | 50 | 285 | 259 | 50 | 285 | 259 |
| Large Shack | 50 | 339 | 326 | 50 | 339 | 326 |
| Small Hovel | 60 | 429 | 1,143 | 60 | 429 | 1,143 |
| Large Hovel | 70 | 498 | 1,316 | 70 | 498 | 1,316 |
| Small Casa | 80 | 558 | 1,498 | 80 | 558 | 1,498 |
| Large Casa | 120 | 708 | 1,590 | 120 | 708 | 1,590 |
| Small Insula | 120 | 768 | 2,679 | 120 | 768 | 2,679 |
| Medium Insula | 150 | 873 | 2,756 | 150 | 873 | 2,756 |
| Large Insula | 190 | 1,059 | 2,751 | 190 | 1,059 | 2,751 |
| Grand Insula | 240 | 1,308 | 3,772 | 240 | 1,308 | 3,772 |
| Small Villa | 200 | 1,242 | 4,201 | 190 | 1,212 | 4,231 |
| Medium Villa | 200 | 1,332 | 5,018 | 190 | 1,302 | 5,048 |
| Large Villa | 160 | 1,302 | 5,351 | 190 | 1,392 | 5,261 |
| Grand Villa | 180 | 1,550 | 5,842 | 210 | 1,640 | 5,752 |
| Small Palace | 190 | 1,580 | 6,967 | 300 | 1,910 | 6,637 |
| Medium Palace | 190 | 1,634 | 7,397 | 300 | 1,964 | 7,067 |
| Large Palace | 180 | 1,604 | 9,169 | 300 | 1,964 | 8,809 |
| Luxury Palace | 180 | 1,604 | 10,492 | 300 | 1,964 | 10,132 |

At Luxury Palace, nominal Vespasian production needs 300 workers plus 262 service staff, all supplied from plebeian districts. Augustus needs 180 plus 262. Supporting workers' own consumption, industrial land, and distribution facilities still need accounting. These are capacity/payroll estimates, not guaranteed profits.

## Balance implications

Large Tent illustrates the density benefit: the same 69 annual service budget covers 1,008 residents instead of Small Tent's 720. Annual tax potential rises from 302 to 423 while service cost per resident falls from 0.096 to 0.068. Required wells add no wages or maintenance.

Small Insula remains a strong transition from Large Casa: both merged houses hold 76, with the same resource demand. The extra amphitheater and gladiator school cost 60/year in the district, while the tax step adds approximately 1,149/year. The resulting balance improves about 1,089/year in either stack. This implementation preserves that tax step.

Vespasian also loses the old consumption discount from merging four houses: four full unmerged Small Insulae and one full merged Small Insula now both demand 22.8 pottery/year. Augustus preserves its vanilla 48 versus 24 totals. Population and residential density are identical in this comparison; the difference is the resource rule.

At 0.3, every extra plebeian at Large/Grand Insula requires 1.68/year of imported pottery, oil, and furniture, or 1.29 at export values. Grand Insula tax potential is 1.68/person/year before food or services. This means the import-valued manufactured goods alone use the entire tax potential.

The wine exception softens villa demand: imported manufactured goods cost 4.005/person/year through Grand Villa, then 4.65 for palaces. Villa wine at 0.3 instead of 0.6 saves 0.645/person/year at import prices, or 1,032/year for the current Grand Villa district. At export prices, the saving is 0.48/person/year, or 768/year for that district.

Luxury Palace's Vespasian import-valued expenses are 13,178 against 12,096 tax potential, for a balance of -1,082. Its import-case break-even tax rate is 7.63%. At export values its expense is 10,388 and balance +1,708, versus Augustus's +6,460. These figures use current capacities. Density can amortize shared services, but it cannot amortize a per-resident goods bill.

## Proposed patrician capacity rework — not implemented

Keep footprints and tax multipliers unchanged. Start Small Villa at **22 residents per tile**, just above Grand Insula's 21, then add **2 residents per tile per patrician tier**. Capacity equals density times footprint, so neither the move to 3x3 nor the move to 4x4 causes a density drop. Only the calculator's `-ProposedPatricianCapacities` switch applies this scenario; shipped BuildingType capacities are unchanged. All preceding tables use current capacities.

With the current tax multipliers, Small Villa already beats Grand Insula in gross tax: `40 × 9 = 360` versus `84 × 4 = 336`, a 7.14% advantage on the same four tiles. The tax constraint alone requires at least 38 villa residents. Strictly increasing density requires at least 85; the proposed 88 gives a simple 22/tile starting point. At 7% tax on Hard, the proposed Small Villa collects 332.64 per house/year versus Grand Insula's 141.12, a **135.7% increase**. Both the per-house and equal-area tax tests are satisfied.

| Tier | Footprint tiles | Current capacity | Current residents/tile | Proposed capacity | Proposed residents/tile | Current annual tax/house | Proposed annual tax/house |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Grand Insula | 4 | 84 | 21.00 | 84 | 21.00 | 141.12 | 141.12 |
| Small Villa | 4 | 40 | 10.00 | 88 | 22.00 | 151.20 | 332.64 |
| Medium Villa | 4 | 42 | 10.50 | 96 | 24.00 | 176.40 | 403.20 |
| Large Villa | 9 | 90 | 10.00 | 234 | 26.00 | 415.80 | 1,081.08 |
| Grand Villa | 9 | 100 | 11.11 | 252 | 28.00 | 462.00 | 1,164.24 |
| Small Palace | 9 | 106 | 11.78 | 270 | 30.00 | 534.24 | 1,360.80 |
| Medium Palace | 9 | 112 | 12.44 | 288 | 32.00 | 564.48 | 1,451.52 |
| Large Palace | 16 | 190 | 11.88 | 544 | 34.00 | 1,197.00 | 3,427.20 |
| Luxury Palace | 16 | 200 | 12.50 | 576 | 36.00 | 1,344.00 | 3,870.72 |

The following applies **both the implemented 0.3/0.6 consumption rates and the proposed capacities** to the same 144 residential tiles. Augustus remains at its current capacities; its unchanged export-value comparison is above. Shared service staffing is held constant for the density experiment. More residents may require extra capacity-limited facilities or market throughput, which would raise these expenses.

| Tier | Proposed district residents | Current tax | Proposed tax | Current export-value balance | Proposed export-valued expenses | Proposed export-value balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Grand Insula | 3,024 | 5,080 | 5,080 | -4,126 | 9,206 | -4,126 |
| Small Villa | 3,168 | 5,443 | 11,975 | -1,852 | 15,278 | -3,303 |
| Medium Villa | 3,456 | 6,350 | 14,515 | -1,367 | 16,699 | -2,184 |
| Large Villa | 3,744 | 6,653 | 17,297 | -822 | 18,119 | -822 |
| Grand Villa | 4,032 | 7,392 | 18,628 | -1,138 | 19,961 | -1,333 |
| Small Palace | 4,320 | 8,548 | 21,773 | -1,248 | 23,388 | -1,615 |
| Medium Palace | 4,608 | 9,032 | 23,224 | -1,315 | 24,934 | -1,710 |
| Large Palace | 4,896 | 10,773 | 30,845 | 851 | 26,426 | 4,419 |
| Luxury Palace | 5,184 | 12,096 | 34,836 | 1,708 | 27,918 | 6,919 |

Gross taxes rise at every proposed tier, but more density does not guarantee a better opportunity-cost balance. The relevant equation is `balance = population × (tax/person − resource export value/person) − shared services`. With negative per-resident margins, additional residents deepen the district's absolute opportunity-cost deficit even while service cost per resident falls.

| Tier | Annual tax/person | Food and goods export value/person | Margin/person before services |
| --- | ---: | ---: | ---: |
| Grand Insula | 1.68 | 2.85 | -1.17 |
| Small Villa | 3.78 | 4.62 | -0.84 |
| Medium Villa | 4.20 | 4.62 | -0.42 |
| Large Villa | 4.62 | 4.62 | 0.00 |
| Grand Villa | 4.62 | 4.70 | -0.08 |
| Small Palace | 5.04 | 5.18 | -0.14 |
| Medium Palace | 5.04 | 5.18 | -0.14 |
| Large Palace | 6.30 | 5.18 | 1.12 |
| Luxury Palace | 6.72 | 5.18 | 1.54 |

This proposal satisfies the requested density and gross-tax constraints. Keeping the existing villa tax multiplier while removing the density drop produces a much larger gross-tax step than the current progression; that should be considered alongside any later tax-multiplier rework. At the current 7% tax and full export opportunity, the first six patrician tiers still have nonpositive per-resident margins (Large Villa is exactly zero before services). Capacity alone cannot make those tiers opportunity-cost positive. Large and Luxury Palace have positive margins, so their added density improves this balance. Patricians still provide no workers, and supporting plebeian housing and industrial land remain necessary.

## Reproduce and verify

Run `./research/write_housing_consumption_report.ps1` to regenerate this report from `housing_cost_model.ps1` and the authored XML stacks. Run `./research/housing_cost_model.ps1 -ProductionBenchmarks` for resource-by-resource staff, nominal annual output, and average loads per 1,000 ticks. The calculator also accepts `-ResidentialTiles`, `-TaxPercent`, `-Wage`, `-HippodromeShare`, `-ProductionEfficiency`, `-FarmFields`, `-VanillaFarmOutput`, and `-ProposedPatricianCapacities`; changing area or population does not automatically change coverage or service staffing. Export-valued results are emitted alongside import and domestic-payroll results.

Regression coverage includes all shipped Julius/Augustus/Vespasian profiles and merged variants, exact full-year totals, partial occupancy, decimal/fraction parsing, market buffers, and fractional carry through the housing save-state bridge. Build and executable startup/save/render validation results are recorded in [validation notes](housing_consumption_validation_2026_09_09.md).

Local implementation sources: [rates](../src/building/HousingProfileDef.cpp), [consumption](../src/building/house_evolution.cpp), [distribution](../src/figure/service.cpp), [save layout](../src/building/state.cpp), [taxes/wages/levies](../src/city/finance.cpp), [food](../src/city/resource.cpp), [production progress](../src/building/production_method_runtime.cpp), [production output](../src/building/production.cpp), and [regression cases](../tools/startup_parser_test/housing_profile_registry_layering_test.cpp).
