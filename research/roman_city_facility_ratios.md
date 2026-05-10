# Roman City Facility Ratios for Population, Labor, and Area Tuning

Snapshot: 2026-05-10

## Purpose

This document collects historical anchors for Roman urban service density from the
Republican and Imperial periods, then turns them into practical ratio bands for
city simulation. The goal is not to make a one-to-one archaeological model. It
is to give `BuildingType`, `HousingType`, service walker, labor, and area tuning
credible starting points.

Use the tables as calibration bands:

- "Hard count" means a count reported directly by an ancient text, excavation
  catalogue, or modern archaeological database.
- "Ratio" means hard count divided by an explicit population assumption.
- "Gameplay band" means a conservative tuning range derived from several
  samples, with uncertainty made visible.

## Related Documents

- Start with [Roman City Size and Social Ratios](roman_city_size_and_social_ratios.md)
  when choosing the resident population and elite/common mix for a scenario.
- Use [Caesar III / Julius Housing Progression Defaults](caesar3_julius_housing_progression_defaults.md)
  to keep service-ratio tuning aligned with the vanilla housing service gates
  and the intended shelter-to-metropolis progression.
- Use [Caesar III Housing Balance and Play Analysis](caesar3_housing_balance_play_analysis.md)
  before changing service gates; it separates easy binary service coverage from
  hard goods logistics in actual Caesar play.
- Use [Vespasian Housing Progression Design Notes](vespasian_housing_progression_design_notes.md)
  for proposed capacity, city-size expectation, and market-revenue mechanics.
- Use [Roman Building and Infrastructure Maintenance Needs](roman_building_maintenance_needs.md)
  after selecting counts; it converts each facility family into recurring labor,
  fuel, water, cleaning, and failure pressure.
- Cross-check physical coverage with the walking-range and tile-scale notes:
  [Preindustrial Walking Service Ranges](../docs/preindustrial_walking_service_ranges.md)
  plus [Tile Scale and Walker Timescale](../docs/tile_scale_and_walker_timescale.md).
- For health implications, especially baths and water quality, see
  [Roman and Early Medieval Mortality Research](../docs/roman_early_medieval_mortality_research.md).

## Read This First

Roman cities were not zoned like modern cities. A house could contain shops,
workshops, rented rooms, storage, and elite reception space in one footprint.
Many facilities were private businesses that served public demand. In game
terms, that means the same historical "employment" can map either to civic labor
or to private employment depending on the mechanic.

Population is also unstable. Pompeii is often treated around 10,000 people, but
published ranges run lower and higher. Rome can be modeled as about 450,000
people if following Glenn Storey's density argument, or 800,000-1,000,000 if
following the traditional "great Rome" model. The same building counts therefore
produce different ratios.

## Population Baselines Used Here

| Sample | Date range | Population assumption used | Why it matters |
| --- | ---: | ---: | --- |
| Pompeii | Mostly 1st c. BCE to 79 CE | 8,000 to 10,000, with 10,000 as the practical midpoint | Best excavated mid-sized Roman city. Good for food, water, baths, workshops, and mixed-use blocks. |
| Rome | 1st to 4th c. CE | 450,000 low model; 800,000 to 1,000,000 high model | Only sample with a near-citywide late antique service/building catalogue. |
| Ostia | 1st to 3rd c. CE | 22,000 to 60,000 in published estimates; use 36,000-50,000 for most game planning | Port/warehouse city; useful for storage, dock services, migration, and commercial weighting. |
| Timgad | Founded 100 CE, flourishing 2nd to 3rd c. | Unknown; model as 10,000 to 15,000 if a number is needed | Planned provincial colony with unusually visible baths and public conveniences. |
| Volubilis | 1st to 3rd c. CE | Often modeled around the low tens of thousands; avoid strict ratio unless a scenario sets a population | Strong production-city sample: baths, aqueduct, shops, bakeries, and many olive presses. |
| Herculaneum | 1st c. BCE to 79 CE | 4,000 to 5,000 | Small Campanian city comparator; useful for checking that Pompeii-scale ratios do not force every small city to become Pompeii. |

The Pompeii density anchor comes from recent scaling work that notes a consensus
near 10,000 and gives an 8,000 estimate from about 130 people per hectare across
about 60 hectares. Britannica gives a broader 10,000-20,000 range. Storey's
low-Rome estimate uses Pompeii and Ostia densities to argue for roughly 450,000
inhabitants in imperial Rome, while other historians still accept about one
million. Ostia estimates are especially wide: Calza's 36,000, Meiggs'
50,000-60,000, Packer's 27,000, and Storey's 22,000 all appear in modern
discussion. Use Ostia primarily as a role model for port infrastructure rather
than as a clean people-per-building denominator.

## Evidence Confidence and Use

Use each sample for what it is good at:

| Evidence kind | Best use | Main caution |
| --- | --- | --- |
| Pompeii building counts | Dense mid-sized service ratios, mixed-use shops, baths, bakeries, bars, fountains | It is unusually well preserved and may overrepresent visible street commerce relative to less excavated cities. |
| Rome Regionary totals | Capital-scale water, baths, housing fabric, warehouses, public safety, registered facilities | Textual categories are late antique and not always equivalent to game buildings. |
| Ostia | Port storage, dock-linked services, migrant/commercial society | Population estimates range from about 22,000 to 60,000, so avoid single-point ratios when possible. |
| Timgad | Planned provincial amenity density and bath visibility | The 14-bath count is strong, but population is an estimate. |
| Volubilis | Production-city weighting and non-Italian provincial urbanism | Oil presses are export/industry capacity, not resident service buildings. |
| Herculaneum | Small-city Campanian comparator for size and preservation | The excavated area is partial; avoid deriving citywide facility ratios from it alone. |

For tuning, count-based neighborhood services such as fountains, food counters,
ordinary baths, bakeries, and local shrines can scale with population and walking
coverage. Capacity-based facilities such as amphitheaters, great baths, major
warehouses, and forums should scale with city status, hinterland, trade role, and
event demand.

## Count Anchors

| Place | Service or building type | Hard count | Population assumption | People per facility | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| Pompeii | Public street fountains/lacus | 42 public fountains, 35 simple lacus | 8,000-10,000 | 190-238 per fountain | Strong neighborhood-water anchor. |
| Pompeii | Public baths | 4 public bath buildings in Flohr database | 8,000-10,000 | 2,000-2,500 per bath | Public baths average 2,142 sqm in Flohr's database. |
| Pompeii | Stabian Baths | 1 complex, about 3,300-3,500 sqm | 8,000-10,000 | Citywide destination | Largest operating public bath after the 62 CE earthquake. |
| Pompeii | Bakeries/pistrina | about 35 known bakeries | 8,000-10,000 | 229-286 per bakery | Very high commercial bread density; many had mills and ovens. |
| Pompeii | Food and drink counters/bars | 158 counters; 128 with cooking evidence | 8,000-10,000 | 51-63 per counter; 63-78 per cooking counter | Best anchor for street-food density in dense non-elite housing. |
| Pompeii | Marble-clad bars surveyed | 49 bars | 8,000-10,000 | 163-204 per surveyed decorated bar | Decorated subset, not total bars. |
| Pompeii | Fulleries/fullonicae | about 10-11 in older lists, most confirmed by fieldwork | 8,000-10,000 | 730-1,000 per fullery | Water-heavy craft; use as an odor/desirability pressure building. |
| Rome | Insulae | 46,602 | 450,000-1,000,000 | 10-21 people per insula if read as units; not stable if read as buildings | The term may mean apartment blocks, apartments, or taxable units in late antique administration. |
| Rome | Domus | 1,790 | 450,000-1,000,000 | 251-559 per domus | Better as elite-house count than population denominator. |
| Rome | Horrea | 290 | 450,000-1,000,000 | 1,552-3,448 per warehouse | Major storage, especially grain; individual regional sums are textually messy. |
| Rome | Balnea | 856 | 450,000-1,000,000 | 526-1,168 per bathhouse | Ordinary baths, separate from named great thermae. |
| Rome | Lacus | 1,352 | 450,000-1,000,000 | 333-740 per water point | Late antique catalogue calls these `lacus`; translated as cisterns/fountains/water basins. |
| Rome | Pistrina | 254 | 450,000-1,000,000 | 1,772-3,937 per mill/bakery | Low ratio compared with Pompeii because Rome also had annona, industrial concentration, and varied definitions. |
| Rome | Public latrines | 144 | 450,000-1,000,000 | 3,125-6,944 per public latrine | Public toilets only; private and shared facilities are not counted. |
| Rome | Lupanaria | 45-46 | 450,000-1,000,000 | 9,783-22,222 per listed brothel | Only useful as a caution against over-literal vice-building ratios. |
| Rome | Aqueduct staff | 700 aquarii in Frontinus | 450,000-1,000,000 | 1 worker per 643-1,429 people | Central infrastructure staff, not fountain attendants. |
| Rome | Vigiles | 7 cohorts of about 1,000 each | 450,000-1,000,000 | 1 firefighter/watchman per 64-143 people | Rome's special capital-scale fire/night-watch force. |
| Timgad | Baths | 14 baths visible | 10,000-15,000 model only | 714-1,071 per bath if modeled | UNESCO count; population assumption is not a hard source. |
| Volubilis | Public baths | 4 substantial Roman baths | scenario-set population | n/a unless population chosen | Provincial city amenity anchor; source also records an eighth-century bath showing technical continuity. |
| Volubilis | Oil-pressing complexes | 58 known complexes | scenario-set population | n/a; industry/export ratio | Strong warning that production counts can dwarf local resident-service needs. |
| Volubilis | Bakeries and mills | 16 baker's shops and about 20 mills/querns by 1980s survey | scenario-set population | n/a unless population chosen | Useful industrial/commercial comparator for Pompeii bakeries. |
| Volubilis | Temples | 6 temples plus Capitolium context | scenario-set population | n/a unless population chosen | Cult density reflects local history as much as population. |
| Herculaneum | City population | 4,000-5,000 inhabitants | n/a | n/a | Size anchor only; do not derive full service ratios from partial excavation. |
| Ostia | Significant warehouses | perhaps 20 significant warehouses | avoid strict ratio | n/a | Port-city storage anchor; Grandi Horrea storage alone estimated 5,660-6,960 metric tons on ground floor. |

## Count, Capacity, and Role Multipliers

Some buildings answer "how many residents need one nearby?" Others answer "what
kind of city is this?" Keep those apart.

| Scaling type | Examples | Recommended model |
| --- | --- | --- |
| Neighborhood coverage | fountains, small baths, food counters, bakeries, barbers, shrines | Scale by residents and walking range. |
| District utility | market/macellum, public bath complex, school cluster, latrine, fire/watch post | Scale by residents, density, and route network. |
| City role | warehouses, dock services, oil presses, fulleries, export workshops | Scale by trade role, local resources, and production chain. |
| Civic status | theaters, amphitheaters, large temples, libraries, basilicas, great thermae | Scale by wealth, elite patronage, government status, and hinterland. |
| Capital exception | Rome's annona, vigiles, aqueduct staff, hundreds of balnea | Use only for Rome-like or scenario-specific capitals. |

Volubilis is the clearest warning. Four substantial baths are ordinary urban
amenities, but 58 oil-pressing complexes indicate a production economy tied to
its hinterland and export trade. In game terms, oil presses should be driven by
olive supply, trade demand, and industrial labor rather than by resident count
alone.

## Micro-Scale Access Anchors

These details are useful when tuning individual `BuildingType` footprints,
walker ranges, and service coverage.

| Topic | Specialist anchor | Tuning implication |
| --- | --- | --- |
| Pompeii household water walking | Notarian models 39 public fountains used for household water. Simelius, using that data, reports dwelling-to-fountain distances from 2 m to 215 m, with average 47 m and median 42 m. | Daily water should be the densest service. The historical walk can be much shorter than the game's ordinary service-walker radius. |
| Pompeii water pipes | Olsson counts 14 water towers with 42 street-fountain connections, 5 public-bath connections, and at least 91 or perhaps 103 private house/workshop connections. | A water tower or reservoir node should cover multiple service endpoints; do not staff every fountain separately. |
| Pompeii fountain flow | Monteleone, Crapper, and Motta estimate surveyed lacus discharge values from 0.03 to 2.9 l/s, with average modeled discharges around 0.08, 0.43, and 1.18 l/s depending on water level. | Fountain output is a throughput variable, not only a binary service. Low-flow fountains can satisfy access but create crowding or time-cost penalties. |
| Macella | Hanson measured 50 macella with footprints from 196 sqm to 7,573 sqm and average about 1,265 sqm; sizes scale sublinearly with city population. | Use one macellum per town/district in most cases. Increase size and shop count with wealth and traffic rather than multiplying buildings linearly. |
| Tabernae and bars | Ellis treats tabernae as ubiquitous Roman urban fabric, especially along busy streets and intersections, with early Imperial specialization from shops toward food/drink bars with masonry counters. | Food counters and shops belong in frontage-heavy plebeian and mixed districts. They can share footprints with houses. |
| Fulleries | Ostia preserves tiny and very large fulleries; large examples use halls with basins, pressing bowls, detergents, and sometimes finishing areas. | Fullery scale should be a building-size choice. Small shop fulleries and large industrial fulleries should not have one shared ratio. |
| Streetside benches | Hartnett counts about 100 streetside benches at Pompeii, built in front of shops, bars, and houses. | Some public-life amenities are urban furniture, not full service buildings. They can become decoration/desirability overlays. |

## Suggested Building Granularity

The same historical category often hides several game-relevant sizes.

| Historical family | Small / local | Standard | Large / special |
| --- | --- | --- | --- |
| Water | well, cistern, 1-tile fountain marker | lacus connected to a tower/reservoir | water tower, castellum, aqueduct node |
| Baths | small balneum, 2x2 or 3x2 | public bath complex, 3x3 to 4x4 | great thermae, monument-scale |
| Retail food | taberna counter embedded in frontage | popina/caupona with rooms | macellum or market hall |
| Bread | small bake shop | mill-bakery with oven and mills | annona/industrial bakery tied to grain storage |
| Textiles | shop fullery | standard fullonica with basins/stalls | Ostia/Rome-scale large fullery |
| Storage | local granary/storehouse | horreum with guarded rooms | port/annona horrea with ramps, raised floors, offices |
| Sanitation | private/shared toilet or downpipe | public latrine | sewer/drain node serving a district |
| Religion | aedicula/shrine | temple | capitolium, sanctuary, pilgrimage cult |
| Entertainment | odeon/small theater service | theater/amphitheater | circus, imperial amphitheater, great festival complex |

This granularity keeps a 10,000-person town from needing miniature versions of
every capital building while still letting common services appear at street
scale.

## Gameplay Ratio Bands

These bands translate the evidence into practical starting values. They assume a
walking city without modern commuting and should be read alongside
[Preindustrial Walking Service Ranges](../docs/preindustrial_walking_service_ranges.md)
and [Tile Scale and Walker Timescale](../docs/tile_scale_and_walker_timescale.md).

| Facility class | Good default people per facility | Tight urban range | Sparse/elite range | Labor default | Area default using 15 m tiles | Notes |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| Public fountain/well/lacus | 300-500 | 200-350 | 500-800 | Usually none locally; central water crew | 1 tile or less | Pompeii supports high density; Rome supports a broader citywide baseline. |
| Small bath/balneum | 800-1,500 | 500-1,000 | 1,500-2,500 | 4-10 | 2x2 or compact 3x2 | Fits Rome's 856 balnea better than Pompeii's larger public-bath count. |
| Public bath complex | 2,000-3,000 | 1,500-2,500 | 3,000-5,000 | 12-30 | 3x3 to 4x4 | Pompeii's four public baths are the core model. |
| Great thermae | 50,000-100,000+ | n/a | n/a | 100-500+ | Monument footprint | Capital/regional destination, not a neighborhood service. |
| Bakery/pistrinum | 300-800 | 225-400 | 800-2,000 | 4-10 plus animals if represented | 1x1 to 2x2 | Pompeii is dense; Rome's 254 pistrina are too low for ordinary neighborhood bread if used alone. |
| Food counter/tavern/popina | 75-150 | 50-100 | 150-300 | 1-4 | 1x1 or attached frontage | Pompeii's 158 counters make this a strong non-elite street-life signal. |
| Fullery/fullonica | 800-1,500 | 700-1,000 | 1,500-3,000 | 4-20 | 1x2 to 3x3 | Water, odor, and low desirability should matter more than pure count. |
| Public latrine | 1,000-3,000 | 750-1,500 | 3,000-7,000 | 1-4 | 1x1 to 2x1 | Rome's public-latrine ratio is sparse because private/shared sanitation is invisible. |
| Local shrine/aedicula | 500-1,500 | 300-800 | 1,500-3,000 | 0-1 | 1 tile | The Augustan vici and aediculae model neighborhood ritual density. |
| Ordinary temple | 2,500-10,000 | 1,500-5,000 | 10,000+ | 2-8 | 2x2 to 4x4 | More status/cult driven than population driven. |
| Large temple/cult complex | 15,000-50,000+ | n/a | n/a | 8-30 | Monument footprint | City prestige and pilgrimage matter. |
| Teacher/ludus | 800-1,500 | 500-1,000 | 1,500-3,000 | 1-3 | 1x1 | Archaeological counts are weak; tune to child-walker coverage. |
| Library | 20,000-100,000 | 10,000-30,000 | 50,000+ | 4-20 | 2x2 to monument | Elite/civic marker more than a basic service. |
| Barber | 400-800 | 250-500 | 800-1,500 | 1-2 | 1x1 | No good citywide counts; keep as common local service. |
| Clinic/doctor | 2,000-5,000 | 1,000-2,500 | 5,000+ | 2-8 | 1x1 to 2x2 | Roman medicine was mixed private, household, military, and cult practice. |
| Market/macellum | 5,000-20,000 | 3,000-10,000 | 20,000+ | 8-30 | 3x3+ | One macellum can serve a whole town district; shops spread along streets. |
| Warehouse/horreum | 1,500-4,000 | 1,000-2,500 | 4,000+ | 6-30 | 2x2 to 5x5 | Port cities need more storage per resident than inland towns. |
| Prefect/fire/watch post | 1,000-3,000 abstracted | 500-1,500 | 3,000-5,000 | 4-12 | 1x1 to 2x2 | Literal Rome vigiles are much denser; game buildings usually abstract patrol radius. |
| Engineer/maintenance post | 1,000-3,000 abstracted | 500-1,500 | 3,000-5,000 | 4-12 | 1x1 to 2x2 | Tie to road/building density, not just population. |
| Theater | 5,000-30,000 | 5,000-15,000 | 20,000+ | 8-30 during events | Monument | Pompeii had a large theater and odeon for a small city. |
| Amphitheater | 10,000-50,000+ | n/a | n/a | Event staff, not constant | Monument | Pompeii's amphitheater capacity exceeded the city population and served the hinterland. |
| Circus/hippodrome | 50,000+ | n/a | n/a | Event staff, stables, factions | Huge monument | Capital/provincial spectacle building. |

## Facility Area Guidance

At 15 m/tile, one tile is about 225 sqm.

Pompeii gives a rare public-bath area anchor: four public baths average 2,142
sqm each, for about 8,568 sqm total. If Pompeii's inhabited area is modeled as
60 hectares, public baths occupy roughly 1.4 percent of city area and about 0.86
sqm per resident at 10,000 people.

Use these area bands:

| Facility | Historical anchor | Suggested area share or footprint |
| --- | --- | --- |
| Neighborhood baths | Below Pompeii public-bath average; more like ordinary balnea | 450-1,350 sqm, or 2x2 to 3x2 tiles. |
| Full public baths | Pompeii average 2,142 sqm; Stabian 3,300-3,500 sqm | 1,800-3,600 sqm, or 3x3 to 4x4 tiles. |
| Food counters | Flohr taberna units average about 42 sqm; many bars are small street-front units | 1 tile, often attached to housing/commercial frontage. |
| Bakeries | Pompeian bakeries often combine mill room, oven, work space, and sometimes sales | 100-400 sqm, or 1x1 to 2x2 tiles. |
| Fulleries | From tiny shop installations to large halls with basins | 100-900 sqm, 1x2 to 3x3 tiles. |
| Fountains | Small basin plus street space | Less than one full tile; gameplay can represent with a 1x1 service marker. |
| Latrines | Communal bench and drain/water channel | 1x1 for local; 2x1 or 2x2 for public latrine. |
| Macellum | 50-structure sample ranges 196-7,573 sqm, average about 1,265 sqm | 2x2 for small, 3x3 for ordinary, 4x4+ for major city market. |
| Warehouses | Ostia Grandi Horrea rectangle 78 x 91 m, with central rows | 2x2 for small granary; 4x4+ for major horreum. |

## Worked Example: Pompeii-Like City, 10,000 People, 79 CE

This model assumes dense street life, mixed-use frontages, commercial bread,
good fountain coverage, and public bathing. It should feel busy even before
large monuments.

| Facility | Count | Area model | Employees model | Evidence pressure |
| --- | ---: | ---: | ---: | --- |
| Public fountains | 39 household-use fountains, 42 catalogue fountains | 35-42 tiles if represented directly | 0 local; 6-12 city water workers | Direct Pompeii count plus network-analysis access work. |
| Water towers/reservoirs | 14 towers | 14 infrastructure nodes | central water crew | Olsson water-connection model: 138-150 known public/private connections. |
| Public baths | 4 | 8,500-10,000 sqm total | 48-80 total | Direct Pompeii count and area. |
| Bakeries | 25-35 | 3,000-10,000 sqm total | 125-280, mostly private | 35 known Pompeian bakeries, but some may not be active simultaneously. |
| Food counters/bars | 100-160 | 4,000-9,000 sqm total | 150-320, mostly private | 158 counters, 128 with cooking evidence. |
| Fulleries | 6-12 | 1,000-5,000 sqm total | 40-120, mostly private | Pompeian lists around 10-11; strong water/odor footprint. |
| Public latrines | 2-5 | 450-1,500 sqm total | 2-10 | Sparse hard evidence outside Rome; tune for health coverage. |
| Macellum/market | 1 | 1,500-4,000 sqm | 10-30 | One central food market plus street shops; macella scale with population and wealth. |
| Theater district | 1 large theater plus 1 odeon | Monument | Event labor | Pompeii has both. |
| Amphitheater | 1 | Monument | Event labor | Serves hinterland as well as city. |
| Warehouses/granaries | 2-6 | 1,000-8,000 sqm | 20-80 | Depends on port, grain imports, and campaign economy. |

## Worked Example: Port City, 36,000-50,000 People, Ostia-Like

Ostia should not simply scale Pompeii by five. Storage, dock labor, shipping
services, fire risk, and transient population should grow faster than ordinary
elite amenities.

| Facility | Count band | Employment emphasis |
| --- | ---: | --- |
| Warehouses/horrea | 15-30 significant facilities | High. Storage, guards, porters, clerks, pest/moisture maintenance. |
| Bakeries | 60-120 | High private employment, with grain access lowering scarcity. |
| Food counters/taverns | 300-600 | High, especially near docks and streets. |
| Baths | 25-60 ordinary baths or 8-15 larger complexes | Moderate to high; port workers need public baths. |
| Fountains/water points | 80-160 | Central water crew plus local repair. |
| Fulleries/dirty industry | 20-50 | Place near water/drains and away from elite housing. |
| Temples/cult buildings | 10-30 | More diversity from trade communities. |
| Fire/watch posts | 20-50 abstract posts or one large barracks plus posts | Heavier than inland city due to warehouses, ovens, and docks. |

## Worked Example: Production City, Volubilis-Like

A Volubilis-like city should feel wealthy and productive, but not because every
facility is a resident service. Its excavated pattern supports substantial
baths, aqueduct water, a forum/basilica/capitol complex, temples, bakeries, and
an unusually strong oil economy.

| Facility | Count or pressure | Simulation interpretation |
| --- | ---: | --- |
| Public baths | 4 substantial Roman baths | Use normal provincial bath coverage; do not inflate because of oil wealth alone. |
| Oil presses | 58 complexes | Industry chain. Scale by olive supply, export access, and merchant demand. |
| Bakeries/mills | 16 baker's shops, about 20 mills/querns | Commercial food production comparable to Pompeii but less extreme. |
| Temples | 6 temples plus Capitolium context | Cult mix and local identity; not a pure population ratio. |
| Aqueduct/fountains | aqueduct from a large spring, secondary channels to houses, baths, fountains | Water network unlocks baths, elite houses, and production cleanliness. |

For gameplay, this argues for a "productive provincial city" archetype: high
private industrial employment, moderate-to-high bath/water service, and a larger
elite merchant/decurion class than a subsistence town of the same population.

## Worked Example: Rome, Late Antique Catalogue Counts

For Rome, keep two models available:

| Facility | Hard count | 450,000 people | 1,000,000 people | Simulation interpretation |
| --- | ---: | ---: | ---: | --- |
| Balnea | 856 | 526 people/bath | 1,168 people/bath | Good default for dense neighborhood bath access. |
| Lacus | 1,352 | 333 people/water point | 740 people/water point | Strong large-city fountain network. |
| Pistrina | 254 | 1,772 people/pistrinum | 3,937 people/pistrinum | Do not use alone for bakery scarcity; Rome's bread system is special. |
| Horrea | 290 | 1,552 people/horreum | 3,448 people/horreum | Grain and bulk storage are capital-scale systems. |
| Public latrines | 144 | 3,125 people/latrine | 6,944 people/latrine | Sparse public sanitation count; do not make all hygiene depend on this. |
| Domus | 1,790 | 251 people/domus | 559 people/domus | Elite-house count; see social-ratio document. |

## Period Modifiers

| Period/city type | Service character | Ratio adjustment |
| --- | --- | --- |
| Early Republican town | Wells/cisterns, forum, temples, few or no monumental baths | Fewer baths, fewer food counters, more household production. |
| Late Republican Campanian town | Dense street commerce, commercial bakeries, public baths, theaters | Pompeii bands are most useful here. |
| Early Imperial provincial city | Baths, macellum, aqueduct/fountains, theater/amphitheater if status supports it | Use middle bands; public amenity count rises with civic wealth. |
| Planned colony / veteran foundation | Orthogonal grid, forum, market, baths, theater, later expansion outside walls | Timgad supports high amenity visibility but population assumptions should stay explicit. |
| Imperial port city | Warehouses, bars, baths, bakeries, dock services, cult diversity | Storage and food service scale faster than resident population. |
| Production city | Workshops or processing facilities tied to hinterland resources | Use role multipliers; Volubilis oil presses should not be read as resident-service buildings. |
| Imperial Rome | Exceptional water, baths, fire/watch, grain distribution, spectacle buildings | Do not apply Rome's capital infrastructure literally to smaller cities. |
| Late antique capital | Large inherited infrastructure plus maintenance burden | Counts can stay high while population falls; maintenance pressure should bite. |
| Post-Roman continuity town | Reduced urban scale with selected inherited technologies | Baths/water craft can persist locally, but large network maintenance becomes the limiting factor. |

## Implementation Takeaways

- Treat baths as a family, not one ratio. Small balnea can sit near 500-1,500
  people each; public bath complexes fit 2,000-3,000; great thermae are
  citywide monuments.
- Street food should be common in dense plebeian districts. Pompeii supports far
  more food counters than a modern player might expect.
- Bakeries are a good place to make private employment visible. Pompeii suggests
  dense commercial bread, but Rome's registered pistrina are too low to stand in
  for every bread outlet.
- Water coverage should be generous in cities with aqueducts. Pompeii and Rome
  both support short walks to public water.
- Latrines should not be the only sanitation model. The hard Roman public-latrine
  count is sparse compared with water points and baths.
- Warehouses should key off trade and annona pressure, not just resident count.
- Entertainment buildings are lumpy. A small city can have one amphitheater that
  serves its whole hinterland; do not scale arenas linearly.
- Public employees and total employees are different. Bath attendants, bakers,
  fullers, food sellers, and warehouse laborers may be private unless the game
  wants a state-labor abstraction.
- Separate resident services from production. Volubilis-style oil presses,
  Ostian horrea, and Rome's annona infrastructure are role buildings first and
  population-ratio buildings second.
- Record population assumptions beside every scenario count. Changing Pompeii
  from 8,000 to 10,000 or Ostia from 36,000 to 60,000 materially changes the
  apparent ratio.

## Sources

- Regionary catalogues / Chronography of 354, translated table of totals:
  https://www.ccel.org/ccel/pearse/morefathers/files/chronography_of_354_14_regions_of_rome.htm
- LacusCurtius Regionaries Latin text:
  https://penelope.uchicago.edu/Thayer/L/Gazetteer/Places/Europe/Italy/Lazio/Roma/Rome/_Texts/Regionaries/text%2A.html
- Glenn R. Storey, "The population of ancient Rome", abstract and metadata:
  https://iro.uiowa.edu/esploro/outputs/journalArticle/The-population-of-ancient-Rome/9984271556002771
- Cambridge Core page for Storey article:
  https://www.cambridge.org/core/journals/antiquity/article/population-of-ancient-rome/BACD7DF32B0B77609CD6713B8AF88882
- Pompeii population range, Britannica:
  https://www.britannica.com/place/Pompeii
- Pompeii scaling/population density and built-environment discussion:
  https://link.springer.com/article/10.1007/s10816-023-09604-x
- Pompeii public fountains/lacus study:
  https://link.springer.com/article/10.1007/s12685-021-00281-9
- Pompeii public fountain pipeline discharge study:
  https://www.sciencedirect.com/science/article/pii/S2352409X22004321
- Pompeii water quantities and connection counts:
  https://link.springer.com/article/10.1007/s12685-023-00333-2
- Matthew Notarian, "A Spatial Network Analysis of Water Distribution from Public
  Fountains in Pompeii":
  https://ajaonline.org/article/4585/
- Samuli Simelius, "Networks of Inequality: Access to Water in Roman Pompeii":
  https://journal.caa-international.org/articles/10.5334/jcaa.116
- Pompeii water distribution summary:
  https://ancientengrtech.wisc.edu/pompeii/water-distribution/
- Miko Flohr, Database of Pompeian Houses:
  https://www.mikoflohr.org/pompeii/
- Steven J. R. Ellis, The Roman Retail Revolution, Oxford Academic page:
  https://academic.oup.com/book/4620
- AJA review of Ellis, The Roman Retail Revolution:
  https://ajaonline.org/book-review/3848/
- Pompeii bakeries, Monteix abstract:
  https://academic.oup.com/book/9085/chapter/155642961
- Pompeii bars/counters, Steven Ellis count quoted in food-history source:
  https://dokumen.pub/a-cultural-history-of-food-in-antiquity-volume-1-9781350044562-9780857850232-9781474269902.html
- Pompeii and Herculaneum bars / marble-counter survey:
  https://www.cambridge.org/core/product/5F6EF940DFF9D253CFE197AECBDE1510/core-reader
- University of Edinburgh publication metadata for the same bar survey:
  https://www.research.ed.ac.uk/en/publications/marble-use-and-reuse-at-pompeii-and-herculaneum-the-evidence-from
- J. W. Hanson, macella scaling study:
  https://www.cambridge.org/core/journals/journal-of-roman-archaeology/article/new-approaches-to-the-architectural-design-amenities-and-function-of-macella-typologies-scale-and-the-macellum-magnum/75C1747256BD9D46707E99A6B56E8F6C
- Jeremy Hartnett, Pompeii streetside benches:
  https://ajaonline.org/article/216/
- Pompeii Stabian Baths excavation note:
  https://pompeiisites.org/en/excavations-plan-en/stabian-baths-and-republican-baths/
- Stabian Baths area and last-stage layout:
  https://pompeiisites.org/e-journal-degli-scavi-di-pompei/public-saunas-in-the-stabian-baths-a-privilege-of-men/
- Pompeii amphitheater capacity:
  https://www.pompeii.org.uk/m.php/museum-amphitheatre-pompeii-en-69-m.htm
- Timgad UNESCO entry:
  https://whc.unesco.org/en/list/194/
- Herculaneum population, Britannica:
  https://www.britannica.com/place/Herculaneum
- Volubilis map and building summaries:
  https://sitedevolubilis.org/map-of-volubilis/
- Volubilis overview, Britannica:
  https://www.britannica.com/place/Volubilis
- Ostia population estimates:
  https://www.ostia-antica.org/dict/topics/population/population-composition.htm
- Ostia warehouse catalogue:
  https://www.ostia-antica.org/dict/topics/horrea/horrea.htm
- Ostia warehouses / Grandi Horrea summary:
  https://www.worldhistory.org/Ostia/
- Ostia storage and port infrastructure:
  https://ostia-antica.org/introduction/rome-storage-ports.htm
- Frontinus, aqueduct staff, Wikisource page:
  https://en.wikisource.org/wiki/Page%3AFrontinus_-_The_stratagems%2C_and%2C_the_aqueducts_of_Rome_%28Bennet_et_al_1925%29.djvu/505
- Vigiles overview:
  https://www.worldhistory.org/Vigiles/
- Pompeii Republican Baths water-quality study metadata:
  https://openscience.ub.uni-mainz.de/items/def61ce2-3174-4fe5-90cc-6224ef941da5
