# Building Graphics Runtime Working Memory

Snapshot: 2026-04-06
Workspace: `C:\Users\imper\Documents\GitHub\augustus_cpp_huge`

## 2026-05-04 payload rendering correction
- XML `BuildingType.graphics` entries now render as `ImageGroupPayload` entries, not as legacy integer image groups.
- `building_image_get()` is legacy-only; it must not load an XML payload and squeeze the selected image back through `assets_get_image_id_from_path_or_name`.
- Runtime-owned buildings still populate terrain/building/multitile map bookkeeping, but `map_image` receives a neutral flat-tile sentinel while `city_draw` reads footprint/top/animation from cached `RuntimeDrawSlice` payload data.
- `building_runtime_apply_graphic_if_native()` means "the XML runtime handled map-tile graphics ownership"; callers that get true should not immediately overwrite the tile with `building_image_get()`.
- Native housing uses the same runtime graphics ownership as other XML BuildingTypes. House-specific image lookup must not coerce `Housing\House_Tent` XML graphics into a legacy image id; native tents install their sentinel/cache through `building_runtime_apply_graphic_if_native()` and are excluded from the legacy house image switch.
- Vespasian's native housing chain now routes every house level through BuildingType graphics, from tents through luxury palace. The legacy house image table remains only for non-native/compatibility house definitions; native Vespasian houses must render through the normal runtime BuildingType payload path.
- Pottery workshops are no longer treated as graphics-data-only; their XML `Industry\Pottery_Workshop` payload is the live renderer path.
- Runtime animation frames normalize an active XML animation to frame 1 when the saved sprite cursor is empty, so ON states that use an OFF base plus animation overlay do not flash the OFF image between animation ticks.
- Payload animation frame materialization now reconstructs each explicit or implicit frame as a full `PART_BOTH` raster payload before runtime drawing. Bare XML frame references no longer reuse only the referenced entry footprint, which preserves top/full overlay content for animated XML buildings.
- Native XML animations now advance explicitly from `city_draw_runtime_building_animation()`, the same city animation layer that calls legacy `building_animation_offset()`. `graphic_animation()` reads the current cursor and returns the already-selected payload frame instead of secretly owning the tick.
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
- Augustus/Vespasian lararium and latrines now have complete BuildingType XML for identity, build-menu placement, model/desirability, graphics, labor, flags, and event attrs. Lararium uses `TR_BUILDING_LARARIUM`; latrines use `TR_BUILDING_LATRINES` and climate-specific `Health_Culture\Latrine_*` graphics. The static property rows, hardcoded menu rows, image cases, and redundant latrines construction case were removed. Remaining legacy-specific behavior is intentional: lararium still uses the legacy road-within-2 warning, and latrines still use the legacy radius/access updater because the current XML water provider schema only supports well/fountain/reservoir providers.

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
- Large statue definitions now explicitly declare `water_access mode="reservoir_range"` so the existing legacy animation gate for fountains can still key off `has_water_access` under the runtime-managed path.
- The old "best-effort fallback" assumption is no longer the active rule for explicit new-path invariants in this slice; targeted failures now prefer fatal+log.

## Goal
- Route live city building rendering through `building_runtime` plus `ImageGroupPayload` when a `BuildingType` defines graphics.
- Keep compatibility by falling back to legacy `building_image_get` and legacy `map_image` ids when the new path is absent or incomplete.
- Keep this rollout limited to live city rendering for now.

## User decisions locked in
- Do not build unless the user explicitly asks.
- Keep CRLF on touched files.
- Add this file to `.gitignore`.
- Replace the old `<graphic .../>` node instead of supporting both formats.
- New BuildingType graphics node uses child nodes only.
- New logical path format is relative to `Graphics`, backslash-delimited, without `Graphics\` and without `.xml`.
- First-pass runtime rule:
  - building has runtime BuildingType
  - BuildingType graphics path is non-null
  - use happy path with payload manager
  - else use legacy path
- Live city only for now; placement ghosts and editor previews stay legacy.

## Planned BuildingType XML shape
```xml
<building type="theater">
    <graphics>
        <path value="Health_Culture\Theatre" />
        <upgrade threshold="45" operator="gt" />
        <water_access mode="reservoir_range" />
    </graphics>
</building>
```

## Important code seams
- `src/building/building_type_registry_internal.h`
  - `BuildingType` now owns nested `GraphicsDefinition`
- `src/building/building_type_registry_xml.cpp`
  - parser now expects `<graphics>` with child `<path>`, optional `<image>`, `<upgrade>`, and `<water_access>`
- `src/building/building_runtime.h/.cpp`
  - runtime now resolves new-path building images and still maintains legacy compatibility state
- `src/assets/image_group_payload.h/.cpp`
  - path-keyed group manager now exposes default-image lookup, caches failed loads, stores implicit animation metadata/frame keys plus footprint/top composition data, and clones whole-image aliases including top/animation
- `src/core/image_payload.h/.cpp`
  - payload-backed `image` compatibility view already exists
- `src/graphics/image.cpp`
  - now has pointer-based isometric helpers for direct `const image *` draws and a generic pointer draw helper for payload-backed animation frames
- `src/widget/city_with_overlay.cpp`
  - live overlay city building footprint/top now tries runtime image first, then legacy tile id; runtime animations can also draw from payload-backed frame keys
- `src/widget/city_without_overlay.cpp`
  - base live city building footprint/top now tries runtime image first, then legacy tile id; runtime animations can also draw from payload-backed frame keys

## Current compatibility doctrine
- Keep `map_image` populated with legacy ids for untouched systems.
- New runtime render lookup should prefer runtime-owned footprint/top/animation slices and fall back cleanly to legacy ids where that rollout is still incomplete.
- Missing or incomplete extracted graphics content should warn/fallback in this phase, not hard-fail startup.

## Implemented runtime resolver scope
- Supported first-pass building families:
  - theater
  - amphitheater
  - arena
  - school
  - academy
  - library
  - forum
  - actor colony
  - gladiator school
  - doctor
  - hospital
  - barber
  - well
  - work camp
- The resolver uses guessed candidate image ids per family and falls back to the default image in the group when appropriate.
- Unsupported families currently return `nullptr` and stay on legacy rendering.
- For supported families, the payload path now synthesizes a legacy-style `image` view with `top` and implicit animation metadata from the image-group XML.

## Content caveats
- Live BuildingType XML files in `Mods/Vespasian/BuildingType` and `Mods/Augustus/BuildingType` were migrated to `<graphics>`.
- The current graphics path values are scaffolding guesses based on extracted content naming conventions.
- Augustus extractor now distinguishes materialized images from alias-only wrapper nodes so anonymous aliases no longer consume `Image_####` slots.
- Augustus isometric replacement export now prefers canonical template inheritance for numeric Julius references and recursive same-group part inheritance for local Augustus references.
- Missing Julius template files for brand-new Augustus-only assets are still treated as an expected bootstrap miss, but local isometric wrappers now inherit parts from already-inferred Augustus images instead of collapsing through layer-count heuristics.
- Active Augustus ON variants such as arena, amphitheater, colosseum, and large statue depend on that same-group part inheritance to preserve their top slice and keep animation overlays aligned with the base image.
- Legacy template parsing must tolerate `animation` / `frame` children because extracted Julius XML includes them, even when we only care about layer parts for inheritance.
- Augustus canonical output-path selection should only look at alias-only wrapper images, not materialized composite images that happen to reference a legacy group internally.

## Likely touched files
- `.gitignore`
- `docs/_codex_building_graphics_runtime_working_memory.md`
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_api.h`
- `src/building/building_type_registry_internal.h`
- `src/building/building_type_registry_xml.cpp`
- `src/graphics/image.cpp`
- `src/graphics/image.h`
- `src/widget/city_with_overlay.cpp`
- `src/widget/city_with_overlay.h`
- `src/widget/city_without_overlay.cpp`
- `src/widget/city_without_overlay.h`
- `Mods/Vespasian/BuildingType/_README.md`
- `Mods/Vespasian/BuildingType/_template.xml.example`
- live BuildingType XML files in `Mods/Vespasian/BuildingType` and `Mods/Augustus/BuildingType`

## Re-read after compaction
- `src/building/building_runtime.h`
- `src/building/building_runtime.cpp`
- `src/building/building_runtime_api.h`
- `src/building/building_type_registry_internal.h`
- `src/building/building_type_registry_xml.cpp`
- `src/core/crash_context.h`
- `src/core/crash_context.cpp`
- `src/assets/image_group_payload.h`
- `src/assets/image_group_payload.cpp`
- `src/assets/augustus_asset_extractor.cpp`
- `src/core/image_payload.h`
- `src/core/image_payload.cpp`
- `src/graphics/image.h`
- `src/graphics/image.cpp`
- `src/widget/city_with_overlay.cpp`
- `src/widget/city_with_overlay.h`
- `src/widget/city_without_overlay.cpp`
- `src/widget/city_without_overlay.h`
