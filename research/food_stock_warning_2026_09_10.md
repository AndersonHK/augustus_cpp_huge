# Food reserve and production warnings

Investigated `Praetor 2 11 - running stable.svv` from the installed game on 2026-09-10. Two different counters produced the warnings.

## Food reserves

Citizens' city-wide food complaint and the chief advisor's food-stock row use whole months of food. The old calculation included operating granaries but omitted warehouses, despite markets being able to buy food directly from them.

| Saved city | Amount |
| --- | ---: |
| Population | 11,284 |
| Estimated monthly food need | 5,642 units |
| Granary food | 4,800 units |
| Warehouse food available to markets | 8,100 units |
| Old reported reserve | 0 whole months |
| Correct available reserve | 12,900 units / 2 whole months |

A third full granary would add 2,400 units and push the old granary-only calculation above one month, explaining the user's observation. The fix includes warehouse food in the city-wide reserve and food-type count while retaining granary-specific totals. Warehouse eligibility follows market supply restrictions: road/entry access, market permission, stockpiling, plague, and maintaining orders. This does not guarantee that every house has a working local distribution route.

## Production versus consumption

The chief advisor's production comparison used food delivered into granaries during the last month. That omitted production sent to warehouses and depended on cart delivery timing. The counter now advances from the common production event used for completed harvests and fish landed at wharves. Delivering or transferring food into a granary does not produce it again. Imports remain separate from domestic production.

The comparison still describes actual production, not potential output at full employment. Full storage can stop production, and a quiet harvest month or reliance on imports can leave production below consumption even with ample reserves. Existing saved monthly statistics update as game months roll over; the patch does not invent replacement historical production figures.

## Validation and installation

The reserve-only build passed the original save's 3,000-tick installed-game soak and complete save/render checks at 1,414.4 steady-state simulation ticks per second, after the interactive game closed. The earlier simultaneous run had no load/soak warnings or errors but missed the speed floor at 823.2 TPS. Legacy Augustus and native Julius save checks also passed.

Native regression checks exercise warehouse market permissions and stockpiling, preserving the loaded city's settings. Accounting checks inject a 20-unit harvest, 100 units of landed fish, and a non-food production event, then verify that a subsequent granary delivery does not double-count food; ledger and resource state are restored afterward.

The final accounting build compiled and linked successfully. Its accounting checks passed for the user's latest Vespasian save, legacy Augustus `Citizen.sav`, and native Julius `Citizen - Julius Only Save.svv`. The Augustus and Julius cases passed their complete 3,000-tick save/render gates.

The latest Vespasian city completed its 3,000 ticks with zero load/soak warnings or errors, but missed the 1,000-TPS floor: 840.0 TPS from the development folder and 818.2 TPS in the final installed-build rerun. Both runs reported 12,900 available food units and 2 months of supply and passed the accounting/permission checks. The speed failure occurs before the final post-soak save/render assertions, so these are not complete passes and the combined gate is **not green**. The earlier reserve-only installed run passed at 1,414.4 TPS; the variability remains unresolved. The development and installation folders also contain different SDL2 DLL hashes; that observation does not establish the cause, and those DLLs were not replaced.

The final executable and matching symbols were deployed to `D:/Games/GOG Games/Caesar 3`, and their hashes match the tested build. Replaced files and hashes are in `out/install-backup-2026-09-10-food-accounting/manifest.json`; the preceding reserve-only deployment has its own `out/install-backup-2026-09-10-food-stock` backup. Logs use `out/food-accounting-*`; the earlier installed reserve check is `out/food-stock-installed-latest.log`. No user save or configuration was overwritten by testing.
