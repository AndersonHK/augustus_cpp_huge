# Roman City Size and Plebeian-to-Patrician Ratio Research

Snapshot: 2026-05-10

## Purpose

This document gives city-size bands and social-ratio guidance for tuning Roman
urban populations. It is meant to support `HousingType` resident classes,
employment share, service demand, tax balance, and city growth pacing.

The most important conclusion is simple: for gameplay, "patrician" should mean
elite or high-status household, not literal legal patrician. Literal patricians
were a tiny hereditary-political order and are not a useful population class for
ordinary urban simulation after the early Republic.

## Related Documents

- Use [Roman City Facility Ratios](roman_city_facility_ratios.md) to convert a
  chosen population and social mix into service-building counts.
- Use [Caesar III / Julius Housing Progression Defaults](caesar3_julius_housing_progression_defaults.md)
  when deciding how much the historical elite/common mix should bend around the
  vanilla housing progression and its plebeian-to-patrician gameplay threshold.
- Use [Vespasian Housing Progression Design Notes](vespasian_housing_progression_design_notes.md)
  if classed labor, patrician jobs, or elite-neighborhood desirability penalties
  are being considered.
- Use [Roman Building and Infrastructure Maintenance Needs](roman_building_maintenance_needs.md)
  to budget the labor, fuel, water, and repair burden created by those buildings.
- Use [Demographics Runtime](../docs/demographics_runtime.md) and
  [Roman and Early Medieval Mortality Research](../docs/roman_early_medieval_mortality_research.md)
  when turning social assumptions into birth, mortality, health, or housing
  tuning.
- Use [Tile Scale and Walker Timescale](../docs/tile_scale_and_walker_timescale.md)
  when translating density into map area.

## Terminology Caveat

Roman legal and social vocabulary does not map cleanly to Caesar-style housing
classes.

- In the Republic, patricians were an old hereditary order. Plebeians included
  poor people, rich people, office-holders, and eventually powerful noble
  families.
- By the late Republic and Empire, elite status was better modeled through
  senatorial rank, equestrian rank, municipal decurions, wealth, office-holding,
  freedman success, landholding, and household dependents.
- Legal patrician status remained prestigious, but the population share was
  minuscule. It should not be the same thing as "all rich houses."
- A game "patrician" resident class is better understood as elite households:
  domus owners, municipal notables, large merchants, rentiers, office-holders,
  and their household dependents.

Recommended vocabulary:

| Game word | Historical reading for simulation |
| --- | --- |
| Plebeian | Ordinary urban residents: workers, tenants, small shopkeepers, craftsmen, poor citizens, many freed people, and dependent households. |
| Patrician | Elite households and elite residential fabric, including dependents and slaves if the game does not model them separately. |
| Slave/freed labor | Usually not a separate residential class in Caesar-like games, but should be remembered inside employment and elite-house service demand. |

## Status-Layer Math

Do not collapse every Roman status category into two housing classes. A useful
game model can still expose two resident classes, but the historical layers were
stacked:

| Layer | Historical scale | Gameplay use |
| --- | --- | --- |
| Literal patricians | Tiny hereditary-political order by the late Republic and Empire; after 27 BCE, new patricians depended on imperial creation | Use for offices, flavor, or scenario politics, not ordinary population share. |
| Senatorial and equestrian elites | Very small share of total population, especially outside Rome | High prestige and tax, rare households, not a whole class of urban residents. |
| Municipal decurions/curiales | Often around a hundred councillors in a town, with families and dependents around them | Best historical basis for a provincial "patrician" gameplay class. |
| Wealthy freedmen and merchants | Can be socially powerful, especially in ports and commercial towns | Fold into elite housing, trade demand, or merchant class if the design supports it. |
| Ordinary citizens, tenants, artisans, shopkeepers, workers | The majority of residents | Plebeian/common class. |
| Slaves and dependent household labor | Not always visible in free-citizen counts; Scheidel argues Roman Italy did not exceed about 1.5 million slaves | Model as explicit population only if the design can handle legal dependency; otherwise include them in household capacity and employment demand. |

For a two-class Caesar-like system, "patrician" should therefore mean local
elite household fabric: decurions, large merchants, rentiers, high-status
freedmen, imperial officials, and their dependents. Literal legal patricians are
below the normal simulation layer.

## City Size Bands

| Tier | Population | Roman-world interpretation | Service implications |
| --- | ---: | --- | --- |
| Village / roadside settlement | 250-1,500 | Rural nucleated settlement, vicus, road station, estate cluster | Wells/cisterns, shrine, small market day, little formal service density. |
| Small town | 1,500-5,000 | Minor municipium, small colony, local market center | Forum, temples, some shops, maybe small baths if wealthy. |
| Pompeii-scale town | 5,000-15,000 | Dense Campanian or Italian town; Pompeii midpoint around 8,000-10,000 | Multiple baths, dense shops, bakeries, bars, public fountains, theater/amphitheater if status supports it. |
| Medium provincial city | 15,000-50,000 | Regional administrative, cult, or commercial city | Several bath districts, market buildings, warehouses, broader elite district, theater common. |
| Major port/provincial capital | 50,000-150,000 | Ostia-like port, major colonia, capital city | Warehouses, dock labor, large baths, multiple cult communities, high fire/watch burden. |
| Metropolis below Rome | 150,000-300,000 | Alexandria/Antioch-class outside the western provincial norm | Multiple citywide systems, food supply infrastructure, large entertainment, complex administration. |
| Rome | 450,000-1,000,000 | Exceptional imperial capital | Do not scale ordinary city rules linearly; Rome is a special water, grain, fire, spectacle, and housing machine. |

## Population Density Anchors

| Anchor | Density or population | Use |
| --- | ---: | --- |
| Pompeii scaling estimate | about 130 people per hectare, about 8,000 people over about 60 ha | Conservative mid-sized city density. |
| Pompeii broader range | 10,000-20,000 in Britannica; recent scholarship often works around 10,000 | Use 10,000 for gameplay unless a scenario needs crowding. |
| Ostia in Storey's Rome model | about 31,700 people per square km, or 317 per hectare | Dense multi-story port-city comparator. |
| Ostia published population estimates | 22,000, 27,000, 36,000, and 50,000-60,000 all appear in the modern discussion | Treat Ostia as a range; use port-role ratios more than precise resident ratios. |
| Herculaneum | 4,000-5,000 inhabitants | Small Campanian comparator below Pompeii scale. |
| Rome low estimate | about 450,000 | Good for avoiding absurd density while keeping Rome huge. |
| Rome high/traditional estimate | 800,000-1,000,000 | Useful when modeling annona, baths, fountains, and spectacle at maximum capital scale. |

At a 15 m tile scale, one hectare is about 44.4 tiles. A density of 130 people
per hectare is about 2.9 people per tile of whole-city area. A density of 317
people per hectare is about 7.1 people per tile of whole-city area. These are
whole-city averages, not house-tile occupancy values.

## Population-Estimate Method Cautions

Roman population estimates are usually compound models, not census facts.
Hanson and Ortman's systematic method uses settlement area and comparative
density relationships to estimate Greek and Roman populations, while Hanson's
Roman-world catalogue combines textual status, archaeology, estimated area, and
population-density bands. That is better than using one universal people-per-
hectare number, but it still means every scenario should record its assumptions.

Useful cautions for tuning:

| Issue | Practical handling |
| --- | --- |
| Walls do not equal inhabited area | A city may include gardens, public buildings, cemeteries, slopes, undeveloped land, or extramural neighborhoods. |
| Density rises with building form | Ostia-like multistory insulae support higher density than Pompeii-like mixed house/shop fabric. |
| Capital estimates are model-sensitive | Rome can be tuned at 450,000 or 1,000,000; both produce different service ratios from the same Regionary counts. |
| Port and production roles distort resident ratios | Ostia and Volubilis need more storage/industry than a pure resident count predicts. |
| Excavated samples can overlead | Pompeii and Herculaneum are unusually visible; less-preserved cities may look emptier than they were. |

The safest workflow is to choose a city role first, then population, then area,
then density. Do not choose a population and force every city to use the same
building mix.

## Rome Housing Count Anchor

The late antique Regionary/Chronography totals list:

| Category | Count |
| --- | ---: |
| Insulae | 46,602 |
| Domus | 1,790 |
| Horrea | 290 |
| Balnea | 856 |
| Lacus | 1,352 |
| Pistrina | 254 |
| Public latrines | 144 |

The domus-to-insula ratio is the best citywide elite/common built-fabric anchor:

```text
insulae / domus = 46,602 / 1,790 = 26.0
domus share of insulae + domus = 1,790 / 48,392 = 3.7 percent
```

Do not read that as "3.7 percent of people were patrician." A domus could hold a
large elite household with freedmen, slaves, clients, lodgers, workshops, or
shops, while "insula" may mean a building, apartment, taxable unit, or residential
unit depending on interpretation. Still, it strongly supports a city where elite
residences are a small minority of residential fabric.

## Pompeii Housing and Mixed-Use Anchor

Miko Flohr's Pompeii database gives a useful non-capital sample:

| Unit/building metric | Count or average |
| --- | ---: |
| House units | 499 |
| Taberna units | 467 |
| Apartments | 133 |
| Total units in database | 1,170 |
| Domestic buildings | 461 |
| Commercial buildings | 34 |
| Public baths | 4 |
| Religious buildings | 3 |
| Taberna room type instances | 801 |
| Kitchens identified | 234 |

Interpretation for gameplay:

- Pompeii was full of mixed-use frontage. Plebeian life should not mean only
  isolated house blocks; shops, bars, workshops, and rented rooms belong in the
  same urban fabric.
- The elite is visible through large houses, peristyles, decoration, atria, and
  property control, but the city economy is carried by many small units.
- Only wealthy houses had complete private amenities. Dense food counters and
  baths make sense because many ordinary residents did not have full domestic
  cooking/bathing infrastructure.

## Household and Frontage Granularity

Flohr's Pompeii data also give useful size contrasts:

| Unit type | Count | Average area | Average rooms | Simulation reading |
| --- | ---: | ---: | ---: | --- |
| House units | 499 | about 394 sqm | about 17 | Broad range from modest houses to elite domus. |
| Taberna units | 467 | about 42 sqm | about 3 | Small frontage units; often low-status, commercial, or mixed residential/commercial. |
| Apartments | 133 | not area-estimated in the same table | n/a | Upper-floor or secondary residences that are easy to undercount. |
| Domestic buildings | 461 | about 448 sqm | n/a | Many buildings contain more than one unit. |
| Public baths | 4 | about 2,142 sqm | 15 units/rooms average in database summary | Large public-service buildings even in a mid-sized town. |

At 15 m/tile, a Pompeian taberna average is smaller than one game tile. That is
a useful reminder: one Caesar-like 1x1 commercial building should represent a
symbolic frontage module, not a literal single shop room. A 2x2 evolved house
can plausibly contain multiple historical units: rooms, shops, rentals, storage,
and internal lanes or courtyards.

## Recommended Plebeian-to-Patrician Bands

These bands are for Caesar-style simulation, not legal Roman classification.

| City type | Elite/patrician household share | Elite/patrician population share if dependents are included | Plebeian/common share | Notes |
| --- | ---: | ---: | ---: | --- |
| Small town | 2-5 percent of households | 5-12 percent | 88-95 percent | A few local notables dominate. |
| Pompeii-scale town | 3-8 percent of households | 8-18 percent | 82-92 percent | Large houses are prominent but still few. |
| Medium provincial city | 3-10 percent of households | 8-20 percent | 80-92 percent | Administrative and commercial elites grow. |
| Port city | 2-8 percent of households | 6-18 percent | 82-94 percent | Wealth can be high, but dock/service population is large. |
| Rome, using domus fabric | about 3.7 percent of residential fabric is domus if domus+insulae are compared | 5-15 percent as elite households plus dependents | 85-95 percent | Legal patricians would be far below this. |
| Purpose-built elite district | 20-60 percent locally | 30-70 percent locally | 30-70 percent locally | Use only for a neighborhood, not whole city. |

Practical default for a balanced city:

```text
ordinary plebeian/common residents: 80-90 percent
elite/patrician households plus dependents: 8-15 percent
explicit slave/freed/service population if modeled separately: 10-30 percent overlapping with both classes
literal legal patricians: below 1 percent and usually below the simulation layer
```

If the engine has only plebeian and patrician residents, use:

| Scenario | Plebeian | Patrician |
| --- | ---: | ---: |
| Conservative historical default | 90 percent | 10 percent |
| Wealthy provincial capital | 85 percent | 15 percent |
| Poor industrial/port district | 95 percent | 5 percent |
| Elite villa/palace district only | 40-70 percent | 30-60 percent |
| Early settlement before luxury growth | 95-98 percent | 2-5 percent |

## Slaves, Freed People, and Dependents

Slavery should affect employment and household demand even if the game does not
represent it as a separate resident class. Scheidel's model for Roman Italy
argues for an upper limit of about 1.5 million slaves and a huge inflow during
the last two centuries BCE. Other estimates vary, but the important gameplay
point is not the exact empire-wide percentage. It is that elite houses, baths,
warehouses, fulleries, bakeries, mines, farms, and domestic service did not draw
only on free wage labor.

Use these assumptions unless a scenario needs more detail:

| Model choice | Recommended interpretation |
| --- | --- |
| No explicit slave class | Fold slaves and household dependents into elite-house capacity, private employment, and service demand. |
| Explicit dependent class | Start around 10-20 percent for an urban Italian/provincial scenario; push toward 20-30 percent only for late Republican/early Imperial Italy or heavily elite/domestic-service districts. |
| Explicit freed class | Put many freed people in ordinary commercial and craft housing, with a path into wealthy merchant or elite housing in port cities. |
| Public labor | Use only for infrastructure crews, public slaves, civic staff, and state-owned facilities; most service labor can remain private. |

This keeps "plebeian" from becoming a purely free-citizen category. In practice,
the common urban fabric included tenants, small proprietors, freed people,
craftspeople, dependents, and laborers with very different legal statuses.

## Period and Region Modifiers

| Context | Population/social adjustment |
| --- | --- |
| Early Republican or Latin town | Smaller elite share, fewer luxury services, status expressed through landholding, office, forum, and temples more than through dense luxury housing. |
| Late Republican Italy | More visible slavery, commercial workshops, elite competition, and mixed domestic/commercial property. |
| Early Imperial Campania | Strong taberna, bakery, bath, fountain, and entertainment visibility; Pompeii is the main model. |
| High Imperial port | More freedmen, guilds, migrants, dock labor, warehouses, transient housing, and religious diversity. |
| African or resource-rich provincial city | Production wealth and local elites can be strong without Rome-scale population; Volubilis is a useful warning. |
| Late antique capital or shrinking city | Elite and church/civic monumental fabric may remain while ordinary population falls; maintenance burden rises per resident. |

## Suggested City Templates

### Early Republican/Latin Town

| Parameter | Tuning |
| --- | ---: |
| Population | 2,000-8,000 |
| Plebeian/common | 92-97 percent |
| Elite/patrician gameplay class | 3-8 percent |
| Dominant infrastructure | Wells, cisterns, forum, temples, simple market, roads |
| Services to suppress | Large bath network, dense entertainment, libraries |

Early Republican status competition should appear through forum/temple/civic
buildings, not through a large population of luxury residents.

### Late Republican / Early Imperial Pompeii-Scale Town

| Parameter | Tuning |
| --- | ---: |
| Population | 8,000-15,000 |
| Plebeian/common | 82-92 percent |
| Elite/patrician gameplay class | 8-18 percent |
| Dominant infrastructure | Public fountains, baths, bakeries, bars, fulleries, forum, theater |
| Distinctive note | Commercial street life is unusually important. |

This is the best template for "busy but human-scale" cities. Many ordinary
people buy services outside the home.

### Medium Provincial City

| Parameter | Tuning |
| --- | ---: |
| Population | 15,000-50,000 |
| Plebeian/common | 80-90 percent |
| Elite/patrician gameplay class | 10-20 percent |
| Dominant infrastructure | Several bath districts, macellum, temples, theater, larger warehouses |
| Distinctive note | City status and euergetism can produce public amenities beyond what population alone predicts. |

### Port / Warehouse City

| Parameter | Tuning |
| --- | ---: |
| Population | 30,000-100,000 |
| Plebeian/common | 85-94 percent |
| Elite/patrician gameplay class | 6-15 percent |
| Dominant infrastructure | Horrea, docks, bars, baths, fire watch, cult diversity, transient housing |
| Distinctive note | Storage and service employment should be high relative to resident population. |

### Production Provincial City

| Parameter | Tuning |
| --- | ---: |
| Population | 10,000-40,000, scenario-dependent |
| Plebeian/common | 82-92 percent |
| Elite/patrician gameplay class | 8-18 percent |
| Dominant infrastructure | Resource-processing workshops, baths, aqueduct/fountains, market, temples, warehouses |
| Distinctive note | Volubilis-like cities can have unusually high industrial counts, such as oil presses, without needing a Rome-sized resident population. |

This template is useful when a city is wealthy because of a countryside product:
oil, wine, grain, textiles, ceramics, mining, or portage. Raise merchant/decurion
wealth and private industrial employment before raising the share of idle elite
residents.

### Rome-Like Capital

| Parameter | Tuning |
| --- | ---: |
| Population | 450,000-1,000,000 |
| Plebeian/common | 85-95 percent |
| Elite/patrician gameplay class | 5-15 percent |
| Dominant infrastructure | Aqueducts, 1,000+ water points, hundreds of bathhouses, annona, vigiles, spectacles |
| Distinctive note | Capital systems are exceptional and should not define the average city. |

## Employment and Class Interaction

Elite houses should not only consume luxury goods. They also create employment:

- domestic service and household slaves
- skilled decoration, construction, and maintenance
- food, oil, wine, textiles, furniture, and luxury demand
- patronage for temples, games, baths, and public works
- rents from shops/apartments attached to elite properties

Plebeian districts should generate:

- high street-food demand
- high public bath demand
- high fountain/water reliance
- dense local shops and tabernae
- fire risk from ovens, lamps, crowding, and multi-story housing
- labor supply for public and private buildings

This supports a useful gameplay split:

| Resident class | Service demand emphasis | Employment contribution |
| --- | --- | --- |
| Plebeian/common | water, food market, baths, barbers, religion, local schools, safety | most workforce, shops, crafts, docks, farms/workshops |
| Elite/patrician | desirability, luxury goods, high culture, gardens, temples, entertainment, tax | high tax, patronage, domestic employment, specialized luxury demand |

## Implementation Takeaways

- Keep legal patricians out of normal population math unless the design adds a
  political-office class.
- Use 90/10 plebeian/patrician as the safest default whole-city ratio.
- Let elite districts exist locally, but do not let them dominate whole-city
  population without an explicit scenario reason.
- Model ordinary residents as service-dependent. Dense bars, baths, bakeries,
  fountains, and workshops are historically plausible.
- Treat port cities and capitals as special cases. They have more service labor
  and infrastructure than their resident population alone would predict.
- Treat production cities as another special case. Wealth can come from
  workshops and hinterland processing without requiring a high elite population
  share.
- Do not make every large house equal to a purely idle aristocratic household.
  Roman elite properties can contain shops, rentals, workshops, storage, slaves,
  freedmen, and clients.
- Keep explicit slavery optional but do not forget it in labor math. A two-class
  resident model should still make elite houses and major industries consume
  private service labor.

## Sources

- Regionary/Chronography totals:
  https://www.ccel.org/ccel/pearse/morefathers/files/chronography_of_354_14_regions_of_rome.htm
- LacusCurtius Regionaries Latin text:
  https://penelope.uchicago.edu/Thayer/L/Gazetteer/Places/Europe/Italy/Lazio/Roma/Rome/_Texts/Regionaries/text%2A.html
- Glenn R. Storey, "The population of ancient Rome":
  https://iro.uiowa.edu/esploro/outputs/journalArticle/The-population-of-ancient-Rome/9984271556002771
- Cambridge Core page for Storey article:
  https://www.cambridge.org/core/journals/antiquity/article/population-of-ancient-rome/BACD7DF32B0B77609CD6713B8AF88882
- Pompeii population range, Britannica:
  https://www.britannica.com/place/Pompeii
- Pompeii scaling/population-density discussion:
  https://link.springer.com/article/10.1007/s10816-023-09604-x
- J. W. Hanson and S. G. Ortman, systematic population-estimation method:
  https://www.cambridge.org/core/journals/journal-of-roman-archaeology/article/systematic-method-for-estimating-the-populations-of-greek-and-roman-settlements/32413192CB46847949394D750342B5D4
- Hanson, An Urban Geography of the Roman World, Archaeopress page:
  https://www.archaeopress.com/Archaeopress/Products/9781784914721
- AJA review of Hanson, An Urban Geography of the Roman World:
  https://ajaonline.org/book-review/3530/
- Hanson, Ortman, Bettencourt, and Mazur, urban form and infrastructure:
  https://www.cambridge.org/core/journals/antiquity/article/urban-form-infrastructure-and-spatial-organisation-in-the-roman-empire/0A1433E14B6469E08B2B027B7C54ED6B
- Miko Flohr, Database of Pompeian Houses:
  https://www.mikoflohr.org/pompeii/
- Timgad UNESCO entry:
  https://whc.unesco.org/en/list/194/
- Pompeii and Herculaneum bars / marble-counter survey:
  https://www.cambridge.org/core/product/5F6EF940DFF9D253CFE197AECBDE1510/core-reader
- University of Edinburgh publication metadata for the same bar survey:
  https://www.research.ed.ac.uk/en/publications/marble-use-and-reuse-at-pompeii-and-herculaneum-the-evidence-from
- Ostia population estimates and social composition:
  https://www.ostia-antica.org/dict/topics/population/population-composition.htm
- Herculaneum population, Britannica:
  https://www.britannica.com/place/Herculaneum
- Volubilis map and building summaries:
  https://sitedevolubilis.org/map-of-volubilis/
- Patrician summary, Britannica:
  https://www.britannica.com/topic/patrician
- Decuriones / municipal council summary:
  https://www.livius.org/articles/concept/decuriones/
- Walter Scheidel, "Human Mobility in Roman Italy, II: The Slave Population":
  https://www.cambridge.org/core/journals/journal-of-roman-studies/article/human-mobility-in-roman-italy-ii-the-slave-population/72D80C5839817A1F4C456A33C9F62E7A
