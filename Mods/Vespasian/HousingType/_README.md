# HousingType XML

HousingType definitions hold residential data shared by one or more BuildingType definitions.
BuildingType owns identity, footprint, cost, desirability radius, graphics, transitions, and whole-building capacity.
HousingType owns resident class, evolution thresholds, service/goods requirements, prosperity, and tax multiplier.

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
<housing_type type="house_small_tent">
```

Required children:

- `<residents class="plebeian|patrician" />`
- `<evolution devolve_desirability="N" evolve_desirability="N" />`
- `<requirements entertainment="N" water="none|well|fountain" religion="N" education="N" barber="N" bathhouse="N" health="N" food_types="N" pottery="N" oil="N" furniture="N" wine="N" />`
- `<prosperity value="N" />`
- `<tax multiplier="N" />`

BuildingType references a HousingType with:

```xml
<housing path="house_small_tent" capacity="7" evolve_to="house_large_tent" merge_to="house_small_tent_2x2" />
```

`capacity` is required on the BuildingType `<housing>` node and is the capacity of the complete building footprint. Transition attributes are optional, but any non-empty `evolve_to`, `devolve_to`, `merge_to`, or `split_to` value must resolve to an existing BuildingType text id during BuildingType load.

Vespasian, Augustus, and Julius currently define native HousingType data for the full legacy residential chain from `house_small_tent` through `house_luxury_palace`.
The matching BuildingType XML files own footprint, whole-building capacity, and graphics, including explicit `_2x2` merged variants for the 1x1 plebeian levels.
Julius uses vanilla residential tuning values and Julius-owned `Aesthetics\House_*` graphics; Augustus and Vespasian use the Augustus residential tuning values for now. Treat the vanilla curve as a design baseline, not as a hard requirement when scenario pacing or city-builder feel needs a deliberate adjustment.
