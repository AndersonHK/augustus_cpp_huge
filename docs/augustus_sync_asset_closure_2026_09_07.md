# Asset-only commit closure

Target: `87b7d8b4f41bbb485f5c4ccf38c9961b422dd1b1`. The installed `assets-4.0.0.1495-719f4860a-development.zip` was revalidated against final upstream blobs for every asset path touched by the queue, including changed-then-reverted and subsequently removed files. Result: 13 exact files, 11 equivalent packed XML groups, 411 source images represented in packed groups, 136 unused source images and four removed upstream paths. No missing/different pixels or animation metadata. `tools/audit_augustus_assets.py` reproduces `out/upstream-completion/assets.json` without extracting images into the repository.

Each row below has only asset paths; no source-code hunk is closed by this table. Later revisions supersede earlier images. Unused artwork has no final graphics-XML consumer; source code also has no Reed_Gatherer, Hunting_Lodge, Papyrus_Maker or GameMeat consumer, so no additional gameplay is inferred. Existing native monument, caravanserai, temple-module, mint, arena, scenery and weather consumers use the named extracted groups. The current hidden startup/catch-up/3000-frame run reports no binding warnings or errors. New scrollbar art is available through extraction while XML windows retain their selected controls. Julius retains its original graphics.

| Commit | Added or refined | Exact / packed XML / packed image / unused image / removed |
| --- | --- | --- |
| `83c1ed5bb` | Southern native large-monument image tweak. | 0 / 0 / 1 / 0 / 0 |
| `226d08563` | Remaining game-meat carts, reed-gatherer/papyrus stocks, reeds storage/panels. | 0 / 0 / 20 / 0 / 1 |
| `30333fada` | Industry art refinements and industry/UI/walker XML bindings. | 0 / 3 / 16 / 0 / 1 |
| `5efdf13eb` | Climate land/water and large marshland asset set/XML. | 0 / 1 / 79 / 0 / 0 |
| `0e3c389bf` | Marshland art/XML and rain/latrine/marsh/thunder audio. | 6 / 1 / 11 / 0 / 0 |
| `779aa81ed` | Education upgrade/college/armoury art, central land, native war horn. | 1 / 0 / 4 / 1 / 0 |
| `89c141af8` | Snow and wind audio updates. | 2 / 0 / 0 / 0 / 0 |
| `258bb0dbd` | Garden walls, haystacks, reeds/game-meat stocks, land masks and XML. | 0 / 3 / 25 / 0 / 3 |
| `979bfd966` | Empire fort icon assets. | 0 / 0 / 0 / 3 / 0 |
| `abdba560a` | Beach tiles and central-climate hills. | 0 / 0 / 0 / 92 / 0 |
| `56fe293e9` | Northern and southern hill assets. | 0 / 0 / 0 / 24 / 0 |
| `945c98e98` | Nesting-ground assets and bird animation. | 0 / 0 / 0 / 12 / 0 |
| `7d0bd503c` | Venus Grand Temple module art/XML. | 0 / 1 / 5 / 0 / 0 |
| `22c08c95b` | Caravanserai active animation artwork. | 0 / 0 / 6 / 0 / 0 |
| `65ee7f099` | Upgraded arena and Mercury/Venus module art. | 0 / 0 / 4 / 0 / 0 |
| `1ac6f6c32` | Mint animations and executions/games/naumachia overlay art. | 0 / 0 / 8 / 0 / 0 |
| `9b527a450` | Northern native meeting-hut artwork. | 0 / 0 / 1 / 0 / 0 |
| `9c5ee86db` | Southern native huts/meeting hut and all-climate native watchtowers. | 0 / 0 / 5 / 0 / 0 |
| `8ec484240` | Native decoration/watchtower changes and empire panel artwork/XML. | 0 / 2 / 4 / 0 / 0 |
| `c1bcd15cc` | Scrollbar arrow/thumb state images and XML. | 0 / 1 / 12 / 0 / 0 |
| `0ea7cde03` | Dark scrollbar track and plus/minus assets/XML. | 0 / 1 / 10 / 0 / 0 |
| `dea75f8d7` | Scrollbar thumb/line assets and XML refinements. | 0 / 1 / 6 / 0 / 0 |
| `b38e4680c` | Willow top-layer art and aesthetics XML. | 0 / 1 / 1 / 0 / 0 |
| `a9ef93d60` | Reed-gatherer base reference and reeds stock offsets. | 0 / 1 / 0 / 0 / 0 |
| `b2a26698f` | Four beach source images; none is referenced by final upstream XML. | 0 / 0 / 0 / 4 / 0 |
| `e83cb28ae` | Depot/game-meat and station stock layers and images. | 0 / 1 / 6 / 0 / 0 |
| `407ebb242` | Trade-ledger button states and compositions. | 0 / 1 / 2 / 0 / 0 |
