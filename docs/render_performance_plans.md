# Render Performance Plans

## Current Evidence
The first post-refactor large-city performance log points at rendering as the remaining frame-rate limiter. During active speed-1000 samples, city drawing is a large share of frame time, and the `city_draw_main_row` bucket dominates the city draw bucket. The hot path is the city view row traversal that draws building tops, figures, and animations.

The current renderer already uses SDL textures and an accelerated `SDL_Renderer` when available, so the issue is not "no GPU at all." The issue is that the game still does CPU-directed immediate-mode rendering: runtime code decides every visible sprite, resolves or checks state, and submits individual SDL draw calls.

## Plan A: CPU Draw-Loop and Cache Pass
Goal: recover short-term FPS without changing the renderer backend.

Current implemented seams:
- `city_draw_main_row` now has child buckets for top, figures, and animation.
- Render metrics now count 2D requests, managed-image requests, submissions by texture source, texture switches, texture misses, grid overlays, visible render tile rows, and visible render tiles.
- The normal city draw row path uses `CityViewRenderPhase` entries so phases can share visible row traversal and carry `Building` objects per visible tile; top/animation/elevated-ornament paths defer legacy image lookups until native draw or compatibility fallbacks need them.
- Renderer requests now carry an optional fixed logical-size bridge. Existing float/source-size behavior remains the fallback while callers and XML are migrated.
- `Render2DPipeline` owns render-domain classification and scale-filter config interpretation; SDL renderer setup asks the pipeline for texture filter hints instead of duplicating `scale_filter` policy.

## Progress Checkpoint

- [x] Add render metrics for request counts, texture sources, texture switches/misses, grid overlays, visible rows, and visible tiles.
- [x] Centralize `scale_filter` and render-domain policy in `Render2DPipeline`.
- [x] Add fixed logical-size fields to renderer requests as a bridge.
- [~] Collapse normal city draw rows through `CityViewRenderPhase`; deletion/editor phase-major paths remain separate.
- [~] Carry `Building` objects through some draw phases; broad row command/prepass ownership remains open.
- [ ] Add renderer seam pixel checks for terrain/water/filter modes before logical-size XML work.
- [~] Introduce city render command lists and instance-data abstraction. The city draw side now builds one row-preserving object-reference command buffer per frame, and sealed revisioned renderer packets, retained snapshots, and off-thread adjacent-texture batch spans are implemented; production city capture and true backend instancing remain.
- [ ] Move terrain/water/figure/building assets away from atlas fallback into native GPU-friendly resources.
- [ ] Add Vulkan backend and orthographic z-buffered city renderer.
- [ ] Add HDR scene target and shader-side lighting/material policies only as an explicit visual milestone with manual review.

Scope:
- Reduce repeated visible-tile traversal in the city draw path while preserving layer order.
- Cache stable visible tile rows for a camera/viewport/orientation where practical.
- Move native graphics toward explicit invalidation and cheap cached reads instead of per-draw signature scanning.
- Prefer `Building&` / `Building` objects handed down from owner phases over repeated `Building::from_id` lookups.
- Keep extending draw-side tracker buckets before making broad claims. Top, figures, animation, SDL/managed/atlas submissions, texture switches, texture misses, and visible row/tile counts are covered; native cache checks/rebuilds and legacy image draw families still need dedicated buckets.
- Consider thread-pool preprocessing only for state preparation or command-list construction. Do not parallelize into more renderer calls until draw-call overhead is measured.

Implementation sequence:
1. [x] Split `city_draw_main_row` into top/figure/animation buckets.
2. [x] Add first renderer metrics for request counts, texture-source submissions, texture switches/misses, grid overlays, and visible row/tile counts.
3. [x] Centralize render-domain and `scale_filter` policy interpretation in `Render2DPipeline`.
4. [x] Collapse repeated row traversal overhead. One `CityViewRenderCommandBuffer` preserves row spans and replays normal, elevated, deletion, ghost, footprint, and custom-overlay phases without recomputing the visible viewport or object heads.
5. [x] Add a city draw command prepass that carries direct `Building *` and first-`Figure *` references.
6. [ ] Replace broad graphics signatures with dirty flags or generation counters owned by the building runtime.
7. [ ] Remove low-risk `Building::from_id` call sites from draw code by carrying object references through the row command path.

Expected benefit:
- Low to medium. This reduces single-threaded CPU waste and gives clearer measurements. It will not solve the long-term draw-call ceiling by itself.

## Plan B: SDL Command-List and Atlas Bridge
Goal: reshape the renderer so compatible graphical work can be batched before committing to Vulkan.

Scope:
- Introduce a city render command list with texture handle, source rect, destination rect, color, layer, and z/order metadata.
- Move XML payload graphics into shared GPU-friendly atlases or texture arrays instead of many independent managed textures.
- Sort or group compatible commands where it does not violate isometric layer order.
- Keep SDL2 initially; SDL3 only becomes useful after the renderer boundary accepts command lists instead of immediate image calls.
- Use worker threads to build command lists or update dirty instance/state buffers, then submit on the render thread.

Implementation sequence:
1. [~] Add renderer-facing command structs and a recording mode behind the existing draw API; all ordinary, custom-image, and saved-region draw operations record, while pixel-parity coverage remains.
2. [~] Batch obvious same-texture UI/terrain/building runs; the preparation pool emits order-preserving managed/custom/saved texture spans, and backend draw-call coalescing remains.
3. Convert image payload materialization to allocate from shared atlases.
4. Measure draw-call count, texture switches, and command-list build time. Request/submission and texture-switch metrics exist; command-list build timing is still pending.
5. Decide whether SDL3 materially improves the command-list backend.

Expected benefit:
- Medium to high. This bridges current code to a future Vulkan backend while still allowing incremental verification.

## Plan C: Vulkan Hardware Renderer
Goal: make high-resolution, HDR, shaded, large-city rendering viable long term. This repository can keep "Vulcan" as shorthand in discussion, but the implementation target is Vulkan.

The design constraint is no longer the original 1 source pixel to 1 logical pixel world. Vespasian should eventually be able to replace legacy art with assets that are 6x larger horizontally and vertically, or 36x the source pixels, while preserving the same logical footprint on screen. That makes CPU-side per-sprite scaling, atlas bleed, immediate-mode draw submission, and movement/render math that assumes integer source-pixel offsets unacceptable as the final architecture.

CityBuilder reference:
- `<legacy renderer repository>\docs\design\vulkan-migration-plan.md`
- `<legacy renderer repository>\docs\design\renderer.md`
- `<legacy renderer repository>\City Builder\VulkanRendererSupport.h`
- `<legacy renderer repository>\City Builder\RendererPayload.h`

Platform target:
- The long-term Vespasian target should be modern Vulkan-capable hardware across
  Windows, Linux, macOS, Android, and iOS where the platform stack can support
  it. The new renderer, thread pool, resource lifetime model, and asset pipeline
  should stay behind portable abstractions instead of baking in Windows-specific
  APIs. It is acceptable to stop fitting legacy low-power targets such as PS
  Vita or the original Switch, but the architecture should not make future
  desktop and mobile ports harder by assuming one OS at the foundation.

The important transferable lessons from that renderer are:
- simulation publishes immutable snapshots; rendering does not read mutable simulation buffers
- persistent scene data lives in VRAM; per-frame uploads send only compact typed deltas
- chunk or region freshness decides when GPU data is rebuilt
- shaders own ramps, tinting, exposure, tone mapping, and output encoding
- renderer scalar payloads are typed data, not CPU-precolored RGBA
- missing hardware requirements should fail loudly instead of silently falling back to CPU work

Scope:
- Add a Vulkan backend fed by persistent GPU resources: texture arrays or bindless descriptor tables, sprite instance buffers, terrain buffers, animation/state buffers, overlay payload textures, and a small set of pipelines.
- Keep asset source resolution separate from logical size. The renderer must accept source pixel dimensions, logical destination dimensions, and filter policy as independent data so 6x assets can render to the same world size as legacy assets.
- Store authored logical sizes in a fine-grained integer or fixed-point unit rather than floats. The current request boundary has a fixed logical-size bridge, but XML/callers are not migrated and the final authoring grain is still open.
- Treat figure movement positions as logical world/camera data. Tile progress, cross-country positions, sprite offsets, and continuous zoom should not require a source pixel to equal a logical pixel.
- Move city terrain, building, figure, tile, overlay, ghost, weather, and UI presentation into renderer-owned command or instance data instead of letting the main tick loop make thousands of individual draw decisions.
- Use worker threads for command-list construction, dirty chunk classification, texture upload staging, and instance-buffer rebuilds. The render thread should submit already-built batches and process GPU synchronization.
- Keep gameplay, placement validation, walker/path state, building state, and animation state resolution on the CPU. The GPU receives resolved IDs, frame indices, transforms, tint/material IDs, lighting flags, and semantic overlay payloads.
- Treat VRAM/RAM as acceptable tradeoffs when they remove single-threaded CPU draw work, repeated scaling, texture switches, and bandwidth-heavy per-frame submission.

Stretch goal: orthographic z-buffered city renderer:
- Replace tile-by-tile draw ordering with an orthographic camera, a depth buffer, and per-instance depth values derived from tile row/column, elevation, layer, object class, local sprite offset, and explicit priority.
- Draw the whole visible city through broad instanced passes rather than iterating every tile and making one-off draw calls.
- Use `VK_FORMAT_D32_SFLOAT` for world and preview depth images, matching the CityBuilder direction.
- Start with conservative depth domains for terrain, roads, buildings, figures, weather, ghosts, overlays, and UI; tune the formulas only after screenshot parity tests expose exact ordering conflicts.
- Keep screen-space UI depth disabled and composited last. UI should use the same renderer command model but not participate in world z.

Hardware rendering targets:
- Vulkan instance/surface creation should use the platform windowing layer only for surface creation and input, not OpenGL compatibility.
- Use dynamic rendering, explicit staging buffers, batched image/buffer copies, explicit image layout transitions, and a real allocator such as VMA or an equivalent suballocator.
- Upload native image groups into persistent GPU resources. Atlas fallback may exist only as a temporary compatibility bridge; final rendering should not depend on atlas sub-rect sampling.
- Prefer texture arrays or descriptor-indexed image tables for image-group frames, terrain variants, figures, buildings, UI, and weather.
- Use persistent per-object/per-tile instance buffers with revision counters. Dirty objects update their own records; stable objects remain resident.
- Compile XML paths plus named entries into stable GPU resource/material indices. Gameplay publishes compact semantic state (pose, direction, frame, variant, resource, color, and ordered layers); shader tables turn that state into atlas UVs and layer draws without restoring CPU-side absolute image-ID mutation.
- Keep simulation decisions on the CPU. Shaders may select declared frames/layers and apply presentation policy, but must not decide gameplay state transitions.
- Track renderer metrics from day one: draw calls, instance count, texture bindings, descriptor updates, staging bytes, upload waits, command build time, render thread time, and GPU frame time.

Parallelism targets:
- Create a startup thread pool shared by renderer preparation and other background systems.
- Keep Vulkan API calls and swapchain ownership on the render thread unless a specific Vulkan operation is deliberately made thread-safe.
- Run these tasks off-thread where practical:
  - visible chunk discovery
  - dirty chunk classification
  - sprite instance generation
  - terrain and road instance rebuilds
  - overlay scalar payload packing
  - animation/frame table updates
  - texture decode and staging buffer preparation
  - screenshot-parity and debug capture preparation
- Publish renderer snapshots through revisioned immutable data. The main tick loop should not block on draw preparation unless it is crossing a hard resource lifetime boundary.

HDR and lighting stretch goals:
- Use a scene-linear internal color target, preferably FP16 (`R16G16B16A16_SFLOAT`) before presentation.
- Prefer HDR output in the order proven in CityBuilder: FP16 scRGB first, HDR10 10-bit PQ second, SDR only when explicitly allowed by config or hardware fallback policy.
- Add startup logs for Vulkan device, monitor HDR capability, supported surface formats, selected swapchain format/color space, internal scene format, and depth format.
- Move tint, overlay ramps, weather, fire, torchlight, lightning, dusk/dawn, and daytime shading into shader-side material/light policies.
- Keep authored colors and XML colors as sRGB inputs converted at the renderer boundary; do not reintroduce CPU-packed presentation colors as the data contract.
- Model simple light sources first as compact GPU payloads: position, radius, intensity, color, flicker seed, and occlusion/material flags. Complex shadows can wait until depth and sprite instancing are stable.
- Treat HDR and lighting as a user-involved visual parity milestone, not a silent renderer cleanup. Legacy RGB assets can look wrong under HDR if exposure, tone mapping, material response, light policy, and day-cycle assumptions are not tuned together, so the first implementation should boot into neutral SDR-equivalent output and only enable HDR/light effects behind explicit config and screenshot/manual review gates.

Implementation sequence:
1. Finish the command-list/instance-data abstraction from Plan B so city/world/UI draw calls can be recorded without immediate SDL submission.
2. [~] Add renderer snapshots with explicit revisions for terrain, buildings, figures, overlays, ghosts, animations, weather, and UI. Sealed renderer command packets now carry command/resource revisions and retained immutable lifetime; typed world/object snapshots remain.
3. [~] Add a thread-pool-backed preparation stage that builds visible chunk lists, dirty instance buffers, and staging uploads before the render thread submits. The bounded pool and newest-packet publication path are live for command batching; chunk discovery and staging uploads remain.
4. Create a Vulkan backend behind the renderer interface, initially for city terrain/building sprites and one screen-space UI pass.
5. Move native image payloads and extracted legacy images into Vulkan texture arrays or descriptor-indexed image tables. Keep atlas fallback visible as debt and measure every time it is used.
6. Add per-instance buffers for position, fixed-point logical size, source rectangle/frame index, layer/depth, tint/material ID, animation frame, filter policy, and shader flags.
7. Feed figure/world positions as logical coordinates with normalized progress or fixed-point world units, so the renderer can interpolate through continuous zoom without depending on legacy pixel increments.
8. Add orthographic camera math and a conservative depth model for terrain, roads, buildings, figures, overlays, ghosts, and weather. Keep UI screen-space and last.
9. Convert figures and remaining legacy graphics to the same native resource model as buildings so high-resolution Vespasian assets can declare logical size without bespoke draw branches.
10. Add HDR scene target and output selection. Start with neutral SDR-equivalent presentation, then enable HDR10/scRGB output and tone mapping after parity is stable.
11. Add shader-side lighting and material policies for torches, fire, lightning, weather, and daytime changes.
12. Delete SDL/OpenGL-style immediate city draw hot paths once the Vulkan path covers terrain, buildings, figures, overlays, ghosts, weather, and UI with screenshot parity.

Expected benefit:
- Very high. CityBuilder saw around a 10x frame-time reduction moving from OpenGL-style rendering to Vulkan, and Augustus has the same shape of problem: too much CPU-directed draw work, too many tiny submissions, and too much legacy atlas/scaling behavior.
- This is also a prerequisite for 6x-by-6x assets. Without GPU-resident resources, logical-size-aware draw requests, batching/instancing, and shader-side filtering/tinting, 36x asset pixels will turn the renderer into the next main bottleneck.
