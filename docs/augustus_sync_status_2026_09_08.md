# Merge status — September 8, 2026

Final upstream target: `95e120d80babd55e93a6e5d755a4971ddbb4b2a5`. The queue contains 209 unique reachable commits; 19 additional April commits were already ancestors and required renewed semantic review. Every row was inspected against its code/data owners. Native closure commit `65a597c63` passed the final installed-binary checks. The accompanying merge records the pinned upstream target as a parent, leaving this branch zero commits behind that target while preserving the audited native ports and approved deferral.

**Implemented** includes native ports, verified equivalents/supersessions, approved exclusions and empty history. It does not mean literal code identity or certification on untested platforms. **Partial** is retained for a mixed commit with an explicitly deferred payload. Counts describe implementation dispositions, not Git distance.

| Status | Current queue | April revisits | Total |
| --- | ---: | ---: | ---: |
| Not started | 0 | 0 | 0 |
| Deferred | 0 | 0 | 0 |
| Implemented | 208 | 19 | 227 |
| Partial | 1 | 0 | 1 |
| **Total** | **209** | **19** | **228** |

The one partial commit is `2b8d428a6`: its sidebar features are implemented; production-average rounding and the 102-percent efficiency arithmetic remain explicitly deferred. The combined workforce slider is also deferred as a separate requested design change, not another upstream commit. D19 is complete.

## April revisits

| Commit | Status | Disposition and evidence |
| --- | --- | --- |
| `165b7c2a3` | implemented | Draw-tile guards prevent duplicate overlay tops. Native overlay owners retain their layering; rendered city/overlay contracts and the full gate cover the replacement. |
| `8251ca91f` | implemented | Hippodrome composition children follow the owner's mothball state and suppress duplicate employment columns/tooltips. The real multipart building contract checks both. |
| `c122ab18f` | implemented | Reconciled singular/plural health labels, four-load depot threshold and formation-standard visibility. Generic plural identity and figure visibility data replace ordinal switches; current contracts and catalog audit pass. |
| `ddfcde631` | implemented | All sixteen added wild-boar frames are accounted for in the distributed asset audit. Upstream adds artwork, not a new gameplay type. |
| `5b2a592d3` | implemented | The proposed shared-building singleton representation is superseded by native tile/foundation ownership and compositions. Reviewed destruction/undo/count/save consumers; source surface-record repair, native placement/cancel/undo and canonical save tests validate the replacement. |
| `6c4c82c30` | implemented | Upstream render-file consolidation is superseded by native render commands/phases and building graphics modules. Retained functional tile guards, layering, water/grid colors and callbacks; do not import obsolete C ownership or duplicate build entries. |
| `e69bfb98f` | implemented | Native overlay owners implement terrain/building/figure selection and per-overlay tooltips. Source API/file rearrangement is superseded; actual composition and rendered overlay tests exercise the replacement. |
| `83b3c57e7` | implemented | The follow-up's footprint/top ordering is represented by native foundation anchors and draw-tile top dispatch. Water, desirability and native overlays use those owners. |
| `bb56ac880` | implemented | Editor active price/demand counts are checked through creation, activation and deletion; all affected catalogs are reconciled. |
| `ae6c183fe` | implemented | Real depot cart reroutes when destination changes, retains its carried resource when the order resource changes, recalls to source, and unloads four loads exactly once. The contract uses native owners and action dispatch. |
| `1bedb8599` | implemented | Granary and warehouse tests protect maintained stock, make empty-all stock available, respect Caesar permissions and dispatch exactly the requested quantity. |
| `105c02e70` | implemented | Real reservoir publication replaces nine aqueduct cells and gives all cells the correct owner. |
| `5a9a8c6f6` | implemented | Real road and highway publication crosses existing aqueducts while retaining both surfaces. |
| `fe9637540` | implemented | Cancel and undo restore exact terrain, graphic identity and owner at those crossings. |
| `cbd181ae1` | implemented | Editor tree/meadow/rock/custom-earthquake previews use the reviewed terrain group/ring logic. The editor gate renders its map and action windows and roundtrips terrain references. |
| `7931b9e22` | implemented | Arbitrary aqueduct placement under a reservoir is handled by its actual foundation cells, covered by the nine-cell test above. |
| `b8923f053` | implemented | Source readers retain their versioned identity widths; native previews use current ledger identities. Source-produced imports and canonical save/scenario readers pass. |
| `caa61f5ce` | implemented | Minimap selection checks aqueduct/wall terrain before building colors and gives palisades wall colors. Source verified and exercised by city/editor minimaps; no pixel-perfect equivalence claim across native terrain palettes. |
| `2de6361d8` | implemented | Reviewed all mixed paths. Mouse focus, nullable text width, versioned hotkey migration and extraction offsets have native equivalents. Added the missing Android system-bar inset listener, toolbar and scroll layout. Retain the reviewed SDL2 Gradle/SDK toolchain; upstream SDL3 packaging/version churn and its absent native Flatpak manifest are not imported. Catalog audit passes; Android XML parses, but no device/APK certification is claimed. |

## Current queue

The full rationale and code/data links remain in the [main ledger](augustus_sync_2026_09_05_ledger.md). The [closure audit](augustus_sync_closure_2026_09_08.md) records current test coverage and limits, including source fixtures versus synthetic boundary cases and platform limits.

| Commit | Status | Recorded disposition |
| --- | --- | --- |
| `016d5254c` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `5ff7e3d24` | implemented | ported + asset equivalence verified |
| `a1b14e6f7` | implemented | Implemented; Release and extraction verified |
| `90a7a9c11` | implemented | Ported + verified by >4-billion-pixel regression |
| `537e15ca0` | implemented | Ported + verified |
| `395b9f38d` | implemented | Equivalent + source verified |
| `427e4ae5b` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `5b24bc2e0` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `a2bab8c9e` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `00d6860d8` | implemented | Ported/equivalent + verified |
| `9b18c25ac` | implemented | ported + asset/binding verified |
| `56f90fa7f` | implemented | Ported + native render verified |
| `50e257d8e` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `58a0592fb` | implemented | Superseded + replacement verified |
| `10b44c769` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `fea55b845` | implemented | ported + asset/binding verified |
| `6516d83a5` | implemented | Equivalent + native render verified |
| `b2b05d726` | implemented | Ported/equivalent + verified (limits recorded) |
| `dd8b684a0` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `33316b4d7` | implemented | ported + asset/binding verified |
| `69847ce5e` | implemented | ported + asset/binding verified |
| `3eba78982` | implemented | Adapted/omitted + source verified; device limits recorded |
| `2f577a6e4` | implemented | Addressed: intentionally omitted |
| `562300a15` | implemented | Addressed: intentionally omitted SDL3-only change |
| `5bb85d5ec` | implemented | Intentionally omitted + SDL2-only reason |
| `589efb113` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `21cfcd6d6` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `3596e1b6d` | implemented | Adapted/omitted + source verified; device limits recorded |
| `9c3374492` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `83c1ed5bb` | implemented | Assets and final metadata verified; native binding gate passes |
| `68b6c312b` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `6f076248f` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `dd599b9f7` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `1de00fae9` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `c2073190e` | implemented | Ported + verified |
| `8dd168d7a` | implemented | equivalent + verified |
| `2f9aa4c74` | implemented | Ported + verified |
| `76b820f05` | implemented | equivalent + verified |
| `b803bb6fd` | implemented | equivalent + verified; inactive script intentionally omitted |
| `bbb5d428c` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `a9b649d9c` | implemented | Equivalent + verified: absent-route contract and 3000-frame native gate |
| `6ac18fd93` | implemented | Superseded + Logger contracts verified |
| `8cfd50238` | implemented | Superseded + Logger contracts verified |
| `e200efebc` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `28d9b3b23` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `49897e5ff` | implemented | Superseded + Logger contracts verified |
| `9d8fb07bb` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `6c6becbb9` | implemented | Superseded + replacement verified |
| `a81cf0b50` | implemented | Addressed: SDL2 equivalent verified |
| `80b3a0dda` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `d80f1696e` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `d43550376` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `51c9100d9` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `9c45f5f52` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `ec37f58c7` | implemented | Addressed: intentionally omitted |
| `74a82aa9b` | implemented | Intentionally omitted + dependency ownership reason |
| `dc7d304a9` | implemented | Addressed: intentionally omitted |
| `42d1c66c1` | implemented | Intentionally omitted + dependency ownership reason |
| `42547c482` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `e76f61bd7` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `6b6d84e18` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `a79fd5923` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `ce2cc9683` | implemented | Equivalent + verified |
| `0505be87f` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `cd5d90a33` | implemented | Equivalent/ported or intentionally superseded; source verified, native gate passes (limits recorded) |
| `4eed71bff` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `9092fb526` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
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
| `9dedfb2a0` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
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
| `37fff96c4` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `c9aa24b8a` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
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
| `ecf4278d1` | implemented | Ported/equivalent + source/native validation complete (see closure audit) |
| `b2925cea0` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `9949a5aad` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `ae8c92165` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `4bcccdcfa` | implemented | Ported + catalog reconciliation verified |
| `83991cf22` | implemented | Ported + catalog reconciliation verified |
| `a79c54d48` | implemented | Ported/equivalent + source/native validation complete (see closure audit) |
| `ee79d327e` | implemented | Ported + catalog reconciliation verified |
| `01f774b39` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `9280fea3a` | implemented | Ported/equivalent + verified (limits recorded) |
| `a91c6873a` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `ca8470f06` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `d73354f94` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `bf7255d5a` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `547b18c2f` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `19bb6f58d` | implemented | Ported + verified |
| `b20d57491` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `d9870f5bd` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `905594575` | implemented | ported + verified |
| `17b05668b` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `d0b08cbfa` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `15e3f575d` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `78fea7b2f` | implemented | Ported + catalog reconciliation verified |
| `09f3d1543` | implemented | Ported + catalog reconciliation verified |
| `570f27707` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `8aa190664` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `f9f98ac4e` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `af9ea7d88` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `e59ca3c3d` | implemented | Source/data audit complete; native contracts and rendered gate pass (limits recorded) |
| `1dcac2922` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `d39119ca8` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `dea75f8d7` | implemented | Assets and final metadata verified; native binding gate passes |
| `abef5c48f` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `a20aa0dd9` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `494425b8b` | implemented | Equivalent/ported + verified (source and native gate; limits recorded) |
| `b260518ca` | implemented | Adapted/equivalent + source and Windows contracts verified; platform limits recorded |
| `a3a83f785` | implemented | Implemented; editor render checked |
| `b38e4680c` | implemented | Assets and final metadata verified; native binding gate passes |
| `9c451b94c` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `9ea738786` | implemented | Implemented + source model invariants and roundtrip verified |
| `1f578c690` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `d9a93750d` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `381449f16` | implemented | Ported/equivalent + source/native validation complete (see closure audit) |
| `f3fef1a34` | implemented | Ported + verified |
| `d2bfabc5e` | implemented | Equivalent/ported + verified (see evidence and limits) |
| `46e9537f3` | implemented | Ported/equivalent + verified (limits recorded) |
| `aa9f31ab4` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `d8b9e41bc` | implemented | Addressed: intentionally omitted |
| `974f7e529` | implemented | Ported/equivalent + boundary contracts, source imports and canonical reload verified (matrix limits recorded) |
| `b4f123b82` | implemented | Implemented + real upstream action-44 import, native roundtrip and 3,000-frame soak verified |
| `0e81902b9` | implemented | Ported/equivalent + source/native validation complete (see closure audit) |
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
| `ff6eacd9d` | implemented | Ported/equivalent + source/native validation complete (see closure audit) |
| `76b51d7ab` | implemented | Ported/equivalent + source/native validation complete (see closure audit) |
| `95e120d80` | implemented | Equivalent + source verified |

## D19

Recruitment uses a generic production-method delay factor, bound from Julius mod data and inherited by Augustus/Vespasian. The existing staffing/food delay is multiplied by the scenario percentage before calendar conversion. The upstream set/add domain is retained, including imported values.

| Percentage | Base delay 8 becomes |
| ---: | ---: |
| 0 | 0 (eligible attempts without an added delay) |
| 50 | 4 |
| 100 | 8 |
| 150 | 12 |
| 200 | 16 |

These are delay thresholds, not guaranteed soldier throughput: staffing, food, eligible recruits and destination capacity still apply. Native contracts cover bounds, zero, set/add/reset and sparse roundtrip; real source-produced 50%/200% saves import and reload cleanly after their initial repairs.

The deployed public save formats remain SVV211 and scenario28. This pass does not change those schemas.
