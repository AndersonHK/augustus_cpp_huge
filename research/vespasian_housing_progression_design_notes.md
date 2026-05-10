# Vespasian Housing Progression Design Notes

Snapshot: 2026-05-10

## Purpose

This document is explicitly forward-looking. It is not a description of vanilla
Julius/Augustus behavior and it is not a historical reconstruction. It records
Vespasian design directions that may rebalance housing, labor, roads, services,
goods, and revenue while preserving the feeling of a Caesar-style city builder.

Use the Caesar-specific docs first:

- [Caesar III / Julius Housing Progression Defaults](caesar3_julius_housing_progression_defaults.md)
- [Caesar III Housing Balance and Play Analysis](caesar3_housing_balance_play_analysis.md)

Use Roman historical docs as plausibility bounds:

- [Roman City Facility Ratios](roman_city_facility_ratios.md)
- [Roman City Size and Social Ratios](roman_city_size_and_social_ratios.md)
- [Roman Building and Infrastructure Maintenance Needs](roman_building_maintenance_needs.md)

Comparative design notes live in
[Comparative City-Builder Design](comparative_citybuilder_design/README.md) and
should be consulted only for patterns, never for direct tuning values.

## Design Stance

Realism is one pillar. The other pillars are progression feel, player
readability, labor pressure, logistics pressure, and the fantasy of growing a
poor settlement into a Roman metropolis.

The default Caesar curve has known balance quirks: some tiers are unusually
efficient, some services are binary and easy, and some goods gates are much
harder than their numeric requirement suggests. Vespasian can intentionally
smooth or sharpen those quirks, but changes should be explicit and testable.

## Proposed Design Axes

| Proposal | Intended gain | Main risk | Documentation anchor |
| --- | --- | --- | --- |
| Let certain jobs require patricians | Turns elite housing into an economic class, not only tax/prosperity | Weakens vanilla labor cliff if too many jobs accept patricians | Caesar housing balance; SimCity 4 and Anno workforce tiers |
| Make patricians dislike high-density plebeian housing | Gives elite districts a spatial identity and limits mixed super-blocks | Can become opaque if desirability penalties are hidden | Roman social-ratio notes; desirability docs |
| Increase road access from 2 to 4 tiles | Allows deeper neighborhoods and less road frontage pressure | Weakens road-access puzzle; walker coverage still needs clarity | Caesar labor-access rule and walking-range docs |
| Scale service expectations with city size | Lets small towns remain rough while metropolises expect more | Can feel like moving goalposts if thresholds are not surfaced | Roman city-size bands; facility ratios |
| Make upgrades depend on housing demand from employment | Prevents automatic elite conversion when the economy needs workers | Adds a new abstract system that must be readable | SimCity RCI demand and Anno ascension rights |
| Give service buildings assignable capacity and overcrowding | Turns doctors, libraries, baths, markets, and entertainment into quantitative systems | Mandatory services can become punitive if capacity is too tight | Maintenance needs; comparative service-capacity patterns |
| Make market sales generate city revenue | Makes internal consumption economically meaningful, not only export trade | Can over-reward high-consumption cities and trivialize taxes/exports | Facility ratios, trade/economy docs |

## Patrician Labor Redesign

Vanilla Caesar uses a clean split: plebeians work, patricians do not. That split
creates the labor cliff when Grand Insulae become Villas. If Vespasian lets
patricians fill jobs, the design should avoid making them interchangeable with
plebeians.

Good candidates for patrician or elite labor:

- administration, census, legal, and treasury offices
- high education, academy, library, or rhetorical/cultural institutions
- high temples, priesthood administration, cult festivals, and civic religion
- merchant, banking, shipping, estate, or tax-farming roles
- advanced medical or luxury-service roles, if the design wants elite expertise

Bad candidates:

- farms, mines, clay pits, timber yards, low workshops, prefecture patrol, and
  ordinary cart pushing, because these should preserve plebeian labor demand

Suggested rule shape: patrician jobs should be scarce, high value, and unlocked
late. They should consume elite population without replacing the need for
plebeian districts.

## Elite Aversion to Dense Plebeian Housing

A patrician dislike for dense plebeian housing can reinforce neighborhood
identity. This should be framed as desirability/status pressure, not as a claim
that Romans never mixed elite and commercial/common space. Roman houses,
workshops, shops, tenants, and dependents often coexisted in dense urban fabric.
The gameplay rule is about making elite land use legible and preventing one
perfect all-class block from solving every pressure.

Possible implementation shapes:

- `desirability_penalty_from_resident_class="plebeian_dense"` on elite housing
- stronger penalty from Large/Grand Insula than from Casas or Hovels
- thresholded penalty only above a local density count
- scenario override for ports, Rome-like districts, or mixed-use cities

## Road Access Expansion

Raising road access from 2 to 4 tiles would let houses sit deeper behind roads,
supporting courtyards, gardens, larger villas, and less repetitive frontage. It
would also make local labor access more forgiving.

This should not automatically extend walker service coverage. A good separation
would be:

- road/building validity can reach 4 tiles
- service walkers still need route contact, coverage radius, or explicit access
- overlays must distinguish "has road access" from "receives service"

That split lets neighborhoods become physically deeper without making services
magically omnipresent.

## City-Size Service Expectations

Small settlements should not need metropolis services. Large cities should not
be satisfied forever by the same minimum civic stack.

Possible structure:

| City size | Added expectation |
| --- | --- |
| Village / early town | wells, food, shrine, basic safety |
| Small town | fountains, market stability, basic religion, school/library choice |
| Pompeii-scale town | baths, doctors, multiple entertainment types, more food variety |
| Provincial city | hospitals, libraries and schools, fuller entertainment, civic administration |
| Metropolis | multi-service capacity, high sanitation, elite institutions, major venues |

This can scale requirements, service quality, or capacity demand. Scaling should
be visible in UI so the player reads it as civic maturity rather than arbitrary
punishment.

## Demand-Gated Housing

Vanilla housing upgrades when needs, desirability, and space allow it. A demand
layer could ask whether the city actually has a reason for more of a tier.

Candidate demand signals:

- unfilled plebeian jobs create plebeian housing demand
- unfilled patrician jobs create patrician housing demand
- unemployment suppresses low-tier immigration but can encourage elite leisure
  only if tax/prosperity conditions are met
- high taxes, poor sentiment, housing shortages, or service overload reduce
  upgrade demand

The lesson from SimCity is useful but should not be copied whole. Vespasian
should remain a walker-and-block city, not a zoning simulator. Demand gates
should explain why a house can or cannot evolve without hiding the familiar
goods/service puzzle.

## Capacity and Overcrowding

Binary walker coverage makes some services too easy. Assignable capacity could
let a player decide that one doctor covers 300 people, one library supports a
district, a bath has a comfort capacity, and a theater has seats or event
capacity.

Potential capacity stats:

- nominal residents served
- overcrowded residents served with reduced quality
- employees required for full capacity
- consumables per resident or per capacity band
- queue/overload effect on health, desirability, evolution, or sentiment

The UI must show capacity debt clearly. If a house says it has a doctor but the
doctor is overloaded, the player needs to see the difference between missing
coverage and insufficient capacity.

## Market Revenue

Markets currently mediate food and goods delivery, while the city's direct
income is mostly taxes, exports, and scenario sources. Adding market sales
revenue would make internal consumption part of the fiscal game.

Possible forms:

- sales tax percentage on food/goods delivered to houses
- stall rent per active market worker or per household served
- luxury excise on pottery, oil, furniture, and wine
- market revenue reduced by overcrowding, corruption, or poor administration

Design risk: consumption revenue can make elite, goods-heavy cities too rich. It
should probably interact with tax collectors, administration, market capacity,
or corruption rather than being a free passive bonus.

## Test Questions

- Does a city still need a large plebeian base after elite jobs are introduced?
- Can a player intentionally hold housing at Small Casa, Large Insula, or Grand
  Insula for understandable reasons?
- Does the road-access increase create better neighborhoods without making
  walker routes unreadable?
- Does service capacity add meaningful planning without turning every required
  service into hidden math?
- Does market revenue make internal trade interesting without trivializing export
  industry or taxation?
- Does the settlement still feel like it grows from poor rural housing to a
  proper Roman metropolis?
