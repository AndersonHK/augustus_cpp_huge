# Renderer Scaling Seam Findings

## Problem

Scaling mode 3 currently exposes visible seams between city-view sprites, especially on terrain and water tiles. The issue should not be treated as a simple filter choice. All scaling modes need to produce exact, shared pixel boundaries so adjacent sprites continue to mesh.

## Findings

- `scale_filter=best` maps to SDL's best scaling mode through the render pipeline, but the city draw path still mixes native managed textures with legacy atlas-backed images.
- Main climate and terrain images are still loaded through `ATLAS_MAIN` in `src/core/image.cpp`.
- `src/platform/renderer.cpp` falls back to drawing from the legacy atlas texture when a render request does not have a managed texture handle.
- `src/graphics/image.cpp` still bridges legacy image ids through `ImageManager::from_id()` and derives logical draw size from source pixel size divided by scale.
- The city renderer previously had a destination-geometry correction in `draw_texture_request()` that shrank and offset isometric textures at some zooms. The destination mutation is removed, but the remaining atlas source-edge crop and atlas fallback still need pixel validation.
- Atlas-backed rendering is especially vulnerable under linear or best filtering because sampling near an atlas image edge can bleed transparent, black, or neighboring pixels unless the atlas has padded gutters.
- Moving terrain into native image resources is necessary, but not sufficient by itself. The renderer also needs exact destination geometry so adjacent tiles share the same rounded screen-space edges.

## Current Source Status

- Render requests now carry `render_logical_size fixed_logical_size` alongside the legacy float logical width and height.
- `Render2DPipeline` resolves fixed logical dimensions first, then falls back to float logical dimensions or source image size, preserving current rendering for existing callers.
- `Render2DPipeline` owns `scale_filter` config interpretation for both per-request city draw filtering and renderer texture quality hints.
- Managed runtime texture requests pass the fixed logical-size fields through to the renderer request bridge.
- Figure drawing has started passing fixed logical-size requests, but broad XML/image metadata ownership is not migrated.
- `RENDER_LOGICAL_UNITS_PER_PIXEL = 120` is the selected integer authoring grain. It represents halves, thirds, sixths, twentieths/0.15 scale, and six-times scaling without authored floats.
- `ATLAS_MAIN` fallback now has one-pixel duplicated-edge gutters and no longer uses the old source-edge crop. Exact shared-edge city tile geometry is selected by mesh-critical footprint submissions; near edges floor and far edges ceil so fractional draws close rather than expose the target clear color.
- `JuliusGraphicsExtractor` preserves the real 58x30 flat-tile, grass, and water pixels after each climate pass. `RendererSeamTest` decodes those snapshots and requires the 4,608-case climate/backend/filter/scale/grid/orientation/scene matrix to pass with zero skips. The current Release result is 4,608/4,608.

## Progress Checkpoint

- [x] Rename the config concept to global `scale_filter` and route it through `Render2DPipeline`.
- [x] Add a fixed logical-size bridge to render requests.
- [~] Start passing fixed logical-size requests from figure drawing.
- [x] Choose the final fine-grained integer logical-size unit for city-view XML: 120 units per logical pixel.
- [x] Add automated terrain/water seam pixel checks.
- [x] Remove grid rendering tile-size mutation.
- [x] Add exact shared-edge city tile destination geometry.
- [x] Add atlas edge padding while atlas fallback remains.
- [x] Migrate terrain, water, and climate images into managed native resources.
- [ ] Remove atlas fallback from city draw after native coverage is complete.
- [ ] Author Vespasian scaled graphics only after figure-owned native graphics and every seam item above are complete. Active Vespasian FigureTypes are path-backed, but scaled logical dimensions must be authored on clean graphics assetlists rather than on FigureType XML.

## Prescription

This whole prescription is a prerequisite for Vespasian scaled FigureType XML.
Do not author the scaled XML slice until every seam item below is implemented
and validated. The source-pixels versus fixed-point-logical-size split is the
specific dependency that makes scaled figure XML meaningful, but the seam
work must land as a complete renderer slice so the new logical sizes do not
inherit broken destination geometry or atlas filtering artifacts.

1. Add a focused render test matrix for city terrain:
   - scale filters: nearest, linear, best
   - grid on and off
   - close zoom levels where seams are obvious
   - atlas-backed images versus managed native images

2. Remove tile-size mutation from grid rendering:
   - stop using `grid_correction` to shrink isometric tile sprites
   - draw the grid as a separate overlay instead of changing terrain sprite dimensions

3. Introduce exact city-tile destination geometry:
   - compute screen-space left, right, top, and bottom edges from canonical logical tile bounds
   - round shared edges once, so neighboring tiles use identical pixel coordinates
   - avoid per-sprite floating-point drift for mesh-critical terrain and water tiles

4. Add temporary atlas edge padding while legacy atlas rendering remains:
   - duplicate edge pixels into 1-2 pixel gutters during atlas packing or upload
   - keep source rects on the real image content while allowing filtering to sample safe padded pixels

5. Migrate terrain, water, and climate images into the native managed image pipeline:
   - treat `ATLAS_MAIN` as extraction/bootstrap input only
   - require city-view draw calls to resolve to managed resources for terrain and buildings
   - remove the atlas fallback from the city draw path after coverage is complete

6. Split source pixel dimensions from logical dimensions in image metadata:
   - image groups should be able to declare logical width and height independently from source size
   - logical size should be represented in a fine-grained integer unit or fixed-point format, not as authored floats
   - choose a grain fine enough to represent common ratios such as 1/2, 1/3, 1/6, 2/3, 1/6.67, and 6x source art without awkward decimal values
   - do not treat the current `RENDER_LOGICAL_UNITS_PER_PIXEL` bridge as the final grain; city-view graphics may need a substantially finer unit such as subpixel logical units per legacy pixel so authoring remains integer-only
   - the render request already carries a fixed logical-size bridge; migrate image metadata and callers to use it once the final grain is chosen
   - this supports future high-definition assets, such as a 360x360 source rendered as a 60x60 logical sprite

7. Verify with pixel checks:
   - render controlled terrain/water patches
   - sample expected shared edges for black, transparent, or unmatched pixels
   - keep screenshots for visual regression comparisons at the problematic zoom levels

8. Unlock Vespasian scaled FigureType graphics after figure graphics ownership and all seam work land:
   - prerequisite: complete every item above, including the split between source pixel dimensions and fixed-point logical image dimensions
   - prerequisite: figures must own native graphics like buildings do, with the game loop asking the `Figure` object for a resolved draw request instead of reconstructing image ids from type/action branches
   - follow `docs/figure_owned_native_graphics_plan.md` before authoring this data slice
   - add Vespasian FigureType XML overrides for each resized figure
   - point those overrides at the same extracted pixel art as before
- declare logical dimensions in the final fixed-point logical-size unit, with scaled figures as the first validation case
   - keep all logical-size declarations as integers in the chosen fine grain, so later 1/3-size, 0.15x, or 6.67x source-to-logical relationships do not introduce floating-point drift into city rendering
   - verify that tile anchoring, sprite offsets, carts/overlays, corpses, selected-figure coordinates, and zoomed scaling still line up

## Pixel-Check Test Matrix

The next implementation-light milestone is a focused renderer seam tester. This is not a manual screenshot checklist; passing pixel checks should be treated as a deployment-worthy milestone for renderer seam work. Manual testing can still inspect the screenshots, but a change should not be considered ready for broad deployment until the matrix reports clean terrain/water seams.

### Harness Shape

- Add a future `RendererSeamTest` executable beside the existing focused tools, using `tools/renderer_seam_test/` plus `RendererSeamTest.vcxproj`.
- Keep it separate from `StartupParserTest`: the startup parser test validates definitions and generated asset availability, while `RendererSeamTest` should own draw setup, screenshot capture, and pixel assertions.
- Reuse the same game-root and generated-asset prerequisites as `StartupParserTest`: require Julius/Augustus extraction stamps before loading graphics, and fail loudly when generated assets are missing.
- Run in a deterministic headless/window-hidden mode that can render one fixed city-view viewport to an offscreen target and write both PNG screenshots and JSON assertion results under `out/renderer_seams/`.
- Initial backend coverage is `atlas-fallback`; `managed-native` entries should be explicit expected-skips until terrain/water native resources are available, then become required.

Suggested command shape:

```powershell
RendererSeamTest.exe --game-root "<game install path>" --matrix terrain-water --artifacts out\renderer_seams
```

### Inputs

Each matrix case should record these inputs in its JSON result:

- `mod`: at least `Vespasian`; add `Augustus` and `Julius` when comparing extracted legacy coverage.
- `climate`: central, northern, desert, because terrain and water art differ by climate.
- `backend`: `atlas-fallback`, then `managed-native` after terrain/water migration.
- `scale_filter`: nearest, linear, best.
- `city_scale_percent`: 100, 125, 150, 175, 200, 250, 275, 300. The 250/275/300 cases cover the zoom range where previous source/destination corrections were most visible.
- `grid`: off, on.
- `orientation`: all four city orientations.
- `scene`: the controlled patch name listed below.

The first executable slice can run a smaller smoke subset by default, but CI/deployment signoff should run the full matrix.

### Controlled Scenes

Use synthetic or fixture city-view patches with no walkers, buildings, animations, tooltips, ghost previews, or overlay UI except the optional grid pass.

- `solid-terrain-8x8`: adjacent identical flat terrain tiles. This catches transparent, black, or background-color seams where there should be continuous terrain coverage.
- `water-8x8`: adjacent water tiles with a fixed animation frame. This catches atlas/filter bleed and destination rounding gaps on animated water art.
- `terrain-water-cross`: terrain, shore, and water arranged in a cross or checker pattern with known intentional transition edges. This verifies the seam detector can distinguish real shore boundaries from gaps between same-surface tiles.
- `mixed-elevation-smoke`: a small terrain patch with elevation/rock edges, run as informational at first because intentional visual discontinuities are more common.

### Pixel Assertions

For every rendered case, compute expected interior shared-edge masks from canonical isometric tile bounds, not from whatever destination rectangles were submitted. Exclude the outer viewport border and any intentionally visible shore/elevation transition pixels.

- `coverage_no_background`: With grid off, no sampled interior seam pixel may be fully transparent or equal to the test target clear color.
- `no_black_gap`: With grid off, no sampled interior seam pixel may be opaque black unless the source fixture explicitly marks that pixel as allowed.
- `same_surface_delta`: For same-surface neighbor pairs, sample one pixel on each side of the shared edge and require RGB delta <= 1 for nearest and <= 3 for linear/best. Record larger deltas with coordinates, tile ids, filter, zoom, and backend.
- `grid_overlay_only`: With grid on, compare against the matching grid-off render. All changed pixels must fall inside the expected grid-line mask, plus a one-pixel tolerance for filtering. Sprite coverage outside the grid mask must be identical to grid-off.
- `no_grid_side_effect_gap`: With grid on, pixels immediately adjacent to the expected grid-line mask must still satisfy `coverage_no_background` and `no_black_gap`.
- `backend_parity`: Once `managed-native` terrain/water exists, compare atlas and native renders for the same case. Differences are allowed only where atlas gutters/source-crop compatibility is explicitly recorded; no backend may introduce transparent or black seam pixels.

Each failure should write:

- the screenshot,
- a seam-mask PNG,
- a failure-overlay PNG highlighting bad pixels,
- a JSON row with matrix inputs, failing assertion, coordinates, sampled colors, and the two tile ids involved.

### Deployment Threshold

Renderer seam changes become deployment-worthy only when:

- the smoke subset passes before local manual testing,
- the full matrix passes before a release deploy or before authoring Vespasian scaled FigureType XML,
- every expected-skip has an owner and a prerequisite listed in this plan,
- screenshot artifacts are retained for failed cases so manual review starts from exact pixels instead of eyeballing the whole city.

## Future Slice Shape

The likely clean slice is:

1. Fix grid overlay rendering so it no longer changes tile sprite dimensions.
2. Add exact destination rect snapping for city terrain.
3. Add atlas gutters as a short-term safety net.
4. Move terrain and water graphics out of `ATLAS_MAIN` and into managed image resources.
5. Choose the final integer logical-size grain, then add explicit logical dimensions to image group XML and migrate callers onto the fixed request fields.
6. Complete the figure-owned native graphics payload migration.
7. Add Vespasian scaled FigureType logical-size overrides using the same source art only after the full seam slice is validated.
