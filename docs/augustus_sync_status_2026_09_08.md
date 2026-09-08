# Merge status for manual testing — September 8, 2026

Upstream target: `d879725d22dcf6cefdf84c75d84634d22645111d`. Current queue: 206 unique reachable commits. April carry-over: 19 additional unique commits already in the branch ancestry. They increase the audit workload, not Git’s behind count.

These are conservative ledger-based counts, not a new claim that every behavior has been independently re-audited. Each commit is counted once. A mixed commit is **partial** when any required payload or verification remains open, including explicitly deferred arithmetic. **Implemented** means its recorded disposition is complete: actual ports, verified native equivalents/supersessions, approved exclusions, and an empty commit. **Deferred** means the whole outstanding April change was explicitly postponed; it does not include an approved permanent exclusion. **Not started** describes the outstanding reconciliation work, and does not prove that no equivalent code or asset exists today.

| Status | Current queue | April revisits | Total |
| --- | ---: | ---: | ---: |
| Not started | 0 | 2 | 2 |
| Deferred | 0 | 4 | 4 |
| Implemented | 168 | 0 | 168 |
| Partial | 38 | 13 | 51 |
| **Total** | **206** | **19** | **225** |

D19 is contained in the partial event/import work; it is not an additional commit. The four April architectural deferrals require an equivalence review, not revival of obsolete architecture.

## April carry-over scope

The first eight rows expand the named September carry-over items. The remaining eleven expand the April ledger’s untranslated counters, unrun regression checklist and explicitly partial mixed merge into traceable commit entries. Already closed April changes, approved exclusions and superseded intermediate commits are excluded.

| Commit | Status | Work still needing closure |
| --- | --- | --- |
| `165b7c2a3` | partial | Native overlay visual parity needs current in-game verification. |
| `8251ca91f` | partial | Hippodrome overlay fix needs current visual verification. |
| `c122ab18f` | not started | The broader building-name reconciliation remains unclosed; do not infer completion from individual naming fixes. |
| `ddfcde631` | not started | The wild-boar payload needs a current asset/extraction provenance and binding check; the April ledger records it as not imported. |
| `5b2a592d3` | deferred | Shared-building implementation was deliberately deferred; reconcile native ownership and the old save representation before closing it as superseded. |
| `6c4c82c30` | deferred | Deferred city-rendering refactor; verify the native replacement instead of replaying old architecture. |
| `e69bfb98f` | deferred | Deferred overlay refactor; verify native replacement parity. |
| `83b3c57e7` | deferred | Deferred overlay-refactor follow-up; reconcile together with its parent change. |
| `bb56ac880` | partial | Counters were ported; remaining catalog reconciliation and the promised editor counter regression check need explicit closure. |
| `ae6c183fe` | partial | Ported depot behavior still needs the listed recall/change-orders/change-resource-with-cargo regression sweep. |
| `1bedb8599` | partial | Ported storage request dispatch still needs the listed emptying/request interaction sweep. |
| `105c02e70` | partial | Recheck reservoir-over-aqueduct behavior through current foundations. |
| `5a9a8c6f6` | partial | Recheck roads/highways under aqueducts through current pathing and placement. |
| `fe9637540` | partial | The promised cancel/undo image-invalidation check needs explicit current closure. |
| `cbd181ae1` | partial | The listed editor terrain-preview regression check needs explicit current closure. |
| `7931b9e22` | partial | Recheck arbitrary-aqueduct reservoir placement with current data-driven footprints. |
| `b8923f053` | partial | Recheck save/scenario minimap readers against current identity widths and foreign producers. |
| `caa61f5ce` | partial | Recheck final aqueduct/wall/palisade minimap appearance; the intermediate superseded 4e81f5af3 is not another open item. |
| `2de6361d8` | partial | April explicitly retained a partial mixed-merge disposition; Android/packaging/localization omissions need review against current platform/data ownership. |

## Current queue classification

| Commit | Status | Recorded ledger status |
| --- | --- | --- |
| `016d5254c` | partial | Implemented adaptation; Android validation pending |
| `5ff7e3d24` | implemented | ported + asset equivalence verified |
| `a1b14e6f7` | implemented | Implemented; Release and extraction verified |
| `90a7a9c11` | implemented | Ported + verified by >4-billion-pixel regression |
| `537e15ca0` | implemented | Ported + verified |
| `395b9f38d` | implemented | Equivalent + source verified |
| `427e4ae5b` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `5b24bc2e0` | partial | Native policy and Vespasian local-labor regression fixed; foreign-save work open |
| `a2bab8c9e` | partial | Implemented; Linux CI execution pending |
| `00d6860d8` | implemented | Ported/equivalent + verified |
| `9b18c25ac` | implemented | ported + asset/binding verified |
| `56f90fa7f` | implemented | Ported + native render verified |
| `50e257d8e` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `58a0592fb` | implemented | Superseded + replacement verified |
| `10b44c769` | partial | Native policy and Vespasian local-labor regression fixed; foreign-save work open |
| `fea55b845` | implemented | ported + asset/binding verified |
| `6516d83a5` | implemented | Equivalent + native render verified |
| `b2b05d726` | implemented | Ported/equivalent + verified (limits recorded) |
| `dd8b684a0` | partial | Native policy and Vespasian local-labor regression fixed; foreign-save work open |
| `33316b4d7` | implemented | ported + asset/binding verified |
| `69847ce5e` | implemented | ported + asset/binding verified |
| `3eba78982` | implemented | Adapted/omitted + source verified; device limits recorded |
| `2f577a6e4` | implemented | Addressed: intentionally omitted |
| `562300a15` | implemented | Addressed: intentionally omitted SDL3-only change |
| `5bb85d5ec` | implemented | Intentionally omitted + SDL2-only reason |
| `589efb113` | partial | Native adaptation verified; Android device validation pending |
| `21cfcd6d6` | partial | Implemented; Android validation pending |
| `3596e1b6d` | implemented | Adapted/omitted + source verified; device limits recorded |
| `9c3374492` | partial | Source adaptation complete; Android build validation pending |
| `83c1ed5bb` | implemented | Assets and final metadata verified; native binding gate passes |
| `68b6c312b` | partial | Equivalent by source audit; Android device validation remains separate |
| `6f076248f` | partial | Native verified; foreign identities/owners remain open |
| `dd599b9f7` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `1de00fae9` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `c2073190e` | implemented | Ported + verified |
| `8dd168d7a` | implemented | equivalent + verified |
| `2f9aa4c74` | implemented | Ported + verified |
| `76b820f05` | implemented | equivalent + verified |
| `b803bb6fd` | implemented | equivalent + verified; inactive script intentionally omitted |
| `bbb5d428c` | partial | Native cycle/tooltip/repair portions verified; foreign altar hydration open |
| `a9b649d9c` | implemented | Equivalent + verified: absent-route contract and 3000-frame native gate |
| `6ac18fd93` | implemented | Superseded + Logger contracts verified |
| `8cfd50238` | implemented | Superseded + Logger contracts verified |
| `e200efebc` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `28d9b3b23` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `49897e5ff` | implemented | Superseded + Logger contracts verified |
| `9d8fb07bb` | partial | Pointer and compile fix audited; real codec validation unavailable |
| `6c6becbb9` | implemented | Superseded + replacement verified |
| `a81cf0b50` | implemented | Addressed: SDL2 equivalent verified |
| `80b3a0dda` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `d80f1696e` | partial | Native contracts verified; Android device validation pending |
| `d43550376` | partial | Source and bounds verified; physical DPI validation pending |
| `51c9100d9` | partial | Equivalent + source verified; physical DPI validation pending |
| `9c45f5f52` | partial | Implemented adaptation; Android validation pending |
| `ec37f58c7` | implemented | Addressed: intentionally omitted |
| `74a82aa9b` | implemented | Intentionally omitted + dependency ownership reason |
| `dc7d304a9` | implemented | Addressed: intentionally omitted |
| `42d1c66c1` | implemented | Intentionally omitted + dependency ownership reason |
| `42547c482` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `e76f61bd7` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `6b6d84e18` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `a79fd5923` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `ce2cc9683` | implemented | Equivalent + verified |
| `0505be87f` | partial | Native adaptation and populated source load verified; matrix open |
| `cd5d90a33` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `4eed71bff` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `9092fb526` | partial | Implemented; native delivery contract and source roundtrip pass; matrix open |
| `20cc33eb9` | implemented | Ported + catalog reconciliation verified |
| `226d08563` | implemented | Assets and final metadata verified; native binding gate passes |
| `30333fada` | implemented | Assets and final metadata verified; native binding gate passes |
| `5fd54b37b` | implemented | Ported + catalog reconciliation verified |
| `a9ef93d60` | implemented | Assets and final metadata verified; native binding gate passes |
| `cfba06c6f` | implemented | Ported + catalog reconciliation verified |
| `e178f2fd7` | implemented | Ported + catalog reconciliation verified |
| `0b4be45eb` | implemented | Audited empty commit |
| `5a55dd5c1` | implemented | Ported + catalog reconciliation verified |
| `a98264142` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `5efdf13eb` | implemented | Assets and final metadata verified; native binding gate passes |
| `0e3c389bf` | implemented | Assets and final metadata verified; native binding gate passes |
| `779aa81ed` | implemented | Assets and final metadata verified; native binding gate passes |
| `e83cb28ae` | implemented | Assets and final metadata verified; native binding gate passes |
| `89c141af8` | implemented | Assets and final metadata verified; native binding gate passes |
| `da6adeabc` | implemented | Ported + catalog reconciliation verified |
| `407ebb242` | implemented | Assets and final metadata verified; native binding gate passes |
| `0e0ac5fd4` | implemented | Ported/equivalent + verified (limits recorded) |
| `a25f80667` | implemented | Ported/equivalent + verified (limits recorded) |
| `258bb0dbd` | implemented | Assets and final metadata verified; native binding gate passes |
| `979bfd966` | implemented | Assets and final metadata verified; native binding gate passes |
| `9dedfb2a0` | partial | Native portion verified; foreign conversion open |
| `0e274b752` | implemented | Ported + catalog reconciliation verified |
| `71138d21c` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `a0d1ab2bd` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `c82bb8687` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `c47b0f6f5` | implemented | Equivalent by source audit; formation regression verification already recorded |
| `f7c09d8c3` | implemented | Ported + catalog reconciliation verified |
| `e602fc2f6` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `2a6baab64` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `6e2d4d515` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `df314cd32` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `1a6474ac6` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `f5d2669b0` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `73e8983ec` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `775f1d7fd` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `abdba560a` | implemented | Assets and final metadata verified; native binding gate passes |
| `b342bfe77` | implemented | Superseded + final replacement verified |
| `e41be213f` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `b2a26698f` | implemented | Assets and final metadata verified; native binding gate passes |
| `b242122f1` | implemented | Addressed: intentionally omitted |
| `4bde994d9` | implemented | Intentionally omitted + API ownership reason |
| `56fe293e9` | implemented | Assets and final metadata verified; native binding gate passes |
| `46cb8980b` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `fd4936dc8` | implemented | Equivalent + water rendering verified |
| `945c98e98` | implemented | Assets and final metadata verified; native binding gate passes |
| `7d0bd503c` | implemented | Assets and final metadata verified; native binding gate passes |
| `22c08c95b` | implemented | Assets and final metadata verified; native binding gate passes |
| `65ee7f099` | implemented | Assets and final metadata verified; native binding gate passes |
| `d3e08f272` | implemented | Addressed: intentionally omitted |
| `1ac6f6c32` | implemented | Assets and final metadata verified; native binding gate passes |
| `37fff96c4` | partial | Native fix + four-rotation roundtrips verified; source fixture open |
| `c9aa24b8a` | partial | Native persistence verified; source migration fixture open |
| `06c5f9d75` | implemented | Addressed: intentionally omitted |
| `f16c7020e` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `e09631256` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `82939eb22` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `ac707d20b` | implemented | Ported + catalog reconciliation verified |
| `9b527a450` | implemented | Assets and final metadata verified; native binding gate passes |
| `9c5ee86db` | implemented | Assets and final metadata verified; native binding gate passes |
| `c5a163b1c` | implemented | Ported + catalog reconciliation verified |
| `5370eafc5` | implemented | Ported + catalog reconciliation verified |
| `8ec484240` | implemented | Assets and final metadata verified; native binding gate passes |
| `9bca59991` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `c1bcd15cc` | implemented | Assets and final metadata verified; native binding gate passes |
| `0ea7cde03` | implemented | Assets and final metadata verified; native binding gate passes |
| `619ae51c2` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `a98c2a2e5` | implemented | Superseded + final replacement verified |
| `d2f55d0ff` | implemented | Ported + catalog reconciliation verified |
| `3bdd1189d` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `0fa5eb2e2` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `8faf80ad3` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `ce2bca95c` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `4d7e8ff53` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `69c698276` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `62a791627` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `ecf4278d1` | partial | Partially addressed; remaining hunks open |
| `b2925cea0` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `9949a5aad` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `ae8c92165` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `4bcccdcfa` | implemented | Ported + catalog reconciliation verified |
| `83991cf22` | implemented | Ported + catalog reconciliation verified |
| `a79c54d48` | partial | Partially addressed; remaining hunks open |
| `ee79d327e` | implemented | Ported + catalog reconciliation verified |
| `01f774b39` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `9280fea3a` | implemented | Ported/equivalent + verified (limits recorded) |
| `a91c6873a` | partial | Native portion verified; foreign conversion open |
| `ca8470f06` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `d73354f94` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `bf7255d5a` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `547b18c2f` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `19bb6f58d` | implemented | Ported + verified |
| `b20d57491` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `d9870f5bd` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `905594575` | implemented | ported + verified |
| `17b05668b` | partial | Native feature slice implemented; mixed compatibility work open |
| `d0b08cbfa` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `15e3f575d` | partial | Native adaptation implemented; foreign conversion open |
| `78fea7b2f` | implemented | Ported + catalog reconciliation verified |
| `09f3d1543` | implemented | Ported + catalog reconciliation verified |
| `570f27707` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `8aa190664` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `f9f98ac4e` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `af9ea7d88` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `e59ca3c3d` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `1dcac2922` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `d39119ca8` | partial | Producer stride adapter implemented and tested; runtime hydration open |
| `dea75f8d7` | implemented | Assets and final metadata verified; native binding gate passes |
| `abef5c48f` | partial | Native five-religion requirements/warnings/demands and SVV 211 roundtrip verified; foreign hydration open |
| `a20aa0dd9` | partial | Exact city sizing and missing-tail decoder verified; runtime repair publication open |
| `494425b8b` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `b260518ca` | partial | Implemented; manual fullscreen interaction acceptance pending |
| `a3a83f785` | implemented | Implemented; editor render checked |
| `b38e4680c` | implemented | Assets and final metadata verified; native binding gate passes |
| `9c451b94c` | partial | Native adaptation implemented; foreign conversion open |
| `9ea738786` | implemented | Implemented + source model invariants and roundtrip verified |
| `1f578c690` | partial | Native adaptation implemented; foreign conversion open |
| `d9a93750d` | partial | Native copy contracts verified; foreign conversion open |
| `381449f16` | partial | Partially addressed; remaining hunks open |
| `f3fef1a34` | implemented | Ported + verified |
| `d2bfabc5e` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `46e9537f3` | implemented | Ported/equivalent + verified (limits recorded) |
| `aa9f31ab4` | partial | Native implemented and tested; corpus timing failures recorded; foreign SVX conversion open |
| `d8b9e41bc` | implemented | Addressed: intentionally omitted |
| `974f7e529` | partial | Locale payload verified; source 26 roundtrip verified; transitional fixture open |
| `b4f123b82` | implemented | Implemented + real upstream action-44 import, native roundtrip and 3,000-frame soak verified |
| `0e81902b9` | partial | Implemented; mixed-storage click verification pending |
| `3b44818cf` | implemented | Ported + catalog reconciliation verified |
| `c2747bc4a` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `ee79be310` | implemented | Ported/equivalent + verified (limits recorded) |
| `984228b76` | implemented | Native arithmetic verified |
| `f13c7d65e` | implemented | Superseded + final replacement verified |
| `d1ffc79b6` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `5a64ec25c` | implemented | equivalent + verified |
| `2b8d428a6` | partial | Ported + UI verified; arithmetic intentionally deferred by D13 |
| `85529e47c` | implemented | Ported + catalog reconciliation verified |
| `719f4860a` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `87b7d8b4f` | implemented | Ported native safety; missing-route and serialized-alignment contracts verified |
| `d879725d2` | implemented | Ported/equivalent + verified (limits recorded) |

Sources: [current ledger](augustus_sync_2026_09_05_ledger.md), [April ledger](archive/augustus_sync_2026_04_21_ledger.md), [implementation evidence](augustus_sync_gameplay_audit_2026_09_08.md).

## Manual-test deployment and D19 explanation

The user authorized deployment on September 8. The refreshed Release build, launcher, GraphicsExtractor DLL, load/save DLL, debug symbols and all four authored mod trees were deployed to `D:/Games/GOG Games/Caesar 3`. All four required runtime hashes match the build output. The deployment verified preservation of 21,181 extracted graphics/metadata files. Logs: `out/upstream-completion/manual-deploy-build.log`, `manual-deploy.log` and `manual-deployed-validation.log`.

The installed executable passed native Consul SVV, legacy Clerk SAV and authored Augustus Clerk SVX loads, native roundtrips and **5,000 rendered frames per city**. Initial repair warnings were respectively 0, 2 and 6; each canonical native reload and subsequent soak was clean. The Augustus fixture also passed exact authored model/accounting comparisons. Tests ran hidden and windowed with an isolated configuration; no interactive/fullscreen game was left open. This is a manual-test deployment, not an ancestry merge or a claim that D19 and remaining migration cases are complete.

This deployment fixes the public test schema at **SVV 211 and scenario 28**. Future incompatible changes must use new version gates; these versions can no longer be treated as unpublished scratch layouts.

D19 concerns the scenario `change_production_rate` action when its resource is `troops`. It is separate from the fixed worker-pool setting. The actual fetched Augustus code computes:

`recruitment delay threshold = integer(base delay × troops percentage / 100)`

Staffing and mess-hall food stress establish the base delay. A fully staffed barracks with no food penalty starts at 8. The rate scales the whole delay, and the same recruitment loop produces soldiers or tower sentries according to priority.

| Scenario value | Delay threshold with base 8 | Effect |
| ---: | ---: | --- |
| 0 | 0 | Recruitment attempt every eligible update; does not disable recruitment |
| 50 | 4 | Faster |
| 100 | 8 | Normal |
| 150 | 12 | Slower |
| 200 | 16 | Slower still |

These are thresholds in the source update cadence, not seconds or exact throughput ratios: the counter must exceed the threshold, and ordinary resource/formation eligibility still applies.

Our native recruitment retains the staffing/food calculation but has no production-rate owner for troops. Therefore ordinary recruitment continues, an event targeting the troop rate currently has no effective consumer, and a foreign archive with a non-default saved troop rate cannot finish import. D19 has not been silently implemented during deployment.

Preserving the upstream arithmetic is straightforward and compatible; the data/UI should accurately call it a **recruitment delay multiplier**. Higher-means-faster semantics would require inverse conversion of imported source values, preservation of zero and integer rounding, and source-domain handling of future add/set formulas. For example, a source event adding 50 to 100 must make the delay 150%, whereas adding 50 to a speed multiplier would do the opposite. Converting the initial saved number alone is insufficient.

After examining the zero and additive-action cases, the recommendation is now to retain upstream semantics and improve the label, with the rate owned by data. This revises the earlier preliminary higher-means-faster recommendation. D19 remains pending the user's choice.
