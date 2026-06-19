# Render Performance Plans

## Current Evidence
The first post-refactor large-city performance log points at rendering as the remaining frame-rate limiter. During active speed-1000 samples, city drawing is a large share of frame time, and the `city_draw_main_row` bucket dominates the city draw bucket. The hot path is the city view row traversal that draws building tops, figures, and animations.

The current renderer already uses SDL textures and an accelerated `SDL_Renderer` when available, so the issue is not "no GPU at all." The issue is that the game still does CPU-directed immediate-mode rendering: runtime code decides every visible sprite, resolves or checks state, and submits individual SDL draw calls.

## Plan A: CPU Draw-Loop and Cache Pass
Goal: recover short-term FPS without changing the renderer backend.

Scope:
- Reduce repeated visible-tile traversal in the city draw path while preserving layer order.
- Cache stable visible tile rows for a camera/viewport/orientation where practical.
- Move native graphics toward explicit invalidation and cheap cached reads instead of per-draw signature scanning.
- Prefer `Building&` / `Building` objects handed down from owner phases over repeated `Building::from_id` lookups.
- Add finer draw-side tracker buckets before making broad claims: top, figures, animation, native cache checks, native cache rebuilds, legacy image draws, SDL/managed draw submissions, texture switches.
- Consider thread-pool preprocessing only for state preparation or command-list construction. Do not parallelize into more renderer calls until draw-call overhead is measured.

Implementation sequence:
1. Collapse repeated row traversal overhead in `city_view_foreach_valid_map_tile_row`.
2. Split `city_draw_main_row` into top/figure/animation buckets.
3. Add a city draw command prepass that can carry `Building` objects or direct building pointers.
4. Replace broad graphics signatures with dirty flags or generation counters owned by the building runtime.
5. Remove low-risk `Building::from_id` call sites from draw code by carrying object references through the row command path.

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
1. Add renderer-facing command structs and a recording mode behind the existing draw API.
2. Batch obvious same-texture UI/terrain/building runs.
3. Convert image payload materialization to allocate from shared atlases.
4. Measure draw-call count, texture switches, and command-list build time.
5. Decide whether SDL3 materially improves the command-list backend.

Expected benefit:
- Medium to high. This bridges current code to a future Vulkan backend while still allowing incremental verification.

## Plan C: Vulkan Orthographic Z-Buffered Renderer
Goal: make high-resolution, HDR, shaded, large-city rendering viable long term.

Scope:
- Add a Vulkan backend fed by persistent GPU resources: texture arrays/atlases, instance buffers, animation/state buffers, and a small set of pipelines.
- Use an orthographic camera and depth values derived from tile row, layer, height, and object class so many layers can be drawn together safely.
- Keep gameplay and building state resolution on the CPU. The GPU should choose UVs, offsets, tint, lighting, animation frame, and weather/time shading from already-resolved instance data.
- Treat VRAM/RAM as acceptable tradeoffs to reduce single-threaded CPU work and bandwidth-heavy per-frame submission.

Implementation sequence:
1. Finish the command-list/instance-data abstraction from Plan B.
2. Create a Vulkan backend behind the renderer interface, initially for city terrain/building sprites only.
3. Move payload images into Vulkan texture arrays or bindless-style descriptor tables.
4. Add per-instance buffers for position, source rectangle, layer/depth, color, animation frame, and shader flags.
5. Add weather/time/HDR shading once sprite submission is stable.

Expected benefit:
- High, especially for 9x assets and future visual upgrades. This is the right long-term target, but only after the draw data has been separated from immediate SDL calls.
