# Caesar III / Julius Housing Progression Defaults

Snapshot: 2026-05-10

## Purpose

This document records the vanilla Caesar III / Julius housing progression as a
design reference for `HousingType`, `BuildingType`, employment, tax, prosperity,
and city-growth pacing. It should be used beside
[Caesar III Housing Balance and Play Analysis](caesar3_housing_balance_play_analysis.md),
[Vespasian Housing Progression Design Notes](vespasian_housing_progression_design_notes.md),
[Roman City Facility Ratios](roman_city_facility_ratios.md),
[Roman City Size and Social Ratios](roman_city_size_and_social_ratios.md), and
[Roman Building and Infrastructure Maintenance Needs](roman_building_maintenance_needs.md).

The progression from tents to palaces is not a historical demographic model. It
is a city-builder grammar: early settlement is poor, rural, temporary, and
labor-heavy; the middle game becomes dense, serviced, and recognizably Roman;
the late game converts scarce, well-served land into elite tax and prestige.
Research should keep the values directionally plausible, but final tuning should
preserve progression feel, player readability, and the intended spirit of the
game. Wealth, footprint, service complexity, and visual polish should correlate
in the player's mind, even when the exact numeric curve uses jumps for balance.

## Source Stack

Julius is the vanilla reference because its stated goal is to preserve Caesar
III logic and save compatibility. The public Julius source loads `c3_model.txt`
and parses 20 house rows into `model_house` values; the evolution code then
checks desirability, services, food, pottery, oil, furniture, wine, and space for
expansion before changing a house level. Julius does not replace the original
game data file: it requires the original Caesar III install.

The values below are taken from the local Julius XML materialization in
`Mods/Julius/HousingType/*.xml` and `Mods/Julius/BuildingType/house_*.xml`,
cross-checked against the public picture table at
[Caesar III Augustus: Housing Levels with pictures](https://mmxl.wz.cz/c3a/housing2.html).
The MMXL table reports full-block population for the 1x1 levels after merging
into 2x2 groups; the XML capacity value is per individual 1x1 house until the
chain expands into fixed 2x2, 3x3, or 4x4 elite structures.

Primary references:

- [Julius README](https://github.com/bvschaik/julius) - Julius preserves Caesar
  III logic and requires original Caesar III files.
- [Julius `model.c`](https://github.com/bvschaik/julius/blob/master/src/building/model.c) -
  model loader for `c3_model.txt`, `NUM_HOUSES = 20`, and house-model fields.
- [Julius `house_evolution.c`](https://github.com/bvschaik/julius/blob/master/src/building/house_evolution.c) -
  evolution, devolution, resource checks, and expansion checks.
- [MMXL housing table](https://mmxl.wz.cz/c3a/housing2.html) - readable table
  with images, population, prosperity, entertainment, tax, desirability, and
  qualitative requirements.
- Local XML: `Mods/Julius/HousingType`, `Mods/Julius/BuildingType`,
  `Mods/Vespasian/Graphics/Housing`, and
  `extracted_graphics_sample/Julius/Graphics/Aesthetics`.

## Readable Default Table

`Cap` is the XML capacity for one occupied building unless the level has already
expanded. `Max block cap` is the population of the natural full footprint: 2x2
for merged tents through medium insulae, then the fixed footprint for large
insulae and above. `Cap/tile` is useful for area pressure: plebeian density rises
until grand insulae, then patrician housing deliberately spends much more land
per person for tax, prestige, and service demand.

| Level | Class | Size | Cap | Max block cap | Cap/tile | Prosperity | Tax x | Devolve / evolve desirability |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | Plebeian | 1x1 | 5 | 20 | 5.0 | 5 | 1 | -99 / -10 |
| Large Tent | Plebeian | 1x1 | 7 | 28 | 7.0 | 10 | 1 | -12 / -5 |
| Small Shack | Plebeian | 1x1 | 9 | 36 | 9.0 | 15 | 1 | -7 / 0 |
| Large Shack | Plebeian | 1x1 | 11 | 44 | 11.0 | 20 | 1 | -2 / 4 |
| Small Hovel | Plebeian | 1x1 | 13 | 52 | 13.0 | 25 | 2 | 2 / 8 |
| Large Hovel | Plebeian | 1x1 | 15 | 60 | 15.0 | 30 | 2 | 6 / 12 |
| Small Casa | Plebeian | 1x1 | 17 | 68 | 17.0 | 35 | 2 | 10 / 16 |
| Large Casa | Plebeian | 1x1 | 19 | 76 | 19.0 | 45 | 2 | 14 / 20 |
| Small Insula | Plebeian | 1x1 | 19 | 76 | 19.0 | 50 | 3 | 18 / 25 |
| Medium Insula | Plebeian | 1x1 | 20 | 80 | 20.0 | 58 | 3 | 22 / 32 |
| Large Insula | Plebeian | 2x2 | 84 | 84 | 21.0 | 65 | 3 | 29 / 40 |
| Grand Insula | Plebeian | 2x2 | 84 | 84 | 21.0 | 80 | 4 | 37 / 48 |
| Small Villa | Patrician | 2x2 | 40 | 40 | 10.0 | 150 | 9 | 45 / 53 |
| Medium Villa | Patrician | 2x2 | 42 | 42 | 10.5 | 180 | 10 | 50 / 58 |
| Large Villa | Patrician | 3x3 | 90 | 90 | 10.0 | 400 | 11 | 55 / 63 |
| Grand Villa | Patrician | 3x3 | 100 | 100 | 11.1 | 600 | 11 | 60 / 68 |
| Small Palace | Patrician | 3x3 | 106 | 106 | 11.8 | 700 | 12 | 65 / 74 |
| Medium Palace | Patrician | 3x3 | 112 | 112 | 12.4 | 900 | 12 | 70 / 80 |
| Large Palace | Patrician | 4x4 | 190 | 190 | 11.9 | 1500 | 15 | 76 / 90 |
| Luxury Palace | Patrician | 4x4 | 200 | 200 | 12.5 | 1750 | 16 | 85 / 100 |

## Requirements Table

Requirements are cumulative. `Water` is stored as `none`, `well`, or `fountain`
in local XML. `Education`, `religion`, and `health` are numeric service-depth
thresholds. `Wine = 2` means access to wine plus a second wine type or source,
matching the original "two types of wine" rule.

| Level | New gate or main sustain pressure | Ent. | Water | Religion | Education | Health | Food | Goods |
| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | --- |
| Small Tent | Immigrants and road access; no civic service gate | 0 | none | 0 | 0 | 0 | 0 | none |
| Large Tent | Any water access, including well | 0 | well | 0 | 0 | 0 | 0 | none |
| Small Shack | First food type | 0 | well | 0 | 0 | 0 | 1 | none |
| Large Shack | One god / temple coverage | 0 | well | 1 | 0 | 0 | 1 | none |
| Small Hovel | Fountain-quality water | 0 | fountain | 1 | 0 | 0 | 1 | none |
| Large Hovel | First meaningful entertainment score | 10 | fountain | 1 | 0 | 0 | 1 | none |
| Small Casa | Basic education | 10 | fountain | 1 | 1 | 0 | 1 | none |
| Large Casa | Bathhouse and pottery | 10 | fountain | 1 | 1 | 0 | 1 | pottery |
| Small Insula | Higher entertainment tier | 25 | fountain | 1 | 1 | 0 | 1 | pottery |
| Medium Insula | First health service and furniture | 25 | fountain | 1 | 1 | 1 | 1 | pottery, furniture |
| Large Insula | Full basic education, barber, oil, and 2x2 expansion | 25 | fountain | 1 | 2 | 1 | 1 | pottery, furniture, oil |
| Grand Insula | Second food type and more entertainment | 35 | fountain | 1 | 2 | 1 | 2 | pottery, furniture, oil |
| Small Villa | Elite conversion: two gods and wine | 35 | fountain | 2 | 2 | 1 | 2 | pottery, furniture, oil, wine |
| Medium Villa | Higher entertainment and hospital-depth health | 40 | fountain | 2 | 2 | 2 | 2 | pottery, furniture, oil, wine |
| Large Villa | Academy-depth education and 3x3 expansion | 45 | fountain | 2 | 3 | 2 | 2 | pottery, furniture, oil, wine |
| Grand Villa | Three food types and three gods | 50 | fountain | 3 | 3 | 2 | 3 | pottery, furniture, oil, wine |
| Small Palace | Second wine type | 55 | fountain | 3 | 3 | 2 | 3 | pottery, furniture, oil, 2 wine |
| Medium Palace | Four-god religious breadth | 60 | fountain | 4 | 3 | 2 | 3 | pottery, furniture, oil, 2 wine |
| Large Palace | High entertainment pressure and 4x4 expansion | 70 | fountain | 4 | 3 | 2 | 3 | pottery, furniture, oil, 2 wine |
| Luxury Palace | Maximum service stack: "everything" in the readable table | 80 | fountain | 4 | 3 | 2 | 3 | pottery, furniture, oil, 2 wine |

## Progression Shape

The vanilla curve has five distinct movements.

1. Tents to shacks are survival housing. Population density rises from 5 to 11
   people per tile, prosperity rises by 5 per step, tax stays at 1x, and the
   city only needs water, food, and one religious threshold. This creates the
   feeling of a poor frontier settlement becoming barely stable.

2. Hovels and casas are the first Romanizing layer. Fountain water, basic
   entertainment, education, baths, and pottery appear. Tax rises to 2x and
   prosperity reaches 45, but footprints remain 1x1. The intended feel is still
   ordinary working housing, not elite comfort.

3. Insulae are the dense urban peak for working residents. Entertainment jumps
   to 25-35, education broadens, health and barber services appear, oil and
   furniture join pottery, and the chain expands to 2x2. Grand insulae hold 84
   people on 4 tiles, the highest default residential density.

4. Villas are a class conversion, not just a prettier apartment. The move from
   grand insula to small villa cuts density from 21 to 10 people per tile while
   prosperity jumps from 80 to 150 and tax from 4x to 9x. That is the core
   Caesar design tradeoff: serviced elite land stops being labor housing at all
   in vanilla Caesar III and starts being tax/prestige housing. If Vespasian
   later lets some patricians work, that is a deliberate new rule rather than a
   correction to vanilla.

5. Palaces exaggerate elite land consumption into a metropolis signal. The
   highest levels demand nearly complete services, multiple luxuries, very high
   desirability, and 3x3 or 4x4 footprints. Wealth growth is intentionally
   nonlinear: luxury palace prosperity is 21.9 times grand-insula prosperity,
   while capacity is only 2.4 times greater.

## Design Conclusions

Preserve monotonic service complexity. Exact service thresholds can move for
scenario balance, but the player should still feel the sequence: shelter,
water, food, worship, clean water, entertainment, education, baths, crafted
goods, health, luxury goods, then elite completeness.

Preserve the density-to-prestige tradeoff. Plebeian housing should become dense
and economically useful through urban services. In vanilla Caesar III,
patrician housing should be spacious, service-hungry, high-tax, prestigious, and
labor-exempt. If elite housing becomes too dense, too cheap to service, or too
able to replace plebeian labor, the city loses the villa/palace fantasy and the
labor-management challenge.

Preserve the class threshold. The jump from grand insula to small villa should
remain a deliberate social and planning transition. It is the moment where the
city stops asking only "can I house workers?" and starts asking "can I support
elite districts without starving labor and logistics?"

Preserve visual Romanization. The chain starts with canvas and rough timber,
then gains plaster, tile roofs, apartment facades, courtyards, gardens,
colonnades, and finally palace compounds. Even when numeric values are changed,
the player should visually read a fairly linear climb from rural poverty to
Roman metropolis.

Preserve perceived wealth-and-size continuity. The raw table has deliberate
economic jumps at villas and palaces, but the experience should still feel like
a steady climb from tribal or rural poor housing toward a proper Roman
metropolis rather than a sudden genre switch.

Use historical research as freedom bounds, not as a command. Roman facility
ratios can tell us that baths, fountains, shops, temples, warehouses, and public
venues are plausible or implausible at a given city size. They should not force
values that break pacing, legibility, or the Caesar-like feeling of improving a
city block step by step.

Use gameplay research as a second check. The default table does not show that
some services are easy binary walker gates, some goods require brittle logistics,
and villas remove workers from the labor pool. Those points are covered in
[Caesar III Housing Balance and Play Analysis](caesar3_housing_balance_play_analysis.md).

## Visual Progression Notes

Descriptions are based on the MMXL picture table and local extracted Julius
house sprites. Local Vespasian aliases expose canonical image ids such as
`Small_Casa_1x1`, while the Julius building XML refers to `Aesthetics\House_*`
image groups.

| Level | Local graphics reference | Visual read |
| --- | --- | --- |
| Small Tent | `House_Tent` | Bare canvas shelter on empty ground; temporary camp, no urban investment. |
| Large Tent | `House_Tent` | Same temporary shelter family; numeric upgrade reads as crowding and first stability. |
| Small Shack | `House_Shack/Image_0000` | Rough timber or thatch hut, still closer to a work camp than a town house. |
| Large Shack | `House_Shack/Image_0002` | Larger rough hut; the settlement is permanent enough to repair, but still poor. |
| Small Hovel | `House_Hovel/Image_0000` | Crude enclosed dwelling with low materials and a small yard footprint. |
| Large Hovel | `House_Hovel/Image_0002` | More complete poor house; visibly settled, still visually humble. |
| Small Casa | `House_Casa/Image_0000` | First clearly Romanized small house: plastered walls, tile roof, compact domestic footprint. |
| Large Casa | `House_Casa/Image_0002` | Larger modest town house, a better finished version of ordinary working housing. |
| Small Insula | `House_Insula_1/Image_0000` | Compact apartment facade; the city begins to read as dense and urban. |
| Medium Insula | `House_Insula_1/Image_0002` | Taller, cleaner apartment block; density is now a visible design theme. |
| Large Insula | `House_Insula_2/Image_0000` | Full 2x2 apartment block with stronger facade and urban mass. |
| Grand Insula | `House_Insula_2/Image_0002` | Best common housing: ordered, dense, and serviced, but still not elite domestic space. |
| Small Villa | `House_Villa_1/Image_0000` | Detached elite compound with garden/courtyard cues; the class threshold is visible. |
| Medium Villa | `House_Villa_1/Image_0002` | More polished 2x2 villa, larger roof mass, and clearer private grounds. |
| Large Villa | `House_Villa_2/Image_0000` | 3x3 courtyard residence with colonnaded/garden feeling; true elite land use. |
| Grand Villa | `House_Villa_2/Image_0001` | Richer courtyard estate, still residential rather than monumental. |
| Small Palace | `House_Palace_1/Image_0000` | Monumental 3x3 elite house with portico, garden, and formal compound layout. |
| Medium Palace | `House_Palace_1/Image_0001` | More formal palace compound, stronger elite enclosure and display. |
| Large Palace | `House_Palace_2/Image_0000` | 4x4 monumental complex; elite residence begins to read like civic architecture. |
| Luxury Palace | `House_Palace_2/Image_0001` | Most elaborate palace: colonnades, gardens, water/atrium cues, and full metropolitan wealth. |

## Tuning Degrees of Freedom

Safe knobs:

- Adjust capacity within each band if demographics or tile scale change, while
  keeping plebeian density higher than elite density.
- Move a service threshold by one level when scenario pacing needs it, while
  preserving the overall sequence of civic complexity.
- Change prosperity and tax multipliers to fit economy balance, while keeping
  the small-villa and palace jumps legible.
- Add local or cultural variants before the tent chain, if the scenario starts
  as a tribal or rural settlement, as long as the eventual Romanization climb is
  still readable.
- Increase maintenance, walker, or goods pressure for high housing if elite
  districts are too easy to sustain.

Risky knobs:

- Making villas or palaces labor-efficient, because that erases the plebeian to
  elite tradeoff.
- Making early houses too service-heavy, because the opening should feel poor
  and rural rather than already urban.
- Flattening tax/prosperity too much, because the player needs a strong reward
  for building elite districts.
- Letting visual upgrades run ahead of service needs, because the city then
  looks richer than it behaves.
- Applying archaeological ratios literally when they undermine city-builder
  pacing. A Pompeii bath or tavern ratio is a calibration anchor, not an order
  to reshape every housing tier.

## Cross-Document Use

Use this document to preserve the default progression grammar. Use
[Caesar III Housing Balance and Play Analysis](caesar3_housing_balance_play_analysis.md)
to understand which tiers are strategically efficient or costly in Caesar play.
Use
[Roman City Facility Ratios](roman_city_facility_ratios.md) to decide whether a
scenario of a given population can support the service load implied by the
housing mix. Use
[Roman Building and Infrastructure Maintenance Needs](roman_building_maintenance_needs.md)
to convert late-game housing demands into water, fuel, cleaning, repair, and
staffing pressure. Use
[Roman City Size and Social Ratios](roman_city_size_and_social_ratios.md) to
avoid treating "patrician" as literal legal patrician population; in gameplay it
is better understood as elite household fabric. Use
[Vespasian Housing Progression Design Notes](vespasian_housing_progression_design_notes.md)
for proposed mechanics that intentionally depart from vanilla, such as
patrician jobs, service capacity, or demand-gated housing.

For example, a 10,000-person Pompeii-scale city can plausibly support several
bath and entertainment services, but that does not mean every neighborhood
should become palace housing. Grand insulae can represent dense, successful
common districts; villas and palaces should remain a smaller, heavily serviced,
high-tax layer whose share is constrained by labor balance, desirability, and
goods logistics.
