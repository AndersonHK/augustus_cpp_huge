# Renderer Scaling Seam Findings

## Problem

Scaling mode 3 currently exposes visible seams between city-view sprites, especially on terrain and water tiles. The issue should not be treated as a simple filter choice. All scaling modes need to produce exact, shared pixel boundaries so adjacent sprites continue to mesh.

## Findings

- `scale_filter=best` maps to SDL's best scaling mode through the render pipeline, but the city draw path still mixes native managed textures with legacy atlas-backed images.
- Main climate and terrain images are still loaded through `ATLAS_MAIN` in `src/core/image.cpp`.
- `src/platform/renderer.cpp` falls back to drawing from the legacy atlas texture when a render request does not have a managed texture handle.
- `src/graphics/image.cpp` still bridges legacy image ids through `ImageManager::from_id()` and derives logical draw size from source pixel size divided by scale.
- The city renderer has a grid-specific correction in `draw_texture_request()` that shrinks and offsets isometric textures when the grid is visible at high zoom. That can create dark joins even if the underlying art lines up.
- Atlas-backed rendering is especially vulnerable under linear or best filtering because sampling near an atlas image edge can bleed transparent, black, or neighboring pixels unless the atlas has padded gutters.
- Moving terrain into native image resources is necessary, but not sufficient by itself. The renderer also needs exact destination geometry so adjacent tiles share the same rounded screen-space edges.

## Current Source Status

- Render requests now carry `render_logical_size fixed_logical_size` alongside the legacy float logical width and height.
- `Render2DPipeline` resolves fixed logical dimensions first, then falls back to float logical dimensions or source image size, preserving current rendering for existing callers.
- Managed runtime texture requests pass the fixed logical-size fields through to the renderer request bridge.
- Figure drawing has started passing fixed logical-size requests, but broad XML/image metadata ownership is not migrated.
- `RENDER_LOGICAL_UNITS_PER_PIXEL = 6` is a compatibility seam, not the final authoring grain for city graphics.
- Atlas fallback, grid correction, and exact shared-edge city tile geometry remain open seam risks.

## Prescription

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

8. Add Vespasian half-size FigureType graphics after figure graphics ownership lands:
   - prerequisite: figures must own native graphics like buildings do, with the game loop asking the `Figure` object for a resolved draw request instead of reconstructing image ids from type/action branches
   - follow `docs/figure_owned_native_graphics_plan.md` before authoring this data slice
   - add Vespasian FigureType XML overrides for each resized figure
   - point those overrides at the same extracted pixel art as before
   - declare logical dimensions in the final fixed-point logical-size unit, with half-size figures as the first validation case
   - keep all logical-size declarations as integers in the chosen fine grain, so later 1/3-size, 0.15x, or 6.67x source-to-logical relationships do not introduce floating-point drift into city rendering
   - verify that tile anchoring, sprite offsets, carts/overlays, corpses, selected-figure coordinates, and zoomed scaling still line up

## Future Slice Shape

The likely clean slice is:

1. Fix grid overlay rendering so it no longer changes tile sprite dimensions.
2. Add exact destination rect snapping for city terrain.
3. Add atlas gutters as a short-term safety net.
4. Move terrain and water graphics out of `ATLAS_MAIN` and into managed image resources.
5. Choose the final integer logical-size grain, then add explicit logical dimensions to image group XML and migrate callers onto the fixed request fields.
6. Move figures to native-owned graphics.
7. Add Vespasian half-size FigureType logical-size overrides using the same source art.
