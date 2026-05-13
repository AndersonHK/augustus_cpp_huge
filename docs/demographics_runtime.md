# Demographics Runtime

## Purpose

This note records how the citywide age census, yearly births/deaths, and house populations interact.

## Tuning References

Use [Roman and Early Medieval Mortality Research](roman_early_medieval_mortality_research.md)
for mortality-table shape,
[Roman City Size and Social Ratios](../research/roman_city_size_and_social_ratios.md)
for resident-class and city-size assumptions, and
[Roman City Facility Ratios](../research/roman_city_facility_ratios.md) for
service ratios that can indirectly affect health, desirability, and employment
demand.

## XML Contract

`defines.xml` owns the active demographic tables:

- `birth_table id="default"` contains one `age_decennia` row with ten non-negative yearly birth percentages.
- `mortality_table id="default"` contains eleven `health bucket` rows with ten non-negative yearly death percentages each.

Both tables are merged through the active mod chain, and startup fails if the merged definitions do not provide the active `default` entries.

## Yearly Flow

The year rollover requests a population update. The daily housing/migration tick later applies it:

1. Age the citywide census by one year.
2. Remove deaths from houses using the mortality table and current city health bucket.
3. Remove the matching deaths from the citywide age census.
4. Add births to existing occupied connected houses with available capacity.
5. Add the accepted births to census age `0`.
6. Recalculate the city population from the census.

The census is citywide. Houses do not store their own age structure, so demographic deaths are distributed across houses independently from the age cohort that supplied the death count.

## House Population Consistency

`house_population_remove_from_city` removes actual residents from houses and reports how many were removed. The yearly census death pass only removes cohort deaths that correspond to successful house removals, so census and houses do not drift when removal is partially blocked.

`city_population_check_consistency` also reconciles both directions:

- if the census is larger than the housed population, excess people are removed from the census;
- if houses contain more people than the census, excess residents are removed from houses.

Births are already capacity-limited because census age `0` is only increased by the number of births accepted into houses.
