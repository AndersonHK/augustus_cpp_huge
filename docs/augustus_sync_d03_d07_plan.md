# Approved Augustus slice: D03–D07, D15–D16

Status: earlier implemented review slice, superseded where noted by [D08/UI revisions](augustus_sync_d08_plan.md). The former station/workcamp conflict is resolved by the approved global-stockpile source. No commit or ancestry adjustment has been made. Earlier launcher/mod-settings work remains in the working tree.

## Scope and architecture

1. Verify and install the final distributed inherited asset pack; preserve authored Vespasian graphics and run extraction only in the installation. Record archive URL, hash, revision and compatibility findings.
2. D03: generic data-owned city maintenance effects, resource demand scaled from infrastructure extent, supplier delivery and operational discounts. Augustus defines Highway Station costs, prerequisites, supply needs and the upstream 50% construction/levy benefit. Vespasian inherits these settings until its later highway slice.
3. D04: phased construction data plus a reusable monument-gift event policy for external materials and labor. Preserve award eligibility, road access/orientation and completed arches in legacy saves.
4. D05: remove only confirmed unsupported platform packaging and adapters, retaining Android/iOS and Switch 2 groundwork, document/enforce the supported hardware boundary, retain SDL2 and adapt relevant desktop controller/input fixes.
5. D06: XML-owned religion/reference windows bound to active mod religions, bonuses and translations; font-metric-aware text and safe navigation.
6. D07: transaction-owned import/export, production/consumption and historical accounting with stable resource identities, calendar-aware periods and explicit unknown imported history. XML-owned ledger and affected empire/sidebar UI.
7. D15: approved presentation/convenience fixes through existing owners, preserving current defaults where requested. D16: inherited art/audio refreshed from the verified pack; no generated asset import into repository Mods.
8. Build and run focused contracts, save migration/roundtrip/render soaks, and zero-fallback graphics validation. Deploy the reviewable result and update per-commit evidence without claiming merged ancestry.

## Subsequent decisions

D08 is approved and has a native implementation slice. Highway Stations now draw from the global stockpile, so No Monuments can keep work camps hidden. D09 is the next gameplay decision. See [the current record](augustus_sync_d08_plan.md); older evidence below describes the earlier binary.

## Implementation record

| Decision | Current implementation and principal owners |
| --- | --- |
| D03 | `src/building/CityServiceDef.h`, `BuildingCityService.cpp`, terrain extent caching, finance/construction quotes, and workcamp delivery owners implement infrastructure maintenance without station-specific runtime dispatch. `Mods/Augustus/BuildingType/highway_station.xml` declares the 12 workers, workcamp prerequisite, stone/sand inputs, six-month stock target, 50% highway construction/levy effects and conditional stock graphics. `highway.xml` owns the extent and levy parameters. Julius has no station; Vespasian inherits Augustus. |
| D04 | `src/city/monument_gifts.cpp`, construction definitions and monument/worker owners implement named-event rewards, award eligibility, external materials/workers, delivery quantities, access coordinates and single-building construction phases. Augustus declares its two arch phases and Rome supply. Julius remains instant; No Monuments removes phases while retaining gift eligibility. Save 0xc8 introduced generic gift awards. |
| D05 | `src/platform/hardware_requirements.cpp` enforces 64-bit, 4 GB RAM, Vulkan-capable non-software graphics and 1 GB device-local memory. SDL2 remains. The initial blanket mobile/console removal was incorrect and has been reversed for Android/iOS and reusable Switch adapters; only Vita, original Switch releases and 32-bit targets are excluded. See [corrected platform scope](platform_scope.md); mobile and Switch 2 ports are not yet validated. Desktop controller handle lifetime/mappings, startup diagnostics, unsigned character classification in formulas and headless crash reporting were adapted. This is a Vulkan hardware requirement, not a replacement of the existing SDL2 renderer. |
| D06 | Active religion definitions own epithet metadata and requirements. `src/window/epithets.cpp` and advisor bindings drive `Mods/Augustus/UI/windows/epithets.xml` and `advisor_religion.xml`. XML owns geometry, controls, pagination and presentation; code resolves active religions, requirements, translated text and actions. Rich text uses font metrics. |
| D07 | `src/city/trade_ledger.cpp` records actual production, consumption and trade, aggregating transactions by visit/city/storage/resource/month/price/direction. It retains the current year and seven prior years, with explicit partial imported history. Stable resource text identities and wide totals are serialized in native save 0xc9. Currency/troop pseudo-resources remain outside cartload statistics. `src/window/trade_ledger.cpp`, financial advisor and empire controllers bind XML ledger, financial, sidebar and selected-city windows. |
| D15 | Accurate terrain desirability contributions, damaged defensive-building health, optional climate grid colors, per-category build cycling, persisted empire sorting, optional five-year common-message pruning, optional extended camera bounds, scrollbar state fixes, weather/ambient corrections, willow data and author-selected campaign rank inheritance. Current palette and scrollbar skin remain. Pruning, extended bounds and climate-specific grids default off; no clear-weather wind was added. The mixed health commit's bridge map-restore correction is included; damage thresholds and economic balance are retained. |
| D16 | Final distributed asset archive installed and extracted outside the checkout. Authored overrides remain authoritative. The portrait loader now accepts external portrait groups; the obsolete empire-panel texture consumer was replaced by the XML panel. Existing and new station graphics resolve against the refreshed pack. |

Live mod setting validation also found two static water-definition pointers that survived registry replacement. `src/building/water_access_runtime.cpp` now resolves those definitions from the active registry, so repeated settings reloads cannot reuse freed definitions.

Workcamp figure-allocation failure returns unassigned loads to the source warehouse. A dynamic accounting payload that exceeds its buffer fails the save before replacing the destination file; it cannot silently write a truncated city.

## Installed asset provenance

- Upstream-linked development endpoint: `https://augustus.josecadete.net/download/latest/development/assets`.
- Resolved archive: `https://augustus.josecadete.net/download/assets-4.0.0.1495-719f4860a-development.zip`.
- Size: 15,280,424 bytes. SHA-256: `F0D5FD671CE46408826ED82A2BCD5696E3B86A6B708F65F0F236ADD1072CD44F`.
- Installed beneath `D:\Games\GOG Games\Caesar 3`; previous assets retained as `assets.before-d16-719f4860a`.
- Archive paths were validated before extraction. Generated Julius/Augustus graphics remain in the installation's Mods tree, never the repository's authored Mods directories.

## Save boundary

The shared ancestor used 0xaf. Upstream now uses SVX 0xbd (189), while this fork writes SVV 0xc9 (201). Their post-fork numeric versions are not compatible schema identifiers.

The file and save-info paths explicitly reject `.svx` files newer than 0xaf before native piece decoding. This is a temporary guard, not the S01–S13 converter or a completed source-family detector: renamed files, raw buffers and ambiguous archive origins still require the SB01–SB09 work in the main ledger. The LoadSave DLL currently reads archive bytes; its ABI test does not establish foreign-schema conversion.

Native saves retain historical readers. Gifts and accounting are appended behind separate fork version gates. Existing completed arches remain loadable; imported history starts as unknown/partial rather than invented transactions. Current writes remain atomic and original foreign inputs are untouched.

## Validation evidence

- Release x64 game, launcher, StartupParserTest, ModContentTest and LoadSaveAbiTest builds pass.
- Full startup gate: **70 load/roundtrip/3,000-tick render-soak cases**: 67 Vespasian saves, Julius/Augustus dependency cases, and original campaign 14 SAV. `out/d03-d07-startup.stdout.log` ends with `Startup parser test passed`. Expected old-save relationship repairs are logged and counted; rewritten saves and their soaks pass the strict warning/error checks. Renderer fallback checks remain failure conditions. Deliberately invalid parser fixtures also produce expected diagnostics; this is not a claim that the aggregate stderr file is empty.
- Final focused deployed-build run: `out/catch-up-test.stdout.log`, empty stderr, 3,000 ticks and native 0xc9 roundtrip. Exercises nonzero accounting totals/aggregation, eight-year rollover, byte-stable history serialization, terrain-count updates, station demand/atomic consumption/resource acceptance, station graphics, individual arch phases, Rome convoy/architect ownership and reservation cleanup, reward eligibility, and six new XML UI renders.
- Final No Monuments/live-settings run: `out/no-monuments-final.stdout.log`, empty stderr, 3,000 ticks and native roundtrip on Consul. Effective settings are changed and restored repeatedly; population and treasury are unchanged. The user's mod list is restored byte-for-byte after the test.
- ModContentTest: **863 composed definitions**, **20 instant-monument cost contracts**; non-concrete costs retained and concrete production disabled. The unresolved station/workcamp policy is separate from these passing content contracts.
- Native launcher self-test passes empty/add/remove/reorder/missing/dependency/selection/persistence/override/grouping checks. `out/launcher-test/launcher-tests.txt` records the result.
- Load/save DLL ABI contracts pass. All five deployment-helper tests pass.
- Unsupported upstream 0xbd SVX header is rejected with the explicit foreign-converter diagnostic and nonzero exit; `out/foreign-save-guard.*.log` records this negative check.
- UI review captures: `out/catch-up-ui/*.jpg`, generated from installed-game BMP renders. Navigation labels use the supported literal character path. Manual multilingual, input-device and extended-camera interaction review remains part of user acceptance.

The full 70-case gate preceded the last focused fixes; the final binary then passed the focused catch-up and live-settings/No Monuments regressions above. Rome convoy tests cover ownership, reservations and lifecycle, not a dedicated end-to-end city-entry-to-completed-arch fixture. New upstream SVX fixtures have not been converted or soaked.

## Git/review state

HEAD remains `16fb9319828448589a11180c4727fbed2c7034a0`; target remains `719f4860a6b57617fa323e84488e603e34d66911`. No cherry-pick, commit, merge or ancestry marker was made. The graph remains 264 ahead / 204 behind. This is a feature slice in a dirty working tree, not a contiguous chronological upstream prefix. Mixed commits retain their independent unresolved portions in the main ledger; user review and authorization are required before recording integration history.
