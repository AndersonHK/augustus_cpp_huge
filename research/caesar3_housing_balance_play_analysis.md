# Caesar III Housing Balance and Play Analysis

Snapshot: 2026-05-10

## Purpose

This document records Caesar III / Julius / Augustus housing balance as a game
system, not as historical evidence. Use it with
[Caesar III / Julius Housing Progression Defaults](caesar3_julius_housing_progression_defaults.md)
when changing `HousingProfile`, `BuildingType`, market distribution, service
coverage, or labor rules.

The goal is to preserve the core city-builder tension: better housing gives
population density, tax, prosperity, safety, and visual progress, but it also
adds service, goods, desirability, space, and labor-management pressure. A value
can be historically plausible and still be a bad value if it removes the
housing puzzle.

## Core Correction

In vanilla Caesar III, patricians do not work. The original manual states that
plebeians keep the city functioning while patricians are wealthy residents who
do not join the workforce; when apartments evolve into villas, workforce falls
without reducing the number of people who need to be fed. The Caesar3 Augustus
handbook repeats the same labor-access rule: buildings need access to housing,
then ask the labor advisor for workers, and shortages are resolved by priorities.

This makes the grand-insula-to-villa transition a labor cliff, not just a wealth
upgrade. A district that was solving unemployment as dense plebeian housing can
become an elite district that consumes food and services while removing workers
from the available labor pool. That cliff is a deliberate Caesar pressure and
should be treated as first-class design material.

## Tier Efficiency Map

The following reads the vanilla table through common player strategy rather than
through history.

| Tier band | Why players like or avoid it | Balance reading |
| --- | --- | --- |
| Tents to Large Shack | Cheap growth, low services, but low tax/prosperity, high risk, weak city mood | Opening settlement state. Useful for labor, not for a mature city. |
| Small Hovel to Small Casa | Strong early payoff: fountain, food, religion, basic entertainment, and education give good density without manufactured goods | Small Casa is a stable "bread and butter" tier when goods or supply chains are scarce. |
| Large Casa to Medium Insula | Starts consuming manufactured goods while still often being 1x1; pottery and furniture can become inefficient per resident | Risky middle band if goods are scarce or houses fail to merge. |
| Large Insula | Forced 2x2, high plebeian density, goods are amortized over 84 residents, no second food yet | One of the strongest labor-housing tiers when oil, barber, education, and doctor coverage are stable. |
| Grand Insula | Highest plebeian tier, same 84 residents, better tax/prosperity, but second food and more entertainment are required | Excellent if logistics are robust; dangerous if second-food distribution is brittle. |
| Small/Medium Villa | Major tax/prosperity jump, but sharp density loss and zero labor | Should be intentional. Accidental conversion can collapse employment. |
| Large Villa to Luxury Palace | Huge tax/prosperity and visual prestige, but heavy services, goods, desirability, space, and no labor | Score/tax district, not a workforce solution. Needs plebeian support base. |

An older strategy note makes the goods-efficiency issue explicit: from Large
Casa upward, non-food goods are consumed per house, so larger footprints are
more efficient than small houses for pottery, furniture, oil, and wine. The same
note recommends either going to Large/Grand Insulae or stopping at Small Casa
when pottery, furniture, oil, or the workers to make them are scarce.

## Requirement Difficulty

Not all requirement numbers are equally costly in play. The table below ranks
them by the type of friction they create.

| Requirement | Vanilla role | Practical difficulty |
| --- | --- | --- |
| Road access / labor access | Buildings and housing need proximity to roads and labor-search paths | Foundational layout constraint; easy to understand, but blocks deep neighborhoods. |
| Well / fountain water | Early evolution gate, then clean-water requirement | Easy once infrastructure is planned; fountain routing can constrain blocks. |
| Library / school | Education depth; Large Insula needs both | Usually low logistics difficulty: build, staff, and route walkers; no commodity chain. |
| Doctor | Required from Medium Insula onward in local XML (`health = 1`) | Mandatory for high plebeian housing, but cheap and compact; difficulty is coverage stability, not production. |
| Hospital | Required by `health = 2`, starting at Medium Villa | Higher footprint, cost, and labor; mostly an elite sustain requirement. |
| Barber | Large Insula gate | Small and cheap, but easy to miss in service coverage. |
| Bathhouse | Large Casa gate | Moderate: employees, water adjacency/availability, and walker coverage. Historically should also imply fuel and maintenance pressure. |
| Entertainment | Increasing score from 10 to 80 | Easy early, hard late; high tiers require venue variety and performer logistics. |
| Pottery | First manufactured good, Large Casa gate | Harder than most services because it needs raw clay/imports, workshop labor, storage, market pickup, and per-house consumption. |
| Furniture / oil | Medium/Large Insula goods | Similar to pottery; higher-tier supply chains compete with export industries and labor. |
| Second food type | Grand Insula gate | Often harder than its numeric simplicity suggests: requires food source/import, granary policy, market access, and stable distribution of multiple food inventories. |
| Wine / second wine | Small Villa and Small Palace gates | Luxury chain plus the two-wine availability rule; can force imports or multi-source logistics. |
| Desirability / expansion space | All high tiers, especially villas/palaces | Land-planning constraint. Can be cheap if gardens/statues are strong; punishing when road/service layouts are tight. |

The local Julius XML shows why some requirements feel trivial while others bite:
a doctor is 1x1, costs 30, and employs 5; a library is 2x2, costs 75, employs
20, and has positive desirability; a pottery workshop is 2x2 and employs 10, but
also needs clay, storage, cart movement, market pickup, and house consumption.
The XML number alone does not describe the gameplay cost.

## As-Is System Pressures

Caesar III uses binary walker/service coverage for most housing needs. If the
right walker passes the house often enough and the house has the required goods,
the requirement is satisfied; there is no explicit classroom capacity, clinic
patient load, bath crowding, theater seats, or market queue saturation in the
housing requirement itself.

Goods are different because they have real logistics. A service can be "trivial"
when it is just a building, labor, and a route. Pottery or a second food type can
be hard because it must survive the whole chain from production or import to
storage to market buyer to house inventory.

Labor is citywide after access is established. A building needs local access to
housing before it can request workers, but it does not employ a specific nearby
household. This keeps the game readable, but it means district employment demand
does not directly regulate whether nearby houses should upgrade.

Patrician housing gives no workforce. This is the main reason elite districts
need a plebeian base elsewhere. A redesign that lets certain jobs employ
patricians would change one of the deepest Caesar pressures and should be
treated as a deliberate Vespasian rule, not a vanilla correction.

## Balance Implications

Before changing housing thresholds, ask which pressure is being adjusted:

| Change type | Likely result | Risk |
| --- | --- | --- |
| Make services easier | Smoother upgrades and fewer coverage failures | Turns housing into a goods-only puzzle. |
| Make goods easier | More stable high insulae and villas | Removes the main midgame logistics challenge. |
| Make second food easier | Grand insula becomes much safer | Can make high-density labor housing too dominant. |
| Make doctors/hospitals capacity-limited | Health becomes a real planning axis | Can make mandatory health feel punitive if the UI does not explain crowding. |
| Let patricians work some jobs | Softens labor cliff and creates elite demand | Can erase the need for plebeian districts unless jobs are carefully scoped. |
| Increase road access depth | Allows better neighborhoods and less frontage pressure | Weakens road-layout puzzle and may hide walker instability. |
| Add market sales revenue | Makes internal consumption fiscally visible | Can over-reward goods-heavy cities unless export/tax balance changes too. |

## Sources

- [Caesar III manual mirror](https://manualmachine.com/gamespc/caesariii/1119684-user-manual/) -
  plebeian/patrician workforce rule, labor access, and workforce loss on villa
  conversion.
- [Caesar3 Augustus Handbook: Housing and Desirability](https://www.caesar3augustus.com/book/housingdesirability/start) -
  villa transition, goods/services, and walker access framing.
- [Caesar3 Augustus Handbook: Employment and Labor](https://www.caesar3augustus.com/book/people/employment) -
  labor access, two-tile housing-road rule, and labor advisor behavior.
- [Caesar Jan strategy tips](https://www.geocities.ws/caesar_jan/strategy_tips.html) -
  per-house manufactured-goods efficiency and Large/Grand Insula versus Small
  Casa strategy heuristic.
- [Arqade housing requirements table](https://gaming.stackexchange.com/questions/307997/housing-steps-and-requirements-in-caesar-3) -
  readable consolidation of Caesar III housing requirements and vanilla values.
- Local XML: `Mods/Julius/HousingProfile/*.xml` and `Mods/Julius/BuildingType/*.xml`.
