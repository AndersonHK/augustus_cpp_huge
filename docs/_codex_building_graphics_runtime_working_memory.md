# Building Graphics Runtime Working Memory

Snapshot: 2026-06-05
Workspace: `C:\Users\imper\Documents\GitHub\augustus_cpp_huge`

## 2026-06-05 extraction checkpoint
- Current durable extractor handoff: `docs/graphics_extraction_pipeline.md`.
- The standalone clean-run harness is `AugustusGraphicsExtractor.exe`, built by `AugustusGraphicsExtractor.vcxproj` from `tools/augustus_graphics_extractor/main.cpp`.
- Runtime extraction order is Julius first, Augustus second, from `src/core/image.c`. Do not move Augustus extraction back into early `src/platform/augustus.cpp` startup.
- Clean generated output should contain `Mods\Julius\Graphics` and `Mods\Augustus\Graphics`; `Mods\Vespasian\Graphics` should be absent until Vespasian has real native graphics.
- Current clean sample validation:
  - Julius: 231 XML, 8933 PNG, 8465 logical images
  - Augustus: 3200 XML, 4088 PNG, 3259 logical images
  - BuildingType graphics refs: 505 checked across the Release mod stack, 0 missing
- Julius `Aesthetics\House_Tent` intentionally exposes `Image_0000..Image_0005`. `Image_0001..Image_0005` are full-image aliases to `Aesthetics\House_Tent_Variants`, preserving legacy BuildingType option refs after the Julius group table split.
- Colosseum runtime fallback notes from May are stale. Current BuildingType XML uses generated `Health_Culture\Colosseum` entries directly.

## 2026-05-11 current graphics/animation checkpoint
- Native BuildingType graphics now use `building_runtime_graphics.cpp` for target resolution, stable-option selection, and cached `RuntimeDrawSlice` binding. The renderer-facing accessors are still `graphic_footprint()`, `graphic_top()`, and `graphic_animation(animation_cursor)`.
- Animation frame policy now lives in `src/building/animations.h/.cpp` as `BuildingAnimation`. This object owns frame cursor normalization, legacy animation gates, wine-workshop progress frames, reversible animation high-bit handling, looping animation, storage-yard flags, and fumigation animation.
- The old `src/building/animation.*` facade has been removed. Legacy overlay/non-native C++ draw paths instantiate `BuildingAnimation` directly; this keeps animation policy in the concept object instead of behind another C wrapper.
- Live native animation call chain:
  - `src/widget/city_draw.cpp::city_draw_runtime_building_animation()`
  - `building_runtime_advance_graphic_animation(b, grid_offset)`
  - `building_runtime::advance_graphic_animation(grid_offset)`
  - `BuildingAnimation::runtime_track_offset(track, should_advance=1, grid_offset)`
  - `building_runtime_get_graphic_animation_slice(b, grid_offset)`
  - `building_runtime::graphic_animation(grid_offset)`
  - `BuildingAnimation::runtime_track_offset(track, should_advance=0, grid_offset)`
- `advance_graphic_animation()` is the tick. `graphic_animation()` reads and materializes the current frame slice; it must not secretly advance the animation.
- Placement ghosts use the same generic BuildingType renderer for XML-owned graphics. `city_building_ghost.cpp` saves/restores the map sprite animation byte because the ghost preview reuses the hovered grid offset as a temporary cursor without owning real map state.
- Water-driven graphics now depend on generic BuildingType water rules and projected building state. See `docs/water_access_runtime.md` for the provider/consumer mask simulation that feeds `has_water_access` and related compatibility mirrors.

## 2026-05-04 as-is audit
- Live BuildingType graphics now center on `src/widget/city_draw.cpp` plus `src/building/building_runtime_graphics.cpp`. Older notes that name `city_with_overlay.cpp` or `city_without_overlay.cpp` as live graphics chokepoints describe the pre-split checkpoint and should be read as historical.
- XML-owned buildings with validated `<graphics>` data render through cached `ImageGroupPayload` `RuntimeDrawSlice` entries. `map_image` is kept as neutral tile bookkeeping for native-owned graphics, not as the authoritative image id.
- `building_image_get()` remains a legacy compatibility path for definitions without native graphics and for still-unexpressed special cases such as Hippodrome orientation/part selection and generic watchtower rotation/variant policy.
- Graphics XML discovery follows the active mod list in top-to-bottom precedence via `mod_manager_get_graphics_path_at()`, then root `assets/Graphics`; named Augustus/Julius graphics helpers remain for extractors and explicit source resolution.
- Vespasian, Augustus, and Julius native housing XML currently covers the full legacy house chain. Housing graphics variants are normal BuildingType graphics options selected by saved `building.variant`.

## 2026-05-04 native graphics options
- `BuildingType.graphics` targets can now contain `<options selection="stable_variant">` with one or more `<option image="...">` nodes.
- Each option inherits the enclosing target `<path>` unless it declares a per-option `path`, which supports mixed-source variant sets without adding house-specific render code.
- Runtime selection uses saved `building.variant % option_count`, and `building.variant` is included in the native graphics cache signature.
- New, evolved, converted, and old-save-loaded native graphics buildings clamp or seed `building.variant` through `building_runtime_assign_graphic_variant()`.
- Save version increased to `0xb7`; saves through `0xb6` predate native graphics variant meaning and reseed options from `map_random_get(grid_offset)` during load.
- Vespasian, Augustus, and Julius native house BuildingTypes now use normal BuildingType graphics options for their legacy house variant tables. Julius keeps the vanilla `house_small_tent` desirability threshold from upstream `c3_model.txt` data.
- Conditions and stable graphics options are separate layers: conditions choose the target by live building state, then the selected target applies `building.variant` to choose among equivalent art.
- A target may omit parent `<path>` only when every option supplies its own `path`. Load-time validation materializes each resolved option and checks the effective path/image pair.

## 2026-05-04 payload rendering correction
- XML `BuildingType.graphics` entries now render as `ImageGroupPayload` entries, not as legacy integer image groups.
- `building_image_get()` is legacy-only; it must not load an XML payload and squeeze the selected image back through `assets_get_image_id_from_path_or_name`.
- Runtime-owned buildings still populate terrain/building/multitile map bookkeeping, but `map_image` receives a neutral flat-tile sentinel while `city_draw` reads footprint/top/animation from cached `RuntimeDrawSlice` payload data.
- `building_runtime_apply_graphic_if_native()` means "the XML runtime handled map-tile graphics ownership"; callers that get true should not immediately overwrite the tile with `building_image_get()`.
- Native housing uses the same runtime graphics ownership as other XML BuildingTypes. House-specific image lookup must not coerce generated house graphics into a legacy image id; native houses install their sentinel/cache through `building_runtime_apply_graphic_if_native()` and are excluded from the legacy house image switch.
- Vespasian, Augustus, and Julius native housing chains now route every house level through BuildingType graphics, from tents through luxury palace. The legacy house image table remains only for non-native/compatibility house definitions; native houses must render through the normal runtime BuildingType payload path.
- Pottery workshops are no longer treated as graphics-data-only; their XML `Industry\Pottery_Workshop` payload is the live renderer path.
- Runtime animation frames normalize an active XML animation to frame 1 when the saved sprite cursor is empty, so ON states that use an OFF base plus animation overlay do not flash the OFF image between animation ticks.
- Payload animation frame materialization now reconstructs each explicit or implicit frame as a full `PART_BOTH` raster payload before runtime drawing. Bare XML frame references no longer reuse only the referenced entry footprint, which preserves top/full overlay content for animated XML buildings.
- Native XML animations now advance explicitly from `city_draw_runtime_building_animation()`, the same city animation layer that drives legacy image animations through `BuildingAnimation`. `graphic_animation()` reads the current cursor and returns the already-selected payload frame instead of secretly owning the tick.
- Native XML animations use the caller-provided draw-stage animation cursor instead of `building.grid_offset`. This keeps the cursor transient like legacy rendering until BuildingType migration is far enough along to consider a saved building cursor field.
- Well placement ghosts keep their bespoke water-range behavior, but now try the XML payload ghost renderer before falling back to `building_image_get_for_type()`. Well XML also uses the same legacy string-key bridge format as Theater (`main_strings.28.92`) so dynamic ids do not fall through to an invalid legacy string index in the build menu.
- Augustus/Vespasian small and large pond XML now owns the legacy climate/water matrix: central/northern maps use the north payloads, desert maps use the south payloads, and water access selects the animated ON entries. The redundant `BUILDING_SMALL_POND` and `BUILDING_LARGE_POND` image.cpp switch arms were removed.

## 2026-05-04 BuildingType graphics mapping pass
- Added missing root `<graphics>` data for shared Julius-backed buildings in Julius, Augustus, and Vespasian XML where the extracted Julius group is the real source: actor colony, barber, chariot maker, doctor, gladiator school, governor residence tiers, hospital, and native crops.
- Added Julius-native graphics for native huts and native meeting huts through `Aesthetics\Native`. Julius native decoration, native large monument, and native watchtower were removed from `Mods/Julius` because upstream Julius has no matching building enum entries or extracted graphics payloads for those Augustus-added building types.
- Added Augustus/Vespasian climate-aware graphics for brickworks, concrete maker, workcamp, native decoration, native huts, native meeting huts, native large monuments, and native watchtowers using Augustus extracted groups. Native hut XML currently selects the `01` climate variant; the legacy random alternation with the `02` hut remains a known fidelity gap.
- Added fountain graphics for all three XML sets using desirability thresholds matching the legacy `upgrade_level` tiers. Augustus/Vespasian also select the Augustus `Fountain_Desert_Fix` image for desert maps above the top desirability threshold.
- Added Augustus/Vespasian small temple graphics by reusing the Julius `Health_Culture\Temple_*` small-temple image entry. This is an intentional Julius asset dependency, not an Augustus backport into Julius.
- Added Augustus/Vespasian Pantheon completed graphics and phased construction graphics. Phase requirements mirror the legacy monument resource table so the XML construction graphics do not change delivery costs.
- Added Augustus/Vespasian Colosseum completed graphics plus phased construction graphics. A new XML graphics condition, `festival_games`, selects the Augustus naumachia, imperial games, and execution variants; the default remains the gladiator-fight image.
- Vespasian Hippodrome still has no safe XML graphics mapping. Finished Hippodrome selection depends on building orientation and first/middle/last part, while construction graphics depend on phase plus the same orientation/part split. Leave it on legacy `building_image_get()` until the XML schema can express that target matrix.
- The matching legacy `building_image_get()` switch arms were removed for XML-owned mappings from this pass: actor colony, barber, brickworks, chariot maker, Colosseum, concrete maker, doctor, fountain, gladiator school, governor residence tiers, hospital, native hut, native meeting hut, native decoration, native large monument, native watchtower, Pantheon, small temples, and workcamp.
- Farm/data-only graphics stay legacy for now, including native crops. Hippodrome also stays legacy because its correct target still depends on orientation plus first/middle/last-part selection that BuildingType graphics XML cannot express yet.
- Watchtower's new XML payload is not enough to retire the old generic watchtower `image.cpp` policy yet. The remaining legacy case still carries building variant plus city-view rotation offsets that the current BuildingType graphics condition set cannot express.

## 2026-05-04 BuildingType button localization correction
- BuildingType XML for the implemented Vespasian/Augustus/Julius button-owned buildings now uses registered `TR_*` localization keys instead of placeholder `building.*.name` keys where those keys exist.
- Theater uses the existing base-game legacy label key `main_strings.28.31`; `TR_BUILDING_THEATRE_UPGRADE_DESC` is only the upgraded-building description, not the button label.
- `BuildMenuButton` now resolves `button text_key` by key string first. It falls back to legacy `lang_get_string(28, building)` only for XML entries that still use unresolved placeholder keys.
- The temporary theater/display-key shim and the matching specific XML-owned cases were removed from `src/core/lang.cpp`; no legacy localization function ran out of cases.
- Known gap: many older XML buttons still use `building.*.name` placeholders because no corresponding registered `TR_BUILDING_*` button label exists yet. Those still rely on the generic legacy fallback until localization keys are added.

## 2026-05-04 Julius localization JSON extraction
- Julius language extraction now writes legacy Caesar group/index strings into flat `project_keys` in `Mods/Julius/Localization/<locale>.json`, matching the Augustus JSON style.
- Known legacy slots that correspond to existing `TR_*` enums use those enum names; unmapped slots remain explicit `main_strings.<group>.<index>` or `editor_strings.<group>.<index>` keys.
- JSON merge backfills legacy string slots from those project keys in mod-list order, so later mods replace only the keys they define.
- The hard-coded `lang_get_string` aliases for small temples, editor strings, and Augustus-added building labels were removed after the JSON merge learned those aliases. The function still has locale fixes, custom translations, XML identity lookup, and the generic fallback.
- The source `c3*` language binaries are not present in this workspace, so the new Julius locale JSON files could not be materially generated during this static pass.

## 2026-05-04 graphics mod-list source chain
- Assetlist loading and ImageGroupPayload source merging now follow the active mod list in precedence order instead of always probing selected mod, Augustus, Julius, and root. A Julius-only mod list therefore no longer imports Augustus graphics XML or materializes Augustus-only payload entries.
- The legacy `mod_manager_get_augustus_graphics_path()` and `mod_manager_get_julius_graphics_path()` helpers remain for named source resolution and extractors, but runtime graphics discovery starts from `mod_manager_get_graphics_path_at()`/`mod_manager_get_mod_name_at()`.
- Fountain placement ghosts now try the XML runtime ghost renderer before falling back to the legacy bespoke fountain path. The remaining fallback still handles non-runtime asset sets and the old reservoir-range animation overlay behavior.

## 2026-05-04 incomplete BuildingType completion pass
- Completed Julius, Augustus, and Vespasian BuildingType XML metadata for chariot maker, governor residence tiers, Colosseum, Hippodrome, native huts, native meeting huts, and native crops. Native decoration, native monuments, and native watchtowers are Augustus/Vespasian-only XML definitions.
- Placeable additions now include real legacy name keys and button text keys: chariot maker uses `main_strings.28.37`, Colosseum uses `main_strings.28.33`, Hippodrome uses `main_strings.28.32`, and governor residences use `main_strings.28.77` through `main_strings.28.79`.
- Chariot maker spawn moved to BuildingType XML with a new supported `charioteer` spawn figure id, preserving the legacy 7/15/30/60/90 delay bands and 50-house labor-seeker threshold. The old chariot-maker spawn branch was removed.
- Governor residences, chariot maker, Colosseum, and Hippodrome are now enum-retained XML-owned types in `building_type_legacy_migration.cpp`; their hardcoded build-menu entries and static property rows were removed. Colosseum and Hippodrome still retain legacy spawn/construction special cases where the current XML schema does not yet express all side effects.
- Native decoration, native monument, and native watchtower name switch cases were removed from `lang.cpp`; their XML identities use the existing `TR_BUILDING_NATIVE_*` keys. Native crops and Julius-native hut labels use legacy `main_strings.41.*` keys.
- Julius native decoration, native monument, and native watchtower XMLs were removed after rechecking upstream Julius `src/building/type.h`: Julius stops at `BUILDING_TYPE_MAX = 115` and does not define those Augustus extension types. The matching `Terrain_Maps` payloads also do not exist under `extracted_graphics_sample/Julius/Graphics`.
- Hippodrome still has no safe XML graphics or phased-construction payload mapping. Finished and construction graphics depend on orientation plus first/middle/last building part, which the current BuildingType graphics schema cannot express.
- Follow-up correction: Augustus/Vespasian Colosseum and native meeting hut XML now avoid extracted assetlists with missing referenced entries. See `docs/missing_graphics_payloads.md` for the source-vs-extracted payload gaps and the fallbacks currently used.
- Julius Colosseum was checked against upstream Julius `src/core/image_group.h`, `src/building/construction_building.c`, `src/building/properties.c`, and `src/widget/city_without_overlay.c`: the base city graphic is group 48 (`Health_Culture\Colosseum/Image_0000` in the extracted sample), the worker/show overlay remains legacy group 193 (`Health_Culture\Colosseum_Show`), and Julius does not mark the Colosseum fire-proof.
- Follow-up Julius mismatch pass: upstream Julius marks Hippodrome fire-proof as false (`{5, 0, 213, 0}`) and Well fire-proof as true (`{1, 1, 23, 0}`), so `Mods/Julius/BuildingType/hippodrome.xml` and `well.xml` now match those flags. The Julius entertainment button order now follows upstream `menu.c`: theater, amphitheater, Colosseum, Hippodrome, gladiator school, lion house, actor colony, chariot maker; touched labels use legacy `main_strings.28.*` keys instead of placeholder `building.*.name` keys.
- The Julius static pass also replaced remaining placeholder `building.*.name` labels in implemented Julius BuildingType XML with the corresponding legacy `main_strings.28.*` keys, corrected ad hoc button icon names to resolvable extracted Julius assetlist paths, and aligned the XML menu order for water, health, education, administration, and entertainment rows with upstream Julius `menu.c` where those rows are represented by XML buttons.
- Augustus/Vespasian lararium and latrines now have complete BuildingType XML for identity, build-menu placement, model/desirability, graphics, labor, flags, and event attrs. Lararium uses `TR_BUILDING_LARARIUM`; latrines use `TR_BUILDING_LATRINES` and climate-specific `Health_Culture\Latrine_*` graphics. The static property rows, hardcoded menu rows, image cases, redundant latrines construction case, and old latrines-only water coverage path were removed. Remaining lararium legacy behavior is intentional: it still uses the legacy road-within-2 warning.

## 2026-04-06 building graphics resolution checkpoint
- Native building footprint rendering now routes through `src/widget/city_draw.cpp`, not the older direct `const image *` helper path documented below.
- `src/building/building_runtime.cpp` is now split by ownership: wrapper/facade flow stays in `building_runtime.cpp`, graphics cache/rebuild behavior lives in `building_runtime_graphics.cpp`, and spawn policy execution lives in `building_runtime_spawn.cpp`.
- The footprint-placement bug was ultimately an extractor/data-contract bug first and a runtime ownership bug second:
  - Julius footprint exports were preserving bottom transparent padding and needed that padding trimmed at extraction time, with XML height / layer `y` preserving logical placement
  - Augustus direct-crop footprint exports needed the same bottom-padding trim rules
  - Augustus active ON variants were losing their top slice because local `group="this"` isometric inheritance collapsed to footprint-only during extraction
  - the native footprint stage also needed to submit the whole-building footprint only on the owning draw tile
- Current true state:
  - Julius and Augustus extractors now carry the main placement fix
  - Augustus extractor now resolves same-group part inheritance recursively and expands inherited full-image references into `footprint` + `top` where appropriate
  - native footprint draw ownership is corrected through `grid_offset`
  - the temporary runtime footprint compensation in `src/assets/image_group_payload_materialize.cpp` has been removed; extractor output is the placement contract now

## 2026-04-02 extractor/runtime follow-up
- `BuildingType.graphics` now also needs optional `<image value="..."/>` for grouped assets like `large_statue`.
- `large_statue.xml` in both mods now pins `Aesthetics\Statue` to `l statue anim` instead of relying on the group default.
- New generic crash/runtime breadcrumbs now live in `src/core/crash_context.h/.cpp` and are logged by `src/platform/crash_handler.c`.
- Preferred naming for new code is now:
  - `ErrorContextScope` for the C++ scope object
  - `error_context_report_*` for warning/error/fatal reporting
- `CrashContextScope` and direct `log.h` usage remain compatibility names for older code, but should not be the default choice in new runtime/widget work.
- `building_runtime.cpp`, `tile_runtime.cpp`, `image_group_payload.cpp`, and `augustus_asset_extractor.cpp` now stamp stage/context before high-risk graphics operations.
- Red runtime error dialogs should now close the game after the user presses OK instead of trying to continue.
- Augustus extractor startup failures must remain fatal to startup; do not warn-and-continue there.
- Building graphics resolution is temporarily back on soft-fallback semantics: if the new building path cannot resolve an image or animation frame, it logs once and returns `nullptr` so the legacy renderer path takes over.
- `ImageGroupPayload` needed a same-group alias fast path plus a re-entrant load guard; local references like `group="this"` or aliases to earlier images in the same group must resolve against the payload currently being built instead of recursively reloading the group.
- Water-driven graphics now key off BuildingType `<water_access>` requirement rules. The old state-level reservoir-range hook has been removed; large statues, ponds, concrete makers, fountains, and reservoirs use the generic typed-mask rule path.
- The old "best-effort fallback" assumption is no longer the active rule for explicit new-path invariants in this slice; targeted failures now prefer fatal+log.

## Current Runtime Shape
- Live city building rendering routes through `building_runtime` plus `ImageGroupPayload` when a `BuildingType` owns validated graphics.
- `building_runtime_graphics.cpp` resolves conditional targets, applies stable options with `building.variant`, and caches footprint/top/animation `RuntimeDrawSlice` entries.
- `city_draw.cpp` is the shared live-city draw seam for native building footprints, tops, and animation overlays.
- Legacy `building_image_get()` and legacy `map_image` ids remain compatibility paths for buildings without native graphics and special cases that the XML condition set cannot yet express.
- Placement ghosts and editor previews are mixed: several XML-owned ghosts now try the runtime path first, but the whole placement/editor surface is not yet fully native.

## User decisions locked in
- Build only when useful for the current task, and use `Release|x64`.
- Keep CRLF on touched files.
- Add this file to `.gitignore`.
- Replace the old `<graphic .../>` node instead of supporting both formats.
- New BuildingType graphics node uses child nodes only.
- New logical path format is relative to `Graphics`, backslash-delimited, without `Graphics\` and without `.xml`.
- Current runtime rule:
  - building has a runtime wrapper and a BuildingType definition
  - the definition resolves a validated graphics target for the live building state
  - the runtime cache materializes that target as payload-backed draw slices
  - otherwise the building stays on the legacy rendering path

## Current BuildingType XML Shape

```xml
<building type="house_small_shack">
    <graphics>
        <default>
            <path value="Aesthetics\House_Shack" />
            <options selection="stable_variant">
                <option image="Image_0000" />
                <option image="Image_0001" />
            </options>
        </default>
    </graphics>
</building>
```

```xml
<building type="future_mixed_variant_building">
    <graphics>
        <default>
            <options selection="stable_variant">
                <option path="Aesthetics\Variant_Source_A" image="Image_0000" />
                <option path="Aesthetics\Variant_Source_B" image="Image_0000" />
            </options>
        </default>
    </graphics>
</building>
```

## Important code seams
- `src/building/building_type_registry_internal.h`
  - `BuildingType` now owns nested `GraphicsDefinition`
- `src/building/building_type.cpp`
  - `GraphicsTarget::resolved_option()` applies stable option selection and inherited target paths.
- `src/building/building_type_registry_xml.cpp`
  - parser now expects structured `<graphics>` with `<default>`, optional conditional `<variant>`, target `<path>`, optional `<image>`, and optional stable `<options>`.
- `src/building/building_runtime.h/.cpp` and `src/building/building_runtime_graphics.cpp`
  - runtime now resolves new-path building images, assigns stable graphics variants, and still maintains legacy compatibility state.
- `src/widget/city_draw.cpp`
  - live city drawing asks native runtime-owned buildings for payload-backed footprint/top/animation slices before legacy tile-id rendering.
- `src/assets/image_group_payload.h/.cpp`
  - path-keyed group manager now exposes default-image lookup, caches failed loads, stores implicit animation metadata/frame keys plus footprint/top composition data, and clones whole-image aliases including top/animation
- `src/core/image_payload.h/.cpp`
  - payload-backed `image` compatibility view already exists
- `src/graphics/image.cpp`
  - now has pointer-based isometric helpers for direct `const image *` draws and a generic pointer draw helper for payload-backed animation frames
- `src/building/image.cpp`
  - legacy image-id fallback remains for non-native graphics and special cases, but native house graphics are excluded from the house-specific legacy image table.

## Current compatibility doctrine
- Keep `map_image` populated enough for untouched systems, but native-owned payload graphics may use a neutral sentinel rather than a meaningful legacy sprite id.
- New runtime render lookup prefers runtime-owned footprint/top/animation slices and falls back to legacy ids only when the definition does not own validated native graphics or when a known special case remains legacy.
- Missing optional extracted graphics overrides may warn/fallback, but malformed live XML and invalid graphics options should fail at BuildingType load time.
- Native graphics option failures are caught at BuildingType load time. Validated options should render through the normal runtime payload path instead of falling back to house-specific or image-group-table logic.

## Current runtime resolver scope
- Supported BuildingType graphics are no longer a small first-pass family allowlist. Any loaded BuildingType with a validated root graphics target can render through the native payload path unless code explicitly keeps that building on a legacy special path.
- Conditional graphics select a target from live building state; options then select among equivalent images using `building.variant`.
- The main known legacy exceptions are still data-contract gaps such as Hippodrome orientation/part matrices and generic watchtower city-view rotation/variant handling.

## Content caveats
- Live BuildingType XML files in `Mods/Vespasian/BuildingType` and `Mods/Augustus/BuildingType` were migrated to structured `<graphics>`.
- Vespasian, Augustus, and Julius housing XML now cover the full native house chain. Julius uses only graphics available under the Julius source chain, with `Aesthetics\House_*` paths and the upstream Julius house variant offsets.
- Older entries in this working-memory file may mention scaffolding guesses. Current bundled housing and the documented native graphics mappings have been checked against the extracted payload names available in the workspace.
- Augustus extractor now distinguishes materialized images from alias-only wrapper nodes so anonymous aliases no longer consume `Image_####` slots.
- Augustus isometric replacement export now prefers canonical template inheritance for numeric Julius references and recursive same-group part inheritance for local Augustus references.
- Missing Julius template files for brand-new Augustus-only assets are still treated as an expected bootstrap miss, but local isometric wrappers now inherit parts from already-inferred Augustus images instead of collapsing through layer-count heuristics.
- Active Augustus ON variants such as arena, amphitheater, colosseum, and large statue depend on that same-group part inheritance to preserve their top slice and keep animation overlays aligned with the base image.
- Legacy template parsing must tolerate `animation` / `frame` children because extracted Julius XML includes them, even when we only care about layer parts for inheritance.
- Augustus canonical output-path selection should only look at alias-only wrapper images, not materialized composite images that happen to reference a legacy group internally.

## Current files to re-read before graphics work
- `docs/building_type_legacy_reference_ledger.md`
- `docs/save_load_runtime_bridges.md`
- `Mods/Vespasian/BuildingType/_README.md`
- `Mods/Vespasian/HousingType/_README.md`
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_graphics.cpp`
- `src/building/building_runtime_api.h`
- `src/building/building_type.cpp`
- `src/building/building_type_registry_internal.h`
- `src/building/building_type_registry_xml.cpp`
- `src/building/building_type_registry.cpp`
- `src/building/image.cpp`
- `src/widget/city_draw.cpp`
- `src/assets/image_group_payload.h`
- `src/assets/image_group_payload.cpp`
- `src/assets/image_group_payload_materialize.cpp`
- `src/core/legacy_image_extractor.cpp`
- `src/assets/augustus_asset_extractor.cpp`
