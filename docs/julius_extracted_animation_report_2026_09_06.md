# Julius extracted animation report and proposed fix

2026-09-06. The original investigation and proposal are retained below as historical findings. The implementation record at the end describes the subsequently authorized changes and validation.

The requested target is correct: `Walkers/Group_115.xml` should itself contain the complete named animations currently assembled by `Mods/Julius/Graphics/Walkers/criminal.xml`. Once its consumers use that generated group, the authored reconstruction should be removed. Each logical image group should still have one XML file containing all its directions and actions.

**What the installed files contain**

I inspected the installed files in place under `D:/Games/GOG Games/Caesar 3/Mods/Julius/Graphics`. No graphics were copied into the repository.

| Comparison | Extracted Group_115 | Authored criminal.xml |
| --- | --- | --- |
| Image entries | 112 individual sprites, `Image_0000` through `Image_0111` | 10 named sequence entries |
| Animation representation | 112 self-closing `<animation frames="1" .../>` elements; no frame children | Explicit `<frame>` lists |
| Movement | No directional sequences | Eight directions, 12 frames each, stride 8 |
| Corpse | Individual sprites 96–103 | One eight-frame sequence |
| Gesture | Individual sprites 104–111 | One 16-frame sequence, including repeats and reverse ordering |

For example, `move_ne` uses source indices `0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88`. The next direction uses `1, 9, 17, ... 89`. The gesture uses `104, 104, 105, 106, 107, 108, 109, 110, 111, 111, 110, 109, 108, 107, 106, 105`. Those repeated endpoints are part of the current sequence and must survive conversion.

The broader read-only audit used the installed `Graphics.legacy_extract.manifest` to distinguish generated files from authored overlays:

| Measure | Count |
| --- | ---: |
| Manifest-listed XML files, all present and parseable | 255 |
| Image entries in those files | 8,773 |
| Animation elements | 5,099 |
| Animations without explicit frame children | 5,098 |
| Of those, declarations with `frames="1"` | 5,054 |
| Of those, declarations with more than one frame | 44 |
| Animations with explicit frame children | 1 |

The sole explicit sequence is `Industry/Granary.xml` → `Granary_Empty`, with seven frames. The 66 files named `Group_*.xml` contain 4,722 animation declarations and no explicit frame lists. These figures describe the current installed manifest, not every possible climate extraction or a count of visibly broken animations.

**Cause and runtime consequences**

The ordinary Julius writer, [append_image_xml](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/core/legacy_image_extractor.cpp:1217), serializes each source image separately and copies its animation count, speed, reversal flag, and offsets into a self-closing element. [export_group](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/core/legacy_image_extractor.cpp:1427) calls that writer for each exported sprite. It does not reconstruct movement directions or action sequences. The granary helper is a special exception.

The missing step is interpretation of the legacy image layout before writing XML. Per-image metadata alone does not express Group_115's eight interleaved directions or its repeated gesture frames. Simply expanding each `frames="1"` tag into a one-frame list would preserve the wrong abstraction.

The runtime currently compensates for some implicit sequences. In [resolve_animation](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/assets/image_group_payload_materialize.cpp:451), a declaration without explicit frames takes the next N entries in XML document order, starting after the current entry. That can express some contiguous building animations, but cannot express the criminal's movement layout. It also silently stops at the end of the document. The numeric-group audit found 56 declarations whose requested following entries extend beyond their document; these are structural candidates for truncation, not 56 demonstrated rendering failures.

For Group_115, the implicit interpretation of `Image_0000` with `frames="1"` points at `Image_0001`; it does not reconstruct `move_ne`. Existing authored sequences and numeric selection paths can hide this deficiency during gameplay. The sprite files are present; the exported animation structure is inadequate.

There is also a metadata completeness concern: Julius's source fingerprint includes `animation.start_offset`, but the ordinary XML writer does not serialize it. The implementation should audit how each legacy animation field affects frame selection and placement, rather than assuming a frame list alone completes conversion.

The general XML format already supports explicit frames, including direct `src` references. [The frame parser](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/assets/image_group_payload_parse.cpp:314) and materializer support the structure needed here. The [Augustus exporter](C:/Users/imper/Documents/GitHub/augustus_cpp_huge/src/assets/augustus_asset_extractor.cpp:1550) also preserves explicit frame lists when present in its source. This is not a global loss of nested-animation support.

**What history establishes**

The earliest tracked introduction of `legacy_image_extractor.cpp`, commit `78821e308` on 2026-04-02 ("Add image group payloads & legacy extractor"), already contains the ordinary self-closing animation writer. Searching this file's reachable history for frame emission found the granary frame-list addition in `37f6a23d3` on 2026-06-19. The authored `criminal.xml` appeared in `547a3471e` on 2026-08-31 ("Major Cleanup and Extractor Graphics Refactor").

I have not identified a commit that removed general Julius frame-list export. The evidence establishes the current defect and a long-standing limitation in this writer; it does not establish that the August refactor caused the remembered regression. An older working output or a different historical extraction path would be needed to attribute that regression precisely. The proposed output contract does not depend on finding that revision.

**Proposed implementation**

1. Extend the extractor's legacy group mapping with compact sequence layouts. Reuse templates for interleaved directional movement and contiguous animations; allow explicit index lists for exceptional sequences such as the criminal gesture. Derive sequences from trustworthy source metadata where it is sufficient. For layouts it cannot describe, keep the necessary legacy-format knowledge in the extraction layer. Mods should not need to reconstruct those layouts, and the runtime should not infer them from XML entry order.

2. Make extraction emit complete, named entries within each group's single XML. For Group_115, emit the eight `move_*` entries, `corpse`, and `gesture`. Reference the extracted PNGs directly inside the frame lists. Keep static alternatives as static images; a single sprite's metadata should not automatically create a bogus animation.

3. Preserve rendering and playback semantics explicitly. Validate dimensions, origins, sprite offsets, speed, reversal, repeated frames, isometric footprint/top composition, and climate differences. The authored criminal XML is the sequence reference, but offsets and playback flags must also be compared with actual current rendering. Blindly copying every raw sprite's `reversible="true"` onto the reconstructed movement cycle would be unsafe.

4. Migrate consumers together with the exporter. Point Julius figure definitions at `Walkers/Group_115` and update cross-mod references, including the Augustus thief gesture. Retarget the Vespasian graphics overrides while retaining their logical scaling. Remove `Graphics/Walkers/criminal.xml` once its reconstruction is redundant. Apply the same process to other wrappers that only assemble legacy animations; retain genuinely authored compositions and distinct art.

5. Audit compatibility before removing raw image entries. Existing source XML and Augustus template translation still use `Image_NNNN` references, and the implicit loader relies on positional entries. Update these consumers and the legacy reference resolver to resolve the intended named entry or frame. If temporary aliases are needed during implementation, keep them in the same group and preserve existing ordering until those consumers are converted. Aliases must not become a permanent duplicate representation of every animation frame. The completed Group_115 should expose the meaningful sequences, not require 112 public animation entries to support a second authored wrapper.

6. Regenerate and invalidate deliberately. Bump the Julius extraction version from its current `legacy_extract_v12`, include sequence-layout changes in invalidation, and verify regeneration of dependent Augustus templates and compiled graphics caches. Remove obsolete generated output through its ownership manifest and separately remove the redundant authored wrappers from source/deployment. Exercise both a clean install and an upgrade from today's installed output. Generated graphics must remain outside the checkout's `Mods` tree.

7. Eliminate document-order reconstruction on converted paths. All newly generated animations should have explicit frame lists and validated targets. Report missing frames or invalid sequence bounds as errors rather than accepting shortened animations. Inventory any remaining authored implicit declarations before removing the legacy behavior globally; converted paths must pass with zero compatibility fallbacks.

The intended generated shape for one direction is below. This is an illustrative output contract, not an asset added to the repository; placement and playback attributes are omitted pending parity validation.

```xml
<assetlist name="Walkers\Group_115">
    <image id="move_ne" src="Image_0000">
        <animation>
            <frame src="Image_0000"/>
            <frame src="Image_0008"/>
            <frame src="Image_0016"/>
            <frame src="Image_0024"/>
            <frame src="Image_0032"/>
            <frame src="Image_0040"/>
            <frame src="Image_0048"/>
            <frame src="Image_0056"/>
            <frame src="Image_0064"/>
            <frame src="Image_0072"/>
            <frame src="Image_0080"/>
            <frame src="Image_0088"/>
        </animation>
    </image>
    <!-- The remaining directions, corpse, and gesture belong in this file. -->
</assetlist>
```

**Acceptance checks for the implementation**

- Group_115 has all ten named sequences with exactly the current criminal frame ordering, including the gesture repeats. Every referenced sprite resolves. No authored criminal reconstruction is required.
- Coverage checks classify the source sprites as static images, animation frames, or deliberate exclusions; they catch accidental losses and permit intentional reuse of frames.
- Reordering unrelated XML entries does not alter animation output. Missing frames, invalid indices, and filtered source-image gaps fail validation rather than shifting sequence membership.
- Representative directional walkers, warriors, carts, ships, contiguous building animations, granary aliases, and layered images retain their frame images, offsets, and timing. All supported climates are covered.
- Julius, Julius+Augustus, and Vespasian resolve their graphics and retain the intended scale. Validate movement, corpse, and gesture rendering, plus dependent Augustus template extraction.
- Clean extraction, repeat extraction, and upgrade from v12 produce the same intended group structure, without stale wrappers or extra XML files per direction.
- Run the repository's recent `.svv` and legacy `.sav` load/render soak gate for several thousand frames per city, with zero warning/error output and zero renderer compatibility fallbacks on converted paths.

The original report was inspection only. The following record supersedes that implementation status.

## Implementation record

The Julius extractor now owns a compact legacy sequence catalog in `src/assets/legacy_animation_layouts.h`. It emits full named animations for the converted walkers, gladiator and sentry attacks, criminal gesture, animals, gulls, explosion, flotsam, and racing horses. Group_115 has precisely eight movement sequences, `corpse`, and `gesture`; its 112 sprites remain available as PNG frame sources, without 112 public XML image aliases. Each logical group remains in one XML file. Static source images stay static. Source-defined contiguous animations expand while absolute source indices, gaps, and start offsets are available to the extractor.

The generated format records sprite anchors separately from playback. A generic, one-based `frame` attribute selects a frame from a named group/image reference. Extraction-only `legacy` mappings and absolute source ranges let the Augustus extractor resolve old numeric references across groups without counting runtime XML entries. Julius's v13 extraction fingerprint includes the sequence catalog; Augustus's v6 stamp also depends on the Julius extraction stamps.

For example, `<image id="gesture_pose" group="Walkers\Group_115" image="gesture" frame="16"/>` selects a static pose from the declared sequence. Without `frame`, an image reference inherits the complete target animation. `<image value="move_{dir}"/>` in a figure's graphics binding selects a named direction; it does not imply a numeric atlas stride. Missing selections fail rather than advancing to a neighboring XML entry.

Augustus now expands implicit source animations and assembles its figure models into complete `move_*`, corpse, and action sequences. Model generation preserves inherited Julius anchors; the dog canvas anchor is declared by extraction. The missing construction-shadow source shorthand is also resolved during extraction, replacing the runtime name-based shadow fallback. The standalone Julius harness requests Julius extraction explicitly rather than invoking the combined runtime bootstrap.

Runtime parsing requires explicit frame lists, checks declared counts, and rejects malformed or out-of-range frame selectors. Materialization reads those references directly. It no longer constructs animation membership from document order. Figure binding accepts named direction selectors; native gladiator rendering no longer reconstructs legacy atlas rows, sentry selection uses named sequences, and Augustus figures no longer depend on implicit `default_ne_01` naming. Selecting an unavailable active frame is an error rather than silently displaying the base sprite. Legacy simulation image-ID producers still exist and are not claimed to be fully removed by this slice.

Consumers were migrated with the exporter. Redundant authored criminal, animal, gull, explosion, and dog reconstructions were removed; useful flotsam/horse compositions and the Augustus thief action composition remain. Vespasian's logical scaling overrides still apply to the generated groups. Native explosion, sheep, and racing-horse playback lengths are declared explicitly rather than relying on the generic twelve-frame default.

This is a bounded separation step, not a claim that every legacy image path is gone. Auxiliary caravan, camel, lion, ship, and other unconverted numeric consumers remain supported with static entries. Their future conversion must migrate their consumers together. No extracted XML, PNG, atlas, stamp, or manifest was added to source `Mods`; extraction validation uses the installed game and ignored `extracted_graphics_sample` directories.

### Validation record

- Release builds pass for the game, GraphicsExtractor DLL, both standalone extractors, and StartupParserTest. The extraction CLIs suppress Windows crash dialogs and report fatal exceptions to stderr.
- Fresh extraction covers central, northern, and desert Julius graphics. Comparing fresh output with the upgraded installation gives **9,841 identical Julius XML/PNG files** and **10,373 identical Augustus XML/PNG files**. Repeating extraction accepts the current stamps without regenerating output.
- A subsequent hash check confirms repeat extraction preserves all **20,214 generated files**. The installed game/extractor binaries and all **1,018 authored XML files** match the workspace; obsolete authored animation wrappers are absent. The installed `dog.xml` is now generated output. See `out/animation-deployment-verify.log`.
- `tools/validate_extracted_animations.py --animations-only` checks **371 groups, 651 explicit animations, 10,728 references, and 170 model anchors**. All **9,556 Julius source PNGs** are byte-identical between clean extraction and the upgraded installation. This audit includes all generated Julius groups and Augustus groups containing explicit animations; it is not a claim that every static Augustus image is valid.
- Startup parser tests compare converted frame pixels with source PNGs, including all criminal directions/gesture/corpse, gladiator attacks, sentry attacks, and existing figure families. New fixtures verify document-order independence and reject implicit sequences, count mismatches, malformed/zero/PNG frame selectors, missing entries, and out-of-range frames. Expected negative-fixture diagnostics are distinct from the strict load/soak warning counters.

The full generated-asset audit identifies two pre-existing missing catapult rock references in the distributed Augustus `assets/Graphics/warriors.xml`: `catapult_fr_n_06_rock` and `catapult_fr_n_07_rock`. They are outside the converted animation models; the broad audit reports them as failures rather than treating unresolved references as valid output. Logs: `out/animation-asset-audit.err`, `out/animation-converted-audit.log`, and `out/animation-extraction-parity.log`.

The full startup gate exercised **70 save cases**, including all three mod stacks, recent native `.svv` files and legacy `.sav`/`.svx` files, at **3,000 rendered ticks per case**. Its first completed run passed 67 cases and reported three failures. The failures and subsequent checks remain explicit:

| Case | Full run | Final targeted recheck |
| --- | --- | --- |
| `Engineer attempt 1 26.svv` | One unresolved gladiator graphics request during combat | Fixed the generic action-selection guard: zero wait requirements mean no gate, so a negative unrelated wait counter cannot select the walking sequence instead. The city now passes migration/reload and 3,000 rendered ticks with zero soak warnings/errors. Parser coverage exercises 512 action selections across negative/zero/positive wait counters and verifies that explicit positive wait gates still apply. |
| `autosave-year.svv` | 935.4 simulation ticks/s, below the unchanged 1,000 threshold; zero soak warnings/errors | 960.8 simulation ticks/s, still below threshold; zero soak warnings/errors. This save already failed before the extractor work (966.1 in the earlier tower gate; a separate pre-tower baseline measured 981.3). This performance issue remains open. |
| `autosave-year-bak-1.svv` | 962.1 simulation ticks/s; zero soak warnings/errors | Passes at 1,001.4 simulation ticks/s with zero soak warnings/errors. Its margin is small; the initial failure is not discarded. |

The final wait-guard change was validated with the parser suite and targeted reruns of those three cases, rather than another complete 70-case run. **The full gate is not green** because the main autosave still misses the required performance threshold. No threshold or fallback counter was relaxed. Older-save repair warnings are logged during migration; the repaired save is reloaded before the strict soak. Logs: `out/animation-startup-final.log/.err`, `out/animation-parse-final.log/.err`, and `out/animation-retest.log/.err`.

The focused gameplay run passes with **empty stderr**: 384 native-caravan alive/dead selections, 504 cart direction/load variants, 12,960 criminal frames, 64 anchored dog frames, 96 citizen walk frames, road transitions and Vespasian scaling. Placement support, tower refresh, scenario overrides, accounting and other existing catch-up contracts pass as well. Its subsequent 3,000-frame soak passes at 8,039.3 steady-state simulation ticks/s. Logs: `out/animation-focused.log/.err`.

The final runtime and generated assets are deployed to `D:/Games/GOG Games/Caesar 3`. Source `Mods` contains no newly generated graphics. `git diff --check` passes. No git commit, upstream merge or ancestry adjustment was made.
