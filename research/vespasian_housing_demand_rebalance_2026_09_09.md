# Vespasian housing demand rebalance: community evidence and cost limits

Historical proposal: demand unlock moves and fractional taxes remain unimplemented. The later authorized consumption change and current budget tables are documented in [Annual housing consumption](housing_annual_consumption_2026_09_09.md).

Planning snapshot: 2026-09-09. Source checkout: `bd0affbcf`.
Status: discussion proposal; no gameplay changes or playtest results.

Follow-up: [Whole service-cost tables and fractional taxes](vespasian_housing_service_cost_tables_2026_09_09.md)
adds all-tier operating budgets and evaluates the proposed smoother tax curve.
Its corrected benchmark holds residential area and walker catchment constant,
letting population and workforce rise with density. Recurring wages,
maintenance, and resource demand are the balance focus; one-time expenses are
outside the main comparison. The per-1,000 figures below describe commodity
intensity only, not a complete ranking of housing efficiency.

The proposed changes make manufactured goods necessary much earlier. They weaken
Small Casa as a cheap stopping point, but leave the main reason to stop at Small
Insula intact: the next upgrade still adds furniture and health requirements for
only 5.3% more capacity and no increase in the tax multiplier. Early oil is the
largest economic and scenario-compatibility risk. The second god at Large Insula
is a much lighter gate, especially if the district already receives two priests.

## What the community evidence actually supports

This is a qualitative sample of public player discussions and strategy writing,
not a survey or a measurement of how most players build. Several closely related
Reddit recommendations may come from the same experienced players; repeated
comments are not independent votes. These are opinions about Caesar III,
Julius, and Augustus, not direct evidence about this Vespasian runtime.

| Finding | Evidence | Interpretation |
| --- | --- | --- |
| Small Casa is a useful low-logistics endpoint | The August 2024 Leptis Magna discussion recommends it when pottery is scarce; an older strategy guide likewise recommends stopping before goods when resources or workers are scarce. | Its strength is avoiding manufactured goods while retaining 17 residents per tile and a 2x tax multiplier. |
| Small Insula is a common efficient default | The Leptis Magna discussion describes repeated Small Insula builds; September 2024 advice recommends Small Casa or Small Insula depending on pottery availability. | Strong evidence for a recognizable optimization pattern, not universal agreement that it should be nerfed. |
| Medium Insula is often criticized | August 2024 Lugdunum advice points to four extra residents per merged house, unchanged tax multiplier, and furniture logistics; the player reports using furniture at the end to reach the prosperity target. | This is as much a weak-next-upgrade problem as an overpowered-Small-Insula problem. |
| Higher insulae have defenders, especially in Augustus | The January 2025 temple discussion explicitly debates space/industry efficiency against Grand Insula taxation, sentiment, and health benefits. | Optimize taxes, labor, land, rating targets, and reliability separately; they need not select the same tier. |
| Expensive imports can reverse the ranking | Players discussing Reconquered Londinium in May 2024 report avoiding oil-supported higher insulae until the finish because of that scenario's economics. | Oil availability, price, and trade quota are central balance variables, not minor exceptions. |

Sources: [Leptis Magna, August 2024](https://www.reddit.com/r/impressionsgames/comments/1f58kml/),
[combat-mission housing advice, September 2024](https://www.reddit.com/r/impressionsgames/comments/1fpr1hx/),
[Lugdunum, August 2024](https://www.reddit.com/r/impressionsgames/comments/1eogx9x/),
[temple and housing debate, January 2025](https://www.reddit.com/r/impressionsgames/comments/1hz1840/),
[Reconquered Londinium, May 2024](https://www.reddit.com/r/impressionsgames/comments/1chhdkr/),
[Caesar Jan strategy tips](https://www.geocities.ws/caesar_jan/strategy_tips.html).
Some Reddit direct opens failed; the relevant discussion text was available in
indexed search results. No quantitative community-consensus claim is inferred.

Augustus's own manual documents stronger housing-inequality sentiment penalties
for poorer housing. That is a reason to treat classic efficiency advice as a
starting hypothesis rather than the complete objective function.
[Official Augustus 4.0 manual](https://raw.githubusercontent.com/Keriew/augustus/master/res/manual/augustus_manual_en_4_0.pdf).

## Current local baseline and interpretation of the proposal

Vespasian depends on Augustus, which depends on Julius. Most housing requirements
and all capacities below are inherited. The existing May housing research is
useful background, but its generic per-house goods discussion should not override
the current explicit consumption cadence.

Two assumptions make the proposal concrete without deciding its open questions:

- Moving a requirement means moving both the entry and sustain requirement,
  preserving it at all higher tiers. Amounts remain `1` for pottery and oil.
- "School/Library" means school **or** library (`education=1`), not both.
  Large Insula keeps school **and** library (`education=2`).

The water change needs a design decision before implementation. Small Hovel,
Large Hovel, and Small Casa currently accept `latrine_or_fountain`, whereas
Large Casa requires `fountain`. For this analysis, the proposed hovels need only
well-level water and Small Casa needs an actual fountain. Keeping the latrine
alternative at Small Casa would make that transition cheaper than the strict
fountain interpretation. Delaying the evolution gate also does not remove any
independent health or sanitation reasons to build water infrastructure early.

All rows below are **additional requirements on reaching the tier**; earlier
requirements continue. Desirability and footprint rules also still apply.

| Tier | Current new requirements | Proposed new requirements |
| --- | --- | --- |
| Small Hovel | Latrine-or-fountain water | Pottery; retain well-level water |
| Large Hovel | Entertainment 10 | Entertainment 10 |
| Small Casa | School or library | Fountain and oil |
| Large Casa | Strict fountain, bathhouse, pottery | School or library and bathhouse |
| Small Insula | Entertainment 25 | Entertainment 25 |
| Medium Insula | Health 1 and furniture | Health 1 and furniture |
| Large Insula | School and library, barber, oil; mandatory 2x2 | School and library, barber, two gods; mandatory 2x2 |
| Grand Insula | Second food and entertainment 35 | Second food and entertainment 35 |
| Small Villa | Two gods and wine; patrician conversion | Wine; patrician conversion |

The small-villa transition still removes workforce. Moving religion earlier
removes one separable way to hold a district below villas; wine restrictions,
desirability, and any configured evolution controls become more important.

## Why the existing stopping points are strong

Use a full 2x2 merged house or fixed 2x2 insula for fair comparisons. `Tax index`
is capacity times the raw multiplier, not denarii. Prosperity is the profile's
contribution to the housing average, not an immediate increase in city rating.

| Tier | Residents | Tax multiplier | Tax index | Prosperity |
| --- | ---: | ---: | ---: | ---: |
| Large Shack | 44 | 1 | 44 | 20 |
| Small Hovel | 52 | 2 | 104 | 25 |
| Large Hovel | 60 | 2 | 120 | 30 |
| Small Casa | 68 | 2 | 136 | 35 |
| Large Casa | 76 | 2 | 152 | 45 |
| Small Insula | 76 | 3 | 228 | 50 |
| Medium Insula | 80 | 3 | 240 | 58 |
| Large Insula | 84 | 3 | 252 | 65 |
| Grand Insula | 84 | 4 | 336 | 80 |

- Large Casa to Small Insula: same population and goods, **50% greater raw tax
  multiplier**, and prosperity 45 to 50, for more entertainment and desirability.
- Small Insula to Medium Insula: **5.26% more population**, same multiplier,
  prosperity 50 to 58, plus furniture and health. It can be worthwhile for ratings
  and health, but its density reward is small.
- Medium to Large Insula: **5% more population**, same multiplier, prosperity
  58 to 65. Mandatory merging helps if the medium houses were unmerged; it gives
  no additional merging dividend if they were already 2x2.
- Large to Grand Insula: same population, **33.33% greater raw tax multiplier**,
  prosperity 65 to 80, for food variety and entertainment.

Difficulty adjusts and truncates each house's tax multiplier before aggregation.
The raw-index ratios are exact at Hard's 100% money adjustment, but need not be
the realized tax ratios at other difficulties. Occupancy and tax coverage matter.

For workforce at the Vespasian fixed-pool setting of 38%, four additional
residents yield about **1.52 additional potential workers** per upgraded merged
house. Ten Small-to-Medium upgrades add only 15.2 potential workers. A furniture
workshop and timber yard together require 20 employees; a doctor adds five.
That example is about startup indivisibility, not steady-state production
requirements: spare production, spare staff, existing health coverage, and shared
services can make the marginal burden much smaller. Actual workforce can also
differ with settings and blessings.

## Goods consumption changes the answer

Current local XML explicitly assigns one goods-consumption event per month to
a 1x1 house and two to a merged 2x2 house or fixed insula. At each event the
runtime consumes the profile requirement amount, independently of occupancy,
subject to available inventory and active reduction effects. One load is 100
inventory units. Thus a sustained `1` requirement uses:

- One 1x1 house: 12 units/year.
- Four 1x1 houses: 48 units/year.
- One merged 2x2 house: 24 units/year.

**Merging halves consumption for the same full population in this checkout.**
Do not import the fourfold merging advantage asserted in some classic advice.
With partial occupancy the per-resident burden is worse still. These are steady
state sustain amounts; initial household inventories, market buffers, and goods
distributed in anticipation of upgrades require additional working stock.

For population `P`, full capacity `C`, monthly events `e`, requirement `r`, and
occupied house count `H`, annual goods demand is `Q = 12 * H * e * r` units.
For a continuous full-occupancy estimate, substitute `H = P / C`; actual designs
need integer buildings and enough supply margin for delivery interruptions.

| Tier | Annual units per required good per 1,000 residents, merged | Proposed manufactured goods |
| --- | ---: | --- |
| Small Hovel | 461.54 | Pottery |
| Large Hovel | 400.00 | Pottery |
| Small Casa | 352.94 | Pottery and oil |
| Small Insula | 315.79 | Pottery and oil |
| Medium Insula | 300.00 | Pottery, oil, furniture |
| Large / Grand Insula | 285.71 | Pottery, oil, furniture |

Moving pottery to Small Hovel makes its per-resident sustain burden **46.15%
higher** than at the old Large Casa entry point, assuming merged full housing.
Oil at Small Casa costs **23.53% more per resident** than oil at Large Insula.
These ratios precede the much bigger citywide effect: many more residents now
need each commodity. A 10-load annual oil supply sustains roughly 2,833 residents
in merged Small Casas versus 3,500 in Large Insulae, before buffers, other oil
consumers, or supply losses. Both numbers halve for equivalent unmerged casas.

## Cash break-even: what would be too onerous?

There is no universal profitable-tier threshold because exports, rating targets,
land scarcity, domestic production, and stability have value. There is a useful
**tax-only marginal test** that identifies upgrades whose new upkeep cannot be
paid from their new taxes.

Ignoring aggregate integer rounding, annual taxes are
`T = 6 * t * P * adjusted_multiplier * coverage`, where `t` is the tax fraction.
For unchanged coverage and an upgrade from A to B:

`Delta T = 6 * t * (P_B * adjusted_m_B - P_A * adjusted_m_A)`.

Annual imported-goods cost is `Q * price_per_load / 100`. The remaining budget
must cover added service wages, levies, food for additional residents, logistics,
and construction amortization, less genuinely avoided costs. At wage setting
`w`, annual wages are approximately `w * employees / 10`.

The worked example uses **Hard difficulty, its base 7% tax, full tax coverage,
full merged houses, default resource prices, and no bonuses**. The prices are
180 denarii/load for pottery and oil, 200 for furniture. Actual scenario prices,
trade modifiers, quotas, and taxes can differ. Domestic goods require their
actual marginal production cost; use foregone export profit only where an
additional sale is feasible, and avoid double-counting production costs.

One merged house therefore imports pottery or oil for **43.20 denarii/year per
good**, or furniture for **48.00**.

| Upgrade | Additional annual taxes per 2x2 | Newly required goods in proposal | First fiscal reading |
| --- | ---: | --- | --- |
| Large Shack to Small Hovel | 25.20 | Pottery: 43.20/year | Import cost exceeds new taxes by 18.00, before crediting delayed water infrastructure. The tax multiplier doubling is nevertheless a substantial reward. |
| Large Hovel to Small Casa | 6.72 | Oil: 43.20/year | Oil exceeds new taxes by 36.48; fountain is another cost. Education is deferred relative to today's Small Casa, but the baseline Large Hovel had no education cost to remove. |
| Small Casa to Large Casa | 6.72 | None | School/library and baths share a small direct tax budget; density, ratings, and progression must carry part of the value. |
| Large Casa to Small Insula | 31.92 | None | Remains an attractive next step if entertainment can be shared. |
| Small Insula to Medium Insula | 5.04 | Furniture: 48.00/year | Furniture alone exceeds new taxes by 42.96; the criticized marginal tradeoff remains. |
| Medium to Large Insula | 5.04 | None | Earlier oil removes the new commodity cost, making this step easier to justify than today's oil gate, despite the second god. |
| Large to Grand Insula | 35.28 | None | A material tax reward remains; measure the incremental food and entertainment cost. |

As a generous upper bound, if the **entire** Large-Hovel-to-Small-Casa tax gain
went to oil, the break-even oil price would be `6.72 / 0.24 = 28 denarii/load`,
versus the default 180. Furniture at Small-to-Medium Insula has an analogous
ceiling of **21 denarii/load**, versus 200. These are not recommended prices;
they show how little of the upgrade is funded by incremental taxes alone.

An absolute check is even clearer: proposed Small Casa yields **57.12** annual
taxes but needs **86.40** for imported pottery plus oil. Small Insula yields
**95.76**, leaving only **9.36** after those two goods, before all other costs.
This does not prove the city is unviable: domestic industry, exports, and elite
taxes can finance it. It does mean an import-dependent worker district cannot be
assumed to pay its own way. On Normal at the base 8% tax, the adjusted Small Casa
multiplier is 3, giving **97.92**; even that leaves only **11.52** after the goods.

At fixed population rather than fixed residential area, Large Hovel to Small Casa
has **no tax-multiplier benefit**. Its compensation is fewer houses and less
residential land, better rating potential, and any associated stability benefit.
Always compare both fixed-population and fixed-land cases.

## Assessment of the five changes

| Change | Judgment | What would make it work |
| --- | --- | --- |
| Pottery at Small Hovel | Plausible thematic and progression change, but economically consequential. It removes the old goods-free hovel/casa economy. | Reliable early clay/pottery access, a deliberate lower-tier fallback, and a consumption/startup-cost model that tolerates small or unmerged settlements. |
| Fountain at Small Casa | Reasonable pacing change by itself. It gives wells a longer role. | Decide whether latrines remain an alternative and measure independent health effects; water savings cannot be assumed on maps where players need the infrastructure anyway. |
| School or library at Large Casa | A modest, relatively predictable relocation. | Keep the school/library choice; check that coupling education with baths produces a worthwhile step. |
| Oil at Small Casa | Highest-risk part of the package. Adds a second commodity chain to basic working housing for an eight-resident gain per merged house. | Either make early oil broadly accessible and much less onerous to sustain, give this tier a larger reward, or move oil less far. |
| Two gods at Large Insula | Reasonable civic progression with a comparatively small marginal cost. It partly replaces the removed oil gate. | Count local distinct-god coverage, not just temples elsewhere in the city; account for existing coverage and retain player control of villa conversion. |

A small temple costs 50, occupies four tiles, and needs two employees in the
current definitions. Its labor can be shared over many houses. Oil requires
recurring units for every occupied qualifying house. Those gates are therefore
not economically interchangeable even though both are a single requirement.

The package likely creates **Large Shack** as the goods-free fallback and
**Large Hovel** as the pottery-only fallback. It then encourages pushing through
Small Casa and Large Casa to Small Insula once oil is available. That can be a
valid intended progression, but it changes where players stop rather than
eliminating the incentive to stop.

Shortages also become more severe. Ignoring other constraints and the details of
transition timing, loss of pottery could eventually reduce a Large Insula's
84-person footprint to Large Shack capacity 44: **47.6% lost capacity**, versus
the current Small Casa fallback of 68 (**19.0%**). Loss of oil could fall to
Large Hovel capacity 60 (**28.6%**), versus current Medium Insula capacity 80
(**4.8%**). These are endpoint capacity comparisons, not predictions of immediate
evictions. Falling population can worsen local staffing and delay recovery.

## Recommended comparison before committing to a curve

Keep the exact proposal as candidate A. Compare it with candidate B: early
pottery, delayed fountain and education, two gods at Large Insula, but **oil at
Medium Insula**. This preserves a pottery-only casa/small-insula economy and
removes oil from the opening. It deliberately concentrates goods at Medium
Insula, so it needs a separate reward or a redistribution of furniture/health;
it is a gentler opening, not a complete solution to tier efficiency.

If the objective is specifically to reduce Small Insula dominance, prioritize
the **Medium Insula reward** as a separate experiment. Options include shifting
some later capacity gain into Medium Insula, moving part of a later tax reward
there, or reducing its new sustain burden. Capacity is already close to the
plebeian ceiling (80 versus 84), so capacity alone has little room without
retuning the band. A whole tax step from 3 to 4 is a large change; later tiers
must be considered with it. Do not assume any particular number is chosen yet.

Early oil can still be the intended design if the goal is a trade-dependent
Roman town. In that case, judge it as a coordinated housing, supply, and scenario
change rather than a small requirement-table adjustment.

## Later validation plan

No game runs were performed for this documentation slice. Before implementation,
choose the intended goods-free endpoint, the fountain/latrine interpretation,
and whether the goal is opening difficulty, varied district types, or greater
incentive to develop beyond Small Insula.

Compare current rules, candidate A, and candidate B using the same starting city,
settings, and RNG seed where practical. Include:

- Local clay and olives; local clay but imported oil; imported finished goods;
  restricted quotas or delayed routes; and no obtainable oil/pottery.
- Small startup districts and mature cities, with full and partial occupancy;
  both merged blocks and irregular/unmerged housing.
- Hard and Normal base taxes, the intended Vespasian difficulty, local workforce
  and global labor configurations, and no monument bonuses before bonus cases.
- A controlled goods interruption and recovery, measuring level churn, lost
  capacity, staffing, migration, and time to restore stable supply.
- Existing saves whose viable housing now lacks newly required goods, and early
  campaign scenarios whose construction menus/trade routes assume the old gates.

For each tier measure annual delivered and consumed units, import spending,
taxes, employed workers, net available workforce, total supporting land,
occupancy, upgrade delay, devolution frequency, health, sentiment, and prosperity
ceiling/rating progress. Record resource scarcity separately from delivery
failures. Measure output delivered per workshop under the actual calendar and
staffing; do not equate a production-rate XML field with observed cart throughput.

Use these decision rules rather than pretending there is one universal score:

1. An inaccessible good must not silently make an intended scenario progression
   impossible. Deliberately constrained scenarios need explicit alternative goals
   or supply access.
2. If a tier costs more in cash, workers, and total supported area than its
   predecessor, its rating or stability reward must have a demonstrated purpose.
3. Upgrading basic labor housing should have a planned way to finance and staff
   its support chain; count discrete startup buildings as well as shared mature
   infrastructure.
4. If Small Insula still wins every relevant non-rating comparison, the experiment
   has increased entry cost without resolving the weak-next-tier incentive.
5. Save loading must preserve loadability. A valid city losing housing due to a
   newly adopted rule is a balance migration issue, not serialized corruption;
   decide versioning/grace/scenario policy explicitly before shipping.

Any later runtime implementation must also meet the repository's representative
recent `.svv` and legacy `.sav` startup/soak gate, with no load/soak warnings or
errors. That is future validation, not a check claimed by this note.

## Local evidence and related planning

- [Julius housing profiles](../Mods/Julius/HousingProfile/house_small_casa.xml),
  [Augustus Small Casa override](../Mods/Augustus/HousingProfile/house_small_casa.xml),
  [Vespasian dependency/settings](../Mods/Vespasian/mod.xml).
- [Merged Small Casa definition](../Mods/Julius/BuildingType/house_small_casa_2x2.xml),
  [merged Small Insula](../Mods/Julius/BuildingType/house_small_insula_2x2.xml),
  [merged Medium Insula](../Mods/Julius/BuildingType/house_medium_insula_2x2.xml).
- [Consumption schedule](../src/building/HousingDef.cpp),
  [requirements and consumption](../src/building/house_evolution.cpp),
  [resource units](../src/game/resource.cpp),
  [market supply selection](../src/building/market.cpp).
- [Tax and wage calculations](../src/city/finance.cpp),
  [difficulty adjustments](../src/game/difficulty.cpp),
  [prosperity averaging](../src/city/ratings.cpp),
  [sentiment](../src/city/sentiment.cpp).
- [Pottery price](../Mods/Julius/Resources/pottery.xml),
  [oil price](../Mods/Julius/Resources/oil.xml),
  [furniture price](../Mods/Julius/Resources/furniture.xml),
  [trade-price modifiers](../src/empire/trade_prices.cpp).
- [Existing housing design notes](vespasian_housing_progression_design_notes.md),
  [classic housing balance analysis](caesar3_housing_balance_play_analysis.md),
  [local gameplay divergences](../docs/gameplay_divergences_from_augustus.md).
