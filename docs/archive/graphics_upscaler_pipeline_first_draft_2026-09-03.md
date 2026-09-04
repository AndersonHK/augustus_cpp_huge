# Runtime Graphics Upscaler Pipeline: First-Draft Baseline

Date captured: 2026-09-03

Status: Preserved planning baseline. This file records the proposal that the project owner described as more than 90% aligned. Continue revisions in `docs/graphics_upscaler_pipeline_plan.md`; do not silently rewrite this snapshot.

## Intent

Provide a local, one-time, AI-based graphics upscaler behind its own load/call/unload module boundary. Enabling the `High Resolution Upscales` mod causes the game to generate six-times-side-length raster replacements for Julius and optional Augustus building-owned graphics. The generated images retain the same relative paths as their source images and render at the same logical size.

The result should look like a faithful high-resolution remaster rather than enlarged pixel art:

- smooth rather than deliberately pixelated
- the same structures, silhouettes, composition, and visual style
- full true-color output rather than palette-constrained output
- preserved baked shadows and transparency
- preserved animation timing, frame count, and subtle motion
- additional spatial detail without changing gameplay meaning

This is a generated-content system. No Caesar 3 graphics, bulk extracted Augustus graphics, generated group trees, or generated upscales belong in the repository.

## Stack And Package Shape

The intended stack is:

```text
Julius -> Augustus -> High Resolution Upscales -> Vespasian
```

The supported Julius-only form is:

```text
Julius -> High Resolution Upscales
```

`High Resolution Upscales` depends on Julius. Augustus is an optional input source; an unconditional dependency on Augustus would make the Julius-only arrangement impossible.

A clean installation contains the upscaler module and its model package with the game distribution. The shipped mod directory contains only authored metadata:

```text
Vespasian.exe
GraphicsUpscaler.dll
upscaler model package

Mods/
  High Resolution Upscales/
    mod.xml
```

After the first successful generation, runtime output beneath the installed game may contain:

```text
Mods/
  High Resolution Upscales/
    mod.xml
    Graphics/
      ...same relative raster paths as Julius/Augustus...
    .graphics_upscale.manifest
    .graphics_upscale.stamp
```

The high-resolution mod does not generate or ship replacement asset-list XML.

## Mandatory Logical Geometry

Extracted Julius and Augustus XML is the canonical geometry contract. Every extracted `<image>` entry must contain:

- explicit positive canonical `width` and `height`
- explicit positive `logical_width` and `logical_height`
- logical dimensions on direct, layered, isometric, construction, animation, and alias entries
- explicit logical dimensions on aliases rather than runtime-only inference through the target

The fixed logical-size grain is 120 units per logical pixel. For example, a canonical 58 by 30 image has a logical size of 6960 by 3600 units. A six-times-density raster for that entry is 348 by 180 source pixels, but its logical dimensions remain 6960 by 3600.

Missing or invalid logical dimensions are schema errors that identify the source XML and image id. Once the migration is complete, the loader must not silently infer that one source pixel equals one logical pixel.

## Raster Overlay Semantics

`High Resolution Upscales` is an opt-in generated raster overlay, declared in mod metadata. A tentative declaration is:

```xml
<generated-graphics
    provider="GraphicsUpscaler"
    kind="raster-overlay"
    density="6"
    scope="building-assets">
    <source mod="Julius" required="true" />
    <source mod="Augustus" required="false" />
</generated-graphics>
```

Raster resolution for an already-merged XML image entry follows these rules:

1. Begin with the mod that owns the winning XML entry.
2. Consider only declared raster-overlay mods above that owner in the active stack.
3. Accept an overlay path only if its valid generated manifest maps that path to the owning source and current source fingerprint.
4. Otherwise resolve from the XML owner and downward under the ordinary graphics rules.

Consequences:

- an Augustus-owned XML entry can use a same-path high-resolution raster
- a Vespasian-owned XML entry is not overridden by the lower high-resolution mod
- a normal mod does not gain image-only override behavior merely by containing a coincidentally named file
- Julius-only operation naturally ignores the absent optional Augustus source

## Density-Aware Coordinate Rules

The materializer must represent three distinct concepts:

```text
canonical source geometry
resolved raster density
fixed logical geometry
```

For a density-six raster:

- source crop coordinates and crop dimensions are multiplied by six
- source-space layer rectangles are multiplied by six
- isometric footprint, split, and source-canvas calculations operate at density six
- the loaded raster must be exactly six times the canonical size on each axis
- logical width and height remain unchanged
- draw offsets, anchors, sprite offsets, animation timing, animation order, and reversibility remain unchanged

Padding used by the model must be removed before validation. The final raster dimensions and origin must be exact; automatic cropping or silhouette recentering is forbidden.

## First-Slice Content Boundary

The scope is the transitive graphics dependency closure rooted at effective Julius and optional Augustus `BuildingType` graphics. It includes:

- normal, climate, and visual variants
- every construction and partially built stage
- base, top, footprint, shadow, damage, fire, working, and decorative layers
- every explicit and implicit animation frame
- water, ducks, banners, awnings, smoke, production motion, and similar activity embedded in building-owned layers
- cross-group aliases and image references needed by those entries

It excludes independently rendered `FigureType` graphics, including service walkers or other figures merely spawned by a building. Figures are a separate future slice.

If a referenced raster is a shared atlas, processing the complete raster is permitted when necessary to preserve its source-space contract. This does not expand the logical feature scope or cause unrelated XML definitions to be generated.

Animation is spatially upscaled without interpolation. Frame count, order, speed, reversible behavior, and loop timing stay unchanged. Animation families use identical model settings and padding and receive loop and temporal-difference validation to detect shimmer or structural wobble.

## Upscaler Direction

The first engine candidate is Real-ESRGAN through ncnn/Vulkan, owned inside `GraphicsUpscaler.dll` rather than invoked as a Python installation or loose subprocess.

FSR is not the asset-generation mechanism. Its spatial and temporal forms are aimed at reconstructing rendered frames, while this system must create persistent, independently validated sprite rasters.

The initial model bake-off includes:

- `realesr-general-x4v3` with conservative denoising
- `RealESRGAN_x4plus_anime_6B`
- full `RealESRGAN_x4plus` as a slower comparison

The model is selected by visual and temporal validation rather than sharpness alone. Acceptance prioritizes:

- unchanged structure and silhouette
- stable baked shadows and alpha edges
- no invented doors, windows, roof lines, or decorative elements
- no lost small motion
- stable animation loops
- faithful color relationships with expanded true-color nuance

The first exact-six-times path is a learned four-times pass followed by a deterministic 1.5-times finishing resize. A native or project-fine-tuned six-times model remains a possible later improvement.

## Alpha And Color

Transparent graphics require an explicit edge pipeline:

1. Dilate hidden RGB underneath transparent edge pixels.
2. Run RGB through the selected model.
3. Upscale alpha separately with a deterministic edge-preserving filter.
4. Recombine, remove model padding, and crop to the exact target dimensions.

The generated asset format is sRGB RGBA8 PNG. It uses the full 8-bit-per-channel true-color range and does not preserve legacy palette constraints.

AI inference and image transforms should use FP16 or equivalent higher-precision working data. Runtime lighting and compositing may later use linear RGBA16F targets for precision and lighting headroom, while persistent sprite textures remain RGBA8 sRGB. Display output depth is a separate swapchain and color-management concern.

## Module Contract

The upscaler follows the existing one-shot module direction:

```text
load GraphicsUpscaler.dll
validate ABI and model package
call one versioned generation entry point
receive a typed result and diagnostics
unload GraphicsUpscaler.dll
```

The ABI should be a narrow C boundary. Its request includes installed-game paths, active source mods, requested density and scope, current source fingerprints, and callbacks for progress, logging, and cancellation. It does not expose engine C++ objects to the DLL.

Progress callbacks are part of the first ABI even though the proper GUI loading screen is deferred. Expected phases include discovery, cache validation, device benchmarking, inference, encoding, validation, and atomic promotion.

## Startup Sequence

The tentative order is:

1. Validate mod metadata and ordering.
2. Complete required Julius extraction.
3. Complete optional Augustus extraction when Augustus is active.
4. Build the building-owned graphics dependency plan.
5. Validate the high-resolution manifest and stamp.
6. If output is missing or stale, load the upscaler DLL and generate into a sibling staging directory.
7. Validate every output and atomically promote the completed tree.
8. Unload the upscaler DLL.
9. Continue ordinary asset parsing and Vulkan upload.

The generated manifest fingerprints at least:

- upscaler ABI and implementation version
- exact model package and settings
- scope-planner version
- source mod identity and order
- relevant source XML and raster contents
- density and output format
- every generated relative path, canonical dimensions, generated dimensions, and output hash

An exact valid fingerprint skips generation. A stale fingerprint requires complete regeneration. Failure to regenerate never exposes a stale or partially generated tree as current output.

## Performance Policy

The DLL builds the complete input plan before expensive work and performs a representative preflight measurement. The estimate includes input pixels, file count, inference throughput, decode/encode overhead, and validation.

The tentative backend policy is:

1. Prefer Vulkan inference.
2. If Vulkan inference is unavailable, measure the identical model on CPU.
3. Proceed only when the conservative projected total is at or below 15 minutes.
4. Otherwise fail before bulk generation begins.

CPU execution is an alternate execution backend, not a lower-quality algorithm. The model and output contract do not change. The estimate should include a safety margin so an apparently acceptable run is unlikely to exceed the threshold materially.

A static planning pass over the current tree found an approximate upper envelope of 1,735 raster files and 9.9 megapixels of input, producing roughly 356 megapixels at six-times side length. Building-embedded dependencies may adjust the final closure.

Initial planning estimates are:

| Hardware and model class | Approximate first-run time |
| --- | ---: |
| Recent discrete GPU, compact model | 2-6 minutes |
| Older or entry discrete GPU | 5-15 minutes |
| Modern integrated Vulkan GPU | 12-35 minutes |
| Full `x4plus` model | Approximately 2-4 times compact-model time |

Allow an additional 30-120 seconds for image I/O, PNG compression, manifest construction, and validation. These figures are provisional until a representative in-repository benchmark harness measures the chosen model.

The six-times output represents about 1.33 GiB as uncompressed RGBA. Expected PNG output is provisionally 0.4-1.2 GiB. Generation should require approximately 2.5 GiB free for transactional staging and should stream or tile images rather than retaining the complete output in memory.

## Failure Contract And Recovery

An active high-resolution mod promises valid high-resolution assets. These are fatal startup errors:

- missing or incompatible DLL
- missing, corrupt, or mismatched model package
- no inference backend capable of meeting the configured time limit
- insufficient disk space or write permission
- missing mandatory logical dimensions
- source or output dimension mismatch
- corrupt or incomplete manifest
- decode, inference, encode, validation, or promotion failure

There is no original-resolution, xBRZ, software-renderer, OpenGL, or stale-cache fallback.

The DLL returns structured failure information. It never edits `mod-list`. The host should present, where possible:

- `Retry`
- `Disable mod and restart`
- `Open log`
- `Exit`

The disable action atomically removes only `High Resolution Upscales` from `mod-list`, preserves all other ordering, leaves generated cache files in place, and restarts the game. Proper progress presentation after the initial window is deferred, but the callback contract is established now so extraction and upscaling can later move behind a full GUI loading bar without changing the module boundary.

## Platform Direction

Windows x64 is the first implemented and validated target.

The core request/result ABI, manifest format, scope planner, and output contract remain platform-neutral. Linux, SteamOS, and Android validation is expected next. macOS remains architecturally reachable without committing to near-term testing or platform fixes. Platform-specific dynamic-library loading and filesystem behavior stay in thin host adapters.

The rendering direction is Vulkan-only. Successful generation is required when the mod is active; renderer compatibility fallbacks are not part of this system.

## Validation Plan

Validation should cover:

- extractor output requires complete logical geometry
- source-to-logical and density-six coordinate math
- isometric split and footprint geometry at both densities
- aliases, layered composites, and explicit and implicit animation frames
- exact path and extension preservation
- manifest invalidation after source, model, setting, or planner changes
- transactional behavior under interruption and simulated failures
- Julius plus high-resolution stack
- Julius plus Augustus plus high-resolution stack
- Julius plus Augustus plus high-resolution plus Vespasian stack
- Vespasian ownership winning over lower raster overlays
- deliberate DLL, model, Vulkan, permission, disk, and corruption failures
- disable-and-restart behavior that changes only the requested mod entry

The visual golden set should include construction stages, large and small buildings, all climates, baked shadows, alpha-heavy edges, working buildings, market awnings, water, ducks, smoke, fire, and other subtle loops. It should check silhouettes, anchors, logical bounds, temporal stability, and loop seams.

The production startup gate must eventually exercise representative recent `.svv` and legacy `.sav` cities for several thousand headless frames and fail on every warning, error, or renderer fallback.

## First-Draft Implementation Order

1. Make canonical source and logical dimensions mandatory in generated Julius and Augustus XML.
2. Add validation coverage for direct images, aliases, animation, and isometric entries.
3. Introduce density-aware materialization without changing current density-one output.
4. Define generated raster-overlay metadata, resolver precedence, and manifest schema.
5. Build the building-owned dependency planner and inventory harness.
6. Build the one-shot upscaler DLL prototype and representative model bake-off.
7. Add alpha-safe six-times output and temporal visual validation.
8. Add transactional caching, preflight timing, and structured diagnostics.
9. Integrate startup failure handling and disable-and-restart behavior.
10. Extend the startup and save soak gates before release.

## Decisions Deferred Beyond The First Slice

- independently rendered figures and walkers
- UI, terrain, ships, military units, and other non-building asset families
- frame interpolation or authored additional animation frames
- runtime lighting, rain, cloud, fire, day-night, HDR, and tone-mapping implementation
- moving extraction and upscale work behind the fully initialized GUI loading screen
- native or custom-trained six-times model work
- validated non-Windows releases

## Research References

- Real-ESRGAN: https://github.com/xinntao/Real-ESRGAN
- Real-ESRGAN ncnn/Vulkan: https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan
- ncnn: https://github.com/Tencent/ncnn
- AMD FidelityFX Super Resolution documentation: https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-temporal/
- Existing project extraction pipeline: `docs/graphics_extraction_pipeline.md`
- Existing image payload pipeline: `docs/image_group_payload_pipeline.md`
- Existing renderer scaling plan: `docs/renderer_scaling_seam_plan.md`
- Existing runtime DLL boundary plan: `docs/runtime_dll_boundary_refactor_plan.md`
