# Augustus Manual Sync Ledger — 2026-09-05

## Scope and review state

**D03–D08 and D15–D16 have implemented native review slices, with mixed compatibility work still open.** See [implementation and validation record](augustus_sync_d03_d07_plan.md) for exact owners, tests, limitations and the earlier No Monuments/workcamp decision. D08 native capabilities and UI revisions are implemented for review in [the current slice](augustus_sync_d08_plan.md), with the [editor compatibility gaps](vespasian_scenario_editor_compatibility.md) explicitly open. D09–D14 and semantic foreign-save conversion remain open. No Git commit, merge or ancestry adjustment has been made; the graph is still 204 commits behind. Unannotated rows remain proposals or unaudited work, not completed ports.

| Snapshot | Value |
| --- | --- |
| Working branch | `Catching-Up-to-Commits` |
| Fork HEAD at audit | `16fb9319828448589a11180c4727fbed2c7034a0` |
| Upstream | `https://github.com/Keriew/augustus.git`, `upstream/master`, fetched 2026-09-05 |
| Pinned upstream target | `719f4860a6b57617fa323e84488e603e34d66911` |
| Common ancestor / previous integrated target | `caa61f5ceca8cf19e1caa785e9af2a55859c52cb` |
| Exact queue | `16fb93198..719f4860a` — **204 reachable commits**, **203 first-parent commits** |
| Initial graph count | `git rev-list --left-right --count HEAD...upstream/master` = `264 204` |
| Previous ledger | [April manual sync ledger](archive/augustus_sync_2026_04_21_ledger.md), including its April 24 follow-up and May 2 note |

The commit table below covers the complete reachable queue, including the side-parent controller commit and its merge. It does not substitute first-parent counts for Git's behind count. Commits are listed in `git rev-list --reverse` order. A fix series should usually be implemented at its final reviewed behavior rather than replaying each intermediate bug and revert.

The old ledger is historical reference, not proof of present parity: the fork has since replaced building, figure, routing, save, graphics, UI, and mod ownership in many areas. This pass inspected the commit inventory and changed paths, examined feature and mixed-purpose diffs, and compared important fork contracts and implementations. **It is not a completed hunk-by-hunk port audit or a claim that all proposed equivalents have passed behavioral tests.** The implementation pass must inspect every relevant hunk and close each row with evidence.

## How to read and close a row

| Action | Meaning |
| --- | --- |
| **Port** | Preserve the upstream behavior through the fork's current C++ owners and mod definitions; a literal cherry-pick is generally inappropriate. |
| **Data** | Candidate for content-only integration through logical graphics references, XML, localization JSON, or licensed source audio. Still requires schema, provenance, and runtime checks; “free” does not mean unverified. |
| **Keep ours; audit** | Retain a demonstrably different fork architecture or policy and check the upstream regression against it. This does **not** yet mean “already fixed.” |
| **Fold** | Review this commit's delta as part of the named final feature/fix series; keep its separate row for traceability. |
| **Skip proposed** | Deliberately omit an inactive platform, dependency, or superseded change, with the reason recorded. |
| **Decide Dxx** | User-facing choice remains open in the decision register. Dependent rows inherit that choice. |
| **Implemented; review pending** | Working-tree implementation exists with linked evidence. This does not close unresolved mixed hunks, foreign-save conversion, user acceptance or Git ancestry. |
| **History** | Empty/merge commit: inspect its tree delta and record ancestry without duplicating already accounted work. |

After review, replace each proposed action with the agreed disposition. During implementation add the local implementing commit(s), exact affected owner/data paths, and validation evidence to each row (or link to a shared validation record). Terminal results should be `ported + verified`, `equivalent + verified`, `intentionally omitted + reason`, or `superseded + final replacement verified`. A decision is not an implementation result. No `Decide`, unaudited equivalent, unexplained omission, or unresolved regression may remain at sign-off.

## Settled direction from the user

- **Force placement:** retain our Shift behavior. Do not adopt upstream's automatic vegetation-clear option or its replacement placement policy (`cd5d90a33`). Still inspect the later waterfront bug fix (`8faf80ad3`) for a regression that also affects Shift placement.
- **Beggars:** retain the existing unemployment-driven system. Its relationship to ambient plebeians needs a design decision (D01), rather than blindly adding a parallel hardcoded spawn path.
- **Dogs:** pursue a mod-data implementation first. The user's suggestion is a direction, not evidence that every required capability already exists. D02 records defaults and spawn semantics still to decide.
- **Integration:** implement approved decisions in reviewable slices on this branch. The user validates each slice before authorizing its commit and ancestry accounting, and will manually merge the completed branch into `master`.

## Decision register — for the user's review

Decision status: D03–D07 and D15–D16 were approved by the user on 2026-09-05 with the binding directions below. D01–D02 have an implemented launcher/mod-settings slice documented in `launcher_and_mod_settings_plan.md`. D08 is approved: scenario actions override mod data for the current scenario, reset on a new scenario, and restore their own saved overrides. D09–D14 remain open. Implementation evidence is recorded separately from decisions; no upstream commits are considered merged until the user validates the slice and explicitly authorizes the Git commit and ancestry adjustments.

### Approved implementation directions (2026-09-05)

- **D03:** match Augustus upstream Highway Station behavior using generic, explicitly data-owned functionality. Vespasian inherits it for now; its highway redesign is deferred.
- **D04:** match Augustus's staged arch construction through the construction definition. Externally supplied labor/materials belong to a parameterized monument-gift event capability usable for any monument.
- **D05:** retain SDL2 and port relevant controller mappings, input and safety fixes. Supported machines require 64-bit architecture, Vulkan support, at least 4 GB RAM and 1 GB VRAM. Android, iOS and Switch 2 remain potential targets on qualifying hardware. Retire Vita, original Switch releases and 32-bit targets only; retain reusable Switch groundwork. Omit SDL3 and irrelevant dependency churn. See [corrected platform scope](platform_scope.md).
- **D06:** integrate temple epithets in the existing religion UI, migrating the affected UI to native declarative XML windows with only data bindings and callbacks in code.
- **D07:** adopt complete trade, production and consumption accounting for this architecture. Migrate affected UI surfaces to declarative XML windows using the existing `UI/windows` system.
- **D15:** accepted recommendations. Preserve existing visual/default choices where the table did not prescribe a new one; optional message pruning and extended bounds start off, and fixed-rank campaigns remain fixed unless authored otherwise.
- **D16:** adopt final licensed inherited assets and preserve Vespasian overrides. Download the latest distributed development asset pack linked by upstream GitHub into the installed game and run the current extractor there. The previous pack was `assets-4.0.0.1305-58a0592fb-development.zip`.

Current slice: implement approved capabilities and applicable fixes, then stop at **D09** for wildlife/criminal gatehouse policy. D08 was approved on 2026-09-05. Do not implement later unresolved decisions merely because their commits occur earlier chronologically.

### D01 — Ambient plebeians and existing beggars

Upstream `5b24bc2e0`, `10b44c769`, and `dd8b684a0` add optional aimless plebeians, limit them to occupied plebeian housing/global-labor mode, and fix spawn frequency and vacant-lot behavior. Our [gameplay divergence notes](gameplay_divergences_from_augustus.md) describe XML-owned beggar spawning with different Augustus/Julius and Vespasian unemployment policies. Current `Mods/Augustus/FigureType/beggar.xml` uses `transient_wanderer` and `stand_still`, whereas the inherited patrician uses `roaming_service` and `vanilla_roaming`; sharing infrastructure does not make their behavior identical.

**Decide:** add a separate ambient citizen using the existing generic walker machinery; broaden beggars into a combined ambient/unemployment role; or retain beggars alone? If adding citizens, should they appear under both local and global labor or only upstream's global-labor mode? **Recommendation:** separate mod-defined ambient citizens in both modes, keep unemployment meaningful, and author modest per-house chances with an explicit active-walker limit. Confirm spawn slots do not crowd out service, beggar, or patrician walkers.

### D02 — Dogs and the hidden patrician change

`6f076248f` adds house-spawned road-roaming dogs and barking. It also changes patrician delay from 40 to 16 and removes the one-per-generation citywide throttle. Those are separate behaviors. Our inherited patrician profile now has `owner slot="none"`; archived descriptions of slot ownership must be checked against live house spawn definitions.

**Decide:** which mods enable dogs by default, what density/active cap to use, and whether to retain our current patrician frequency? **Recommendation:** optional dogs in Augustus and Vespasian, Julius behavior unchanged, retain current patrician tuning. Start with `FigureType` + house spawn XML + logical dog graphics/audio. Check naming/registration, combat classification, death/owner removal, save identity, sound/phrase selection, and option gating before claiming zero code changes. If an engine gap exists, propose the smallest generic extension rather than `FIGURE_DOG` switches.

### D03 — Highway Station

`0505be87f` adds one station per city, requires a workcamp, consumes stone and sand based on highway extent, and halves highway construction cost and monthly levies while operational. `9092fb526`, `0e0ac5fd4`, and `a25f80667` fix operation, price preview, and visible stocks.

**Decide:** adopt the mechanic in Augustus and Vespasian, Augustus only, change the mechanic, or omit? **Recommendation:** adopt through building/production/storage/finance definitions and owner notifications; author Vespasian consumption against its longer calendar rather than transplanting fixed upstream tick or resource constants. Do not implement upstream's city scan as the permanent source of road totals if our owners can publish changes.

### D04 — Rome-supplied triumphal arch construction

`9dedfb2a0` changes the awarded arch from immediate placement to staged construction; Rome supplies workers and materials free. Local `Mods/Augustus/Tiles/triumphal_arch.xml` currently has no monument construction module.

**Decide:** adopt the Rome-supplied stages, keep instant construction, or require local resources? **Recommendation:** adopt Rome-supplied stages for Augustus/Vespasian through our construction modules. Preserve award eligibility, road passage, rotated footprint, and already-completed arches on import. Local-resource construction would be a deliberate economic divergence.

### D05 — SDL3, controllers, and platform scope

`3eba78982` adds a second platform backend and broad packaging changes. Our root project explicitly uses SDL2 2.32.10, SDL2_mixer, SDL2_ttf, and `ext/easyav1_dummy`; renderer ownership is different. April's ledger intentionally omitted inactive external platform/CI maintenance.

**Approved, corrected scope:** retain SDL2 and port relevant controller, input, DPI and logging fixes. Platform eligibility follows the hardware baseline, not the currently validated Windows build. Android, iOS and Switch 2 remain on the table; Vita, original Switch releases and 32-bit targets are excluded. Android/iOS files and shared hooks have been restored. Legacy Switch adapters are retained as reference, with old packaging disabled. Mobile/Switch 2 build and device validation remain open; see [platform scope](platform_scope.md). SDL3-only changes can still be omitted for the dependency reason, independently of platform eligibility.

### D06 — Temple epithets reference panel

`56f90fa7f` adds a religion-advisor panel explaining grand temple/Pantheon bonuses, with UI and Asian line-height fixes afterward.

**Decide:** add a separate panel, integrate the reference into our existing religion UI, or omit? **Recommendation:** integrate equivalent information into our current UI and source descriptions from active mod definitions/localization, preserving the fork's religion design. Do not assume upstream fixed god indexes describe every mod.

### D07 — Trade ledger and empire UI

`ecf4278d1` adds trade/production/consumption accounting, historical data, and trade/empire/sidebar UI; its two explicit follow-ups correct consumption and historical display. It touches transaction producers and persisted history, not just a new window.

**Decide:** adopt the full accounting feature with our UI, a smaller current-period view, or omit? **Recommendation:** full feature through authoritative resource/finance transactions and existing declarative/shared UI. Account once at the actual transfer or consumption, preserve fork resources and calendar, and distinguish unknown pre-import history from measured zero. Keep this independent of cosmetic empire panels.

### D08 — Second event editor and scenario model overrides

`17b05668b` adds actions for house models, goals, camera, weather, route visibility/locking/prices/resources, variable colors, immigration percentage, monument requirements, city naming, killing area walkers, and warnings. It adds conditions for enemies, trade disruptions, festival timing, desirability/population/figures in areas; formula/text storage, copy/paste, empire editing, and UI changes accompany them. Migration fixes follow repeatedly.

**Approved:** adopt native capabilities through per-scenario overlays above mod definitions. Reset on a new scenario, restore on loading its save, and preserve the overlay across live mod-setting recomposition. Native implementation and tests are recorded in [D08](augustus_sync_d08_plan.md). Foreign binary/XML dialect conversion and other editor gaps remain explicit; this mixed commit is not fully closed.

### D09 — Wildlife, criminals, and gatehouse rules

`f16c7020e` blocks wolves at gatehouses and makes rioters attack them; it also enables lighthouse access with docks in legacy allowed-building conversion. `e09631256` and `4d7e8ff53` change wolf routing around elevations/rubble/bridges. `f5d2669b0` uses thief graphics for robbers/looters instead of the riot figure.

**Decide:** adopt those combat/pathing rules, and adopt the thief appearance? **Recommendation:** adopt the intended rules via PathingMode/UnitType/figure graphics data, retaining the new formation runtime. Treat wolf traversal fixes separately from intentional gate blocking. Integrate lighthouse legacy availability with D10, not as an unnoticed combat side effect.

### D10 — Lighthouse placement

`73e8983ec` requires nearby open water rather than any nearby water. Existing saves must not lose their lighthouses.

**Decide:** adopt the stricter rule for new placement, or retain current foundation rules? **Recommendation:** adopt in Augustus; choose Vespasian's rule explicitly through its foundation data. Preserve existing placed structures and check irregular coastlines, low bridges, river exits, rotation, and the legacy dock/lighthouse availability change in D09.

### D11 — Towers without prebuilt walls

`b20d57491` permits towers on valid bare ground and charges for missing wall tiles; later commits add overlap and rotated-footprint checks.

**Decide:** allow automatic supporting walls or continue requiring prebuilt walls? **Recommendation:** allow through the same atomic foundation/construction plan used for price, preview, placement, undo, and repair. This must not weaken the settled Shift clearing policy or permit overlapping buildings.

### D12 — Bridge demolition and retreat speed

`46e9537f3` prevents demolition while more than three invading enemies remain, tightens occupied-bridge restrictions, improves fleeing enemies, and adds an option to retreat twice as fast.

**Decide separately:** citywide invasion demolition lock versus only locally occupied/threatened bridges; and normal versus optional double-speed retreat. **Recommendation:** preserve our bridge/formation ownership, adopt safe fleeing behavior, make the demolition policy explicit in mod/config data, and leave double-speed retreat optional/off until checked against our movement timescale. The arbitrary upstream threshold should not silently become a fork rule.

### D13 — Economy and efficiency

`b342bfe77` raises the fixed worker percentage to 45. `f13c7d65e` makes both mint conversion directions follow denarii production speed. `2b8d428a6` rounds production averages and computes displayed efficiency using **102 × output / expected**, not 100; it also adds housing/population sidebar information. `984228b76` independently fixes “set” versus “add” in production-rate events.

**Decide separately:** fixed-workforce percentage by mod; whether both mint recipes share one rate; and whether to adopt the 2% display allowance. **Recommendation:** preserve Vespasian demographic/workforce/recipe balance, set Augustus compatibility defaults where applicable, fix event arithmetic and genuine rounding drift, and keep truthful 100-based efficiency rather than silently inflating it. Port housing/population information independently of the efficiency choice.

### D14 — Shallow water

`aa9f31ab4` adds editor-painted shallow water and blocks ship routing through it. This introduces a new terrain bit and scenario/save format changes; it is not simply new water artwork. Current `src/map/terrain.h` has 21 terrain bits and no shallow-water flag.

**Decide:** add shallow water with upstream's non-navigable semantics, choose other traversal rules, or omit? **Recommendation:** adopt the editor capability and upstream ship exclusion, with explicit terrain serialization and weighted-route invalidation. Decide any special bridge/ford/walker behavior separately rather than inventing it during the port.

### D15 — Presentation and convenience defaults

The following can be reviewed independently; recommending the feature does not decide its default for every mod.

| Choice | Commits | Recommendation / question |
| --- | --- | --- |
| Shoreline/elevation desirability and new colors/radius styling | `e76f61bd7`, `e602fc2f6`, `e41be213f` | Show accurate terrain contributions; retain fork palette unless the new palette is preferred. Do not change economic bonuses incidentally. |
| Defensive durability and enemy-overlay health bars | `c82bb8687`, `01f774b39`, related UI fixes | Adopt; derive health from actual damage/destruction rules and composed footprints. Confirm desired bar visibility. |
| Per-climate grid colors | `ce2bca95c` | Add configurable colors through our renderer; preserve current defaults initially. |
| Empire sort persistence / auto-cycle categories | `69c698276` | Adopt persistence and independent category settings; preserve existing defaults unless selected otherwise. |
| Automatic deletion of common messages after five years | `9949a5aad` | Add optional/off by default; protect important messages and use calendar years. |
| Extended camera bounds | `ae8c92165`, `494425b8b` | Optional/off initially; adapt to our camera and picking instead of copying pixel bounds. |
| Scrollbar appearance | `e59ca3c3d`, `1dcac2922`, `c2747bc4a` and assets | Keep our widget behavior; choose upstream new skin, current skin, or an optional skin. Include thumb correctness fixes whichever skin wins. |
| Rain/snow/wind and city ambience | `427e4ae5b`, `50e257d8e`, `2a6baab64`, `df314cd32`, `d2bfabc5e`, audio rows | Adopt corrections and content through existing renderer/audio owners; confirm preference for wind during otherwise clear weather. |
| Willow ornamental tree | `381449f16`, `f3fef1a34`, `b38e4680c` | Add as mod data with correct info/menu/graphics; no hardcoded building-range expansion. |
| Campaign rank inheritance | `6b6d84e18` | Adopt as an author-selected campaign feature; fixed-rank missions remain fixed. |

### D16 — Asset update policy

There are many source-art revisions, including terrain, industry, native buildings, temples, UI, thief/dog sprites, and audio. **Decide:** take final upstream revisions for inherited Augustus content, or name appearances that should remain pinned? **Recommendation:** adopt final licensed source revisions for inherited content, preserve Vespasian-owned overrides, and process them through the existing extractor. Do not claim every bitmap is safe or mechanically conflict-free merely because the commit says “Update assets.” Inspect source provenance and authored overrides at integration.

The [graphics extraction contract](graphics_extraction_pipeline.md) and repository boundary are mandatory: no Caesar 3 graphics or bulk generated Augustus output in `Mods`. Runtime extraction belongs under the installed game's mod graphics directories or ignored `extracted_graphics_sample`. Prefer direct logical-group references; no duplicate wrappers. Editable masters belong in `res/graphics_source`. A graphics refresh is unfinished until the current extractor and actual runtime consumers pass zero-fallback validation.

## Commit ledger

Every action below is **proposed**, except the explicitly settled Shift policy. All rows are **NOT STARTED** as implementation/verification work. `Dxx` references inherit the open decision above. The hash identifies the exact upstream commit and can be inspected with `git show <hash>`; do not infer its entire payload from the original subject.

### April and May (through the SDL/platform transition)

| Commit | What it added or changed | Agreed/proposed action | Why / integration notes | Implementation status |
| --- | --- | --- | --- | --- |
| `016d5254c` | Controller automatic bindings and Android controller support. | Adapt to SDL2 controller mapping and Android behavior; D05 | Existing automatic bindings retained; controller handle safety, SDL game-controller subsystem initialization and Android enablement implemented. Mobile device validation remains open. | Implemented adaptation; Android validation pending |
| `5ff7e3d24` | Hunting-lodge animations and stored game-meat artwork. | Data; D16; final pack installed/extracted | Refresh inherited industry assets through extraction; preserve authored fork graphics. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `a1b14e6f7` | Image creation/packing type and allocation fixes. | Keep ours; audit | Compare image sizes and overflow behavior in our image/extractor owners; do not restore obsolete C image ownership. | Unaddressed / audit pending |
| `90a7a9c11` | Integer-overflow fixes in image packer arithmetic. | Port applicable safety fixes | Check surviving packer and extraction arithmetic, including large images; C++ conversion alone is not proof of safety. | Unaddressed / audit pending |
| `537e15ca0` | Granary/forum animation offsets and granary animation metadata. | Keep ours; audit + Data | Preserve XML offsets and BuildingAnimation/native draw contracts; visually check stock/animation alignment. | Unaddressed / audit pending |
| `395b9f38d` | Merges the controller branch; first-parent delta is 18 joystick lines. | History + Fold into D05; approved portion implemented, review pending | Account for merge resolutions as well as `016d5254c`; do not apply controller behavior twice. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `427e4ae5b` | Less artificial rain rendering. | Port; D15; approved portion implemented, review pending | Adapt final weather-series behavior to the current graphics backend. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `5b24bc2e0` | Optional occupied-house ambient plebeians. | Implemented D01 through mod-owned spawn policies; review pending | Existing beggars are unemployment-driven; implement chosen ambient role via generic profiles. See [D08/UI regressions](augustus_sync_d08_plan.md). | Native policy and Vespasian local-labor regression fixed; foreign-save work open |
| `a2bab8c9e` | AppImage/CodeQL dependency installation fix. | Re-audit restored AppImage/CodeQL dependency scripts; D05 | Shared Linux packaging is retained. Inspect relevance to the current dependency/build configuration; Windows-only scope is not an omission reason. | Unaddressed / audit reopened |
| `00d6860d8` | Editor localization, Chinese encodings/translations, and Chinese font payloads. | Port + Data | Convert catalogs into JSON and check current font/encoding support; not a translation-only cherry-pick. | Unaddressed / audit pending |
| `9b18c25ac` | Dog portrait and initial directional walk frames. | Data; D02/D16; final pack installed/extracted | Use logical extracted dog groups if dogs are selected; no generated sprite dump in Mods. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `56f90fa7f` | Temple/Pantheon epithets reference advisor. | Approved D06; implemented approved portion, review pending | Integrate content with our religion/UI architecture. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `50e257d8e` | Weather rendering adjusted to settings, including config defaults. | Fold into weather; D15; approved portion implemented, review pending | Review settings changes separately from visual fixes; use the final weather behavior. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `58a0592fb` | Epithets layout and Russian text corrections. | Fold into D06 + Data; runtime counterpart implemented; translation reconciliation pending | Include final layout/localization if the feature is adopted. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `10b44c769` | Plebeian spawning cadence and eligibility fixes. | Implemented D01 through mod-owned spawn policies; review pending | Desired spawn policy must be authored for our time/house ownership model. See [D08/UI regressions](augustus_sync_d08_plan.md). | Native policy and Vespasian local-labor regression fixed; foreign-save work open |
| `fea55b845` | Highway Station graphics/XML and more dog directions. | Data; D02/D03/D16; implemented approved portion, review pending | Two feature dependencies in one asset commit. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `6516d83a5` | Asian-language rich-text line-height handling in epithets. | Port relevant text fix; D06; implemented approved portion, review pending | Use our font metrics; validate CJK layout even if information moves to another panel. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `b2b05d726` | Demand/resource activation from zero and related demand import/editor fixes. | Ported native behavior; review pending | Zero-to-positive limits activate city resource flags and docks. Legacy direction inference waits for the newly bound empire; editor contract verifies both directions. [Evidence](augustus_sync_d08_plan.md). | Implemented; native contracts tested |
| `dd8b684a0` | Prevent ambient plebeian generation at vacant lots. | Implemented D01 through mod-owned spawn policies; review pending | Occupancy and valid owner checks belong in the chosen spawn contract. See [D08/UI regressions](augustus_sync_d08_plan.md). | Native policy and Vespasian local-labor regression fixed; foreign-save work open |
| `33316b4d7` | Dog southeast frames plus dog bark and hawk audio. | Data; D02/D16; final pack installed/extracted | Audio provenance and sound binding are separate from graphics extraction. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `69847ce5e` | Game-meat carts/panels and final dog direction/XML bindings. | Data; D02/D16; final pack installed/extracted | Final logical groups should cover the full direction set; verify resource-cart consumers. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `3eba78982` | SDL3 backend, SDL2 path split, platform APIs and broad build changes. | Approved D05; approved portion implemented, review pending | Do not replace our renderer/platform architecture by replaying upstream's directory move. Audit mixed runtime changes separately. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `2f577a6e4` | Windows ARM64 release deployment fix. | Intentionally omit obsolete platform/dependency delta; D05 | Upstream packaging script, not the root x64 build. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `562300a15` | Initial Android SDL3 integration workaround. | Omit superseded SDL3-only workaround; D05 | Android is retained, but this intermediate SDL3 workaround is superseded and the project stays on SDL2. | Addressed: intentionally omitted SDL3-only change |
| `5bb85d5ec` | Revised Android initialization/build workaround. | Re-audit Android initialization/build changes against SDL2; D05 | Android is an eligible target. Separate transferable fixes from superseded SDL3 integration. | Unaddressed / audit reopened |
| `589efb113` | SDL3 Android back-button hint timing and SDL-version logging. | Skip SDL3 specifics; audit logging under D05; approved portion implemented, review pending | Preserve useful startup diagnostics in our platform owner if absent. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `21cfcd6d6` | Android manifest back-button behavior. | Adopt manifest opt-out of predictive back callback; D05 | Keeps Android back events on the restored SDL2 keyboard/back-button handling path. Android device validation remains open. | Implemented; Android validation pending |
| `3596e1b6d` | Gradle/project updates and SDL3 Android event handling. | Re-audit mixed Android build/runtime changes; D05 | Retain relevant Gradle and Android event behavior while omitting incompatible SDL3-specific implementation. | Unaddressed / audit reopened |
| `9c3374492` | Android APK/AAR Gradle resolution fixes. | Re-audit Android APK/AAR resolution fixes; D05 | Android packaging is restored; compare final upstream resolution behavior with the retained SDL2 project. | Unaddressed / audit reopened |
| `83c1ed5bb` | Southern native large-monument image tweak. | Data; D16; final pack installed/extracted | Inherited source-art refresh through extraction. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `68b6c312b` | Final Android installer Gradle fix. | Re-audit final Android installer Gradle fix; D05 | Android remains eligible; account for the final fix rather than replaying superseded intermediate attempts. | Unaddressed / audit reopened |
| `6f076248f` | Dogs, barking, residential spawning restructure, and faster patricians. | Implemented D01/D02 data profiles; retain explicit cadence audit | Prefer mod data; explicitly retain or change patrician cadence instead of importing it incidentally. | Dogs/citizens have Vespasian logical-96 XML overrides; road-wander contracts pass; mixed-hunk audit open |
| `dd599b9f7` | Figure phrase metadata/refactor, including depot and figure-info consumers. | Keep ours; audit then port missing behavior | Our figure profiles/phrases must preserve sound/text selection without reintroducing type switches. | Unaddressed / audit pending |
| `1de00fae9` | Follow-up phrase refactor. | Fold into phrase series | Implement final phrase behavior once; retain our ownership. | Unaddressed / audit pending |
| `c2073190e` | Gray coverage for inactive fountains. | Keep ours; audit | Our water-access runtime distinguishes active supply and authored preview; verify live inactive coverage semantics. | Unaddressed / audit pending |
| `8dd168d7a` | Phrase/type corrections after refactor. | Fold into phrase series | Include fixes for the adopted walker set and save identities. | Unaddressed / audit pending |
| `2f9aa4c74` | Restored hover submenus and multilevel overlay navigation. | Port | Preserve current shared UI behavior, correct hover/focus/submenu transitions. | Unaddressed / audit pending |
| `76b820f05` | Correct granary-boy audio selection. | Port through phrase series | Observable sound mismatch; validate current profile mapping. | Unaddressed / audit pending |
| `b803bb6fd` | Phrase build fix and CI script adjustment. | Port relevant compile fix; skip inactive CI | Mixed payload must be split; no obsolete C API restoration. | Unaddressed / audit pending |
| `bbb5d428c` | Four rotation issues across placement, save/repair state, and tooltips. | Keep ours; audit and port gaps | Check composed footprints/orientation and old-save hydration, not old subtype/record layout. | Unaddressed / audit pending |
| `a9b649d9c` | Avoid advancing a nonexistent roaming route id. | Port equivalent guard/invariant | Check current route ownership; do not let a zero route mutate path storage. | Unaddressed / audit pending |
| `6ac18fd93` | Buffer startup log text before files are available. | Keep ours; audit | Retain current severity/context/startup reporting and ensure early messages survive file failures. | Unaddressed / audit pending |
| `8cfd50238` | Buffer additional early log messages. | Fold into logging audit | Cover all startup paths, not just the first init stage. | Unaddressed / audit pending |
| `e200efebc` | Epithets close-button crash fix. | Fold into D06; implemented approved portion, review pending | Include safe close/focus handling in whichever UI hosts the feature. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `28d9b3b23` | Ellipsized empire city names with full-name tooltips. | Port | Measure with our text system; avoid truncating multibyte names incorrectly. | Unaddressed / audit pending |
| `49897e5ff` | Consolidated platform logging code and callback API. | Keep ours; audit | Existing severity and retained startup failures are authoritative; port behavior gaps, not architecture churn. | Unaddressed / audit pending |
| `9d8fb07bb` | easyav1 pointer update plus missing logging string header. | Skip codec pointer; audit compile fix | Root project uses dummy encoder; logging fix remains separately reviewable. | Unaddressed / audit pending |
| `6c6becbb9` | Removes conflicting core-log includes from SDL log adapters. | Fold into logging audit | Relevant only to adapters/API structure actually retained. | Unaddressed / audit pending |
| `a81cf0b50` | SDL3 joystick crash fix. | Fold into D05; approved portion implemented, review pending | SDL3-specific implementation absent locally; inspect analogous SDL2 handle lifecycle. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `80b3a0dda` | Depot source/destination counts plus an initial defensive-health implementation. | Port counts; Fold health into D15; approved portion implemented, review pending | Commit includes more than its title; the health portion is reverted then reintroduced later. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `d80f1696e` | Android/gamepad/cursor changes, other platform fixes, and unsigned formula-parser cleanup. | Port relevant input/formula changes; D05; approved portion implemented, review pending | Do not discard formula safety merely because the subject says Android; use C++ parser boundaries. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `d43550376` | DPI scaling and mouse-warp corrections in SDL2/SDL3. | Keep ours; audit and port gaps | Our logical UI/native pixels/world zoom are separate; inspect coordinate transformations. | Unaddressed / audit pending |
| `51c9100d9` | Corrects the SDL2 mouse-warp scaling follow-up. | Fold into DPI/input fix | Final transform must round-trip at multiple DPI/UI scales. | Unaddressed / audit pending |
| `9c45f5f52` | Android SDL2 cursor visibility with gamepads. | Adapt Android SDL2 gamepad cursor behavior; D05 | Force the software cursor at startup when a gamepad is connected, retaining the explicit software-cursor option. Android device validation remains open. | Implemented adaptation; Android validation pending |
| `ec37f58c7` | Workflow SDL/SDL_mixer version updates. | Intentionally omit obsolete platform/dependency delta; D05 | Keep supported vendored dependencies rather than upstream workflow versions. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `74a82aa9b` | Reverts those dependency versions on macOS/ARM64. | Fold into platform omission | Do not replay update/revert churn for inactive builds. | Unaddressed / audit pending |
| `dc7d304a9` | Windows ARM64 workflow correction. | Intentionally omit obsolete platform/dependency delta; D05 | No root x64 runtime change. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `42d1c66c1` | Another Windows ARM64 SDL version revert. | Fold into platform omission | Same inactive dependency series. | Unaddressed / audit pending |
| `42547c482` | Reverts the premature defensive-health changes. | Fold into `c82bb8687` final health series | Retain independent depot counts from `80b3a0dda`; do not undo unrelated fork health work. | Unaddressed / audit pending |

### May and June (features, assets, gameplay, and UI)

| Commit | What it added or changed | Agreed/proposed action | Why / integration notes | Implementation status |
| --- | --- | --- | --- | --- |
| `e76f61bd7` | Shoreline/elevation desirability overlay options and shared bonus calculations. | Approved D15; audit calculations; approved portion implemented, review pending | Show values from our actual runtime rules; avoid incidental balance changes. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `6b6d84e18` | Campaign missions can inherit the preceding rank. | Port proposed; D15; approved portion implemented, review pending | Explicit inherited-rank sentinel must survive campaign transitions and fixed-rank missions. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `a79fd5923` | Thief walk/death sprites, portrait, and XML. | Data; D09/D16; final pack installed/extracted | Bind selected criminal figures through mod graphics. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `ce2cc9683` | Warehouse/distribution declaration and UI warning fixes. | Port applicable cleanup | C++ signatures and current storage owners must agree. | Unaddressed / audit pending |
| `0505be87f` | Highway Station building, supply walkers, discounts, levies, UI, and ids. | Approved D03; implemented approved portion, review pending | Adapt complete mechanic, not just building art; include saved/runtime registration. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `cd5d90a33` | Automatic clearing of vegetation during construction and preview. | **Keep our Shift policy**; audit useful fixes | User explicitly rejects upstream placement policy. Keep transactional clearing/price/undo consistency. | Unaddressed / audit pending |
| `4eed71bff` | Latrines advisor button opens Health. | Port | Small UI correction through current building-info definition/action. | Unaddressed / audit pending |
| `9092fb526` | Highway Station resource/workcamp operation fixes. | Fold into D03; implemented approved portion, review pending | Final station state and supply behavior only. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `20cc33eb9` | Russian translation updates. | Data | Convert relevant keys to localization JSON; excluded features need no active UI strings. | Unaddressed / audit pending |
| `226d08563` | Remaining game-meat carts, reed-gatherer/papyrus stocks, reeds storage/panels. | Data; D16; final pack installed/extracted | Verify logical ResourceGraphics and building stock bindings after refresh. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `30333fada` | Industry art refinements and industry/UI/walker XML bindings. | Data; D16; final pack installed/extracted | Inspect final source XML compositions and animation offsets through extractor. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `5fd54b37b` | Italian localization update. | Data | Merge into JSON by key rather than restoring legacy C tables. | Unaddressed / audit pending |
| `a9ef93d60` | Industry XML asset correction. | Fold into industry Data | Use final corrected composition. | Unaddressed / audit pending |
| `cfba06c6f` | English localization wording update. | Data | Preserve fork-specific descriptions; incorporate applicable corrections. | Unaddressed / audit pending |
| `e178f2fd7` | French localization update. | Data | Map adopted feature keys into JSON. | Unaddressed / audit pending |
| `0b4be45eb` | Commit titled Italian update, with no tree delta. | History | `git diff-tree` is empty; nothing to port, but retain ancestry row. | Unaddressed / audit pending |
| `5a55dd5c1` | Italian text correction. | Data | Final catalog text, not prior empty commit. | Unaddressed / audit pending |
| `a98264142` | Avoid bad tile redraw after repairing variant/oriented buildings (“black holes”). | Keep ours; audit | Our independent rubble and construction-plan rebuild own the footprint; test variant/composed repairs. | Unaddressed / audit pending |
| `5efdf13eb` | Climate land/water and large marshland asset set/XML. | Data; D16; final pack installed/extracted | Final source-art refresh through canonical extraction; do not copy generated tiles into Mods. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `0e3c389bf` | Marshland art/XML and rain/latrine/marsh/thunder audio. | Data; D15/D16; final pack installed/extracted | Separate visual groups from ambient audio bindings. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `779aa81ed` | Education upgrade/college/armoury art, central land, native war horn. | Data; D16; final pack installed/extracted | Preserve authored fork upgrade/composition choices while refreshing inherited sources. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `e83cb28ae` | Depot/game-meat and Highway Station stock art/XML. | Data; D03/D16; implemented approved portion, review pending | Gate station use on D03; depot graphics stand independently. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `89c141af8` | Snow and wind audio updates. | Data; D15/D16; final pack installed/extracted | Validate loops, volume, and weather/menu lifecycle. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `da6adeabc` | Italian localization update. | Data | JSON key merge and placeholder checks. | Unaddressed / audit pending |
| `407ebb242` | Trade-ledger UI button assets/XML. | Data; D07/D16; implemented approved portion, review pending | Feature UI dependency, not the accounting implementation. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `0e0ac5fd4` | Show discounted highway price when a station operates. | Fold into D03; implemented approved portion, review pending | Preview/menu and charged construction cost must use the same quote. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `a25f80667` | Draw station sand/stone stocks. | Data-first adaptation; D03; implemented approved portion, review pending | Prefer declared resource/stock graphics over city-draw type checks. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `258bb0dbd` | Garden walls, haystacks, reeds/game-meat stocks, land masks and XML. | Data; D16; final pack installed/extracted | Respect mod definitions, resource bindings, and final extraction semantics. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `979bfd966` | Empire fort icon assets. | Data; D16; final pack installed/extracted | Keep logical empire icon identity and save import stable. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `9dedfb2a0` | Staged Rome-supplied triumphal arch, workers, messages, and save changes. | Approved D04; implemented approved portion, review pending | Build through our monument/relationship modules; preserve completed imported arches. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `0e274b752` | Russian triumphal-arch translations. | Data; D04; runtime counterpart implemented; translation reconciliation pending | Translate the selected construction policy accurately. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `71138d21c` | Correct natives always being attacked. | Port equivalent combat fix | Preserve native neutrality/hostility using current UnitType/formation combat rules. | Unaddressed / audit pending |
| `a0d1ab2bd` | Prevent criminals targeting themselves. | Port equivalent combat fix | Validate target identity and faction logic in current runtime. | Unaddressed / audit pending |
| `c82bb8687` | Reintroduces defensive-building durability display and health calculations. | Approved D15; Port final health series; approved portion implemented, review pending | Use our damage/footprint model and later minimum-tile-health correction. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `c47b0f6f5` | Soldiers blocked by combat while returning to forts no longer disappear. | Keep ours; audit and port gaps | New asynchronous routing/formation lifecycle must retry safely without removing soldiers or resurrecting stale routes. | Unaddressed / audit pending |
| `f7c09d8c3` | English localization update. | Data | Apply current feature wording to JSON. | Unaddressed / audit pending |
| `e602fc2f6` | Desirability-radius preview styling. | Approved D15; approved portion implemented, review pending | Render with our native overlay geometry and chosen palette. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `2a6baab64` | City ambient sound assignments. | Port + Data; D15; approved portion implemented, review pending | Match refreshed audio to active terrain/buildings without obsolete type-only dispatch. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `6e2d4d515` | UI polish, cart cargo/destination labels, problem/enemy overlays, and weakest-tile health. | Port adapted UI/health | Mixed behavior change: markets accepting nothing, military visibility, info hit selection and health need separate checks. | Unaddressed / audit pending |
| `df314cd32` | Wind ambience in clear weather. | Approved D15; approved portion implemented, review pending | Optional sound preference; preserve current audio lifecycle. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `1a6474ac6` | Spelling corrections including configuration identifiers and editor text. | Port + Data | Keep existing config key compatibility if identifiers change. | Unaddressed / audit pending |
| `f5d2669b0` | Robbers/looters use thief walker appearance instead of rioter. | Decide D09; Data-first | Use FigureGraphics and figure-info metadata while preserving actual crime behavior. | Unaddressed / audit pending |
| `73e8983ec` | Lighthouse requires nearby open water; new warning. | Decide D10 | Foundation-owned new-placement rule, preserve existing saves. | Unaddressed / audit pending |
| `775f1d7fd` | Russian text plus building-health visibility correction. | Data + Fold health into D15; approved portion implemented, review pending | Translation title hides a real UI hunk; terrain info also needs appropriate health visibility. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `abdba560a` | Beach tiles and central-climate hills. | Data; D16; final pack installed/extracted | Validate terrain seams, masks, and climate selection. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `b342bfe77` | Fixed workforce percentage raised to 45. | Decide D13 | Do not overwrite Vespasian local workforce/demographic policy. | Unaddressed / audit pending |
| `e41be213f` | Less-red desirability overlay palette. | Approved D15; approved portion implemented, review pending | Cosmetic choice, distinct from numerical desirability correctness. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `b2a26698f` | Corrected beach tile images. | Fold into terrain Data | Take final assets after D16. | Unaddressed / audit pending |
| `b242122f1` | easyav1 submodule pointer refresh. | Intentionally omit obsolete platform/dependency delta; D05 | Root build uses dummy encoder; no active codec need established. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `4bde994d9` | Float-based text drawing and language-fragment APIs. | Keep ours; audit and adapt callers | Preserve our font/logical-geometry APIs; support required behavior without a second text abstraction. | Unaddressed / audit pending |
| `56fe293e9` | Northern and southern hill assets. | Data; D16; final pack installed/extracted | Climate-complete inherited terrain update. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `46cb8980b` | Removes random empire-icon allocation for old maps. | Port compatibility behavior | Deterministic legacy icon mapping must preserve usable old map imports. | Unaddressed / audit pending |
| `fd4936dc8` | Reset cached water-provider pointers when opening overlay/preview. | Keep ours; audit | Current water runtime must invalidate provider lifetime/state correctly; do not introduce upstream static pointer cache. | Unaddressed / audit pending |
| `945c98e98` | Nesting-ground assets and bird animation. | Data; D16; final pack installed/extracted | Validate animation and climate logical groups. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `7d0bd503c` | Venus Grand Temple module art/XML. | Data; D16; final pack installed/extracted | Preserve active mod module semantics and extractor layer offsets. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `22c08c95b` | Caravanserai active animation artwork. | Data; D16; final pack installed/extracted | Check northern/central/southern animation compositions. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `65ee7f099` | Upgraded arena and Mercury/Venus module art. | Data; D16; final pack installed/extracted | Final inherited source assets; retain Vespasian overrides. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `d3e08f272` | Disables upstream MSVC x64 workflow. | Intentionally omit obsolete platform/dependency delta; D05 | Our active root x64 build must remain enabled. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `1ac6f6c32` | Mint animations and executions/games/naumachia overlay art. | Data; D16; final pack installed/extracted | Verify module/event graphics through current renderer rather than legacy city drawing. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `37fff96c4` | Fort orientation during placement, clone, repair, and save. | Keep ours; audit | Our composed forts/formation grounds use explicit geometry; port missed behavior via those owners. | Unaddressed / audit pending |
| `c9aa24b8a` | Fort saved-state orientation follow-up. | Fold into rotation/save audit | Use final import semantics without copying upstream record byte sizes. | Unaddressed / audit pending |
| `06c5f9d75` | MSVC workflow/build-script fixes. | Intentionally omit obsolete platform/dependency delta; D05 | Inspected payload is CI/scripts, not a root runtime compiler fix. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `f16c7020e` | Gatehouses block wolves/are attacked by rioters; docks unlock lighthouse in old maps. | Decide D09/D10 | Split combat policy from legacy allowed-building compatibility. | Unaddressed / audit pending |
| `e09631256` | Wolf movement over elevations/rubble. | Fold into D09 routing | Preserve intentional gate blocking while fixing ordinary terrain traversal. | Unaddressed / audit pending |

### July, August, and September

| Commit | What it added or changed | Agreed/proposed action | Why / integration notes | Implementation status |
| --- | --- | --- | --- | --- |
| `82939eb22` | Houses sometimes fail to merge after repair. | Keep ours; audit | Our repair produces per-tile vacant lots with original-footprint validation; test subsequent merge/evolution instead of restoring old rubble grouping. | Unaddressed / audit pending |
| `ac707d20b` | Minor English wording correction. | Data | Applicable JSON wording only. | Unaddressed / audit pending |
| `9b527a450` | Northern native meeting-hut artwork. | Data; D16; final pack installed/extracted | Refresh inherited logical group. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `9c5ee86db` | Southern native huts/meeting hut and all-climate native watchtowers. | Data; D16; final pack installed/extracted | Verify climate and alternate variant bindings. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `c5a163b1c` | German localization update. | Data | JSON catalog merge with existing fork keys preserved. | Unaddressed / audit pending |
| `5370eafc5` | German corrections. | Fold into German Data | Use final corrected strings. | Unaddressed / audit pending |
| `8ec484240` | Native decoration/watchtower changes and empire panel artwork/XML. | Data; D16; final pack installed/extracted | Separate terrain consumers from empire presentation. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `9bca59991` | Binds new empire background panels into UI/assets APIs. | Port UI adaptation; D07/D16; implemented approved portion, review pending | Reuse shared panel/image abstractions; account independently of trade-history adoption. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `c1bcd15cc` | Scrollbar arrow/thumb state images and XML. | Data; D15/D16; final pack installed/extracted | Depends on chosen skin, not widget behavior. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `0ea7cde03` | Dark scrollbar track and plus/minus assets/XML. | Data; D15/D16; final pack installed/extracted | Consume final skin through existing ScrollbarWidget. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `619ae51c2` | Request-ready notification inconsistencies. | Port | Check thresholds, duplicate messages, request fulfillment, and cargo accounting. | Unaddressed / audit pending |
| `a98c2a2e5` | Removes redundant labor assignment. | Keep ours; audit | Retain local workforce owner; take cleanup only if redundant there too. | Unaddressed / audit pending |
| `d2f55d0ff` | French localization update. | Data | Merge final adopted keys into JSON. | Unaddressed / audit pending |
| `3bdd1189d` | Earthquake startup no longer removes meadow tiles incorrectly. | Port | Preserve terrain flags and update tile/routing owners only for changed terrain. | Unaddressed / audit pending |
| `0fa5eb2e2` | Trade ships choose reachable river exits around low bridges. | Keep ours; audit and port gaps | Current bridge traversal and weighted routing must agree on navigability; check trapped ships and alternate exits. | Unaddressed / audit pending |
| `8faf80ad3` | Waterfront vegetation clearing works for docks/wharves/shipyards. | Keep Shift; port applicable foundation fix | Audit blocked shore tiles against our explicit force-clear policy; do not enable upstream automatic clearing. | Unaddressed / audit pending |
| `ce2bca95c` | Configurable grid colors per climate. | Approved D15; approved portion implemented, review pending | Implement via current renderer/config geometry; avoid legacy color-only draw forks. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `4d7e8ff53` | Wolves can traverse bridges appropriately. | Fold into D09 | Bridge/pathing metadata should express permitted traversal. | Unaddressed / audit pending |
| `69c698276` | Persist empire sort order and split automatic build cycling by category. | Approved D15; Port; approved portion implemented, review pending | Migrate previous setting defaults; keep mod-defined build menu categories authoritative. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `62a791627` | Restores missing Mausoleum/Nymphaeum levies. | Port economic correction | Check current XML/finance rules; avoid charging twice if generic upkeep already covers them. | Unaddressed / audit pending |
| `ecf4278d1` | Trade ledger, historical finance/resource statistics, empire/sidebar UI and saved data. | Approved D07; Save audit S07; implemented approved portion, review pending | Track transaction producers and new ordered payloads as one feature; no direct legacy struct replay. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `b2925cea0` | Repaired palisade gates restore their roads. | Keep ours; audit | Foundation terrain deltas and reconstruction should recreate passage consistently. | Unaddressed / audit pending |
| `9949a5aad` | Optional deletion of common messages after five years. | Approved D15; approved portion implemented, review pending | Respect calendar and message importance; migrate option default. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `ae8c92165` | Optional extended camera bounds. | Approved D15; approved portion implemented, review pending | Adapt to orthographic view/picking and current world scale. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `4bcccdcfa` | Russian trade-ledger translations. | Data; D07; runtime counterpart implemented; translation reconciliation pending | Match adopted accounting labels and periods. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `83991cf22` | Fixes an English translation-table build error. | Fold into localization Data | Legacy C syntax fix is unnecessary in JSON; preserve intended text. | Unaddressed / audit pending |
| `a79c54d48` | Trade ledger consumption corrections across industry, suppliers, workcamps, and requests. | Fold into D07; implemented approved portion, review pending | Count actual consumption once; distinguish transport from consuming/delivering to Rome. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `ee79d327e` | Russian extended-camera translation. | Data; D15; option implemented; translation reconciliation pending | Include if the option is exposed. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `01f774b39` | Enemy overlay health bars/tooltips and related damage/undo updates. | Approved D15; Port relevant fixes; approved portion implemented, review pending | Render from actual current building damage; inspect destruction/industry/undo hunks independently. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `9280fea3a` | Fix custom-variable window dropdowns. | Ported; review pending | Correct dropdown row positioning, native color/index mapping and white font handling; editor contract checks every color. [Evidence](augustus_sync_d08_plan.md). | Implemented and tested |
| `a91c6873a` | Trade ledger/sidebar historical-data fixes. | Fold into D07; implemented approved portion, review pending | Correct rollover and route history, including imports with no prior history. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `ca8470f06` | Selected walker highlight survives elevation-adjusted movement ticks. | Keep ours; audit | Current FigureGraphics/render interpolation should highlight the same final figure geometry. | Unaddressed / audit pending |
| `d73354f94` | Initial monument road-access fix. | Fold into final access series | Evaluate with `bf7255d5a` and caravanserai follow-up against foundation nodes. | Unaddressed / audit pending |
| `bf7255d5a` | Corrects monument access-point handling further. | Keep ours; audit and port gaps | Delivery destinations and composed monument access must refer to a real reachable node. | Unaddressed / audit pending |
| `547b18c2f` | Removes dead code/warnings across finance, images, suppliers, UI and overlays. | Selective Port/Fold | Preserve current owners; inspect non-deletion changes and fold trade/UI-specific cleanup into their feature. | Unaddressed / audit pending |
| `19bb6f58d` | Watchtower walker route preview. | Port | Preview should reuse real spawn/profile/pathing rules rather than an independent route approximation. | Unaddressed / audit pending |
| `b20d57491` | Place towers without prebuilt walls, adding missing wall cost. | Decide D11 | Atomic supporting-terrain/footprint quote and placement, including undo. | Unaddressed / audit pending |
| `d9870f5bd` | Prevent tower placement inside other buildings. | Port invariant; Fold D11 | Overlap safety applies regardless of the chosen tower policy. | Unaddressed / audit pending |
| `905594575` | Correct swapped import/export trade-ship phrases. | Port through phrase metadata | Compare spoken and displayed trade direction using current cargo/state. | Unaddressed / audit pending |
| `17b05668b` | Broad second event editor, house model overrides, new actions/conditions, formulas/texts, and UI. | Approved D08; native actions/conditions, definition overlays, editor controls and roundtrips implemented | Use native housing/construction/resource/city owners, keyed saves and isolated formula/text copies. Foreign dialect conversion and documented editor differences stay open. [Evidence](augustus_sync_d08_plan.md). | Native feature slice implemented; mixed compatibility work open |
| `d0b08cbfa` | Caravanserai road-access correction. | Fold into access series | Validate completed and under-construction composed footprints in each rotation. | Unaddressed / audit pending |
| `15e3f575d` | Formula migration correction. | Folded into native D08 migration | Skip time conditions in general conversion; migrate their own min/max once behind native gates. Foreign producers remain SB04. [Evidence](augustus_sync_d08_plan.md). | Native adaptation implemented; foreign conversion open |
| `78fea7b2f` | Russian second-event-editor text. | Data; D08 | Catalog follows selected capabilities and parameter names. | Unaddressed / audit pending |
| `09f3d1543` | German localization update. | Data | JSON key merge and placeholders. | Unaddressed / audit pending |
| `570f27707` | Rotated-map tower placement checks. | Port invariant; Fold D11 | Check all camera rotations against footprint/world coordinates. | Unaddressed / audit pending |
| `8aa190664` | Rotated-map gatehouse placement checks. | Keep ours; audit and port gaps | Apply to generic foundation orientation, preserving Shift and road rules. | Unaddressed / audit pending |
| `f9f98ac4e` | Correct desirability-building names in house information. | Port through identity/localization | Current BuildingType text ids replace legacy enum-based name lookup. | Unaddressed / audit pending |
| `af9ea7d88` | Better gold/bronze shields on goods UI. | Port UI adaptation | Use current ResourceGraphics/scaling and preserve readable badges. | Unaddressed / audit pending |
| `e59ca3c3d` | New scrollbars, assets bindings, and classic-scrollbar option. | Approved D15; Keep widget architecture; approved portion implemented, review pending | Adopt selected visuals and useful interactions via ScrollbarWidget, not another scrollbar engine. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `1dcac2922` | Fix ghost scrollbar thumb. | Port equivalent fix | Verify no stale thumb after content size or range changes, whichever skin is selected. | Unaddressed / audit pending |
| `d39119ca8` | Building record buffer-size correction for widened evolution texts. | Port schema handling; S08/S09 | Record stride must match actual field widths; neither numeric version nor allocation success proves alignment. | Unaddressed / audit pending |
| `dea75f8d7` | Scrollbar thumb/line assets and XML refinements. | Fold into scrollbar Data; D15/D16; final pack installed/extracted | Use final selected skin. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `abef5c48f` | Fourth/fifth religion requirement fixes and city-data/save gate. | Port semantic and bridge fixes; S09 | Map to mod-owned housing/religion rules while decoding exact upstream city layout. | Unaddressed / audit pending |
| `a20aa0dd9` | Fix unexpected messages by correcting city-data sizing/version behavior. | Port schema repair; S10 | Audit old malformed payload handling and messages rather than suppressing warnings. | Unaddressed / audit pending |
| `494425b8b` | Extended camera no longer sticks at top/left. | Fold into D15 camera; approved portion implemented, review pending | Include round-trip bound/panning tests if option is adopted. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `b260518ca` | Fullscreen checkbox state remains correct on repeated options openings. | Ported; review pending | Reopen fetches actual fullscreen state for both original and pending values; in-game settings retain pending edits through reload. [Evidence](augustus_sync_d08_plan.md). | Implemented; manual fullscreen interaction acceptance pending |
| `a3a83f785` | Redesigned demand-change editor. | Adapted D08 demand table; review pending | One row per change with date, resource icon, amount, explicit buys/sells and city name; refresh counts only when changed. [Evidence](augustus_sync_d08_plan.md). | Implemented; editor render checked |
| `b38e4680c` | Willow top-layer art and aesthetics XML. | Data; D15/D16; final pack installed/extracted | Selected ornamental tree needs correct top/footprint rendering. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Assets refreshed; consumer audit open |
| `9c451b94c` | Correct time-passed event migration. | Folded into native D08 migration | Read old time operands 2/3 and preserve the stored deadline/initialization semantics in native saves. [Evidence](augustus_sync_d08_plan.md). | Native adaptation implemented; foreign conversion open |
| `9ea738786` | Explicit model-data buffer sizes and save/scenario gates. | Port foreign-schema decoding; S11 | Upstream model arrays differ from our definitions/tables; do not reuse local numeric gates. | Unaddressed / audit pending |
| `1f578c690` | Further event migration correction. | Folded into native D08 migration | Separate pre-formula and time-formula gates; native version 203 is not Augustus version 203. [Evidence](augustus_sync_d08_plan.md). | Native adaptation implemented; foreign conversion open |
| `d9a93750d` | Correct zero/one-based event/formula indexing and copying. | Folded into native D08 event ownership | Reserve formula ID zero; clone referenced formulas/texts independently, preserve shared references within a copy and reassign parent event IDs. [Evidence](augustus_sync_d08_plan.md). | Native copy contracts verified; foreign conversion open |
| `381449f16` | Willow tree registration, construction/menu/info and version changes. | Data-first; D15; S12; approved portion implemented, review pending | Stable mod id and compatibility mapping instead of enlarging legacy building ranges. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `f3fef1a34` | Willow building-info correction. | Fold into willow Data/UI | Ensure actual mod identity/name routes to the right info window. | Unaddressed / audit pending |
| `d2bfabc5e` | Prevent wind sounds leaking into main menu. | Keep verified current lifecycle | Current ambient update stops wind outside city view or when disabled; no additional clear-weather wind policy introduced. [Evidence](augustus_sync_d08_plan.md). | Addressed: native lifecycle equivalent |
| `46e9537f3` | Invasion bridge demolition lock, occupied-bridge rules, fleeing changes, optional fast retreat. | Decide D12; port agreed safety behavior | Split demolition balance from route/formation lifecycle corrections. | Unaddressed / audit pending |
| `aa9f31ab4` | Shallow-water editor terrain, ship blocking and save/scenario changes. | Decide D14; S13 | Needs bitfield/schema/route handling even when graphics already exist. | Unaddressed / audit pending |
| `d8b9e41bc` | MSVC ARM64 dependency/install/upload workflow repair. | Intentionally omit obsolete platform/dependency delta; D05 | Inspected changes are confined to install/upload scripts and workflow; no root x64 runtime hunk. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Addressed: intentionally omitted |
| `974f7e529` | Russian text **and scenario version 25 → 26**. | Data + mandatory S13 audit | Translation title hides the final terrain-schema version gate. | Unaddressed / audit pending |
| `b4f123b82` | Correct swapped scenario-event enum values. | Keep native final enum identities; foreign mapping deferred SB04 | Native lock-route 44 and house-model 45 retain their existing identities. Never reinterpret ambiguous same-version upstream producers using native gates. [Evidence](augustus_sync_d08_plan.md). | Native identity audit complete; foreign producer mapping open |
| `0e81902b9` | Depot source/destination list off-by-one selection. | Port | Check filtered lists, first/last rows, and displayed storage versus selected owner id. | Unaddressed / audit pending |
| `3b44818cf` | Brazilian Portuguese localization refresh. | Data | Convert final keys into JSON; preserve fork translations. | Unaddressed / audit pending |
| `c2747bc4a` | Second scrollbar update across assets/UI consumers. | Fold into final scrollbar series; D15; approved portion implemented, review pending | Adopt chosen visuals and interactions through our shared widgets. [Slice evidence](augustus_sync_d03_d07_plan.md); mixed/unapproved and foreign-save portions are not closed. | Partially addressed; remaining hunks open |
| `ee79be310` | Avoid crash importing/exporting empire XML with button icons. | Ported; review pending | Bound both icon tables at 19 and accept button icons. Native empire XML roundtrip covers icon 19, UTF-8 names, zero denarii, resource prices and hidden routes. [Evidence](augustus_sync_d08_plan.md). | Implemented and tested |
| `984228b76` | Production-rate event “set” versus “add” arithmetic fix. | Keep native set/add owners; saturate addition | Set replaces each matching recipe rate; add adjusts it. Contract verifies 73, 146 and zero clamp without changing mint balance. [Evidence](augustus_sync_d08_plan.md). | Native arithmetic verified; overflow guard added |
| `f13c7d65e` | Both mint directions use denarii production rate; event arithmetic cleanup. | Decide D13; fold arithmetic fix | Our recipe-owned production may intentionally keep independent rates. | Unaddressed / audit pending |
| `d1ffc79b6` | Widespread language-fragment UI standardization. | Keep ours; audit and adapt changed callers | Preserve fork text/UI abstractions and observable fixes without mechanical API churn. | Unaddressed / audit pending |
| `5a64ec25c` | Correct armoury labor-seeker preview. | Port | Derive preview from actual mod spawn/workforce profile. | Unaddressed / audit pending |
| `2b8d428a6` | Average rounding, 102-based displayed efficiency, housing availability/population tooltips. | Decide D13; Port independent UI benefit | Split genuine rounding from display inflation and population UI; validate Vespasian demographic interpretation. | Unaddressed / audit pending |
| `85529e47c` | Russian population-rating/sidebar translations. | Data | Match adopted independent population UI from `2b8d428a6`. | Unaddressed / audit pending |
| `719f4860a` | Disables undo when a merged house becomes vacant lots, preventing corruption. | Port equivalent invariant | Check current house split/undo and runtime owner rebinding; do not rely on unrelated existing undo disables. | Unaddressed / audit pending |

## Mandatory save-bridge work: upstream `.svx` → current `.svv`

**Added at the user's explicit request. This is required catch-up work, not an optional feature.** Supporting a newer upstream save means decoding its actual upstream schema and migrating its meaning into the newest fork schema. It does not mean renaming a file or raising a maximum-version check. Save-bridge implementation and fixture verification are **NOT STARTED**.

### Concrete divergence found in this audit

- At the pinned target, upstream `src/game/save_version.h` declares save **`0xbd`** and scenario **`26`**. The fork declares save **`0xc6`** and scenario **`23`**. The final `.svv` version after this port may be higher; assign its new gates when the final persisted changes are known.
- At the graph ancestor, upstream save/scenario versions were `0xaf`/`22`. **A graph merge does not imply identical save layouts:** upstream uses `0xae` as “last without shared buildings”; the fork uses it as “last without mod metadata.” The collision already starts before the first new commit in this queue.
- Upstream save `0xb0` introduces plebeians; fork gates at that range govern road-service history. Upstream `0xb5` adds fort orientation; fork starts writing BuildingType tables. Upstream scenario `23` adds house models; fork scenario `23` adds keyed allowed-building data. These numbers cannot be compared in one common feature table.
- Current `src/game/file_io.cpp` uses `save_version > SAVE_GAME_CURRENT_VERSION` for the upper-bound check and derives local file-piece features in `init_savegame_data(save_version)` from that number. Its inspected file/buffer and info/preview paths do not pass an explicit source schema family into that allocation. Upstream `0xbd` is numerically below fork `0xc6`, yet would activate local resource/type/history/preview assumptions that upstream does not write. **This establishes a real dispatch/layout hazard to resolve; this turn has not run a newer `.svx` to claim a specific observed failure.**

### Required design and audit tasks

| Work item | Required result | Status |
| --- | --- | --- |
| SB01 — Identify source dialect before parsing pieces | Distinguish original/Julius/older Augustus, post-fork Augustus, and historical/current Vespasian layouts. Carry source family, save version, scenario version, resource version and any reliable schema identity through the decoder. | IMPLEMENTED: immutable archive preflight shared by file/buffer/campaign/preview paths; native/shared-legacy/five upstream layouts identified from bytes before mutation. Renamed and truncated input contracts pass. Native 0xcb / scenario 25 remains separate from upstream numbering; semantic conversion is SB04. |
| SB02 — Handle collisions and ambiguous input | Use validated file structure/metadata and explicit import origin where needed. Extension is a UI hint, not schema proof; buffer loads, campaign archives, renamed files, and previews also need reliable dispatch. Never probe by mutating live city state or silently accept the wrong layout. | IMPLEMENTED: explicit-family identification cannot bypass structural validation; ambiguous matches are reported instead of guessed. Contracts cover native/upstream collisions, renamed archives and truncated pieces. |
| SB03 — Enumerate exact upstream layouts | For each boundary below, record ordered pieces, compression/dynamic headers, element sizes, counts, enum identities, and condition gates from upstream writers/readers. Include changes that did not bump the version. | IMPLEMENTED INVENTORY: [68 producers, five piece layouts and 666 record declarations](augustus_save_schema_inventory.json), with exact Git blobs, enum/constants and ordered gated serialization expressions. Generated layout visitors pass boundary fixtures. Semantic interpretation and dynamic-record decoding remain SB04. |
| SB04 — Convert to current semantic identities | Map upstream buildings, figures, resources, terrain, gods, formations, routes, monument workers/deliveries and event ids into current mod ids/objects. Synthesize fork-only tables/history/defaults without reading imaginary upstream pieces. | NOT STARTED |
| SB05 — Preserve newly added feature state | Preserve ambient figures, station inventories/status, arch phases/deliveries, orientation, trade history, housing overrides, formulas/text ids, religion data and shallow terrain. Choosing to omit a feature must not silently delete its meaningful imported state. | NOT STARTED |
| SB06 — Hydrate owners and repair once | Create current objects, rebuild reciprocal relationships and route/formation ownership, warn for each recoverable inconsistency, and fix the producing runtime state so the saved `.svv` is clean. Do not strand a usable save because an old warning was recorded. | NOT STARTED |
| SB07 — Write newest fork format | Use only the current `.svv` writer and fork version namespace, retain historical `.svv` readers, write atomically to a new output, and leave the original `.svx` untouched. Update schema docs and DLL/API contracts if required. | Native 0xc9 gifts/history writer and historical SVV/SAV roundtrips verified in this slice; foreign conversion output remains OPEN. |
| SB08 — All entry points agree | File, buffer, campaign offset, save chooser/minimap/info, full load, and any module boundary must select the same source schema and validate lengths before use. Unsupported future schemas fail clearly before city mutation. | NOT STARTED |
| SB09 — Prove migration and round trip | Execute the fixture matrix below; record source-producing revision, header versions, selected schema, output version, repairs, invariant checks, and soak result. | NOT STARTED |

**D17 — Imported features whose gameplay port is declined (OPEN):** resolve this alongside D01–D16. Recommendation: keep compatibility definitions/adapters sufficient to preserve an existing upstream city even where the feature is not exposed for new construction/spawning in a selected mod. If faithful preservation cannot be represented, agree on an explicit conversion and report it; do not silently replace a station/arch/terrain/event with nothing. No decision to omit UI or a default setting waives SB05.

### Upstream schema boundary matrix

These are upstream version identities, not additions to the fork's enum. The labels `Sxx` are ledger references only. Every row needs exact byte-layout review and boundary fixtures before closure. The `0xb6` test-only layout must be distinguished from master-produced `0xb7`; support must be explicit rather than accidentally inferred.

| Ref | Upstream version / producer | What must be decoded and migrated | Required evidence |
| --- | --- | --- | --- |
| S00 | Baseline `0xaf` / scenario `22`; prior shared-building merge | Shared/surface building representation, original bridge/grid layouts and pre-divergence ids; distinguish fork `0xaf` mod metadata. | Old upstream and same-number fork fixtures remain distinct and load correctly. |
| S01 | `0xb0`, `5b24bc2e0` | Plebeian figure identity and owner/spawn state. | Occupied/vacant housing, ambient figures and current profile mapping. |
| S02 | `0xb1`, `6f076248f` | Dog identity, residential owner/figure state and phrase changes. | Dog-present saves preserve valid walkers under D02/D17; no enum collision. |
| S03 | `0xb2`, `bbb5d428c` | Altar/building orientation and saved-state rotation fixes. | Pre/post-fix rotations, damaged buildings, repair/clone/undo. |
| S04 | `0xb3`, `0505be87f` | Highway Station building/figure ids, stocks and related city state. | Working/idle/under-supplied stations, outstanding workcamp deliveries and discounts after load. |
| S05 | `0xb4`, `9dedfb2a0` | Triumphal-arch construction phases, Rome-supplied workers/deliveries. | Instant old arches and every unfinished/new phase migrate without losing award or blocking roads. |
| S06 | `0xb5`, `37fff96c4` + `c9aa24b8a` | Fort orientation and building state sizing. | Every orientation, composed fort grounds, soldiers at fort and en route. |
| S07 | `0xb6` test-only, `0xb7` master, `ecf4278d1` | Trade ledger and finance overview history; distinguish both layouts. | Source-revision-tagged fixtures with nonzero trade, production and consumption before/after rollover. |
| S08 | `0xb8` / scenario `23`, `17b05668b` and migration follow-ups | House models, formulas/text pools, copy/id indexing, new actions/conditions and enum fixes; building evolution text width. | Pre-editor events and new actions retain parameters/evaluation/timing; same-version producer fixes explicitly tested. |
| S09 | `0xb9`, `abef5c48f`; also `d39119ca8` | Fourth/fifth religion counters, widened state/stride and repaired buffer sizes. | Religious housing requirements and next-record alignment preserved; no accidental table interpretation. |
| S10 | `0xba`, `a20aa0dd9` | Corrected city-data sizing and spurious-message migration. | Old affected cities migrate once; expected messages persist, bogus data is repaired with evidence. |
| S11 | `0xbb` / scenario `24`, `9ea738786` | Model-data explicit sizes and differing scenario/save gates. | Dynamic model lengths, defaults, unknown/truncated data, and mod overlay restoration checked. |
| S12 | `0xbc`, `381449f16` | Willow building id and the `SCENARIO_LAST_NO_WILLOW_TREE = 24` boundary. | Inspect writer behavior: this commit adds the boundary without bumping scenario-current beyond 24; do not invent a clean version split. |
| S13 | `0xbd`, `aa9f31ab4`; scenario `25` then `26` via `974f7e529` | Shallow-water terrain flag/serialization and final scenario gate. | Test both transitional and corrected producer revisions, with ship routes and shallow tiles, not only the final header. |

The matrix is an initial inventory, not an assertion that all files sharing a version have identical semantics. `15e3f575d`, `9c451b94c`, `1f578c690`, `d9a93750d`, `b4f123b82`, and the model/buffer corrections require inspecting producers that share header versions. Versionless repair heuristics must be evidence-based and must not damage already-correct saves.

### Migration fixture and validation matrix

- Keep source fixtures immutable and record their upstream or fork producing commit, dialect, header save/scenario/resource versions, active mod, and meaningful feature contents. Cover every distinct master-produced schema from S00 through S13; mark missing historical fixtures honestly rather than declaring them tested. Add the test-only/transitional layouts where supported. Synthetic malformed pieces supplement actual producer saves; they do not replace them.
- Include original/legacy `.sav`, representative historical `.svv` (including colliding numeric versions), current `.svv`, and current upstream `.svx`. Test both file and buffer paths and campaign-offset loading. Exercise metadata/minimap previews before full load so a corrupt preview decoder cannot bypass the main bridge.
- For a clean source, load → inspect semantic invariants → write newest `.svv` → reload → inspect invariants again → headless advance **at least 5,000 frames with rendering**. Compare population, treasury/resources, buildings and footprints/orientations, figure owners/profiles/rosters, routes/deliveries, monument progress, calendar/RNG continuity, events/formulas, terrain and history. Do not require byte-identical files or unchanged old runtime ids; require preserved meaning and valid current relationships.
- Clean migration and soak fixtures must emit **zero warnings/errors** and converted rendering must emit **zero fallbacks**. For deliberately inconsistent legacy fixtures, use a separate repair test that requires the exact documented warning(s) on first import; then save/reload and require a clean warning-free round trip and soak. The startup gate itself remains strict; do not teach it to accept warning output as success.
- Test truncation, oversized counts/strides, wrong/ambiguous schema metadata, out-of-range identities and unsupported future versions before allocating unbounded arrays or mutating the city. Recoverable inconsistencies must remain loadable with repairs; unrecoverable structural errors must fail safely with stderr and nonzero exit, without operating-system crash dialogs.
- If saved data, identity, layout, or migration behavior changes, update fork save versioning, [save data organization](save_data_organization.md), [runtime bridges](save_load_runtime_bridges.md), and relevant mod/terrain/event contracts. The `.svx` source is never overwritten by conversion.

## Historical carry-over from the April ledger

These are **outside the 204-commit range** but must not disappear behind a new ancestry merge. Reconcile them against later fork work before final cohesion sign-off. The old document's “later” validation is not evidence that it ran.

| Previous item | Current required disposition |
| --- | --- |
| Native overlay parity (`165b7c2a3`), hippodrome overlay (`8251ca91f`) | Re-audit current native overlays and recent hippodrome/racing changes; close with visual evidence or a specific remaining fix. |
| Building-name audit (`c122ab18f`) | Fold into current mod identity/house desirability naming checks, including `f9f98ac4e`. |
| Wild boar payload (`ddfcde631`) | Verify current inherited content/extraction actually supplies it before marking equivalent or obsolete. |
| Shared buildings and city/overlay refactors (`5b2a592d3`, `6c4c82c30`, `e69bfb98f`, `83b3c57e7`) | Keep current native ownership; explicitly account for older upstream shared-building save representation in S00. Do not resurrect obsolete refactors just to match source. |
| Non-English/French/Russian counter translations | Reconcile applicable keys across all retained JSON catalogs rather than perpetuating the earlier known gap. |
| Deferred depot, storage, aqueduct, undo, editor, minimap and water checks | Include these interactions in the current regression pass, using current owners and fresh saves. |

## Implementation order after the user's review

1. Resolve every D decision and dependent row; agree feature/mod defaults and D17 import preservation. Freeze the reviewed upstream target. If upstream has advanced by then, append its new commits before claiming a current zero-behind result.
2. Establish source-schema dispatch and the save boundary/fixture inventory first. Plan current `.svv` changes jointly for accepted features so ids and piece layouts remain coherent. Do not ship an incomplete import path as compatible.
3. Refresh accepted source content/localizations and generic definitions; validate provenance, extraction and missing logical groups. Fold asset revisions to their final state while keeping all ledger rows attributable.
4. Port accepted features and independent correctness fixes through existing owners. Suggested batches: construction/repair/undo + access/bridges; figures/combat/formations + phrases; economics/history + station/arch; scenario editor/migrations; UI/text/input/audio. Recheck mixed commits across batches.
5. Complete Release x64 builds and the relevant regression checks below. Resolve all warnings, repairs that recur after save, mismatched mod behavior, and renderer fallbacks. Add implementing commits and test evidence to every row.
6. Integrate the reviewed upstream ancestry only after dispositions and verification are complete. A manual port/cherry-pick does not make the original upstream commits ancestors. Use a reviewed merge that preserves the completed fork tree and records upstream parentage; any deliberate ours-style ancestry reconciliation needs the closed ledger as its justification. A graph-only merge is not evidence of implemented behavior.
7. Fetch upstream again, append/review/handle any delta, and verify `git rev-list --count HEAD..upstream/master` is **0** and `git merge-base --is-ancestor upstream/master HEAD` succeeds. Report the exact upstream hash and time of the check. Leave `master` unchanged for the user's manual merge.

## Cohesion and acceptance checks for the later port

| Area | Required check |
| --- | --- |
| Build/tooling | Release x64 root solution and affected DLL/extractor/parser/ABI checks. On Windows use absolute executable paths with `Start-Process -UseNewEnvironment -NoNewWindow -Wait -PassThru` and propagate failure. |
| Startup/save gate | Representative recent `.svv`, legacy `.sav`, and the added upstream `.svx` migration corpus; 5,000+ advance/render frames per city; zero warning/error output and zero converted-renderer fallbacks. Apply the separate repair-fixture rule above. |
| Construction | Shift versus ordinary placement, vegetation/waterfronts, all rotations, overlapping/composed footprints, quoted/charged costs, cancellation/undo, house splitting, rubble repair, roads under gates/arches and aqueducts. |
| Military/pathing | Combat-blocked return to fort, new formation ownership, native/criminal targets, wolf terrain/gates/bridges, ship exits/shallows, invasion demolition and retreat policy. |
| Economy | Station supply/discounts and highway counts, levies, mint recipes and event rate overrides, production averages, actual-versus-displayed efficiency, ledger conservation/rollover and requests. |
| Scenario/campaign | Old/new formats, event copy/paste and index boundaries, formula/time migration, model override/reset persistence, terrain painting, icon XML round-trip, inherited/fixed ranks. |
| UI/input/audio | CJK/long city names and clipping, overlays/menu hover, cargo/health/info, dropdowns/scrollbars, multiple DPI/UI scales, fullscreen reopening, camera bounds, controller lifecycle, weather/menu sound transitions. |
| Mod/content | Augustus and Vespasian chosen behavior, Julius preserved defaults, stable definition ids and sparse inheritance, accepted dog/citizen/willow/temple content, licensed asset provenance and fresh extraction. |
| Ledger/history | Exactly one primary row per upstream commit, every mixed payload accounted for, no open decisions or unaudited equivalences, source/implementation/evidence links, final ancestor and zero-behind checks. |

## Planning-pass verification

Only documentation inventory checks apply in this turn. No build, game execution, asset extraction, save migration, or gameplay verification has been performed.

- The primary tables contain **204 rows and 204 unique commit hashes**, matching the pinned reachable range with no missing or extra commits.
- Repository-relative Markdown references resolve to existing documents.
- The only workspace change is this new ledger. The previous ledger and all runtime/mod files remain unchanged.
- Upstream was fetched for the inventory; branch HEAD and ancestry remain unchanged. The branch is still 204 behind at this planning checkpoint.
