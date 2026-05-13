# Stronghold Popularity and Economy Design Notes

Snapshot: 2026-05-10

## Purpose

This is a weak comparative reference for
[Vespasian Housing Progression Design Notes](../vespasian_housing_progression_design_notes.md).
Stronghold is useful because it makes population attraction a simple visible
function of popularity, rations, taxes, religion, ale, fear, and housing space.

## Useful Patterns

Stronghold does not use Caesar-style evolving houses. It uses a castle economy:
hovels provide capacity, popularity controls whether peasants arrive or leave,
and peasants become workers when jobs exist. Food and taxes are the primary
popularity levers, with religion, ale, fear factor, and crowding adding pressure
in different entries.

The design lesson is clarity. The player can see that more taxes produce money
but reduce popularity, more food/ration quality costs supply but attracts
peasants, and insufficient housing causes crowding.

## Possible Vespasian Lessons

| Stronghold pattern | Vespasian use |
| --- | --- |
| Popularity controls immigration/emigration | City sentiment could regulate housing demand more directly. |
| Rations trade food consumption for morale | Food variety and ration quality could matter beyond binary house gates. |
| Taxes are a visible happiness tradeoff | Market revenue or class taxes should have sentiment consequences. |
| Housing capacity and crowding are explicit | Service overcrowding could borrow the same clarity: capacity debt should be obvious. |
| Workers come from a visible idle pool | Caesar unemployment could become more readable in housing and labor overlays. |

## Cautions

Stronghold is castle-defense first and city simulation second. It is more
global, harsher, and more directly manipulated than Caesar III. Borrow the
clarity of visible tradeoffs, not the exact global-popularity model.

## Sources

- [Stronghold Wiki: Popularity](https://stronghold.fandom.com/wiki/Popularity) -
  popularity, peasant arrival/departure, food, tax, religion, ale, fear, and
  crowding.
- [Stronghold Wiki: Peasant](https://stronghold.fandom.com/wiki/Peasant) -
  peasants as labor, tax, food consumers, and recruitable population.
- [Stronghold Wiki: Food](https://stronghold.fandom.com/wiki/Food) - ration and
  food-variety effects.
