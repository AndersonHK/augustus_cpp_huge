# Renderer Resource Refactor Working Memory

Snapshot: 2026-06-26 docs refresh over older renderer/resource notes
Workspace: `C:\Users\imper\Documents\GitHub\augustus_cpp_huge`

## 2026-06-26 maintenance checkpoint
- Treat this file as long-term renderer/resource memory. Current implementation contracts should still be verified against source before editing, especially around atlas fallback and managed image handles.
- The current durable renderer plans are `docs/renderer_scaling_seam_plan.md`, `docs/render_performance_plans.md`, and `docs/figure_owned_native_graphics_plan.md`.
- `git` is available in the current shell. `rg` and `gh` are not installed; use PowerShell-native `Select-String` / `Get-ChildItem` and normal `git` commands.
- Old compile-fix notes in this file are historical only. Do not treat an old "latest known compile issue" as current without checking source or running the relevant build.
- The strategic renderer direction remains path-keyed, logical-size-aware image resources with atlas sampling demoted to temporary compatibility input.

## 2026-06-05 image object and resource graphics pass
- The former separate image payload/data helper layers are no longer public runtime types.
- `Image` in `src/graphics/image.h/.cpp` is the manager-owned renderable image object. It owns the runtime handle plus image metadata and exposes draw/runtime-slice methods with default `COLOR_MASK_NONE` and `SCALE_NONE` arguments.
- `ImageGroupEntryRef` is the semantic handle for "this entry from this image group." It stores a group path plus optional entry id, resolves to the manager-owned `Image`, and exposes `draw(...)`, dimensions, and runtime slices.
- Do not introduce a bespoke `Icon` class for resource/building/button imagery. Existing XML already expresses those visuals as image group paths plus optional entry ids, so callers should store/use `ImageGroupEntryRef`.
- Resource imagery now lives in `ResourceGraphics` (`src/game/resource_graphics.h/.cpp`), loaded from `Mods/<Mod>/Resources/*.xml`.
- Resources no longer expose image id fields or producer/industry fields. Production ownership is inferred from building production methods through `building_output_resource(...)` and `building_producer_for_resource(...)`.
- Border composition moved out of `Image` and into `ImageBorder`, a UI composition primitive that owns explicit image-group segment references instead of relying on neighboring image ids.
- The upcoming image-group class pass should be able to absorb `ImageGroupEntryRef` naturally: callers already speak in group path + entry id terms rather than raw legacy image ids.

## 2026-06-05 graphics extractor split and harness
- Current durable extractor handoff: `docs/graphics_extraction_pipeline.md`.
- Historical notes below that describe Augustus as a copied fallback mirror, a mostly heuristic scaffolding pass, or a bootstrap launched directly from `src/platform/augustus.cpp` are stale.
- Current true extraction shape:
  - Julius extraction lives in `src/core/legacy_image_extractor.cpp`.
  - Augustus extraction lives in `src/assets/augustus_asset_extractor.cpp`.
  - Shared extractor utilities live in `src/assets/graphics_extractor_common.cpp`.
  - Augustus numeric legacy references resolve through `src/assets/augustus_julius_template_resolver.cpp`.
  - The standalone harness lives in `tools/augustus_graphics_extractor/main.cpp` and builds as `AugustusGraphicsExtractor.exe`.
- Runtime order:
  - `src/core/image.cpp` loads the Julius climate atlas and runs Julius extraction first.
  - After Julius atlas decoding, `image_load_climate(..., extract_legacy_graphics = 1)` constructs `RuntimeGraphicsExtractionService` directly from the C++-compiled image loader.
  - `src/platform/augustus.cpp` only prepares directories and should not run Augustus extraction early.
- Clean generated output currently means:
  - `Mods\Julius\Graphics` exists.
  - `Mods\Augustus\Graphics` exists.
  - `Mods\Vespasian\Graphics` does not exist.
- Historical clean sample baseline from `D:\Games\GOG Games\Caesar 3\assets\Graphics` at the 2026-06-05 checkpoint. Newer climate-aware extraction counts are recorded in `docs/_codex_building_graphics_runtime_working_memory.md` under the 2026-06-21 checkpoint:
  - Julius: 231 XML, 8933 PNG, 8465 logical images
  - Augustus: 3200 XML, 4088 PNG, 3259 logical images
  - BuildingType graphics refs: 494 explicit path/image refs plus 152 button icon refs checked across Augustus and Vespasian BuildingType XML, 646 total, 0 missing; button `icon` values are generated graphics group keys and optional `icon_image` values pin image ids.
- Julius `Aesthetics\House_Tent` intentionally includes compatibility aliases for `Image_0001..Image_0005` into `Aesthetics\House_Tent_Variants`; do not collapse those variants into `House_Shack`.

## 2026-04-06 canonical extractor correction
- Historical notes below that describe Julius extraction as not yet real, or Augustus extraction as mainly a scaffolding/heuristic pass, are now stale.
- Current true state:
  - Julius extraction is active and is responsible for canonical footprint/top exports used by the runtime fallback chain
  - Julius isometric footprint exports now trim bottom transparent padding and preserve logical placement through exported XML metadata
  - Augustus extraction is also load-bearing and now handles:
    - canonical direct-crop footprint padding trim
    - recursive same-group `group="this"` part inheritance
    - expansion of inherited isometric full-image references into `footprint` + `top`
- The recent active-building animation glitch was fixed by correcting extractor semantics, not by changing authored XML animation `x/y` offsets.

## User-directed architecture
- Do not do narrow "boundary-first" rewrites that leave the old system as the real core.
- Preferred migration pattern is coexistence/cohabitation like the building/figure runtime work:
  - the new runtime owns the real logic/data model
  - legacy struct access still exists alongside it
- Desired long-term image core:
  - path-based key
  - payload object value
  - simple `Key -> Payload`
- Rendering should stop depending on atlas sub-rect sampling for final draw submission.

## Seam diagnosis
- The visible seams at UI scale `> 100` with linear filtering are strongly consistent with atlas bleed.
- The screenshot path uses legacy Caesar 3 UI groups like `GROUP_DIALOG_BACKGROUND`, not just XML assets.
- Fixing only `res/assets/Graphics` would not solve the screenshot case.

## Important repo facts
- `src/platform/renderer.cpp`
  - current 2D draw path still samples atlas sub-rects from `img->atlas.*`
  - linear filtering is enabled depending on scale/filter config
- `src/core/image.cpp`
  - main climate, enemy, and font images are still decoded then packed into renderer atlases
  - atlas buffers are also used as source pixel data for XML/layer composition
- `src/assets/image.cpp`
  - XML/composed assets are packed into extra-asset atlases or treated as unpacked extras
  - reference translation currently copies atlas metadata from referenced images
- `src/assets/assets.cpp`
  - current XML load order is selected mod then root assets
- `src/assets/xml.cpp`
  - current path resolution is mod/root oriented only
- `src/platform/augustus.cpp`
  - prepares runtime folders, but graphics extraction order now belongs to `src/core/image.cpp`

## Current implementation direction
- Introduce per-image runtime resource handles alongside legacy `image` metadata.
- Keep legacy `image` structs available while new runtime image resources take over draw submission.
- Upload per-image textures from loader-generated pixel data so the renderer can stop drawing from atlas sub-rects.
- Preserve atlas buffers temporarily where they are still needed for load-time composition.
- Keep the migration in a true cohabitation shape rather than a temporary boundary shim.
- New renderer/widget diagnostics should prefer `src/core/crash_context.h`:
  - `ErrorContextScope` is the preferred C++ scope name for new code
  - `error_context_report_*` is preferred over direct `log.h` calls for runtime warnings/errors/fatal dialogs

## Changes already started
- `src/core/image.h`
  - converted to `#pragma once`
  - added `image_handle`
  - added `resource_handle` to `image`
- `src/graphics/renderer.h`
  - converted to `#pragma once`
  - `render_2d_request` now has `handle`
  - renderer interface now exposes:
    - `upload_image_resource`
    - `release_image_resource`
- `src/platform/renderer.cpp`
  - started managed per-image texture storage
  - draw requests now prefer managed resource handles before atlas textures
  - silhouette/custom helper paths started using managed textures when available
- `src/graphics/image.cpp`
  - legacy requests now populate `request.handle`
- `src/graphics/ui_primitives.cpp`
  - UI requests now populate `request.handle`
- `src/assets/image.cpp`
  - reference translation now copies `resource_handle`
  - unload path now releases image resources
  - helper functions added for uploading cropped/atlas-backed image resources
  - packed and unpacked extra assets now begin uploading runtime image resources
- `src/core/image.cpp`
  - reload paths now release image resources for main/enemy/font images
  - atlas conversion paths now upload runtime image resources after decode/packing
- `src/game/mod_manager.h/.cpp`
  - added Augustus/Julius graphics path helpers
- `src/assets/xml.h/.cpp`
  - added Augustus/Julius asset-source enums
  - path resolution now understands current mod, Augustus fallback, Julius fallback, and root fallback
- `src/assets/assets.cpp`
  - XML assetlist loading now includes Augustus and Julius fallback graphics directories
- `src/platform/augustus.cpp`
  - startup bootstrap helpers prepare canonical fallback graphics directories
- `.gitignore`
  - private working-memory file is ignored

## Current status
- The renderer can now prefer managed per-image textures over atlas sub-rects.
- Legacy `image` structs still cohabit and carry the runtime `resource_handle`.
- Main, enemy, font, and extra-asset loaders have started populating runtime image resources.
- Augustus fallback bootstrap now explodes packaged game-folder `assets\Graphics` XML/PNG atlases into canonical per-group output behind a stamp.
- Julius bootstrap performs canonical legacy extraction into `Mods/Julius/Graphics`, and that extracted output is load-bearing for both runtime graphics fallback and Augustus reference translation.

## Refresh notes after context compaction
- Re-read:
  - `docs/renderer_ui_vertical_slice_design.md`
  - `src/building/building_runtime.h`
  - `src/building/building_runtime.cpp`
  - `src/platform/renderer.cpp`
  - `src/platform/augustus.cpp`
  - `src/assets/xml.cpp`
  - `src/assets/assets.cpp`
  - `src/graphics/image.h`
  - `src/graphics/image.cpp`
- Important restated architectural rule from the user:
  - the new image system must become the real core
  - legacy `image` access is allowed as a compatibility view, not as the authoritative model
  - path-based `Key -> Payload` remains the target
- `building_runtime` is the clearest coexistence reference:
  - C++ runtime object owns behavior
  - legacy `::building &data` remains attached and publicly reachable during migration
  - this is the pattern to mirror for images
- Current `Image` direction:
  - `Image` is the authoritative manager-owned image object
  - it owns key, handle, refcount, dimensions, offsets, crop/original dimensions, and image-facing metadata directly
  - legacy `image` structs point at the owning `Image` through opaque `resource_payload` only as a bridge
- Important implementation correction:
  - `image_manager().register_image(...)` must adopt freshly uploaded renderer handles
  - do not release the handle before transferring ownership into the manager-owned `Image`
- Current renderer still violates the final plan in specific ways:
  - `draw_image_request` still falls back to `get_texture(request->img->atlas.id)` if no managed texture exists
  - `draw_texture_request` still uses `img->atlas.x_offset` / `img->atlas.y_offset` for source rects when atlas fallback is active
  - `get_texture(...)`, unpacked-image caching, custom texture IDs, and several helpers still decode atlas IDs directly
  - silhouette and blend-texture helpers still fall back to atlas textures when no managed texture exists
- This means the current state is a real coexistence step, but not yet the plan-complete path-keyed resource core.
- `docs/renderer_ui_vertical_slice_design.md` is still the contract for placement:
  - widgets/adapters convert legacy inputs centrally
  - renderer owns final sampling/domain scale
  - callers must not pre-correct for renderer internals
  - destination rect semantics stay coherent across UI and world
- Source precedence currently in code:
  - current mod
  - Augustus fallback
  - Julius fallback
  - root safety fallback
- For authored path-keyed graphics, including extracted UI groups such as `UI\Top_Menu`, prefer the same logical-key loader flow used by building/tile runtime over flat `assets_get_image_id("UI", ...)` registry assumptions
- Bootstrap status:
  - `image_load_climate(..., extract_legacy_graphics = 1)` runs Julius extraction first, then Augustus extraction
  - Julius extraction produces canonical fallback graphics content under `Mods/Julius/Graphics`
  - Augustus extraction produces canonical fallback graphics content under `Mods/Augustus/Graphics`
- New direct-source happy path now added:
  - `image_manager().load_png(...)` loads a PNG directly into a manager-owned renderer resource
  - `asset_image_load_all()` now detects simple non-isometric single-layer XML/mod images and routes them through that direct image path before atlas packing
  - those direct assets use the resolved PNG path as the image key
  - complex/composed/isometric assets still fall back to the older composition + atlas-assisted path for now
- Julius extraction is now being implemented as a separate C++ module:
  - `src/core/legacy_image_extractor.h`
  - `src/core/legacy_image_extractor.cpp`
  - hooked from `src/core/image.cpp` immediately after `convert_images()` and `make_plain_fonts_white()` for the main climate atlas
- Important user correction during extraction work:
  - do not dump legacy output into a generic `Graphics/Extracted/...` tree
  - extracted Julius art should be split into normal top-level graphics families and look comparable to `res/assets/Graphics`
  - using the Augustus buckets exactly where they fit is preferred
- Current Julius extraction layout:
  - PNGs go under `Mods/Julius/Graphics/<Family>/<Group>/...`
  - XML files live beside those group folders as `Mods/Julius/Graphics/<Family>/<Group>.xml`
  - each XML assetlist name is the canonical backslash group key, for example `Military\Barracks`
  - no `Legacy_<source>` namespace should appear in the logical key
- Current family split being encoded in the extractor:
  - existing Augustus-style buckets:
    - `Admin_Logistics`
    - `Aesthetics`
    - `Health_Culture`
    - `Industry`
    - `Military`
    - `Monuments`
    - `Ships`
    - `Terrain_Maps`
    - `UI`
    - `Walkers`
    - `Warriors`
- Current extractor output semantics:
  - one colocated XML per scaffolded image group
  - assetlist names equal the actual family/source/group folder path
  - image `src` values are local to that folder, for example `Image_0000`
  - isometric images are exported as layered XML with separate footprint/top PNGs
  - animation metadata currently exports as consecutive-frame `<animation frames=...>` metadata on the base image
- Current storage-model correction from the user:
  - do not treat extracted XMLs as per-legacy-group root manifests
  - instead treat them as scaffolded image-group definitions that live with their grouped images
  - later gameplay/building definitions should point at these XML paths directly
  - image-group manager will become the intermediary resource layer above individual image payloads
- Current naming direction:
  - keep using legacy boundaries as a temporary scaffold
  - where obvious, use human-readable scaffold names like `Theater`, `Dialog_Background`, `Road`
  - where not obvious yet, fall back to `Group_<id>` until the building-type and gameplay hookups reveal the better semantic grouping
- Static issue already caught and corrected during implementation:
  - do not classify groups by loose numeric ranges, because the legacy enum space interleaves unrelated categories
  - classification must be explicit per known building/figure group
- Near-term implementation direction after this refresh:
  1. Keep `Image` as the authoritative runtime image object with metadata ownership.
  2. Attach legacy `image` structs to that runtime object in the same spirit as `building_runtime`.
  3. Move renderer draw submission and metadata lookup to the image/handle path first, leaving atlas fallback only as temporary coexistence debt.
  4. Then push the same ownership model into XML assets, legacy climate/font/enemy loads, and external images.
  5. After that, remove more atlas-based draw assumptions and revisit UI seam behavior on top of the cleaner resource core.

## Next concrete steps
1. Finish static cleanup of the current resource-handle wave
   - remove obvious compile issues from the bootstrap/resource changes
   - audit null-safety around renderer interface calls during shutdown/reload
2. Push the managed-resource path further into the legacy climate/UI path
   - ensure the UI reproducer uses managed textures end-to-end instead of silently falling back to atlas sampling
   - inspect silhouette/custom-image helpers for remaining atlas assumptions
3. Flesh out canonical source bootstrap
   - keep Augustus stamp/checksum logic
   - implement real Julius extraction/materialization of legacy graphics into canonical files
   - preserve precedence `Current Mod > Augustus > Julius`
4. Revisit UI primitives
   - keep explicit container/border/fill composition
   - stabilize shared snapped edges where needed after renderer path is changed
5. Move toward the intended path-keyed image core
   - keep the C++ `Image` manager keyed by canonical path
   - keep legacy `image` access as an attached compatibility view rather than the authoritative core

## Constraints
- Build only when useful for the current task, and use `Release|x64`.
- Keep CRLF on touched files.
- `rg` and `gh` are not installed in this shell; use PowerShell-native search and normal `git` commands when needed.
- BuildingType templates/examples are now single-source:
  - keep `_README.md` and `_template.xml.example` only in `Mods\Vespasian\BuildingType`
  - `Mods\Augustus\BuildingType` should contain live XML data only

## 2026-04-02 Importer Refresh
- The user clarified the canonical logical key format again:
  - keys are plain graphics stems, without `Graphics/` and without file extensions
  - the example should be read as `Military\Barracks`
  - forward slashes can be tolerated in parsing, but the intended canonical stored form is backslash-delimited
- Override identity must therefore be path-only:
  - current mod beats Augustus
  - Augustus beats Julius
  - no extra source namespace like `Legacy_c3` may appear in the logical key
- Extractor progress after that correction:
  - `legacy_image_extractor.cpp` now targets canonical group paths instead of `Legacy_<source>` subfolders
  - group XML names are being emitted as backslash logical keys such as `Military\Barracks`
  - PNGs still live under `Mods/Julius/Graphics/<Family>/<GroupName>/...`
  - XMLs live at `Mods/Julius/Graphics/<Family>/<GroupName>.xml`
  - extractor stamps are now global generated-output stamps, and a generated manifest file is being used to clear prior generated output before rewriting
  - extractor stamp contents now combine:
    - a metadata format version in the prefix
    - the legacy source stem
    - a fingerprint of the decoded source data plus extraction-relevant metadata
  - extractor now logs:
    - why extraction is happening
    - when extraction starts
    - a completion summary with exported group/image/png counts
- Shared resolver progress:
  - `xml.h/.cpp` now expose public helpers for:
    - resolving a logical assetlist key to `<key>.xml`
    - resolving a logical group/image pair to `<key>/<image>.png`
    - resolving direct group images
  - these reuse the same precedence chain already established in xml loading
- New importer path:
  - `src/assets/image_group_payload.h/.cpp`
  - `src/assets/image_group_entry.h/.cpp`
  - `ImageGroupPayload` is the temporary path-keyed XML-group manager above manager-owned images
  - `ImageGroupEntry` is a separate bridge for one group image relationship until the later image-group class refactor
  - importer behavior currently:
    - resolve logical key to the winning XML path and winning source
    - parse the assetlist with a dedicated parser instead of reusing the old flat asset registry
    - load `src` PNGs into manager-owned images using logical image keys like `Military\Barracks\Image_0000`
    - support group/image references by recursively loading referenced image groups and retaining the referenced image payload keys
- Important caveat:
  - the new importer exists as a parallel happy path, but it is not yet hooked into building definitions or other gameplay callers
  - next real milestone is to route a first live caller onto `image_group_payload_load(...)` / `image_group_payload_get_image(...)`
- Augustus bootstrap refresh:
  - `augustus.cpp` now fingerprints the source `res/assets/Graphics` tree using recursive file/directory names plus modified times
  - the Augustus bootstrap stamp includes both a version prefix and that source fingerprint
  - bootstrap now logs when it is rebuilding because the stamp is missing, the fingerprint changed, or the fallback directory is empty


- Augustus fingerprint path join bug fixed: the resolved asset root may not end in a slash, so joining it with `Graphics` must insert a separator or it becomes `assetsGraphics` on Windows.
- Augustus bootstrap no longer relies on `platform_file_manager_copy_directory` for the canonical Graphics mirror; it now does its own recursive copy with per-file/per-directory error logging and a success summary, because the generic copier was failing too opaquely during startup.

## 2026-04-02 Augustus Extractor Pivot
- The user confirmed the raw Augustus mirror was the wrong direction:
  - Augustus should be treated like Julius
  - packed `assets\Graphics\*.xml` + `*.png` atlases must be exploded into canonical per-group XMLs and per-image PNGs under `Mods\Augustus\Graphics`
  - folder families should match the Julius/canonical layout shape, not a generic copied root atlas set
- Historical implementation change at that point:
  - new module `src/assets/augustus_asset_extractor.h/.cpp`
  - `src/core/image.cpp` called the then-current Augustus bootstrap after Julius extraction instead of copying `assets\Graphics`; current code compiles that file as C++ and calls `RuntimeGraphicsExtractionService` directly
  - the new extractor:
    - fingerprints top-level Augustus source XML/PNG files using names + modified times with metadata version in the stamp prefix
    - clears/rebuilds `Mods/Augustus/Graphics`
    - parses each packed Augustus atlas XML
    - groups images by wrapper names, local references, alias keys, and translated legacy references
    - writes one XML per extracted image group under `Mods/Augustus/Graphics/<Family>/<Group>.xml`
    - writes direct atlas crops as individual PNGs under `Mods/Augustus/Graphics/<Family>/<Group>/...`
    - resolves local references through the final owning output group after grouping and image-id offsetting
    - rewrites numeric legacy group references through split-aware Julius template resolver calls
    - preserves alias groups for source-visible wrapper names instead of skipping path collisions
    - keeps its extraction stamp under `Mods/Augustus/Graphics/.graphics_extract.stamp`, matching Julius keeping its stamp under `Mods/Julius/Graphics/...`
- Shared helper update:
    - `src/core/legacy_image_extractor.cpp` implements native `JuliusExtractor::resolveLegacyGroup()` and `JuliusExtractor::resolveLegacyImage()`
  - this is the bridge from Augustus packed XML legacy numeric refs back into canonical Julius-style path and image keys
- Important current caveat:
  - Augustus data is still variable and upstream-packaged, so future fixes should inspect wrapper names, alias collisions, and BuildingType references before flattening a case into a broad heuristic
