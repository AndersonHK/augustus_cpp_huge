# Barracks recruitment validation — September 10, 2026

Implemented design: [barracks recruitment](../docs/barracks_recruitment.md).

## Reproduction

The latest `autosave-year.svv` had a live barracks with 10/10 workers, road access, four weapon loads, zero food stress, and four eligible home formations containing 50, 16, 16, and 16 soldiers. The scenario allowed the barracks, but its menu entry was disabled. The delay-only production method selected industrial dispatch and made trade availability treat the barracks as a goods producer of troops.

The initial classification-guard approach was replaced before deployment, as requested. The final design uses ordinary troop production, storage, and an XML spawn policy. The original simple checks in `BuildingType::has_native_production`, `building_producer_for_resource`, and production-runtime creation are restored. Recruitment costs live in `UnitType`; the existing Mars Grand Temple policy uses those same costs and native supply storage.

## Final validation

The Release executable and StartupParserTest built successfully. Definition contracts passed, including nominal upstream production rates, multiple recruitment resources, invalid/duplicate requirements, and production work modifiers. `git diff --check` passed.

| Save | Stack | Result |
|---|---|---|
| `autosave-year.svv` | Vespasian | Full 3,000-tick save/render gate passed; 1,446.3 steady-state simulation TPS |
| `Praetor 2 12 - better.svv` | Vespasian | Full 3,000-tick save/render gate passed; 1,369.2 steady-state simulation TPS |
| `Citizen.sav` | Augustus | Full 3,000-tick canonical save/render gate passed |
| `Citizen - Julius Only Save.svv` | Julius | Full 3,000-tick canonical save/render gate passed |

The two Vespasian saves exercised actual troop production and dispatch. Their buffer capacity was four loads; the other stacks resolved to one load. Tests covered full-buffer bounds, no recruit from an empty buffer, declarative equipment checks, stopped supply orders, exclusion from goods carts, troop persistence, and food stress doubling work at stress 28. A temporary Mars Grand Temple fixture recruited through its original policy. Test mutations and synthetic structures were discarded by reloading the original city before the soak.

The historical Citizen saves emitted their existing migration repair warnings on first import (one surface-binding repair for Augustus; twelve repairs for native Julius). Canonical reloads and soaks were clean. Recent Vespasian loads and soaks had no warnings or errors.

## Deployment and installed verification

The executable, symbols, rebuilt GraphicsExtractor DLL, and seven authored XML files were deployed to `D:/Games/GOG Games/Caesar 3`; the retired `recruitment_delay.xml` was removed. Eleven operations were backed up and hash-verified in `out/install-backup-2026-09-10-barracks-storage/manifest.json`.

Installed executable SHA-256: `99389FB4377B213ED134B55DD41560BB7877774308F627C69F5862436339C3D6`.

The installed executable passed the troop-buffer and Mars policy checks and completed 3,000 ticks with no load/soak warnings or errors. It missed the separate 1,000-TPS performance floor at **911.5 TPS**, so that installed soak is not a full gate pass. This installation-versus-development performance discrepancy also occurred before this refactor. An additional installed run loaded the already-soaked canonical city and passed the remaining save/reload and render checks. The overall installed performance gate remains unresolved; no threshold was weakened and the installation's SDL DLL was not replaced.

Logs: `out/barracks-storage-final-*`, `out/barracks-installed.*`, and `out/barracks-installed-render.*`. Original saves, launcher mod-list, and user settings were not overwritten.
