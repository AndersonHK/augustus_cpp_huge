# HousingProfile XML

HousingProfile definitions hold residential data shared by one or more BuildingType definitions.
BuildingType owns identity, footprint, cost, desirability radius, graphics, transitions, and whole-building capacity.
HousingProfile owns resident class, evolution thresholds, service/goods requirements, prosperity, and tax multiplier.

For resident-class tuning, city-size bands, and the reason `patrician` should
mean elite/high-status gameplay household rather than literal legal patrician,
see [Roman City Size and Social Ratios](../../../research/roman_city_size_and_social_ratios.md).
For service ratios that drive water, bathhouse, food, education, health, and
luxury requirements, see
[Roman City Facility Ratios](../../../research/roman_city_facility_ratios.md).
For the vanilla Caesar III / Julius housing progression curve, including
default capacity, prosperity, tax, service gates, and visual descriptions, see
[Caesar III / Julius Housing Progression Defaults](../../../research/caesar3_julius_housing_progression_defaults.md).
For Caesar-specific gameplay balance, including patrician non-labor, the labor
cliff, tier efficiency, and goods/service difficulty, see
[Caesar III Housing Balance and Play Analysis](../../../research/caesar3_housing_balance_play_analysis.md).
For proposed Vespasian departures from vanilla, see
[Vespasian Housing Progression Design Notes](../../../research/vespasian_housing_progression_design_notes.md).

Root:

```xml
<housing_profile type="house_small_tent" level="0">
```

Required children:

- `<residents class="plebeian|patrician" />`
- `<evolution devolve_desirability="N" evolve_desirability="N" />`
- `<requirements entertainment="N" water="none|well|fountain|latrine_or_fountain" religion="N" education="N" barber="N" bathhouse="N" health="N" food_types="N" pottery="RATE" oil="RATE" furniture="RATE" wine="RATE" wine_sources="N" />`
- `<prosperity value="N" />`
- `<tax multiplier="N" />`

BuildingType references a HousingProfile with:

```xml
<housing path="house_small_tent" capacity="5" goods_consumption_events_per_month="1"
    mars_offering_amount="1" evolve_to="house_large_tent" merge_to="house_small_tent_2x2" />
```

`capacity`, `goods_consumption_events_per_month`, and `mars_offering_amount` are required on the BuildingType `<housing>` node. They describe whole-building balance directly; no value is inferred from foundation dimensions. Transition attributes are optional, but any non-empty `evolve_to`, `devolve_to`, `merge_to`, or `split_to` value must resolve to an existing BuildingType text id during BuildingType load.

Vespasian, Augustus, and Julius currently define native HousingProfile data for the full legacy residential chain from `house_small_tent` through `house_luxury_palace`.
The matching BuildingType XML files own footprint, whole-building capacity, and graphics, including explicit `_2x2` merged variants for the 1x1 plebeian levels.
Julius uses Julius-owned `Aesthetics\House_*` graphics. Julius/Augustus manufactured-goods rates preserve vanilla annual totals at full occupancy; partial houses consume proportionally less. Vespasian uses its own annual per-person rates, described below. Service unlocks and tax multipliers remain unchanged.

## Annual goods rates

`pottery`, `oil`, `furniture`, and `wine` mean inventory units per resident per year. Zero means the good is not demanded. A rate accepts an integer (`1`), a decimal with up to six decimal places (`0.63`), or an exact fraction (`12/19`). Rates must be nonnegative and no larger than 10000; fraction denominators must be positive. One trade load is 100 inventory units.

An ordinary Small Insula declares `pottery="12/19"`: at 19 residents it consumes 12 pottery/year. Its merged 76-resident variant declares `24/76`, preserving 24/year. Avoid rounded decimals when exact vanilla totals matter. Consumption charges actual population at the building's existing monthly event cadence and carries fractional units across events and saves. Existing monument reductions still apply.

`wine_sources` controls source-access requirements independently of quantity. It accepts 0, 1, or 2 and must be positive exactly when wine is demanded. If omitted, it defaults to 1 for a positive wine rate, otherwise 0. Two-source access is required for Small Palace and above.

Vespasian plebeians consume 0.3 per resident/year of each demanded good. Patricians consume 0.6 pottery, oil, and furniture; wine is 0.3 through Grand Villa and 0.6 starting at Small Palace. Market delivery targets cover both current consumption and the next evolution tier.

A root may declare `variant_of="house_small_insula"` to share a canonical profile's compatibility level without creating another level entry. The canonical profile must exist, be enabled, have the same level, and not itself be a variant. Variants are complete definitions with their own requirements; this attribute does not automatically inherit fields from another profile. BuildingType selects the variant through its housing `path`. Merged rate variants currently exist for Large Casa, Small Insula, and Medium Insula.

Normal mod overlays still inherit same-identity root/child definitions from lower mods. A replacement `<requirements>` node must include all required service and goods attributes, even when only a goods rate changes.

See [annual consumption and Augustus/Vespasian cost tables](../../../research/housing_annual_consumption_2026_09_09.md) for the balancing assumptions and reproducible calculator.
