# Figure graphics/native migration and validation checklist

> Completed migration checklist. This records the validated native-graphics conversion; deliberately retained debt is tracked by the active figure and renderer plans.

The deployed build passed the complete save cohort before this checklist was archived.

Status legend: `[x]` passing/completed, `[ ]` pending, `[!]` currently failing or known regression, `[-]` deliberately deferred.

## Non-negotiable graphics contract

- [x] FigureType default graphics use `<graphics><default><path value="Category\Image_Group" /></default></graphics>`.
- [x] Paths name asset XMLs logically: no PNG path, no `.xml` suffix, and no direct bitmap reference.
- [x] Restore sheep, wolf, zebra, and fish-gull paths to their canonical extracted `Environment\Group_*` assets after the rejected synthetic `Sequences` experiment.
- [x] Remove the just-added Julius semantic-sequence extraction code and restore the Julius extractor metadata version.
- [x] Replace older `Walkers\Sequences\*` paths with one conceptual asset XML at `Walkers\<model>` (for example `Walkers\architect`). The aggregation payload shape is useful, but `Sequences` is the wrong namespace and abstraction.
- [x] Delete the temporary `{legacy_group}` path expansion used by flotsam. Its irregular hard-coded resource/visibility schedule is represented by the lowest relevant reference-only authored bridge.
- [x] Confirm every FigureType file in Augustus, Julius, and Vespasian conforms to the final strict nested-target schema; invalid direct attributes, bitmap paths, and file-extension paths are rejected.
- [x] Confirm obsolete `runtime_selected_*`, `path_pattern`, `image_pattern`, asset-level `figure_graphics`, and legacy group-name attributes are rejected, not silently accepted.
- [x] Do not add extraction output or proprietary graphics to `Mods/*/Graphics`.
- [x] Leave `.gitignore` unchanged; authored graphics directories remain valid source directories.
- [x] Prefer metadata-derived or inferred entry ranges and offsets from the canonical extracted group. Only sequence lengths or offset schedules that exist solely as non-inferable legacy magic-number logic justify authored XML.
- [x] Keep nonredundant runtime selection parameters such as `max_image_offset` in FigureType graphics when the extracted asset cannot supply them; reject them only when they duplicate extractable asset metadata.
- [x] Put necessary magic-sequence overlays in the lowest relevant provider mod; Julius backports are inherited by Augustus and Vespasian and are not duplicated across all three.
- [x] Authored edge overlays contain references and semantics only—never extracted PNGs or copied proprietary payload XML.
- [x] Enforce the conceptual-model rule for converted figures: each authored bridge represents one model and contains all named state/composition entries selected by its FigureType.

## Shared graphics architecture

- [x] Introduce a shared `GraphicsAssetReference` used by building and figure graphics.
- [x] Make the strict FigureType parser require nested path nodes and reject the temporary schema.
- [x] Move migrated path validation, payload binding, entry lookup, and cache ownership shared with buildings into `GraphicsAssetReference`; Figure-specific state/frame selection remains in `FigureGraphics`.
- [x] Finish a generic runtime-selected entry mechanism that selects an entry inside the FigureType's directly referenced asset; it never translates through an absolute legacy image ID.
- [x] Make the converted nonstandard layouts expose state through named entries and generic selected layers; the renderer has no wolf/zebra/sheep/flotsam/hippodrome type switch.
- [x] Make multi-layer composition generic and XML-driven; hippodrome horses plus cart/rider is the first required composite.
- [x] Resolver success requires a drawable, semantically complete request or an explicitly hidden presentation.
- [x] Remove converted-path compatibility fallbacks and enforce zero unresolved/incomplete requests across every loaded figure in the save gate.

## Flagged legacy cleanup

- [x] Delete the `#FLAGGED FOR DELETION` FigureType image-group enum parser table.
- [x] Delete the duplicate service-effect known-name table/guard.
- [x] Delete the implicit figure-type corpse-source switch.
- [x] Remove `asset_target_uses_legacy_image_id`, `{legacy_group}` substitution, image-to-group scans, and absolute-image payload lookup after flotsam is converted.
- [x] Remove `has_legacy_default_source` and `has_legacy_resource_cart_graphics`, which returned constant false.
- [x] Remove no-payload legacy branches that had native XML coverage; remaining legacy-named helpers serve still-unconverted military/enemy layouts and are recorded below rather than disguised as converted.
- [x] Remove redundant guards, duplicate dispatch/reset code, and dead helpers in the touched converted paths without reducing readability.
- [x] Audit touched runtime producers for `GROUP_FIGURE_*`, `image_group(...)`, `select_legacy_*`, and hard-coded absolute image IDs; converted animal/crime/water/entertainer producers now publish semantic native state.

## Rendering regressions

- [x] Wolf: moving, resting, attacking, corpse, every direction, and every animation frame resolve through the semantic `Environment\wolf` asset backed by `Environment\Group_234`.
- [x] Zebra: moving, resting variants, corpse, every direction, and every animation frame resolve through `Environment\zebra`; no wolf-group contamination.
- [x] Sheep: moving, resting variants, corpse, every direction, and every animation frame resolve through `Environment\sheep`.
- [x] Missionary: all 104 canonical extracted group entries resolve and are drawable.
- [x] Fish gulls: strict validation uses the real two-range layout and no longer requests nonexistent `Environment\Group_206` frame `Image_0048`.
- [x] Flotsam: all resource variants and the Neptune sheep variant resolve from one semantic asset; runtime-hidden states are explicitly allowed without legacy group enums.
- [x] Hippodrome: horse animations and cart/rider layers resolve together for both teams and all eight directions; runtime orientation selection passes the full save soak.
- [x] Charioteer and related entertainer states retain correct non-race rendering through the full installed-save cohort.
- [x] The complete one-tick installed-save gate validates every alive/offscreen figure with zero unresolved or incomplete draw requests.
- [x] Synthetically materialize every cached live and corpse frame in every direction for every registered FigureType.
- [x] Fail startup validation when a corporeal FigureType lacks a corpse presentation; keep the exact non-corporeal exception set limited to fort standards, fishing boats, map flags, and hippodrome race composites.
- [x] Require conceptual FigureType assets to keep corpse graphics in the same conceptual XML; reject mixed references such as `Environment\zebra` plus `Environment\Group_235` even when both targets resolve.
- [x] Preserve the charioteer's legacy no-death-strip behavior through a one-frame corpse target in the extracted `Walkers\Group_215` asset.

## Diagnostics

- [x] Unresolved requests use `Figure graphics request is unresolved;` and do not claim that a legacy city-draw fallback is disabled.
- [x] Add unresolved and incomplete-composite counters with once-per-state error logging.
- [x] Logs include figure type, action/profile, direction, frame/image offset, and missing composition role.
- [x] Validate every alive loaded figure, including offscreen figures, while honoring explicit hidden states.
- [x] Fail tests on unresolved requests, incomplete composites, converted-path fallbacks, and unallowlisted warnings/errors.

## XML and runtime regression tests

- [x] Add parser rejection cases for removed FigureType graphics syntax and file-extension paths.
- [x] Run and pass the complete parser fixture suite after the latest cleanup; mechanically changed `Test\missing` fixtures remain intentional negative cases.
- [x] Add shared asset-reference tests alongside existing BuildingType contract coverage and FigureType strict-path parser cases.
- [x] Add direct target tests for every wolf, zebra, sheep, missionary, and gull entry/frame layout.
- [x] Add runtime-selected-entry tests proving lookup stays inside the referenced XML asset.
- [x] Add the full blue/red, eight-direction hippodrome composition matrix; invalid cross-asset layer lookup is rejected.
- [x] Make newly built roadblocks call `accept_none` so every permission is disabled by default.
- [x] Run and pass the roadblock default-permission test.
- [x] Exercise both extractor executables against checkout `Mods` targets and confirm exit code 2 rejection.

## Extraction and source-control boundary

- [x] Protect Augustus and Julius extraction entry points against checkout `Mods` destinations.
- [x] Delete and freshly extract `extracted_graphics_sample` from the installed Caesar 3 files and shipped Augustus assets.
- [x] Fresh Julius central/northern/desert extraction completed successfully before the current renderer work.
- [x] Fresh Augustus v3 extraction completed successfully and produced 3,214 groups, 3,259 images, and 4,088 PNGs.
- [x] Re-run a clean extraction after extractor changes settled and validate representative Julius/Augustus payloads through startup and direct asset contracts.
- [x] Scan tracked/staged paths: no added/untracked generated `Group_*` trees, PNGs, stamps, manifests, or unverified proprietary assets; the 6,876 Augustus deletions are the user's intentional cleanup.
- [x] Record the first known proprietary Augustus import as commit `fb138c6440c81a4aa5ad44cc12601484e2089b0d`.
- [-] Do not execute the history scrub yet. Retain a plan covering affected refs, backups, branch protection, force-push coordination, collaborator reclones, and remote object/cache verification.

## Startup and deployment gates

- [x] Release Vespasian, parser harness, Augustus extractor, and Julius extractor built successfully before the latest runtime-selection edits.
- [x] Deploy script preserves installed extracted graphics while overlaying authored source content.
- [x] Deployed Julius, Augustus, and Vespasian startup gates pass after correcting the fish-gull and parser element-table regressions.
- [x] A calibrated 3000-tick/1000% soak completed at 1217.6 average TPS, with the third 1000-tick interval above 1000 TPS.
- [x] Build all modified Release targets after direct-group correction.
- [x] Re-extract into `extracted_graphics_sample` and validate the extractor boundary.
- [x] Redeploy the verified Release executable and authored Mods content; deployed and Release executable hashes match.
- [x] Remove the exact 14-file stale deployed `Augustus\Graphics\Walkers\Sequences` wrapper directory while leaving canonical extraction output intact.
- [x] Pass deployed Julius, Augustus, and Vespasian definition loading with no warning/error.
- [x] Pass semantic animal/gull/missionary/flotsam/hippodrome contracts and all loaded-figure runtime validation with zero unresolved/incomplete requests.

## Required installed-save cohort

Each save must load, advance, and render for the configured several-thousand-frame soak. Initial old-save bridge repairs may emit specifically allowlisted repair warnings; unrelated warnings/errors fail. A post-repair roundtrip must reload cleanly.

- [x] All saves whose names identify the Praetor cohort.
- [x] `Aedile 1 3` through `Aedile 1 15`, including the new-save-format variant.
- [x] All saves whose names identify the Engineer cohort.
- [x] All saves whose names identify the Quaestor cohort, including historical filename misspellings.
- [x] Preserve the existing representative recent `.svv` and legacy `.sav` startup coverage.
- [x] Advance/render every selected city for 3000 ticks at 1000% speed.
- [x] Allowlist only documented save-bridge repair warnings during the first old-save load.
- [x] Save each repaired city and reload it; the repaired roundtrip emits no repair warning.
- [x] Require zero XML, graphics, renderer, simulation, or unrelated warnings/errors during every soak.
- [x] Record every save's load/repair/soak/render/roundtrip output in `temp/full_save_soak_3000.log`; 58/58 passed and the minimum post-warmup TPS was 1323.9.

## Repository hygiene and final report

- [x] Replace machine-specific paths in tracked documentation with `<game install path>` or `<repository checkout>` placeholders.
- [x] Re-scan tracked Markdown/text/code for machine-specific paths; none remain.
- [x] Verify `.gitignore` has no migration changes.
- [x] Preserve user-authored edits and the large intentional proprietary-graphics deletions.
- [x] Run `git diff --check` (clean) and summarize changed/untracked files without staging or committing.
- [x] Record the corruption chains, missed diagnostics, additional findings, and validation evidence for final reporting.
- [x] Deployed startup plus the full required save cohort pass; only user-owned manual visual confirmation remains.
- [x] Re-run the complete post-lifecycle-test gate: 60/60 installed save runs (Julius, Julius+Augustus, and 58 Vespasian cohort saves) completed 3,000 ticks at 1,000% speed with zero unallowlisted warnings/errors; the slowest measured 1,000-tick interval was 1,310.1 TPS.
- [x] Confirm the deployed executable SHA-256 exactly matches the Release artifact after the lifecycle validation deployment.

## Deliberately retained migration debt

- [-] Core legionary/javelin/mounted and enemy-atlas layouts still use explicitly isolated legacy image-ID adapters. They are not used by the converted animal/flotsam/hippodrome paths, are not counted as converted coverage, and should be migrated as a separate semantic-asset batch rather than hidden by this regression fix.
- [-] Do not execute the proprietary-history rewrite in this task. The coordinated scrub plan is retained at `temp/proprietary_graphics_history_scrub_plan.md`.

## Native image-selection parity and update ownership

- [x] Add an executable parity oracle that compares native semantic selections with the exact extracted legacy group entry for every direction, animation frame, pose, corpse frame, variant, and composite layer.
- [x] Verify wolf, zebra, sheep, fish gull, flotsam, criminal, explosion, and hippodrome selectors against the committed legacy formulas, including view rotation, frame stride/divisors, non-linear schedules, and layer offsets/order.
- [x] Audit BuildingType option selection against the committed legacy renderer: orientation, connection masks, storage permissions, production progress, gates/crossings, rubble, composites, and overlays. Restore legacy staggered farm-field growth selection and delete the unused `storage_load` parser/runtime branch already superseded by XML conditional layers.
- [x] Trace graphics update ownership. Remove per-tick FigureType wrappers that only revalidate an already cached XML binding; retain semantic state publishers and load-time refreshes.
- [x] Keep building invalidation/refresh calls that change the generation-cached selection. Remove only wrappers that add no semantic boundary, never the invalidation itself.
- [x] Build, deploy, pass strict startup validation, then rerun Julius, Julius+Augustus, and the required 3000-tick installed-save cohort with zero renderer fallbacks or unallowlisted diagnostics.

## Vulkan atlas handoff constraints

- [x] Preserve XML paths and named entries as stable asset/material identifiers so payloads can become immutable Vulkan atlas descriptors without changing gameplay definitions.
- [x] Define compact per-object render-state records (model, state/pose, direction, frame, variant/resource, color, and ordered layer data) suitable for instance/storage buffers and indirect draws.
- [x] Keep authoritative gameplay transitions and semantic state selection on the CPU; shaders consume state and select atlas UV/layers, but do not own simulation decisions.
- [x] Avoid new per-tick image-ID mutation APIs. State changes should invalidate/publish semantic render state, preparing the renderer for batched GPU submission.
