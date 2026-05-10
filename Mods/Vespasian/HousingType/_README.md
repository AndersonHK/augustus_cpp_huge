# HousingType XML

HousingType definitions hold residential data shared by one or more BuildingType definitions. BuildingType owns identity, footprint, cost, desirability radius, graphics, and transitions; HousingType owns resident class and house-model requirements.

Root:

```xml
<housing_type type="house_small_tent">
```

Required children:

- `<residents class="plebeian|patrician" />`
- `<evolution devolve_desirability="N" evolve_desirability="N" />`
- `<requirements entertainment="N" water="none|well|fountain" religion="N" education="N" barber="N" bathhouse="N" health="N" food_types="N" pottery="N" oil="N" furniture="N" wine="N" />`
- `<prosperity value="N" />`
- `<capacity value="N" />`
- `<tax multiplier="N" />`

BuildingType references a HousingType with:

```xml
<housing path="house_small_tent" evolve_to="house_large_tent" merge_to="house_small_tent_2x2" />
```

Transition attributes are optional, but any non-empty `evolve_to`, `devolve_to`, `merge_to`, or `split_to` value must resolve to an existing BuildingType text id during BuildingType load.

Vespasian, Augustus, and Julius currently define native HousingType data for the full legacy residential chain from `house_small_tent` through `house_luxury_palace`. The matching BuildingType XML files own footprint and graphics, including explicit `_2x2` merged variants for the 1x1 plebeian levels. Julius uses vanilla model values and Julius-owned `Aesthetics\House_*` graphics; Augustus and Vespasian use the Augustus house model values for now.
