# Roman Building and Infrastructure Maintenance Needs

Snapshot: 2026-05-10

## Purpose

This document turns Roman urban maintenance evidence into simulation guidance.
It complements [Roman City Facility Ratios](roman_city_facility_ratios.md) by
asking what each facility costs after it is built: labor, fuel, water, cleaning,
inspection, materials, and risk.

The main design lesson is that Roman infrastructure is not passive. Baths,
fountains, aqueducts, bakeries, fulleries, warehouses, roads, drains, and dense
housing all need recurring maintenance. If those costs are invisible, Roman
cities become too easy to run.

## Related Documents

- Use [Roman City Facility Ratios](roman_city_facility_ratios.md) for the
  population, area, and count bands that create the maintenance burden.
- Use [Roman City Size and Social Ratios](roman_city_size_and_social_ratios.md)
  to decide how much maintenance is public payroll, private household labor, or
  private commercial employment.
- Use [Caesar III / Julius Housing Progression Defaults](caesar3_julius_housing_progression_defaults.md)
  to connect high-tier housing gates to the extra maintenance implied by baths,
  fountains, dense insulae, and elite districts.
- Use [Vespasian Housing Progression Design Notes](vespasian_housing_progression_design_notes.md)
  for proposed service capacity, overcrowding, and market-revenue mechanics that
  would make maintenance pressure visible in gameplay.
- Use [Roman and Early Medieval Mortality Research](../docs/roman_early_medieval_mortality_research.md)
  for the health-table consequences of water, bath, sanitation, and urban
  crowding.
- Use [Preindustrial Walking Service Ranges](../docs/preindustrial_walking_service_ranges.md)
  for the patrol and service-range assumptions behind fire/watch and engineer
  coverage.

## Maintenance Categories

| Category | Main recurring needs | Good game levers |
| --- | --- | --- |
| Water supply | inspection, leaks, illegal taps, pipe repair, reservoir cleaning, fountain upkeep | central water workers, water pressure/failure, maintenance budget, fountain downtime |
| Baths | fuel, furnace labor, hypocaust repair, water, drainage, cleaning, attendants, cloakroom security | employees, wood/charcoal/fuel, water access, fire risk, hygiene output |
| Roads and paving | surface wear, cart damage, drainage, flooding, stone replacement | engineer coverage, traffic wear, road-quality modifier |
| Fire and night watch | patrol, inspections, equipment, pumps/buckets/hooks, response time | prefect/vigiles coverage, fire risk reduction, night crime modifier |
| Sanitation | latrine flushing, sewer/cesspit clearance, street waste, bath/fullery outflow | health, disease risk, odor/desirability, water consumption |
| Bakeries | ovens, mills, fuel, animals, flour dust, fire control, heavy labor | food throughput, fire risk, private employment, fuel demand |
| Warehouses | roof repair, ventilation, moisture, pests, guards, porters, clerks | spoilage, storage loss, guard labor, dock congestion |
| Food counters/markets | waste, washing, small ovens/braziers, street crowding | health risk if unmaintained, local commerce, fire risk |
| Fulleries/dirty industry | water basins, drains, detergents/fuller's earth, odor, cloth presses | water demand, desirability penalty, pollution, skilled labor |
| Entertainment | seating repair, crowd control, awnings, arena/theater surface, event setup | low daily labor, high event maintenance, safety/crowd cost |
| Temples/civic/monuments | cleaning, ritual staff, roofs, statues, marble/stone repair, periodic restoration | prestige upkeep, festival cost, restoration events |
| Housing | roof/wall repair, fire prevention, landlords, collapse risk, water access | collapse/fire risk, rent/tax, engineer coverage, density pressure |

## Maintenance Intensity Matrix

Use this as a fast default when a `BuildingType` needs a maintenance profile
before a custom rule exists.

| Building family | Daily labor | Consumables | Network dependency | Failure severity | Notes |
| --- | --- | --- | --- | --- | --- |
| Fountain/well | low local, medium central | low | water/drainage | medium | Individual objects are small; the network is the real cost. |
| Small bath | medium | high fuel, high water | water/drainage/roads | high | Good neighborhood service but expensive to run. |
| Great bath/thermae | extreme | extreme fuel and water | aqueduct, roads, admin | very high | Treat as capital or provincial monument. |
| Bakery | medium | fuel, grain, animals if modeled | storage/roads | high | Productive but a fire node. |
| Food counter/tavern | low | food, water, fuel | roads/local waste | medium | Many small risks rather than one major plant. |
| Fullery/wet industry | medium to high | water, chemicals, cloth | water/drainage | high | Pollution and odor should be visible. |
| Warehouse/horreum | medium | low direct, high protection | roads/ports/security | high | Spoilage and fire can become citywide problems. |
| Road/drain | central crew | stone, tools | route network | medium to high | Failure slows walkers and amplifies flood/disease. |
| Temple/civic | low daily, high episodic | offerings, roof, stone | roads/status | medium | Prestige should decay if never funded. |
| Dense housing | private/landlord | materials, water | roads/water/fire | high | Collapse and fire risk should rise under neglect. |

The "daily labor" column is total work, not necessarily civic payroll.

## Granular Tuning Fields

When a building needs a maintenance model, prefer several small fields over one
generic upkeep number:

| Field | What it controls | Buildings most affected |
| --- | --- | --- |
| `water_source` | none, well/cistern, fountain, reservoir, aqueduct | baths, fountains, latrines, fulleries, elite houses |
| `water_renewal` | how quickly stale or dirty water is replaced | baths, fountains, basins, markets |
| `fuel_burden` | daily wood/charcoal/olive-pomace demand | baths, bakeries, lime/brick/metal industries |
| `waste_load` | food waste, sewage, industrial runoff, ash | markets, taverns, latrines, fulleries, bakeries |
| `fire_load` | ignition and spread pressure | bakeries, baths, warehouses, dense housing, workshops |
| `traffic_load` | carts, porters, queues, crowd wear | markets, warehouses, gates, baths, entertainment |
| `inspection_need` | how often neglect should create failure | aqueducts, reservoirs, bridges, warehouses, insulae |
| `private_labor_share` | how much labor is household/commercial rather than civic payroll | shops, bakeries, fulleries, elite houses, warehouses |

This makes it possible for a building to be expensive in a historically specific
way. A bath is water/fuel/drainage-heavy. A warehouse is security/moisture/pest
heavy. A taberna is waste/fire/noise-heavy.

## Hard Staffing Anchors

### Aqueduct and Water Staff

Frontinus gives the best ancient staffing anchor. In `De aquaeductu`, the water
staff consisted of two gangs:

| Staff group | Count | Notes |
| --- | ---: | --- |
| State/public gang | about 240 | Originated from Agrippa's staff after his death. |
| Caesar/imperial gang | 460 | Organized under Claudius when the water system expanded. |
| Total aqueduct staff | 700 | Divided into overseers, reservoir-keepers, inspectors, pavers, plasterers, and other workers. |

Frontinus also says some workers were stationed outside the city and others at
reservoirs and fountains inside the city. He restored discipline by assigning
work in advance and recording completed work daily. This is unusually concrete:
Rome's water system had a permanent infrastructure workforce.

Population-normalized:

| Rome population model | Aqueduct workers per 1,000 people | People per worker |
| ---: | ---: | ---: |
| 450,000 | 1.56 | 643 |
| 800,000 | 0.88 | 1,143 |
| 1,000,000 | 0.70 | 1,429 |

Gameplay guidance:

- Use 0.7-1.6 central water workers per 1,000 people for a Rome-level aqueduct
  city.
- Use 0.3-0.8 per 1,000 for a smaller aqueduct town if the system is simple.
- Use local wells/cisterns with much lower central staffing but weaker coverage.
- Make water failure affect baths, fountains, latrines, fulleries, and some
  industries.

### Fire and Night Watch

Augustus organized the vigiles in Rome into seven cohorts, each responsible for
two of the fourteen regions. Standard summaries describe seven cohorts of about
1,000 men each, a combined force of about 7,000 firefighters/watchmen. Some
evidence suggests lower initial strengths and later expansion, so treat the
number as a mature Rome anchor.

Population-normalized:

| Rome population model | Vigiles per 1,000 people | People per vigil |
| ---: | ---: | ---: |
| 450,000 | 15.6 | 64 |
| 800,000 | 8.75 | 114 |
| 1,000,000 | 7.0 | 143 |

This is much denser than a Caesar-style prefecture should literally employ,
because a game building abstracts a patrol station, equipment, alarm system, and
coverage effect. Still, it shows that fire protection in dense Rome was a major
labor system.

Gameplay guidance:

- Abstract ordinary fire/watch posts at 1-3 public employees per 1,000 people.
- Use 5-10 per 1,000 for a Rome-like capital, dense port, or late-game high-risk
  district if the game models patrols more literally.
- Increase required coverage near insulae, bakeries, baths, fulleries,
  warehouses, markets, docks, and timber-heavy buildings.

## Public, Private, and Dependent Labor Accounting

Maintenance labor is not the same thing as state payroll.

| Labor bucket | Historical examples | Game accounting suggestion |
| --- | --- | --- |
| Public infrastructure crew | aquarii, road/drain workers, public fire/watch, some civic staff | Count against public labor or civic maintenance budget. |
| Public slaves/dependent workers | aqueduct gangs, bath/water lifting in some contexts, cleaning and hauling | Represent as public labor if the game abstracts legal status. |
| Private commercial labor | bakers, food sellers, fullers, bath attendants, warehouse porters, guards | Count as private employment unless the building is state-owned. |
| Elite household labor | domestic workers, estate staff, private bath/garden/house maintenance | Fold into elite-house labor demand and luxury upkeep. |
| Event labor | theater, amphitheater, circus, festival crews | Keep low daily payroll but add event spikes. |

This distinction matters for balance. A Pompeii-like city can have hundreds of
people working in bakeries and food counters without requiring hundreds of new
civic employees. By contrast, aqueduct crews, public drains, and fire/watch
systems are true public-capacity gates.

## Building-Type Maintenance Guidance

### Baths

Baths need the heaviest daily maintenance of any common service building:

- furnace tending at the praefurnium
- fuel handling and ash removal
- hypocaust floor/wall void maintenance
- hot, warm, and cold water management
- drains, pools, and basin cleaning
- cloakroom/security service
- cleaning, oil/strigil-related waste, crowd control

Pompeii provides area anchors:

| Bath anchor | Area |
| --- | ---: |
| Pompeii public baths, Flohr database average | 2,142 sqm |
| Stabian Baths last phase | about 3,300 sqm |
| Stabian Baths older guide area | over 3,500 sqm |

The 2026 carbonate/isotope study of Pompeii's Republican Baths is a useful
maintenance warning. Before aqueduct supply, the baths drew from deep wells by
water-lifting devices. The study found geochemical differences between well and
aqueduct deposits, evidence that Republican Bath water was not regularly renewed,
and signs of human-waste and heavy-metal contamination. The later aqueduct
increased available water and likely improved refreshment and hygiene.

Imperial thermae show the other end of the scale. The Baths of Caracalla had
subterranean service corridors, roughly 50 underground furnaces, cart access for
wood, storage rooms for fuel and supplies, and an estimated wood burn around 10
tons per day. Smarthistory summarizes the facility as able to hold about 1,600
bathers at one time and perhaps 8,000 per day. That is not an ordinary city
bath; it is a logistical machine.

Gameplay implication: a bath should not provide its full health bonus just
because the building exists. Its output should depend on water source, water
volume, cleaning/maintenance, fuel, drainage, and crowding. A poorly supplied
well-fed bath can still be a social and desirability amenity while producing
weak or even negative health effects.

Suggested maintenance model:

| Bath type | Standing staff | Extra needs | Failure effects |
| --- | ---: | --- | --- |
| Small balneum | 4-10 | fuel, water, cleaning | reduced hygiene, fire risk, desirability loss |
| Public bath complex | 12-30 | more fuel, furnace crew, cloakroom, cleaners | district hygiene drop, disease risk, local fire risk |
| Great thermae | 100-500+ | huge fuel/water/admin load | capital-scale prestige and health failure |

Do not make bath workers all civic unless the building is public-owned in the
game economy. Many attendants could be private, leased, enslaved, or freed.

### Fountains, Wells, Reservoirs, and Aqueduct Nodes

Fountains look small but imply a network. Pompeii had 42 public fountains, and
Rome's late catalogue lists 1,352 `lacus`.

Maintenance needs:

- clear spouts, overflow channels, and basins
- repair lead/terracotta pipes
- monitor pressure and water allocation
- clean settling tanks/reservoirs
- prevent illegal tapping
- maintain drainage around overflow

Suggested model:

| Asset | Standing staff | System burden |
| --- | ---: | --- |
| Well/cistern | none to 1 | local, low throughput, resilient |
| Fountain/lacus | none locally | counts against central water crew |
| Reservoir/castellum | 2-8 | central distribution, high failure impact |
| Aqueduct segment | 0.3-1.6 workers per 1,000 urban residents | citywide maintenance pool |

Pompeii is a good micro-model. Olsson's water-quantity study treats the visible
system as 14 water towers with 138-150 known connections: 42 street fountains,
5 public baths, and at least 91 or perhaps 103 private houses/workshops.
Aqueduct water for public use is estimated around 4.2 l/s for street fountains
and 1.2 l/s for public baths. In service terms, most residents were close to
public water, but access still had labor and inequality: Notarian models the
daily work of carrying water, and Simelius reports average dwelling-to-fountain
distance near 47 m with a maximum around 215 m.

Simulation levers:

- `flow_rate`: low-flow fountains satisfy a water-access check but increase
  queue/crowding time.
- `fetch_labor`: households without private pipes spend more hidden labor on
  water, reducing productivity or comfort.
- `network_damage`: earthquake, siege, drought, or maintenance failures should
  knock out clusters of endpoints fed by the same tower/reservoir.
- `private_connection`: elite houses and some workshops can consume water
  directly while increasing central network burden.

### Roads, Streets, and Drains

Roman streets carried people, animals, carts, wastewater, and overflow from
fountains. Paved roads and drains are durable but not maintenance-free.

Maintenance needs:

- stone replacement and leveling
- drain clearance
- flood repair
- rut and wheel damage repair
- bridge/culvert inspection
- clearing collapsed frontage or fire debris

Suggested model:

| Road context | Maintenance pressure |
| --- | --- |
| Residential side street | low |
| Market/bath/theater street | medium |
| Warehouse/dock/industrial road | high |
| Gate/bridge/highway approach | high |
| Poor drainage or slope | add flood/clog risk |

Tie road decay to traffic generators, not just road length. A quiet elite street
and a grain-warehouse street should not age at the same speed.

### Bakeries

Pompeii preserves about 35 bakeries, many with mills and ovens. Bakeries are
maintenance-heavy private businesses:

- ovens crack and need repair
- mills wear down
- animals need feeding/stabling if represented
- fuel demand is constant
- flour dust, heat, and confinement raise health/fire risk
- night or early-morning work can strain labor

Suggested model:

| Bakery size | Staff | Area | Risk |
| --- | ---: | ---: | --- |
| Small shop bakery | 3-5 | 100-200 sqm | medium fire |
| Mill-bakery | 6-10 plus animals | 200-500 sqm | high fire, high labor |
| Industrial/annona bakery | 10-30 | 500+ sqm | high fire, high storage dependency |

Bakeries should be good private-employment buildings and meaningful fire-risk
nodes.

### Food Counters, Taverns, and Markets

Pompeii's 158 food/drink counters show how dense prepared-food service could be.
These buildings are low capital individually but high in daily waste and small
fire sources.

Maintenance needs:

- washing counters and jars
- disposing food waste
- managing small ovens/braziers
- cleaning street frontage
- pest control
- crowd/noise management

Suggested model:

| Facility | Staff | Risk |
| --- | ---: | --- |
| Food counter/popina | 1-3 | low to medium fire, low sanitation risk |
| Larger tavern/caupona | 3-8 | medium fire, sanitation, crime/noise |
| Market/macellum | 8-30 | high waste, spoilage, crowding |

### Fulleries and Wet Industry

Fulleries need water, basins, drains, pressing areas, and detergents. They are
useful because they combine employment with real urban nuisance.

Maintenance needs:

- waterproof plaster/basin repair
- water supply and drainage
- chemical/detergent handling
- cleaning and odor control
- press/stall repair

Suggested model:

| Fullery size | Staff | Water/desirability |
| --- | ---: | --- |
| Small shop fullery | 3-6 | medium water, modest desirability penalty |
| Standard fullery | 6-15 | high water, clear odor penalty |
| Large fullery | 15-40 | very high water/drainage, industrial district anchor |

Ostia's fulleries give the clearest large-workshop pattern: basins in the floor,
pressing bowls around the hall, fuller's earth, urine, sulphur bleaching, and
finishing activities. The Ostia Antica summary, drawing on Bradley, also notes
worker health risks from prolonged wet work, irritant detergents, cracked skin,
and sulphur smoke. A fullery should therefore be an employment building with
worker-health, odor, water, and drainage consequences.

### Warehouses and Granaries

Rome's late catalogue lists 290 horrea. Ostia's warehouse evidence shows that a
single major facility could be enormous; the Grandi Horrea ground floor alone
has been estimated at 5,660-6,960 metric tons of grain capacity.

Maintenance needs:

- roof and wall integrity
- moisture control
- pest control
- guards
- clerks and inventory
- loading equipment and porters
- fire prevention

Suggested model:

| Warehouse type | Staff | Failure effects |
| --- | ---: | --- |
| Small granary | 4-10 | spoilage, reduced food buffer |
| Standard horreum | 10-30 | spoilage, theft, trade slowdown |
| Port warehouse | 30-100+ | dock congestion, citywide supply disruption |

Warehouses should increase fire/watch demand, especially near docks and
bakeries.

Ostia's horrea show why storage maintenance is not just capacity. Grain
warehouses used raised floors or `suspensurae` to protect contents from damp and
overheating; other storage buildings used dolia defossa for liquids. Large
warehouses have thick walls, few entrances, high slit windows, locking devices,
long cellae, ramps to upper floors, and often river-facing entrances. Port
storage also implies clerks, guards, porters, grain measurers, guild labor,
contracts, and imperial officials.

### Public Latrines, Sewers, and Waste

Rome's catalogue lists 144 public latrines. The low count suggests public
latrines were only part of sanitation; private toilets, shared facilities,
cesspits, chamber pots, street waste, sewers, and baths all matter.

Koloski-Ostrow's sanitation work is a useful corrective against treating Roman
latrines as modern public-health infrastructure. Public latrines were often
near baths, fountains, kitchens, or other water sources, were not necessarily
well ventilated, and in Pompeii/Herculaneum the small public latrine count did
not remove the need for chamber pots, street disposal, or private/shared toilets.
Another Pompeii study identifies 29 upper-story latrines and 286 wide-bore
downpipes, showing that household drainage was more complex than public-latrine
counts imply.

Maintenance needs:

- constant or frequent water flow
- sewer/channel clearance
- bench/floor cleaning
- odor control
- cesspit removal where not sewered

Suggested model:

| Asset | Staff | System burden |
| --- | ---: | --- |
| Small latrine | 1-2 | needs water or cleaning cycle |
| Public latrine | 2-4 | water/drain dependency, odor if failed |
| Sewer/drain node | central crew | flood/disease risk if neglected |

Do not make latrines a pure health bonus. They should consume water or
maintenance and create odor when neglected.

### Temples, Civic Buildings, and Monuments

Temples and civic buildings have lower daily mechanical maintenance than baths
but high prestige/restoration costs.

Maintenance needs:

- cleaning and guarding
- ritual staff or attendants
- roof repair
- statue, altar, marble, or painted-surface upkeep
- festival/event preparation
- periodic restoration by elite patronage

Suggested model:

| Building | Daily staff | Periodic cost |
| --- | ---: | --- |
| Shrine/aedicula | 0-1 | small offerings/cleaning |
| Temple | 2-8 | roof/statue/restoration events |
| Large temple/cult complex | 8-30 | major festival and restoration cost |
| Forum/basilica | 8-40 | paving, roof, public-order wear |
| Monument | 0-10 | irregular but expensive restoration |

### Entertainment Buildings

Entertainment buildings are lumpy. Daily staff can be modest, but events create
large bursts of maintenance and labor.

Maintenance needs:

- seating and stair repair
- crowd control
- awnings/velarium handling
- arena or stage preparation
- animal/gladiator/equipment holding if modeled
- post-event cleaning
- periodic structural repair

Suggested model:

| Building | Standing staff | Event staff/cost |
| --- | ---: | --- |
| Small theater/odeon | 4-12 | medium |
| Large theater | 8-30 | high |
| Amphitheater | 10-40 | very high during games |
| Circus/hippodrome | 30-100+ | extreme event logistics |

Pompeii's amphitheater could seat more people than the city itself, so
maintenance should scale with event capacity and regional prestige, not just
resident population.

### Housing

Housing maintenance should vary strongly by type:

| Housing type | Maintenance profile |
| --- | --- |
| Poor rental/insula-like housing | high fire and collapse risk; low private maintenance unless regulated |
| Ordinary plebeian houses | moderate maintenance; local water/sanitation dependency |
| Shop-house/taberna unit | fire/sanitation risk from mixed commerce |
| Elite domus | high private maintenance, low collapse risk if staffed, high luxury demand |
| Palace/villa | very high private labor and prestige upkeep |

The Regionary domus/insulae ratio supports making elite housing a small but
labor-intensive share of the city.

## Fuel, Water, and Waste Coupling

Several Roman services should be linked through shared resource pressure:

| Coupled system | Buildings affected | Tuning implication |
| --- | --- | --- |
| Aqueduct or spring-fed water | fountains, baths, latrines, fulleries, elite houses, some markets | Water shortage should degrade multiple services at once, not only fountains. |
| Fuel supply | baths, bakeries, lime/brick production, metalworking, elite kitchens | Fuel scarcity should hit hygiene, bread output, and industry before it is visible as hunger. |
| Drainage and wastewater | baths, fulleries, latrines, streets, markets | Clogged drains should combine odor, disease, road slowdown, and desirability loss. |
| Storage and transport | warehouses, bakeries, markets, dock services | Spoilage or congestion should propagate into food and trade output. |
| Fire/watch coverage | bakeries, baths, warehouses, dense housing, markets, workshops | Risk buildings should increase local coverage demand rather than using one flat citywide fire value. |

The Volubilis evidence also shows cross-resource reuse: excavators found
carbonized olive stones behind a bakery oven, suggesting olive pomace could be
used as fuel. A production city can therefore create both extra maintenance
burden and useful byproducts.

## Example Maintenance Budget: Pompeii-Like City, 10,000 People

| System | Facilities | Suggested workers | Public or private? | Main consumables/risk |
| --- | ---: | ---: | --- | --- |
| Water | 35-42 fountains, one distribution system | 6-12 | public | pipe repair, drainage, pressure |
| Baths | 4 public baths, about 8,500-10,000 sqm total | 48-80 | mixed/public | fuel, water, cleaning, fire |
| Bakeries | 25-35 | 125-280 | mostly private | fuel, grain, mill/oven wear, fire |
| Food counters | 100-160 | 150-320 | private | waste, small fires, street crowding |
| Fulleries | 6-12 | 40-120 | private | water, drainage, odor |
| Warehouses/granaries | 2-6 | 20-80 | mixed | pests, moisture, guards |
| Latrines/sanitation | 2-5 latrines plus drains | 10-25 | public | water, odor, clogging |
| Fire/watch | abstract posts/patrols | 20-60 | public | coverage, equipment |
| Road/drain crew | citywide | 10-30 | public | traffic wear, flood clearing |
| Temples/civic | forum, temples, shrines | 20-60 | mixed | cleaning, ritual, restoration |

This looks labor-heavy, but most of it is private employment. Public payroll
should mostly include water, sanitation, road/drain, fire/watch, and some civic
religious/administrative staff.

## Example Maintenance Budget: Rome-Like Capital

For Rome, avoid scaling every small city to capital intensity. Rome's hard
anchors imply:

| System | Hard anchor | Meaning |
| --- | ---: | --- |
| Water staff | 700 aqueduct workers | Permanent infrastructure staff for water network. |
| Vigiles | about 7,000 mature force | Fire/night-watch system is a major urban institution. |
| Balnea | 856 bathhouses | Baths are neighborhood infrastructure, not rare luxuries. |
| Lacus | 1,352 water points | Water access is dense and network-dependent. |
| Public latrines | 144 | Public sanitation exists but is sparse compared with fountains/baths. |
| Horrea | 290 | Storage is a citywide food-security system. |

If Rome is modeled at 800,000 residents, those anchors imply about:

- 0.9 water workers per 1,000 residents
- 8.8 vigiles per 1,000 residents
- 935 residents per bathhouse
- 592 residents per water point
- 5,556 residents per public latrine
- 2,759 residents per horreum

## Suggested Failure Modes

| Undermaintained system | First symptom | Severe symptom |
| --- | --- | --- |
| Water network | fountains intermittently fail | baths/latrines/fulleries stop, disease risk rises |
| Baths | lower hygiene output, dirtier pools | fire, disease, desirability loss |
| Roads/drains | slower walkers/carts, puddling | flood damage, disease, route penalties |
| Fire/watch | more fire starts spread | major district fire, unrest/crime |
| Bakeries | lower food throughput | fire, food shortage |
| Warehouses | spoilage/theft | famine/trade collapse |
| Latrines/sewers | odor/desirability drop | epidemic risk |
| Fulleries | lower textile output | odor/pollution complaints |
| Entertainment | event quality drop | injury/unrest after crowding |
| Housing | desirability and rent drop | collapse/fire/depopulation |

## Implementation Takeaways

- Treat maintenance as both labor and resource demand. Baths without fuel and
  water should not function at full output.
- Gate bath health output by water quality and renewal rate. A bath can satisfy
  social/desirability demand while failing as a health improvement if water,
  drainage, or cleaning is neglected.
- Make fire risk depend on building type. Bakeries, baths, warehouses, markets,
  fulleries, dense housing, and timber-heavy industry should raise local risk.
- Use central crews for infrastructure. Aqueducts, sewers, roads, and public
  fire/watch should not require one worker per small object.
- Let private employment carry much of the city. Food counters, bakeries,
  fulleries, bath attendants, and warehouse porters can be private labor sinks.
- Let neglected infrastructure produce visible city problems: water outages,
  disease, odor, fire, collapse, spoilage, and slower movement.

## Sources

- Frontinus aqueduct staff, Wikisource:
  https://en.wikisource.org/wiki/Page%3AFrontinus_-_The_stratagems%2C_and%2C_the_aqueducts_of_Rome_%28Bennet_et_al_1925%29.djvu/505
- Frontinus full text:
  https://waters.iath.virginia.edu/front.html
- Regionary/Chronography totals:
  https://www.ccel.org/ccel/pearse/morefathers/files/chronography_of_354_14_regions_of_rome.htm
- LacusCurtius Regionaries Latin text:
  https://penelope.uchicago.edu/Thayer/L/Gazetteer/Places/Europe/Italy/Lazio/Roma/Rome/_Texts/Regionaries/text%2A.html
- Pompeii public fountains/lacus study:
  https://link.springer.com/article/10.1007/s12685-021-00281-9
- Pompeii public fountain pipeline discharge study:
  https://www.sciencedirect.com/science/article/pii/S2352409X22004321
- Pompeii water quantities and connection counts:
  https://link.springer.com/article/10.1007/s12685-023-00333-2
- Matthew Notarian, Pompeii fountain access network analysis:
  https://ajaonline.org/article/4585/
- Samuli Simelius, Pompeii water-access inequality:
  https://journal.caa-international.org/articles/10.5334/jcaa.116
- Pompeii water distribution summary:
  https://ancientengrtech.wisc.edu/pompeii/water-distribution/
- Miko Flohr, Database of Pompeian Houses:
  https://www.mikoflohr.org/pompeii/
- Stabian Baths research note:
  https://pompeiisites.org/e-journal-degli-scavi-di-pompei/public-saunas-in-the-stabian-baths-a-privilege-of-men/
- Stabian and Republican Baths excavation note:
  https://pompeiisites.org/en/excavations-plan-en/stabian-baths-and-republican-baths/
- Pompeii Republican Baths water-quality study metadata:
  https://openscience.ub.uni-mainz.de/items/def61ce2-3174-4fe5-90cc-6224ef941da5
- Phys.org / Johannes Gutenberg University Mainz summary of the same PNAS study:
  https://phys.org/news/2026-01-hygienic-conditions-pompeii-early-poor.html
- Roman bath social/maintenance context, Garrett Fagan book page:
  https://press.umich.edu/Books/B/Bathing-in-Public-in-the-Roman-World
- Smarthistory, Baths of Caracalla:
  https://smarthistory.org/baths-of-caracalla/
- Khan Academy / Smarthistory, Baths of Caracalla:
  https://www.khanacademy.org/humanities/ancient-art-civilizations/roman/middle-empire/a/baths-of-caracalla
- Vigiles overview:
  https://www.worldhistory.org/Vigiles/
- Britannica vigiles summary:
  https://www.britannica.com/topic/vigiles
- Ostia vigiles overview:
  https://ostia-antica.org/dict/topics/caserma/intro.htm
- Ostia warehouses / Grandi Horrea summary:
  https://www.worldhistory.org/Ostia/
- Ostia fulleries overview:
  https://ostia-antica.org/dict/topics/fullones/intro.htm
- Miko Flohr, The World of the Fullo:
  https://www.mikoflohr.org/blog/2013/08/13/the-world-of-the-fullo-1/
- Oxford Roman Economy Project summary of The World of the Fullo:
  https://oxrep.web.ox.ac.uk/osre-fullo
- Ostia horrea overview:
  https://www.ostia-antica.org/dict/topics/horrea/intro.htm
- Ostia grain and bread trail:
  https://ostiaantica.cultura.gov.it/en/trails/grain-and-bread/
- Koloski-Ostrow sanitation book metadata:
  https://scholarworks.brandeis.edu/esploro/outputs/book/The-archaeology-of-sanitation-in-Roman/9924035756301921
- AJA review of Koloski-Ostrow, The Archaeology of Sanitation in Roman Italy:
  https://ajaonline.org/book-review/3598/
- Pompeii upper-story latrines and downpipes:
  https://www.sciencedirect.com/science/article/pii/S2352409X16306344
- Volubilis map and building summaries:
  https://sitedevolubilis.org/map-of-volubilis/
- Pompeii bakery operational-sequence abstract:
  https://academic.oup.com/book/9085/chapter/155642961
- Pompeii and Herculaneum bars / marble-counter survey:
  https://www.cambridge.org/core/product/5F6EF940DFF9D253CFE197AECBDE1510/core-reader
- Pompeii counters with cooking-evidence source:
  https://dokumen.pub/a-cultural-history-of-food-in-antiquity-volume-1-9781350044562-9780857850232-9781474269902.html
