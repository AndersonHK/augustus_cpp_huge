# Research Index

Snapshot: 2026-05-10

This folder holds historical tuning research that is too wide-ranging for the
runtime and XML contract notes. The documents are meant to be used together:
population size, service ratios, and maintenance load all change the meaning of
the same building type.

## Design Use

Historical realism is one pillar of tuning, not the only pillar. These notes are
meant to keep values directionally correct: baths should be water- and
fuel-hungry, ports should need more storage than inland towns, daily water
should be very local, and Rome-like capital systems should feel exceptional.
That does not mean a researched ratio must be copied exactly into gameplay.

Use the evidence to understand the available degrees of freedom. It can show
which values are plausible, which values are suspicious, and which tradeoffs a
change should create. Final values still need to preserve the intended feeling,
progression curve, player readability, service behavior, and spirit of the game
as a city builder.

When research and gameplay tension conflict, prefer an explicit design decision
over false precision. A value can be historically approximate and still be the
right value if it produces the intended planning pressure, pacing, and city feel.
The research should inform the direction and limits of adjustment; it should not
mandate an exact adjustment that damages the design intent.

## Roman City Research

- [Caesar III / Julius Housing Progression Defaults](caesar3_julius_housing_progression_defaults.md) -
  vanilla housing capacities, service gates, prosperity, tax, desirability, and
  visual progression notes for preserving the city-builder progression grammar.
- [Caesar III Housing Balance and Play Analysis](caesar3_housing_balance_play_analysis.md) -
  Caesar-specific gameplay analysis of patrician non-labor, the labor cliff,
  efficient and costly housing tiers, goods friction, and service difficulty.
- [Vespasian Housing Progression Design Notes](vespasian_housing_progression_design_notes.md) -
  forward-looking design proposals for classed labor, city-size expectations,
  road access, demand-gated housing, service capacity, and market revenue.
- [Roman City Facility Ratios](roman_city_facility_ratios.md) - ratios for
  baths, fountains, bakeries, food counters, fulleries, warehouses, temples,
  public safety, and entertainment buildings by population and city type.
- [Roman City Size and Social Ratios](roman_city_size_and_social_ratios.md) -
  city-size bands and a historically cautious way to read plebeian, patrician,
  elite, slave, freed, and dependent populations for gameplay.
- [Roman Building and Infrastructure Maintenance Needs](roman_building_maintenance_needs.md) -
  recurring labor, fuel, water, cleaning, inspection, and failure-mode guidance
  for buildings and infrastructure.

## Closely Related Docs

- [Demographics Runtime](../docs/demographics_runtime.md) explains the active
  birth and mortality table contract in code.
- [Roman and Early Medieval Mortality Research](../docs/roman_early_medieval_mortality_research.md)
  gives the mortality-table tuning context; the maintenance document adds why
  baths, water, and sanitation should not be treated as modern health guarantees.
- [Preindustrial Walking Service Ranges](../docs/preindustrial_walking_service_ranges.md)
  and [Tile Scale and Walker Timescale](../docs/tile_scale_and_walker_timescale.md)
  connect these service ratios to walker coverage and physical footprint
  assumptions.
- [BuildingType README](../Mods/Vespasian/BuildingType/_README.md),
  [HousingProfile README](../Mods/Vespasian/HousingProfile/_README.md), and
  [FigureType README](../Mods/Vespasian/FigureType/_README.md) are the XML
  contracts most likely to consume these ratios.
- [Comparative City-Builder Design Notes](comparative_citybuilder_design/README.md)
  are intentionally separate and weakly linked. They can inform Vespasian design
  patterns, but should not be consulted as Roman or Caesar tuning evidence.

## Use Together

The safest tuning workflow is:

1. Pick the city size and social mix from the city-size document.
2. Check the Caesar III / Julius housing progression and Caesar housing balance
   documents so changes keep the intended shelter-to-metropolis arc and labor
   economy legible.
3. If changing Vespasian mechanics rather than values, record the design intent
   in the Vespasian housing progression notes.
4. Pick service counts and rough areas from the facility-ratio document.
5. Add staffing, fuel, water, and failure costs from the maintenance document.
6. Adjust those values to fit scenario pacing, progression, and the intended
   pressure on the player.
7. Check whether walker ranges and tile footprints still make the city playable.

Avoid applying one table mechanically. Pompeii-like street food density, Ostian
warehouse density, Timgad bath density, and Rome's capital infrastructure are
different urban systems. Their ratios are evidence for design space, not a
requirement that every scenario reproduce their exact numbers.

## Source Quality Notes

The strongest numbers in this folder come from directly counted or measured
evidence: Pompeii fountains, tabernae, baths, and housing units; Rome Regionary
catalogue totals; Ostia horrea; Volubilis workshops; and measured macella
footprints. Population denominators are weaker, especially for Rome and Ostia.

When adding future ratios, record four things beside every number:

1. whether the count is excavated, textual, reconstructed, or gameplay-derived
2. the population assumption
3. whether the facility serves residents, trade, production, or regional visitors
4. whether labor is public payroll, private employment, household service, or event labor
