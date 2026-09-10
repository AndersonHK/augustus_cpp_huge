# Housing consumption validation

Scope: annual manufactured-good requirements per resident, exact Julius/Augustus full-occupancy calibration, and Vespasian's revised 0.3/0.6 rates, including wine. See [the full tables and assumptions](housing_annual_consumption_2026_09_09.md).

The subsequent food-warning investigation and installed fixes are recorded in [Food reserve and production warnings](food_stock_warning_2026_09_10.md), including the latest city's exact stocks and the remaining performance-gate failure.

## Installed-game correction and direct launch, 2026-09-10

- The preceding production fix was built and tested in the checkout but was not deployed. The user's installed `D:/Games/GOG Games/Caesar 3/Vespasian.exe` was still the September 8 binary, explaining why their Augustus playtest retained the old underproduction. The installed and tested binaries had different SHA-256 hashes.
- Direct launch no longer implicitly requests Vespasian. With no `--mod`, it loads the complete saved `config/mod-list` written by the launcher and selects its last entry. An explicit `--mod NAME` still trims the stack at that entry and rejects absent names.
- Rebuilt the executable and definition tests. All definition contracts passed, including saved-stack selection regressions and the previous production-rate checks. Real executable startup without `--mod` passed for saved lists ending in Julius, Augustus, and Vespasian.
- Deployed and hash-verified 70 changed files in the installation: the executable/debug symbols, changed runtime dependency, and this task's housing/production XML. Replaced files and old/new hashes are preserved in `out/install-backup-2026-09-10-housing-production/manifest.json`. User settings, mod-list, saves, and graphics assets were not replaced.
- The **installed executable and installed data** passed four 3,000-tick save/render checks without new warnings or errors after migration: legacy `Citizen.sav` under Julius and Augustus, native `Citizen - Julius Only Save.svv` under Augustus, and native `Engineer attempt 1 30 - just a resave with the refactor.svv` under Vespasian. Tests used isolated settings and separate roundtrip files. Logs: `out/direct-launch-*.log`, `out/direct-launch-definitions.log`, and `out/installed-*.log` / `.err`.
- This is a representative installed-game validation, not another complete 70-case sweep. The two known performance failures recorded below remain unresolved.

## General production-throughput fix, 2026-09-10

- Implemented the general resource-production contract: monthly output is absolute, and declared cartloads scale cycle work once for farms and workshops alike. Standard five-field wheat farms now nominally produce 1,920 units/year in Julius, Augustus, and Vespasian, with their existing different harvest sizes.
- Removed all 25 shipped unit-valued `batch_size` declarations, the template declaration, parser support, runtime member/accessors, and the input-scaling helper. No save field used it. Inputs remain literal per-cycle quantities, including availability checks, consumption, supply-chain queries, blessings, and UI display. They do not scale with cartloads. Non-resource effects and figure-delivery loops retain their existing timing.
- Release/x64 rebuild passed. The full definition suite passed, including 62 shipped producer-rate checks across the three mod stacks against upstream `95e120d80` (whole-farm aggregation and the city-mint gold/denarii rate alias included). Resource effects and fish metadata are distinguished from physical inventory throughput.
- Production-work regressions cover farms and workshops with 0.2, 1, 2, and 5 cartloads per cycle over complete 125-year harvest periods, equal monthly output, unchanged literal input quantities, variable month lengths, transport-capacity independence, one-load defaults, minimum positive work, and overflow saturation.
- All 80 current/proposed housing budget rows reconcile, and all 40 current-capacity staffing rows match the independent vanilla farm-output reference. Both Small Shack districts require 50 farm workers, cost 285/year, and balance at +259.32. Report regeneration is deterministic.
- Full executable startup/save/render gate completed against the isolated updated fixture. All 70 cases completed 3,000-tick soaks, with no new load/soak warnings or errors after legacy migration; 68 passed the complete save/render checks. The same two Praetor saves missed the existing 1,000-TPS floor: `Praetor 2 10.svv` at 990.5 TPS and `Praetor 2 8.svv` at 977.8 TPS. Their speed failures occur before the final post-soak save/render assertions, so they are not complete passes. The gate exited 1 and is **not green**; thresholds were not changed. Logs: `out/production-throughput-build2.log`, `out/production-throughput-definitions.log` / `.err`, and `out/production-throughput-save-gate.log` / `.err`. Definition-suite error messages from deliberately invalid fixtures are expected; they are separate from load/soak diagnostics.

## Earlier domestic production payroll audit (before the runtime fix)

- **Corrected baseline attribution:** upstream's historical wheat rate is 160/month = 1,920/year, feeding 320 people at 6 units/year. The current Vespasian nominal result matches it. The 384/year bundled Julius/Augustus result is underproduction, not evidence that Vespasian is five times stronger than vanilla. The calculator now exposes `-VanillaFarmOutput`, and the report includes all 20 tiers with the same vanilla farm output in both stacks. This is a reference scenario, not a gameplay edit.
- All 40 vanilla-farm reference rows pass budget/payroll reconciliation; both Small Shack rows require 50 farm workers, cost 285/year, and balance at +259.32. Wheat and other crop reference outputs match 1,920/960 units/year with 10 workers. Vespasian staffing and all import/export budgets remain unchanged. These checks validate the reference calculation, not a new engine soak.
- Corrected omitted field employment: a standard farm requires 5 main-building workers plus five 1-worker fields, for 10 in both stacks. This also corrects olive/vine payroll within oil/wine chains. Small Shack production staff is now 210 Augustus / 50 Vespasian, replacing 105 / 25; annual total cash expenses become 765 / 285 and balances -220.68 / +259.32.
- Traced current production progress, output, composition employment, game calendars, and monthly food consumption. The authored nominal wheat output is 384 / 1,920 inventory units per game year, but calendars contain 9,600 / 36,500 ticks. Average output per 1,000 ticks is therefore 0.400 / 0.526 loads. These are source-derived nominal rates, not measurements of delivered production in a playtest.
- Verified all 80 current/proposed housing-budget identities, food/goods staffing decomposition, the Small Shack hand calculation, calendar normalization, and six-field employment (11 workers). Import/export report sections remain identical; report regeneration is deterministic. Earlier row-reconciliation checks only verified arithmetic consistency and did not detect the omitted field workers.
- The reported Vespasian farm supporting roughly 320 people agrees with its nominal output and the vanilla benchmark. The source-derived difference from this checkout's bundled Augustus remains, but is not a comparison against normal vanilla output. Month length cancels out of nominal cycles/month. Per-house food rounding can allow a population slightly above the smooth 320-person benchmark.
- Comparison against pre-change `HEAD` confirms this task did not change food timing or manufactured-goods scheduling. Following history back before `8d885e4f5` establishes the original whole-farm rate of 160/month. That commit reduced it to 32 per field, then imposed another reduction: one-fifth harvests in Julius/Augustus, and fivefold cycle length in Vespasian. Both resulting rates were below vanilla. `f00fb3548` removed the cycle penalty, restoring Vespasian's nominal vanilla output; `168659b83` subsequently set its field batch sizes to 1. My earlier interpretation that equal output at the intermediate commit proved the intended baseline was incorrect. No production fix has been applied.
- This audit changes the research calculator and documentation only. No game definitions or C++ production/food behavior changed, and no new executable soak was run.

## Latest rate, export-value, and capacity-proposal revision

- Vespasian XML now declares 0.3 for demanded plebeian goods, 0.6 for patrician pottery/oil/furniture, and wine at 0.3 through Grand Villa / 0.6 from Small Palace. Source-access counts, food, capacities, and tax multipliers are unchanged.
- Release/x64 build passed. The full definition-contract suite passed with `--definitions-only`, including all three shipped mod stacks and merged profiles. New decimal cases verify fractional full-year demand (5.7, 22.8, and 63.6), a ten-year total of 57 at 0.3 for 19 residents, and revised market stock targets.
- Native Engineer refactor `.svv` and legacy `Citizen.sav` each passed 3,000 ticks, native fractional-carry roundtrip checks, and applicable render checks using the revised rates. Legacy migration repairs are logged; subsequent validation remained clean.
- All 80 calculator rows (40 current-capacity and 40 proposal-scenario rows) reconcile for import, export-value, and domestic-payroll budgets. Checks verify a hand-calculated Luxury Palace export expense of 10,388.4 and balance of 1,707.6, increasing proposed density, both per-house and equal-area Small Villa tax constraints, and unchanged Augustus/plebeian proposal rows. Report regeneration is deterministic.
- The capacity rework is a calculator/document proposal only. No BuildingType capacities were changed. Export prices apply to food as well as manufactured goods; source XML supplies the buy/sell values.

The earlier 70-case sweep and its two performance failures below were **not rerun in full for this data-only revision**. Definition-only testing is not a replacement for that full gate. Latest logs: `out/housing-rate-revision-build-fs.log`, `out/housing-rate-revision-definitions.log` / `.err`, and `out/housing-rate-revision-save-gate.log` / `.err`. The first compiler-database failure was recovered by restarting this task's idle database helper and enabling synchronized writes; no project build settings were changed.

## Earlier 1/2-rate implementation checks

- Release x64 build of `StartupParserTest.vcxproj` and its game/module dependencies passed.
- Shipped-profile checks cover all 20 tiers and three merged variants in the Julius, Augustus, and Vespasian stacks. They verify annual totals and wine source requirements independently.
- Rate tests cover fractions, decimals, malformed input, partial occupancy, zero occupancy, market buffers, and fractional carry through the save-state bridge.
- Executable roundtrips compare all four fractional carries after native save/reload, including nonzero fractions seeded by the test when necessary.
- The cost calculator reconciles all 40 Augustus/Vespasian rows under both import and domestic-production assumptions.
- Executable startup checks passed for Julius, Augustus, and Vespasian.
- The complete save sweep ran 70 cases for 3,000 ticks each: 67 required/representative Vespasian saves, one original campaign save, and the Julius/Augustus dependency-stack cases. All completed their soaks without new load/soak warnings or errors after legacy migration; 68 passed the complete save/render checks. Two failed the existing simulation-speed floor, so the gate exited 1 and is **not green**.

## Remaining performance limitation

| Save | Full-sweep steady-state simulation TPS | Final isolated rerun TPS | Required TPS |
| --- | ---: | ---: | ---: |
| Praetor 2 10.svv | 889.5 | 847.3 | 1,000 |
| Praetor 2 8.svv | 897.8 | 899.5 | 1,000 |

Both failures reproduce in fresh processes. They report zero load/soak warnings and errors, but the gate returns at the speed failure before the final post-soak save/render assertions. Those two cases must not be counted as complete save/render passes. An earlier working-tree run achieved 1,476.8 TPS on Praetor 2 10; the cause of the slowdown remains unresolved. The performance trace shows little time in the housing-evolution/consumption bucket, which alone does not establish the cause.

After the sweep, the clearance fix was narrowed to the original shared routing callback structure. A final Release build compiled and linked successfully; targeted 3,000-tick reruns still reproduce the two speed failures, while the Engineer refactor save passes its full save/render and fractional-carry checks. Thresholds and diagnostics were not relaxed. The 70-case sweep preceded this final routing-scoping adjustment.

Validation uses an isolated runtime fixture under the ignored `extracted_graphics_sample/housing-consumption-game` directory. Authored definitions come from this checkout; runtime graphics remain outside authored `Mods` directories. Original saves are loaded read-only and roundtrip outputs are separate files.

## Runtime regressions exposed by the soak

The Engineer refactor save exposed automatic road-to-Rome clearance deleting aqueduct terrain without retiring its runtime building owners. Clearance now retires the affected owners after its route traversal, including surface layers removed by the same operation. No load warnings are suppressed.

Housing expansion also accepted plazas as gardens because both carry the garden terrain flag. It now rejects other blocking terrain on those tiles and retires the surface records replaced during expansion. This preserves road access and prevents stale garden/plaza ownership. Both house-splitting paths apportion fractional consumption carry.

The road-drag render fixture previously selected an invalid construction site in Quaestor. Its candidate selection now checks the actual placement plan; the rendering assertions remain intact.

The previously failing Engineer city passes 3,000 ticks, native save/reload, fractional-carry comparison, and water/road/menu render checks after these fixes. Legacy migration repairs still produce warnings on the first load; the resulting native save and subsequent soak must be clean.

## Reproduction

Build `StartupParserTest.vcxproj` with Release/x64 using the repository's Windows MSBuild invocation rules. Run:

```text
StartupParserTest.exe --game-root <isolated-game-root> --test-config <test-config-directory> --save-soak-count 3 --save-soak-ticks 3000
```

The gate also includes its required named save cohorts, the original campaign save, and dependency-stack saves. Logs for this run are in ignored `out/housing-consumption-final-gate.log` and `.err`; build output is in `out/housing-surface-fix-build.log` and `out/housing-routing-scope-build.log`. Final targeted results are in `out/housing-routing-final-*.log` and `.err`, with the diagnostic performance trace in `out/housing-praetor-performance.log`.
