# Anno Population Tier Design Notes

Snapshot: 2026-05-10

## Purpose

This is a weak comparative reference for
[Vespasian Housing Progression Design Notes](../vespasian_housing_progression_design_notes.md).
Anno is useful because it makes population tiers, needs, workforce, income, and
upgrade rights explicit.

## Useful Patterns

Anno 1404 keeps lower tiers relevant through ascension proportions. A house can
upgrade only when needs are satisfied, tax conditions are favorable, the house is
full, materials exist, and the lower-tier base is sufficient. The wiki describes
hard proportions such as only 80% of houses being Citizens or above, and notes
that Patricians need Citizens and Peasants below them.

Anno 1800 makes workforce tiered and island-wide. Residences produce workforce
of their tier, and buildings require a specific workforce type and amount.
Insufficient workforce reduces productivity or stops a building; surplus
workforce is generally harmless.

## Possible Vespasian Lessons

| Anno pattern | Vespasian use |
| --- | --- |
| Lower-tier proportions | Prevent every successful district from becoming elite; preserve plebeian base. |
| Manual or denyable ascension | Let players stop accidental villa conversion without crude desirability tricks. |
| Tier-specific workforce | Let patricians work selected high-status jobs while plebeians remain necessary. |
| Needs increase by tier and population threshold | Make service expectations rise with city size without overburdening villages. |
| Workforce shortage reduces productivity | More granular than Caesar's binary employed/unemployed behavior. |
| Surplus workforce is not automatically bad | Avoid overpunishing players for keeping labor reserves. |

## Cautions

Anno is a production-chain and logistics game with explicit tier panels. Caesar
III's charm depends on visible roads, walkers, and house messages. If Vespasian
borrows tier-specific workforce or ascension rights, it needs Caesar-style UI:
house tooltips, overlays, and advisors should explain the rule locally.

Do not import Anno's exact population proportions. Use the pattern: higher tiers
should depend on a lower-tier base, not erase it.

## Sources

- [Anno 1404 Wiki: Population](https://anno1404.fandom.com/wiki/Population) -
  ascension rights, lower-tier proportions, needs, taxes, and material gates.
- [Anno 1800 Wiki: Workforce](https://anno1800.fandom.com/wiki/Workforce) -
  tiered workforce, residence workforce sources, and productivity loss from
  insufficient workforce.
- [Anno 1800 Wiki: Needs](https://anno1800.fandom.com/wiki/Needs) - basic and
  luxury needs, income, happiness, and upgrade requirements.
