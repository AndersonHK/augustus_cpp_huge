# Launcher legacy bug-fix settings audit — 2026-09-06

The launcher exposes two obsolete configuration switches. Neither has a runtime consumer, and toggling either does not change the game. Their absence from the in-game configuration menu is intentional historical cleanup; their appearance in the launcher came from the new shared configuration catalog exposing stale entries.

| Launcher setting | Original purpose | Current implementation |
| --- | --- | --- |
| Fix 100y ghosts (`gameplay_fix_100y_ghosts`) | Prevent population/census disagreement when residents reach age 100: those residents have already fallen out of the age array and must not also be deducted from the surviving age cohorts. | `src/city/population.cpp`, `yearly_advance_ages_and_calculate_deaths`, ages the census, removes house residents, and removes only eligible cohort deaths from the census. It also accounts for residents that could not be removed from houses. Mortality percentages come from mod defines. The INI switch is never read by this code. “Ghosts” means population-accounting inconsistencies, not invisible walkers or rendering. |
| Fix immigration (`gameplay_fix_immigration`) | Correct an original Very Hard difficulty edge case where low default happiness could stop immigration around population 200–300. The old fix raised default happiness to 50 for that case. | The current `src/city/sentiment.cpp` calculates housing sentiment from difficulty, taxes, wages, unemployment, housing, desirability, entertainment, food and other contributions. The old small-city conditional is no longer present in this rewritten algorithm. The INI switch has no consumer; it cannot enable or disable either the old correction or the current sentiment system. This audit does not claim the rewritten system reproduces the old 200–300 population behavior exactly. |

## Evidence and recommendation

Historical commit `5d559800bd4729dce6dc8f930208a98ac50ee7c1` (2020-10-19, “Config menu cleanup”) removed the configuration conditions from both `src/city/population.c` and `src/city/sentiment.c`, making those historical fixes unconditional. The original ghost fix appears in `46eefa753`. Current source references to the two setting IDs are confined to their declarations in `src/core/config.h` and their entries in `src/core/config_options.h`. The launcher consumes that catalog; the old in-game menu does not contain these items.

Recommendation: retire both switches from the exposed catalog and eventually remove their dead INI entries. Do not add toggles to recreate census corruption. If fidelity to the old immigration threshold is desired, that should be an explicit, separately tested sentiment policy in mod data, rather than reviving a misleading “fix immigration” flag. This request asked for a report, so these two switches have not been changed.

The separate storage-over-roads option was also unused, but its removal was explicitly requested and is implemented in the D12 slice. Its catalog entry, enum, in-game checkbox, and shipped labels have been removed; old named INI keys are harmlessly ignored. Native placement and foundation data remain the authority.
