# Runtime Graphics Upscaler Pipeline Plan

Date: 2026-09-03

Status: Living design draft. Planning only; this document does not authorize implementation.

Baseline: `docs/archive/graphics_upscaler_pipeline_first_draft_2026-09-03.md`

## Objective

Add a family of optional high-resolution style mods that generate smooth, true-color, six-times-side-length replacements for building-owned Julius and Augustus graphics. Generation happens locally through a one-shot upscaler module and the recipe declared by the selected style. The replacement rasters preserve the source paths and render at exactly the original logical size.

The initial slice includes complete building presentation, including construction and animation, but excludes independently rendered figures. Enabling the mod is a strict content contract: the game either has a complete, validated high-resolution set or stops with a precise explanation and a way to disable the mod.

## Non-Negotiable Product Decisions

- Scale is exactly 6x on each source-raster axis.
- Logical on-screen size, position, anchors, and animation timing do not change.
- Output is a smooth high-resolution remaster, not enlarged hard-edged pixel art.
- Structures, silhouettes, layers, baked shadows, and stylistic identity remain faithful.
- Output uses true-color sRGB RGBA8 assets without legacy palette restrictions.
- Higher-precision processing is permitted and preferred internally.
- No animation frames are inserted or removed in the first slice.
- No lower-quality or original-resolution fallback is permitted while the mod is active.
- Independently rendered `FigureType` graphics are deferred to another slice.
- Windows x64 is first; the core design must not prevent later Linux, SteamOS, Android, or macOS support.
- Generated and extracted graphics remain installed-game artifacts and never enter the repository's authored `Mods` tree.

## Mod Stack And Ownership

Supported stacks are:

```text
Julius -> High Resolution Upscales
```

```text
Julius -> Augustus -> High Resolution Upscales -> Vespasian
```

Each high-resolution style mod has a required Julius source and an optional Augustus source. It cannot declare Augustus as an unconditional dependency because Julius-only use is supported.

The selected generated overlay is above Julius and Augustus so it can replace their rasters, but below Vespasian so Vespasian-owned XML and art retain precedence.

### Clean And Generated Package Shapes

The game distribution owns the upscaler executable code and shared model store. A clean style mod owns only its metadata:

```text
Vespasian.exe
GraphicsUpscaler.dll
UpscalerModels/

Mods/
  High Resolution Upscales - Selected Style/
    mod.xml
```

Runtime generation adds only installed-game artifacts:

```text
Mods/
  High Resolution Upscales - Selected Style/
    mod.xml
    Graphics/
      ...same relative paths and extensions as resolved source rasters...
    .graphics_upscale.manifest
    .graphics_upscale.stamp
```

The generated overlay contains no replacement asset-list XML. Existing Julius and Augustus XML remains authoritative.

## Style Mods, Recipes, And Model Dependencies

The model choice should be data-driven and mod-selectable rather than compiled into the game. This turns the bake-off into a normal mod-stack comparison and makes alternate rendering styles a supported capability.

Initial examples are:

```text
High Resolution Upscales - Faithful
High Resolution Upscales - Cartoon
High Resolution Upscales - Anime
```

Each style is a separate mod with its own generated `Graphics` tree and cache manifest. Switching back to an already generated style can therefore reuse its cache. Exactly one provider of the `building-raster-upscale` role may be active at a time. Multiple installed styles are valid; multiple active styles are a clear mod-conflict error rather than a precedence accident or an instruction to generate all of them.

The active mod declares an immutable upscale recipe, not merely a friendly model name. A tentative declaration is:

```xml
<generated-graphics
    provider="GraphicsUpscaler"
    role="building-raster-upscale"
    density="6"
    scope="building-assets"
    recipe="vespasian.realesr.anime.v1">
    <requires component="GraphicsUpscaler" abi="1" />
    <requires-model
        package="realesrgan-x4plus-anime-ncnn"
        version="1"
        sha256="..." />
    <source mod="Julius" required="true" />
    <source mod="Augustus" required="false" />
</generated-graphics>
```

The recipe package fixes everything that affects appearance or reproducibility:

- model architecture and exact weight hash
- neural scale and deterministic finishing scale
- denoising and sharpening settings
- color-space and alpha treatment
- tile overlap and padding policy
- preprocessor and postprocessor versions

The mod chooses a recipe id and version. It does not supply arbitrary executable code or silently substitute a locally similar model.

### Model Package Location

Model packages belong to a shared installed-game component store rather than inside generated `Graphics` trees:

```text
UpscalerModels/
  realesrgan-x4plus-anime-ncnn/
    1/
      model.param
      model.bin
      recipe.xml
      license.txt
      package.manifest
```

This allows several style mods to share a model, keeps their authored mod folders essentially metadata-only, and lets the installer or release archive deduplicate common runtime dependencies.

Every package is versioned and content-hashed. The host validates the package before invoking the DLL. The model license and redistribution terms must be recorded and approved before an official package is shipped.

### Fresh-Install Flow

A user downloading Vespasian into a fresh Caesar 3 installation must not be expected to discover model dependencies by trial and error.

The intended release behavior is:

1. The base Vespasian package contains the upscaler runtime component or an installer-visible optional component for it.
2. Each official high-resolution style download contains its small `mod.xml` plus the exact shared model package required by that recipe, placed into the central model store by the archive layout or installer.
3. Merely installing a style does not generate anything; generation begins only when that style is active in `mod-list`. An installed but inactive Augustus tree is likewise not treated as an input source.
4. Startup validates the upscaler ABI, recipe, model version, hash, active sources, and output cache before doing work.
5. A missing dependency produces a targeted error naming the required component and package. The immediate recovery is `Disable mod and restart`; a future mod manager may also provide `Install required model`.

The game should not silently download model weights during startup. Automatic acquisition can be added later through an explicit installer or mod-manager transaction with progress, license presentation, digest validation, and offline-safe failure behavior.

The model files are expected to be tens of megabytes rather than comparable to the generated output, so self-contained official style downloads are practical. The generated 6x cache remains the much larger component.

## Canonical Graphics Geometry

### Mandatory Extractor Output

Every extracted Julius and Augustus `<image>` entry must provide four positive dimensions:

```text
width
height
logical_width
logical_height
```

This requirement applies to direct images, layered and isometric entries, construction stages, animation roots, and aliases. Aliases carry explicit logical geometry even when the referenced target also contains it.

The project grain remains 120 fixed units per logical pixel. A canonical 58x30 image therefore records a 6960x3600 fixed logical size. At density six, its resolved raster is 348x180 while its fixed logical size remains 6960x3600.

Missing, partial, zero, or negative geometry is a fatal schema error that reports the XML path and image id. There is no post-migration one-source-pixel-equals-one-logical-pixel inference.

### Geometry Domains

Materialization must keep three domains explicit:

| Domain | Meaning | Changes at 6x |
| --- | --- | --- |
| Canonical source | Coordinates authored against the extracted 1x raster | No |
| Resolved raster | Actual decoded pixels used for upload and source sampling | Yes |
| Fixed logical | Screen-space dimensions and offsets | No |

At density six, only source-sampling values are scaled:

- source crop origins and dimensions
- source-space layer rectangles
- source-canvas measurements
- isometric footprint and split calculations performed on decoded pixels

These remain unchanged:

- fixed logical size
- draw and sprite offsets expressed in canonical logical pixels or fixed logical units
- anchors and tile ownership
- animation order, speed, frame count, and reversibility
- image and group identity

The final raster must be exactly six times the canonical source size on both axes. Model padding, transparent-border preparation, or tiling cannot change the final bounds or origin.

## Opt-In Raster Overlay Resolution

The mod metadata requires the explicit generated-overlay role, recipe, model package, density, scope, and source declaration described above. The exact XML names remain subject to the normal metadata-schema review.

For each winning merged XML image entry, raster lookup is:

1. Identify the mod that owns that XML entry.
2. Search only valid declared raster overlays above that owner, in normal precedence order.
3. Accept an overlay file only if its manifest maps the exact path to that owning source and current source fingerprint.
4. If no eligible overlay exists, perform the existing owner-and-lower resolution.

An active high-resolution mod with a missing eligible output does not reach step 4 during production startup. Completeness validation fails earlier. The fallback description exists to define general resolver behavior when the overlay mod is absent, not to permit mixed-density output while it is active.

This prevents a same-path high-resolution Augustus raster from overriding a Vespasian-owned entry and prevents ordinary mods from accidentally acquiring image-only overlay semantics.

## First-Slice Asset Closure

The scope planner starts from effective Julius and optional Augustus `BuildingType` graphics and walks their complete image dependency graph.

Included:

- all normal, climate, orientation, and visual variants
- all construction and partial-building stages
- base, top, footprint, baked shadow, damage, fire, working, and decorative layers
- every explicit and implicit animation frame
- embedded people and animals
- embedded water, smoke, fire, flags, awnings, production motion, and similar effects
- aliases and cross-group references required by included image entries
- entire source rasters when included entries share an inseparable atlas

Excluded:

- independently rendered service walkers
- delivery carts represented as figures
- labor seekers, buyers, priests, entertainers, soldiers, and other `FigureType` instances
- unrelated figures merely because an included building can spawn them

The ownership test is visual composition: if the element is a layer or frame of the building-owned image graph, it is included. If the runtime draws it as a separate figure, it is deferred.

## Image Reconstruction Contract

### Candidate Engine

The first engine candidate is Real-ESRGAN using ncnn/Vulkan inside `GraphicsUpscaler.dll`. The game does not require Python, CUDA, or a loose child-process tool.

FSR is not used for asset generation. It is a rendered-frame reconstruction family and does not provide the persistent, per-raster remastering and validation contract needed here.

### Model Bake-Off

The module architecture does not hard-code one universal model before representative testing. Initial recipe candidates are:

- `realesr-general-x4v3` with conservative denoising
- `RealESRGAN_x4plus_anime_6B`
- full `RealESRGAN_x4plus` as a slower quality comparison

The provisional exact-six-times route is 4x learned reconstruction followed by deterministic 1.5x finishing resampling. A native or project-fine-tuned 6x model can replace this later without changing the overlay, manifest, or logical-geometry contracts.

Acceptance is based on a golden set and rejects:

- silhouette or structure changes
- invented architectural features
- moving or softened anchors
- damaged baked shadows
- alpha halos
- lost small details or motion cues
- animation shimmer, wobble, or loop discontinuity

There is one selected model and settings recipe per active style mod and manifest version. Different style mods deliberately choose different recipes; one style never silently changes its algorithm according to the user's machine.

### Alpha Pipeline

1. Decode the source without palette quantization.
2. Dilate hidden RGB beneath transparent boundary pixels.
3. Process RGB using higher-precision model working data.
4. Upscale alpha independently with a deterministic edge-preserving filter.
5. Recombine in a defined premultiplication state.
6. Remove padding and validate exact bounds.
7. Encode sRGB RGBA8 PNG or the exact source extension when path compatibility requires another supported true-color format.

The model must not infer opacity. Baked translucent shadows remain governed by the source alpha channel.

### Animation Stability

The first slice performs spatial reconstruction only. It preserves the original sequence and uses identical model version, settings, alpha treatment, padding, and tiling policy across every frame in an animation family.

Automated checks compare frame silhouettes, alpha centroids, difference masks, static-region stability, and the final-to-first loop seam. Contact sheets and in-game loops remain required because numeric checks cannot prove artistic faithfulness.

## Color And Future Lighting

Generated files are sRGB RGBA8. This matches likely 8-10-bit consumer presentation and avoids doubling persistent asset size.

Higher precision belongs in two separate places:

- FP16 or equivalent working data during AI inference and image transforms
- linear RGBA16F scene targets for future lighting, rain, clouds, day-night cycles, fire, bloom, and HDR-like values

RGBA16 UNORM offers precision but not values above display white when the full normalized range is already occupied. RGBA16F is therefore the appropriate future lighting/compositing target. Sprite textures may remain RGBA8 sRGB and be converted to linear values during sampling. Swapchain depth and display color management remain separate renderer decisions.

## DLL Boundary

The lifecycle is deliberately one-shot:

```text
load module
resolve and validate versioned C ABI
validate model package
submit one generation request
receive typed result and structured diagnostics
unload module
```

The request contains only stable data:

- installed-game and staging roots
- active source mod names and graphics roots
- source manifest fingerprints
- requested scope and density
- time limit and backend policy
- progress, log, and cancellation callbacks

The DLL does not receive or retain engine C++ objects. It does not edit `mod-list`, show host UI, own the game loop, or leave live GPU resources behind after unload.

Progress callbacks are required in the first ABI even though the fully initialized GUI loading screen is deferred. Stable phase names should include:

```text
discover
validate-cache
benchmark
decode
infer
encode
validate-output
promote
```

This allows extraction and upscaling to move past the initial window startup later without another module redesign.

## Manifest And Transaction

The manifest fingerprints:

- manifest schema version
- DLL ABI and implementation version
- exact model bytes and settings
- declared style role, recipe id, recipe version, and recipe-package hash
- scope-planner version
- active source identity and order
- relevant source XML bytes
- every relevant source raster
- density and output pixel format
- each generated relative path and source owner
- canonical and generated dimensions
- output content hash
- completion state

Backend and measured throughput are recorded diagnostically but do not make otherwise equivalent output current or stale.

Generation occurs in a uniquely named sibling staging directory. Validation proves completeness, dimensions, decodability, hashes, and source ownership before one atomic promotion makes the new tree visible. A failed run can leave diagnostic staging data, but the resolver never treats it as active output.

A changed model, appearance-affecting setting, implementation, or schema invalidates every output produced by that recipe version. A source-stack change invalidates the affected ownership and path subset. The transaction may reuse outputs whose complete source and recipe fingerprints remain exact, but it still builds and validates a complete new manifest before promotion. This allows a later-enabled Augustus source to add or replace only affected output while preserving valid Julius work. If regeneration fails, an old mismatched cache is not used.

## Startup And Performance Gate

Temporary early-startup order:

1. Validate mod metadata, order, exclusive style role, upscaler ABI availability, and declared recipe/model package.
2. Fail immediately with dependency recovery actions when a required runtime or model component is absent.
3. Ensure Julius extraction is current.
4. Ensure optional active Augustus extraction is current.
5. Build the building-owned input plan.
6. Validate the selected style's high-resolution manifest.
7. If stale or absent, load the upscaler and perform backend preflight.
8. Generate, validate, and promote the output.
9. Unload the upscaler.
10. Continue normal startup parsing and Vulkan upload.

### Performance Target And Refusal Policy

The product target is a two-to-six-minute first run on a modern discrete GPU, preferably near the low end. This is comparable to a modest game update or shader compilation: noticeable and mildly inconvenient, but acceptable as a one-time cost.

Fifteen minutes is a rule-of-thumb refusal boundary rather than the performance goal. Before bulk work, the upscaler measures representative inference and encoding work and combines it with total planned pixels and file count. The estimate includes a safety margin.

Backend selection is:

1. Try the style's declared recipe through Vulkan.
2. Proceed when the conservative projection is acceptable, targeting two to six minutes and refusing a projected run above approximately 15 minutes.
3. A CPU backend may be considered only when separately implemented, benchmarked, and capable of executing the identical declared recipe.
4. Otherwise return a performance-rejection error before bulk output begins.

CPU inference is not currently assumed to be available through the Vulkan runner. The reference Real-ESRGAN ncnn/Vulkan interface exposes Vulkan GPU selection, while common CPU/PyTorch inference requires FP32. A production CPU route would therefore be a distinct optimized execution backend, such as an explicitly validated ncnn CPU or platform inference path, while preserving the same recipe and output contract.

Available evidence is too backend- and model-dependent for one honest CPU multiplier. The full general `x4plus` network has 23 RRDB blocks, the anime model has 6, and the general-v3 model is a compact VGG-style network; they do not have comparable CPU costs. Optimized published CPU examples show that compact or well-accelerated inference can be viable, while unoptimized FP32 execution can be an order of magnitude slower. The prototype must measure the exact converted package on representative consumer CPUs before enabling CPU execution.

The initial inventory envelope is approximately 1,735 source rasters, 9.9 megapixels of input, and 356 megapixels of density-six output. The exact building-owned closure is measured by the planner and may change as definitions change.

Revised planning targets, to be replaced by measurements from the same in-repository corpus:

| Backend class | Compact-recipe goal or estimate |
| --- | ---: |
| Recent discrete Vulkan GPU | 2-6 minutes |
| Modern integrated Vulkan GPU | Target 4-10 minutes; reject pathological runs near 15 minutes |
| Older or entry discrete Vulkan GPU | Target 5-12 minutes |
| Optimized modern desktop CPU | Plausibly 5-20 minutes, but unsupported until measured |
| Unoptimized or older CPU | Likely tens of minutes to hours; do not start |

Full `x4plus` may take several times as long as a compact recipe. A style whose declared model cannot meet the usability target remains a useful developer bake-off mod, but it should not be presented as the default end-user style without a clear hardware expectation.

The output is approximately 1.33 GiB as raw RGBA. The provisional compressed range is 0.4-1.2 GiB. Require approximately 2.5 GiB free for output, staging, and safety. Decode, process, validate, and release incrementally rather than retaining the full set in memory.

## Failure And Disable Flow

While the mod is active, these fail startup:

- missing or incompatible module
- missing, corrupt, unlicensed-for-distribution, or wrong recipe/model package
- more than one active provider of the exclusive building-upscale role
- incomplete canonical or logical geometry
- unsupported inference device or unimplemented backend
- projected generation time above the configured limit
- insufficient space or write access
- decode, inference, encode, or validation failure
- unexpected output dimensions or alpha contract
- incomplete, stale, or corrupt manifest after a required generation attempt
- failed atomic promotion

There is no mixed-density success state and no use of old rasters as a substitute.

The DLL returns a structured error. The host presents, where the current startup UI permits:

- `Retry`
- `Disable mod and restart`
- `Open log`
- `Exit`

The host owns the disable operation. It atomically removes only the exact active high-resolution style entry from `mod-list`, preserves all other order and content, leaves generated cache data in place, and restarts. The action must not delete output or let the DLL modify configuration directly.

When extraction and upscaling later move behind the initialized GUI, the same callbacks drive a proper progress bar, current file/phase display, estimate, and cancellation flow.

## Platform Shape

Windows x64 supplies the first `GraphicsUpscaler.dll` host adapter and validation matrix.

Portable components are kept free of Windows UI and loader assumptions:

- request/result ABI semantics
- manifest schema
- scope planning
- raster-density and geometry rules
- style-recipe, model-package, and validation rules
- transaction rules

Linux, SteamOS, and Android can provide shared-library and filesystem adapters around the same core. macOS remains a design-reachable target without near-term validation commitments. Vulkan is the intended inference and rendering path. CPU support remains an optional, separately validated backend for the identical declared recipe.

## Verification Gates

### Geometry And Resolver Tests

- mandatory source and logical dimensions on all extractor output forms
- direct, layered, aliased, construction, and animation entries
- density-one behavior unchanged by the new domain separation
- exact density-six source cropping and isometric splitting
- logical bounds and anchors identical at density one and six
- Augustus-owned entries use eligible high-resolution rasters
- Vespasian-owned entries remain Vespasian-owned
- ordinary same-path files do not act as undeclared overlays
- a missing generated file makes an active overlay incomplete

### Generation And Recovery Tests

- exact path, filename case, and extension preservation
- required recipe/model package discovery, version, and hash validation
- multiple installed styles with exactly one active provider
- rejection of multiple active providers for the same upscale role
- independent cache retention and reuse after switching styles
- exact six-times dimensions
- alpha-edge and baked-shadow fixtures
- interrupted generation never becomes visible
- every fingerprint input invalidates the cache when changed
- invalid DLL, model, device, permission, disk, raster, and manifest diagnostics
- performance rejection occurs before bulk generation
- disable-and-restart changes only the requested mod-list entry

### Visual And Runtime Tests

The golden set includes all climates and representative:

- small, medium, large, and multi-tile buildings
- construction stages
- baked and translucent shadows
- market awnings and banners
- water and ducks embedded in building imagery
- smoke, fire, and production motion
- static detail surrounding moving layers
- reversible and looping animations

Review checks structure, silhouette, color, alpha, anchor position, small-detail survival, temporal stability, and loop seams.

Before release, the startup gate must load representative recent `.svv` and legacy `.sav` cities, advance and render each for several thousand headless frames, and fail on every warning, error, missing asset, or renderer fallback.

## Implementation Slices

1. **Extractor geometry:** emit and enforce canonical and logical dimensions everywhere.
2. **Density seam:** separate canonical, raster, and logical geometry while preserving density-one behavior.
3. **Overlay and style contract:** finalize mod metadata, exclusive provider roles, recipe/model dependencies, manifest, completeness, and resolver precedence.
4. **Scope planner:** produce deterministic building-owned image and raster closure plus inventory.
5. **Model harness:** benchmark style recipes on representative discrete GPUs, integrated GPUs, and CPUs and produce golden contact sheets and animation comparisons.
6. **Upscaler module:** implement the versioned one-shot ABI, alpha pipeline, exact 6x output, and progress reporting.
7. **Transaction and timing:** add cache fingerprinting, staging promotion, preflight measurement, and the 15-minute gate.
8. **Startup host flow:** add structured diagnostics and disable-and-restart behavior.
9. **Release validation:** run stack permutations, failure injection, visual approval, and save/startup soak gates.

Each slice receives its own tests. Model integration does not begin until density-one geometry tests pass, and startup activation does not begin until generated-output validation and transactional promotion pass.

## Deferred Work

- independently rendered figures and walkers
- UI, terrain, ships, military units, and other asset families
- frame interpolation or additional authored animation frames
- native or project-trained 6x reconstruction
- runtime weather, lighting, fire-light, day-night, HDR, and tone mapping
- full GUI loading workflow after the initial window
- validated non-Windows releases

## Remaining Experimental Decisions

These require prototype evidence rather than architectural debate:

- final production model and denoising strength
- whether 4x plus deterministic 1.5x meets the quality bar or a custom 6x model is required
- appropriate tile size, padding, and pipeline concurrency by device class
- exact PNG compression/time tradeoff
- exact official package layout and whether the base release includes one default model or every official style remains a separate download
- whether an optimized CPU backend can meet the acceptance window on enough consumer systems to justify shipping it
- whether the future full GUI offers an advanced override for the 15-minute refusal

None of these changes the mod stacking, mandatory logical geometry, image-only overlay, failure, or content-scope contracts.

## References

- Preserved first draft: `docs/archive/graphics_upscaler_pipeline_first_draft_2026-09-03.md`
- Prior exploratory notes: `docs/archive/graphics_upscale_pipeline_notes.md`
- Graphics extraction: `docs/graphics_extraction_pipeline.md`
- Image payloads: `docs/image_group_payload_pipeline.md`
- Renderer scaling seam: `docs/renderer_scaling_seam_plan.md`
- Runtime DLL boundary: `docs/runtime_dll_boundary_refactor_plan.md`
- Mod metadata: `docs/mod_metadata.md`
- Real-ESRGAN: https://github.com/xinntao/Real-ESRGAN
- Real-ESRGAN ncnn/Vulkan: https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan
- ncnn: https://github.com/Tencent/ncnn
- Real-ESRGAN model zoo: https://github.com/xinntao/Real-ESRGAN/blob/master/docs/model_zoo.md
- Real-ESRGAN CPU/FP32 FAQ: https://github.com/xinntao/Real-ESRGAN/blob/master/docs/FAQ.md
- Example optimized CPU comparison: https://huggingface.co/ibrhr/Real-ESRGAN-OpenVINO
