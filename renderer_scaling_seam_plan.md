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
   - the render request should carry logical size explicitly
   - this supports future high-definition assets, such as a 360x360 source rendered as a 60x60 logical sprite

7. Verify with pixel checks:
   - render controlled terrain/water patches
   - sample expected shared edges for black, transparent, or unmatched pixels
   - keep screenshots for visual regression comparisons at the problematic zoom levels

## Future Slice Shape

The likely clean slice is:

1. Fix grid overlay rendering so it no longer changes tile sprite dimensions.
2. Add exact destination rect snapping for city terrain.
3. Add atlas gutters as a short-term safety net.
4. Move terrain and water graphics out of `ATLAS_MAIN` and into managed image resources.
5. Add explicit logical dimensions to image group XML and render requests.
