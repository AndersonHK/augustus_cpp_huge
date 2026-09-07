# D09–D11 implementation and validation

The user committed the validated earlier slice as `b8b645617` and approved D09–D11 on 2026-09-06. This slice does not make a commit or change upstream ancestry. D12 remains the next decision. Git reports `265 204` for `HEAD...upstream/master`.

## Binding decisions and implementation

### Save import and cart regressions

The preserved backup log (`out/d09-original-user-backup.log`) identified figure 76, a mess-hall supplier, when opening a Vespasian city with only Julius. The active stack has neither its runtime profile nor its graphics. The load bridge now discards unsupported later-mod transient figures with a warning, before validating their unavailable owners. Original Julius figures are preserved.

Missing resource/building/water-access definitions already had deterministic import repairs; those diagnostics now report warnings instead of fatal errors. Resources absent from the selected stack cannot retain quantities, and absent building types are removed by the existing building bridge. This permits a smaller-stack import; it does not claim lossless foreign-schema conversion. Overgrown garden properties are cleared with a warning when the selected stack provides only ordinary gardens.

A strict post-soak reload found the runtime producer behind new stale house slots: optional-owner profiles were spawned into a tracked building slot without the reciprocal home reference. Spawn publication now sets both ends immediately; a stale slot cannot steal a walker owned by another building. Julius beggars retain their existing unemployment tuning and movement behavior.

Caravans and their followers now have complete Julius FigureType profiles and UnitType combat stats, executed by native controllers. Missing stats fail validation; the legacy action-table entries no longer implement caravans. Movement ticks, terrain, trade delays, initial progress, follower type/count and animation frame count come from XML. Augustus overrides the profiles with its road/highway preference, live off-road setting, randomized initial progress and a declared 25% working-Mercury-grand-temple speed bonus. The conditional terrain and monument bonus fields are generic profile capabilities.

The normal caravan group belongs to Julius; desert camel substitution belongs to Augustus, following [Julius upstream's caravan implementation](https://github.com/bvschaik/julius/blob/master/src/figuretype/trader.c). Vespasian inherits those complete profiles and supplies only the graphics subtree with logical scale overrides. Directional graphics accept optional `central_path`, `northern_path` and `desert_path` overrides; every authored variant is validated at startup and climate selection occurs at draw time. No duplicate extracted sprites are shipped.

The original caravan group has no dedicated death sprites. Its directional declaration explicitly sets `draw_corpse="true"`, retaining the upstream directional presentation during death. The startup lifecycle gate validates every declared alive/dead frame and every climate variant; it does not exempt caravans from lifecycle validation or borrow another figure's corpse sprites.

Native save version 204 adds a narrow bridge for caravan profile identities absent in versions through 203. It warns when assigning the explicit profile and serializes that identity on the next save. It does not permit missing identities in newly written saves and does not equate native version 204 with Augustus version 204.

Vespasian resource graphics now select authored logical-scale overrides for single, multiple and eight-load carts, including every direction of Augustus resource carts. The scale is 96 logical units per source pixel, matching the walkers. These are XML scale/animation definitions only: no bitmap or bulk extraction output was added to source.

The initial per-direction authoring was incorrect and has been removed from source and the installation. Five cart groups (brick, concrete, gold, sand and stone) each contain all eight directions and both load variants in one XML file. Three existing classic cart groups each have one scale override. Resource references resolve direction within the group; there are no per-direction Vespasian group files.

The ownership audit moved roadblock, highway, distant-water, grand-temple footprint and decorative-gate foundations from Julius to Augustus. The field meadow foundation moved to Vespasian. Julius retains its own terrain/building foundations and inherits none of those later-mod declarations.

### D09: wildlife and criminals

FigureType behavior data controls obstacle rechecks and rioter fireproof targets. Augustus adds two live mod settings, inherited by Vespasian:

- `WILDLIFE_BLOCKED_BY_DEFENSES`: default on. Recheck the next animal step after defenses change; retain elevation/rubble traversal and native bridge/roadblock traversal.
- `RIOTERS_ATTACK_DEFENSES`: default on. The authored targets are gatehouses, palisades and palisade gates, matching the upstream delta. Other fireproof buildings retain their protection.

Turning the settings off restores the earlier behavior. Julius has no new behavioral policy enabled. The implementation uses shared figure behavior and native building metadata, without a wolf or rioter type switch for these rules.

Robbers and looters use the installed thief walk/death sprites and portrait. An authored animation bridge maps the pack's direction-major frames and twelve-frame idle loop. The original sixteen-frame criminal attack gesture is retained, as upstream also does; the thief pack supplies no attack animation. Vespasian adds logical scaling.

Covered upstream commits: `f16c7020e`, `e09631256`, `4d7e8ff53`, `f5d2669b0`, `a79fd5923`. The legacy dock-to-lighthouse availability hunk is included. The shared UI asset correction in the thief/lighthouse series is present in the installed final pack.

### D10: generic proximity requirements

Foundation XML supports, for example:

```xml
<proximity terrain="water" min_distance="2" max_distance="5" min_count="3" />
<proximity terrain="navigable_water" max_distance="9" />
<proximity terrain="sea" max_distance="9" min_count="1" />
```

Distance is minimum Chebyshev distance to an active cell of the rotated foundation. Ordinary terrain masks require all specified bits; `navigable_water` requires boat-passable tiles, and `sea` additionally requires connection to the scenario's river entrance. Multiple requirements must all pass. Invalid ranges/counts fail parsing. Placement failure uses a localized generic proximity warning.

Augustus's lighthouse declares entrance-connected navigable water within nine tiles. Vespasian inherits it. Existing lighthouses are not revalidated as new placements. The sand pit's companion fix now uses its actual 2×2 footprint and ordinary water within nine tiles, also through data; this removes the legacy 3×3 search discrepancy.

Covered upstream commit: `73e8983ec`.

### D11: supporting walls and roads

Foundation profiles name a support definition (`support="wall"` or `support="road"`). A support must be instant and single-cell, and its terrain must be part of the consuming foundation. Missing support tiles are quoted once; existing walls are not charged again. Unrelated buildings remain forbidden. The existing Shift vegetation-clearing policy is unchanged.

Towers receive missing walls; roadblocks and plazas receive missing roads. The transaction publishes the support terrain as part of the final foundation rather than leaving overlapping intermediate building records. Existing wall records are handled by the declared replacement transaction. This keeps ownership, route invalidation and undo coherent.

The quote includes support denarii and instant construction materials. Augustus walls require stone, so tower supports consume that stone and account for it. Undo restores support materials, reverses that consumption entry and restores prior terrain/owners. If storage has filled since placement, undo waits for sufficient space instead of dropping the materials. The tooltip includes material amounts; hover and drag denarii quotes include supports. Support graphics are drawn before the consuming building. Plaza area previews quote missing roads and restore the map on cancellation.

Covered upstream commits: `b20d57491`, `d9870f5bd`, `570f27707`.

Tower rendering regression follow-up (2026-09-06): supporting wall terrain made the wall refresh visit every tower footprint cell. Its legacy fallback changed each cell to a one-tile draw anchor, causing four copies of the tower after refresh. `set_wall_image` now respects non-wall building ownership and leaves those foundations and images intact. Native wall buildings and ownerless wall terrain retain their existing refresh paths. The placement regression test fails before the fix (exit 10, `out/tower-before.stderr.log`) and passes afterward with four repeated area/global wall refreshes, exactly one draw anchor, preserved 2x2 geometry, unchanged tile images, publication/undo, and the existing 3,000-tick render soak (exit 0, `out/tower-after.stdout.log`, empty stderr). The corrected Release build passed (`out/tower-fixed-build.stdout.log`).

The follow-up full save gate ran all 70 cases and **exited 1**, with 69 passing. `autosave-year.svv` had zero canonical-load/soak warnings or errors but missed the unchanged 1,000 simulation-ticks/sec performance threshold: 966.1 in the full run and 978.7 on an isolated repeat. The still-installed pre-fix executable also failed the same isolated case at 981.3, reproducing the performance limitation independently of this fix. Evidence: `out/tower-startup-gate.*`, `out/tower-performance-recheck.*`, and `out/tower-performance-baseline.*`; isolated performance failures exit 5. Initial import repair warnings remain explicit. This is not a clean full-gate pass, and the performance issue remains open. Runtime deployment succeeded (`out/tower-deploy.stdout.log`), with installed executable hash verified against the tested build; mod data and extracted graphics were preserved. No commit or ancestry adjustment was made.

## Validation record

- Release game build: `out/d09-final-game-build.log`; startup gate build: `out/d09-lifecycle-parser-final.log`.
- Full startup/save gate: `out/d09-final-gate.stdout.log` / `.stderr.log`, exit 0. All **70** cases passed: original legacy scenario, Julius-only and Augustus dependency stacks, and 67 required/representative Vespasian saves. Each case received canonical roundtrip validation and 3,000 rendered ticks, followed by strict reload checks. The gate also validated 41 figure definitions, 3,632 live bindings and 2,216 corpse bindings. Initial migration repairs are reported separately; canonical load/soak/reload checks retain strict warning/error and renderer-fallback thresholds. Parser negative fixtures intentionally emit errors for malformed inputs.
- Focused contracts: `out/d09-final-contracts.stdout.log` / `.stderr.log`, exit 0 and empty stderr. Passed four rotations, partial walls, overlap rejection, road quote, material consumption/refund, publication/undo, hover price/map immutability and plaza preview/cancellation. Also passed native caravan controllers/stats, live terrain and route changes, Augustus arrival timing and 384 alive/dead caravan frames; 504 cart direction/load variants; 12,960 criminal frames; existing D08/archive/accounting/gift tests; dog/citizen movement/scaling; and 3,000 rendered ticks. Live in-game settings remained grouped and restored without population or treasury changes.
- Mod composition: `out/d09-final-content.stdout.log`, exit 0. Compiled 898 definitions, verified complete inherited caravan profiles, owning-mod foundations, one cart XML per group, both D09 settings on/off, and preserved instant costs across 20 No Monuments definitions.
- Reported Julius import: `out/d09-final-julius-import.stdout.log` / `.stderr.log`, exit 0. Passed `Aedile 1 14.svv` migration, canonical reload, 3,000 rendered ticks and final canonical reload. Initial cross-stack/historical repairs are explicitly warned (845, including nine newly explicit caravan identities); subsequent canonical load and soak are strict and clean. Original saves remain unchanged; the validation output is under the installation's `out` directory.
- Preview captures: `out/d11-support-tower.png` and `out/d11-support-roadblock.png`, inspected visually. Original BMP captures remain under the installation's `out/catch-up-ui` directory.
- Final deployment: `out/d09-final-deploy.stdout.log`, exit 0. Verified the installed executable hash and all 62 changed mod files, preserving runtime-extracted localization additions. Verified removal of all 80 obsolete per-direction authored cart aliases and the misplaced Julius foundations. No bitmap or bulk extracted graphics were added to source.
- Ledger inventory: all 204 primary rows have unique hashes and exactly match the pinned upstream range. `git diff --check` passes. The graph remains `265 204`; no commit or ancestry change was made.

Earlier interrupted gates and failed intermediate revisions are not successful validation of this final revision. In particular, `out/d09-startup-gate-final-2.*` predates the user's grouping/ownership corrections, `out/d09-corrected-gate.*` found the missing explicit caravan death policy, and `out/d09-lifecycle-gate.*` was stopped to preserve Augustus's full-day arrival delay. The final results above supersede them.

## Next decision

D12 remains open: bridge demolition during invasions versus local occupancy/threat rules, and optional double-speed retreat. No D12 behavioral changes or upstream ancestry accounting are included in this slice. Foreign Augustus save-schema conversion and the editor compatibility gaps remain as recorded in the earlier ledger.
