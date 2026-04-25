# Preindustrial Walking Service Ranges

This note records historical calibration for pedestrian-only service ranges. It
is intended for tuning FigureType walker `max_roam_length` values, not for
turning those values into literal meters. The useful finding is the shape of the
tolerance curve: local services should feel close, ordinary neighborhood
services should sit inside a comfortable walking round trip, and only high-value
or public-safety trips should push beyond that.

For the engine conversion from `max_roam_length` to approximate tiles, meters,
game days, and wall-clock seconds, see
`docs/tile_scale_and_walker_timescale.md`. That note currently treats 15 m/tile
as the practical midpoint because one-tile roads represent arterial corridors
while a 2x2 evolved house block represents four houses with paths between them.

## Research Summary

Before railways and cheap mass transit, most urban residents had to organize
home, work, worship, shopping, and civic services around walking. The best
general rule is a 30-minute one-way practical ceiling for routine trips, with
shorter limits for children and frequent daily needs. That matches the
transport-planning idea often called Marchetti's constant: people tend to budget
around one hour of travel per day, or roughly half an hour out and half an hour
back. Modern summaries caution that this is an aggregate tendency, not a
universal biological law, but it is still a good design prior for a walking city.

At a steady adult walking pace, 30 minutes is about 1.5 miles / 2.4 km. In a
dense premodern city, with crowds, turns, errands, weather, and mixed ages, a
safer gameplay feel is closer to 1.0-1.25 miles / 1.6-2.0 km for ordinary
routine services. A 45-minute one-way walk is already an exceptional trip; beyond
that, most routine needs should either fail, decentralize to a nearer provider,
or be treated as a special journey rather than normal coverage.

## Historical Anchors

- Rome: the Aurelian circuit enclosed a large imperial walking city, but even
  that was not a commuting city in the modern sense. The wall circuit was about
  19-20 km, and the enclosed area was roughly 13.7-14 square km, meaning most
  practical cross-city trips still sat in the one-hour-plus category rather than
  an everyday neighborhood errand.
- Paris: the early thirteenth-century wall of Philip Augustus enclosed about
  253 hectares; the later Charles V enclosure brought the city to about 440
  hectares. Those sizes support a city where a neighborhood walk is short, while
  a full cross-city trip is deliberate and costly in time.
- London: the Roman/medieval wall defined the City of London into the later
  Middle Ages, with the landward wall about 2 miles long and the historic core
  corresponding closely to the later Square Mile. The medieval City had dense,
  narrow streets and around 80,000 residents inside the walls, which again points
  to nearby work and services rather than long routine foot commutes.
- Early modern / industrial transition: London evidence around the transport
  revolution is useful as a contrast case. A 2024 open-access study reports that
  a sizeable majority of working-class Londoners still worked within a short
  walk of home in 1890; by 1930, after rail, tram, bus, and Underground networks
  expanded, over 70 percent commuted at least 1 km. That suggests long routine
  commuting is a product of cheap transport networks, not the default foot-only
  baseline.

## Gameplay Interpretation

Use `max_roam_length` as a normalized tolerance tier:

- `192`: very local. Use for children, fragile walkers, school trips, or service
  loops that should mostly affect adjacent blocks. This is the "families give up
  quickly" tier.
- `384`: ordinary adult neighborhood service. Use as the baseline for labor
  seekers, bathhouse workers, barbers, librarians, priests, teachers, and
  similar walkers whose service should cover a walkable district but should
  still reward distributed city planning. Labor seekers can be temporarily or
  explicitly extended when a workforce policy needs a larger search radius.
- `640`: long patrol / public-safety range. Use for engineers, prefects, and
  similar risk-management walkers where gameplay benefits from broader patrols.
  This tier should not become the default for culture, religion, or labor access
  unless the design intentionally wants modern-style commute tolerance.

For service design, a good "give up" curve is:

- 5-10 minutes one-way: daily essentials, children, household errands.
- 10-20 minutes one-way: ordinary neighborhood services.
- 20-30 minutes one-way: upper bound for routine adult services and workplace
  access in a foot-only city.
- 30-45 minutes one-way: occasional high-value, civic, or safety trips.
- More than 45 minutes one-way: exceptional; require a duplicate provider,
  transport abstraction, or explicit special-case design.

## Tuning Notes

- Avoid making all service walkers long-range just to make cities easier. Long
  foot commutes should trade against density, neighborhood planning, or service
  duplication.
- Let children and frequent household needs have the sharpest falloff. The
  school-child `192` tier is historically plausible even if adults can tolerate
  more.
- Labor should usually stay at the ordinary neighborhood tier. If industry pulls
  from farther away, it should feel like a special workforce policy, not the
  normal state of a walking city.
- Public-safety walkers can justify the long tier because they are patrol
  systems, not voluntary household trips.
- If a future transport system appears, it can raise effective access radius
  without changing the preindustrial walking baseline.

## Sources

- Bloomberg CityLab, "The Commuting Principle That Shaped Urban History":
  https://www.bloomberg.com/news/features/2019-08-29/the-commuting-principle-that-shaped-urban-history
- Independent Transport Commission / Why Travel, "Marchetti's Constant and
  Travel Time Budgets": https://whytravel.org/marchettis-constant-and-travel-time-budgets-one-hours-travel-a-day/
- Seltzer and Wadsworth, "The impact of public transportation and commuting on
  urban labor markets", Explorations in Economic History 91, 2024:
  https://www.sciencedirect.com/science/article/pii/S0014498323000475
- Britannica, "Aurelian Wall": https://www.britannica.com/topic/Aurelian-Wall
- Aurelian Walls size/circuit summary:
  https://en.wikipedia.org/wiki/Aurelian_Walls
- Wall of Philip II Augustus construction summary:
  https://en.wikipedia.org/wiki/Wall_of_Philip_II_Augustus
- Wall of Charles V expansion summary:
  https://en.wikipedia.org/wiki/Wall_of_Charles_V
- City of London Corporation, "London Wall":
  https://www.cityoflondon.gov.uk/things-to-do/architecture/historic-architecture/london-wall
- City of London Corporation, "The Medieval City":
  https://www.cityoflondon.gov.uk/things-to-do/walks-and-itineraries/self-guided-walks-and-trails/the-medieval-city
- Naismith walking baseline, 3 miles / 5 km per hour:
  https://en.wikipedia.org/wiki/Naismith%27s_rule
