# D08 and UI revision review record

Status: implemented working-tree review slice; final validation results are recorded below. No commit, merge or ancestry marker is authorized. D09 is the next gameplay decision. Open compatibility work is identified separately from implemented native capabilities.

## Binding decisions

Scenario actions override composed mod data for the current scenario. A new scenario resets the overlay; a saved city restores its own overlay; changing mod settings recomposes defaults and restores the active city's overlay above them. Highway Stations consume the global stockpile through generic building consumption data. Julius retains its own gameplay, UI and original extracted assets. Android, iOS and Switch 2 remain eligible targets under the hardware baseline.

## Implementation

| Area | Owners and behavior |
| --- | --- |
| Scenario definitions | `src/scenario/definition_overrides.*`, model/production owners: keyed housing requirements/capacity, construction resources, migration multipliers, hidden routes, route costs, city names and scenario text. Housing and construction selectors use active definitions. The reset/capture/load sequence preserves unrelated mod defaults. Native SVV 203 (0xcb), scenario 25, has separate gates from Augustus versions. |
| Event actions | Native actions for route locking, housing, goals, camera, weather, route visibility, variable colors, migration, construction, city names, resource opening costs, area figure removal and warnings. Generic owners replace upstream building/figure enumeration assumptions. Existing production-rate set/add arithmetic is verified, with saturating addition. |
| Event conditions | Enemy and trade disruption state, festival timing, area desirability, housing/population and figure-category conditions. Area selection stays native grid corners; camera uses one position. New time formulas preserve the chosen deadline and once-per-initialization random timing of older native saves. |
| Editor | Installed original map-editor support; fixed initial invasion-sidebar access, MAPX header handling and native scenery identities. New actions have metadata, selectors and descriptions. Direct housing/construction dialogs edit the same scenario overlay. Demand changes have date/resource/amount/direction/city columns. Copy/duplicate clones formulas and texts, preserves condition groups and assigns destination parent IDs. Clipboard data is invalidated by scenario replacement to avoid reusing foreign runtime IDs. |
| Empire XML | Button icons are bounded; encoded city names use UTF-8 without truncation; explicit zero denarii prices survive import; resource opening costs and hidden-route flags round-trip. Resource cost checks charge warehouses/granaries atomically through existing resource owners. Import metadata queries do not rebuild the live empire. |
| Legacy demand actions | Demand increases from zero update both route limits and the city's resource availability, refresh menus and enable dock resources. Old buys/sells inference waits for the newly loaded empire instead of reading the previous scenario. |
| In-game settings | `src/window/config_mod_settings.h` builds the existing General/UI/Difficulty/City Management pages and subcategories with a Hardcoded settings header, followed by each owning mod. Boolean and bounded integer controls apply immediately; overwritten options stay disabled with reasons. Pending hardcoded edits survive mod-definition reloads. Fullscreen state refreshes when reopening. |
| Launcher | Native page/category navigation, complete current hardcoded catalog, mod sections, checkboxes and bounded value/slider controls. Double-click adds/removes mods. Per-monitor DPI resizes windows, columns and number dialogs. Launching the game preserves its debugging console. |
| Vespasian citizens/dogs | All 22 housing definitions retain their existing beggar groups and use a named ambient group referencing Augustus's setting through `$[Augustus:WANDERING_CITIZENS]`. Vespasian no longer requires global labor for ambient citizens; Augustus retains its own upstream eligibility. Dogs and citizens have authored logical-96 graphics overrides. No new bitmaps or extracted groups were added to source. |
| Julius UI ownership | Julius financial/religion/empire layouts use original extracted icons and original string IDs. Augustus owns accounting, epithets, durability, city-service and empire-sidebar windows. Extra sidebar and housing-advisor controls are enabled by mod UI capabilities, not mod-name checks. Identical locale definitions inherit instead of being duplicated. |
| Highway Station | `BuildingCityService` and its XML source option consume directly from the global stockpile. The workcamp prerequisite is removed for that mode, resolving the No Monuments conflict. |
| Save origin SB01–03 | Immutable archive identification covers native, shared legacy and five upstream piece layouts before city mutation. Explicit origin cannot bypass structural validation. File, raw-buffer, campaign and preview paths share it. The generated inventory records 68 producers, five ordered layouts and 666 unique record/header declarations with exact Git identities. A producer identity is not a semantic converter. |

## Compatibility limits

See [scenario editor compatibility report](vespasian_scenario_editor_compatibility.md). SB04 foreign SVX semantic conversion remains open. New upstream SVX files are identified and rejected before native decoding, even when renamed; they have not been converted or soaked. Same-version upstream event enum collisions require producer-aware conversion, not native version-number heuristics.

The native event/editor feature slice is implemented, but `17b05668b` remains a mixed row: foreign MAPX/SVX conversion, non-English reconciliation and the documented native semantic differences are not certified. D09–D14 remain unapproved. Other explicitly unaudited rows in the 204-commit ledger are not considered completed merely because this slice exists.

## Validation record

- Release x64 game and standalone parser builds pass (`out/d08-release-build.log`, `out/parser-final-build.log`), and the installed executable matches its SHA-256. Later source edits were indentation in the religion controller and the standalone parser's no-UI cache-invalidation boundary; the latter was rebuilt successfully.
- Full startup gate passes **70 load/roundtrip cases, 3,000 rendered frames each** (`out/ui-startup-gate-final.stdout.log`). The older-production-table rejection is fixed. Original relationship repairs are individually logged during canonicalization; rewritten saves and their soaks meet the warning/error and renderer-fallback thresholds. Deliberate invalid parser fixtures still produce expected diagnostics in aggregate stderr.
- The full gate preceded the last demand-direction, XML and advisor refinements. The final deployed binary then passed the focused checks below; it is not claimed that all 70 cases ran again on that last binary.
- Final deployed Consul run (`out/d08-release-contracts.*.log`): scenario override precedence/save/reset/restore, production set/add/zero behavior, five archive-origin layouts and adversarial cases, 64 dog frames/27 road transitions, 96 citizen frames/27 road transitions and Vespasian scaling, accounting/gifts/global consumption, live mod-setting changes preserving pending hardcoded edits, and 3,000 rendered frames. Stderr is empty.
- Final deployed older native save 175 and original Caesar III save 102 (`out/d08-final-legacy.*.log`): temporary canonical copies, strict reload, 3,000 frames each, resave/reload. Known migration repairs are logged only during initial canonicalization. Where a legacy production array cannot be identified, it is consumed and mod defaults retained with an explicit warning; the next keyed save is clean.
- Final deployed Julius (`out/julius-deployed.*.log`): financial/religion/empire/settings renders and 3,000 frames, empty stderr. Visual review verifies original advisor icons, the 13-button bar, original finance rows, legacy god order and advice matching the correct god identity. Captures are under `out/ui-window-review` and the installation's matching output folder.
- Final deployed editor (`out/editor-deployed.*.log`): startup/map/attributes/models/events, all 13 new action dialogs, every custom-variable color, demand activation in both directions, empire XML button icon/UTF-8 name/zero price/resource price/hidden-route roundtrip, seven native scenery identities and MAPX event roundtrip. Stderr is empty. Captures and temporary scenarios are under the installation's `out/editor-review`.
- ModContentTest passes **865 composed definitions**, qualified-setting gating and **20 instant-monument cost contracts** (`out/mod-content-final.*.log`).
- Launcher self-test passes complete hardcoded category/grouping, double-click add/remove, selection/dependency/persistence behavior, disabled settings and **96/144/192 DPI** layouts, including number dialogs (`out/launcher-final/launcher-tests.txt`).
- `git diff --check` has no whitespace errors. No user save was overwritten; authored-data deployment preserved extracted graphics. No commit/merge/ancestry change was made.


## Git state

HEAD stays `16fb9319828448589a11180c4727fbed2c7034a0`; target stays `719f4860a6b57617fa323e84488e603e34d66911`. The 204-row ledger tracks partial and unaddressed commits explicitly. User review and subsequent authorization are required before committing or recording upstream ancestry.
