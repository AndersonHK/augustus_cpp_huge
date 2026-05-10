# SimCity RCI Series Design Notes

Snapshot: 2026-05-10

## Purpose

This is a weak comparative reference for
[Vespasian Housing Progression Design Notes](../vespasian_housing_progression_design_notes.md).
It summarizes useful RCI patterns from SimCity, SimCity 2000, SimCity 3000, and
SimCity 4. It should not override Caesar/Rome-specific research.

## Series Pattern

SimCity separates player intent from private development. The player zones
residential, commercial, and industrial land, then demand determines what grows.
The SimCity zone lineage is useful because density, wealth, and demand are not
the same variable:

- Original SimCity uses fixed-size RCI zones.
- SimCity 2000 moves to tile-level zoning and light/dense RCI.
- SimCity 3000 adds medium density.
- SimCity 4 separates density, wealth, developer type, and demand in more detail.

SimCity 4 is the most relevant comparison for Vespasian proposals. StrategyWiki
summarizes its demand loop: residential demand comes from jobs; commercial
demand comes from residents and wealth; industrial demand comes from residents
and education. It also divides residents into low-, medium-, and high-wealth
types and associates high wealth with late-game services, education, health, and
high-end jobs.

## Useful Patterns

| Pattern | How SimCity uses it | Possible Vespasian lesson |
| --- | --- | --- |
| Demand before growth | Positive RCI demand allows growth; negative demand causes stagnation or abandonment | Housing could require local or citywide demand before upgrading, especially into villas. |
| Jobs create residential demand | More jobs support more residential growth | Employment opportunities could gate plebeian immigration or expansion. |
| Wealth-tier jobs | Higher-wealth residents work in higher-wealth offices/industries | Patrician jobs can exist without making patricians general laborers. |
| Density separate from wealth | Dense zoning controls building capacity; wealth depends on desirability and services | Dense plebeian housing and elite housing can remain distinct axes. |
| Demand caps and stages | Population and region size unlock higher stages | City-size service expectations can rise without making every small town overbuilt. |
| Pollution/desirability | Residential wealth avoids pollution and seeks amenities | Patrician dislike of dense/dirty surroundings can be a legible desirability rule. |

## Cautions

SimCity is a macro-zoning game. Caesar III is a walker, road, service, and goods
chain game. Vespasian should not become an RCI simulator where houses upgrade
because an abstract bar says so. The useful idea is not zoning itself; it is the
separation of demand, density, wealth, job class, and service quality.

If Vespasian adds demand, it should answer concrete Caesar-like questions:

- Are there jobs for more plebeians?
- Are there elite jobs or offices that justify more patricians?
- Are services overcrowded?
- Is the city large enough to expect the next civic tier?
- Is the district desirable enough for the class trying to move in?

## Sources

- [SimCity Wiki: Zone](https://simcity.fandom.com/wiki/Zone) - RCI zone
  evolution from SimCity through SimCity 4, density types, and costs.
- [StrategyWiki: SimCity 4/Zoning and Demand](https://strategywiki.org/wiki/SimCity_4/Zoning_and_Demand) -
  SimCity 4 wealth types, developer types, demand sources, caps, and stages.
- [SimPage: SimCity 3000 RCI indicator](https://simpage.net/simcity3/tips/958397502.shtml) -
  readable contemporary explanation of RCI bars as demand/abandonment signals.
