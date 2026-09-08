# Loading stages

The game creates its window before compiling mod definitions or extracting graphics. A bootstrap renderer presents the selected mod's `UI/loading.xml` without depending on the registries it is preparing.

Each `stage` declares an identity, caption and its own background. Current stages are `graphics` (surveying, raw materials and foundations) and `mod_data` (construction progressing). A later upscaling operation can add another stage and report its work through the same observer. No upscaling operation or placeholder progress is implemented in this slice.

The selected theme comes from the last active mod that declares one. Definitions remain authored UI data; background PNGs are direct bootstrap inputs because the graphics registry is not ready yet.

Julius and Augustus declare `font="game"`. Font readiness is deferred: the bar and background render immediately, while labels wait for the original glyph bank. Once the legacy graphics reader has decoded it, the bootstrap renderer receives pixels and metrics from the same bank used by ordinary game text. Neither mod adds a typeface dependency. Vespasian points to its existing Marcellus font.

Loaders report actual completed work and totals synchronously. The extractor DLL accepts an optional, size-versioned progress callback; older callers remain valid. Progress draws pump close/window events and restore the SDL render target, viewport, clipping, scale, draw color and blend mode. They never call the simulation or draw a partially loaded city.

Hidden validation: `--loading-screen-test <absolute-output-directory>`. This captures all six mod/stage combinations, checks distinct backgrounds and renderer-state restoration, and exercises cancellation. For both original-font themes it also verifies that changing labels produces identical pixels while fonts are pending, progress still changes the bar, and fulfilling font readiness makes labels visible. `--test-config <absolute-directory>` isolates configuration for headless tests; it is rejected outside a validation mode. Headless shutdown does not save interactive preferences.

The readiness and stage checks passed in `out/font-readiness-final.log` with empty stderr, including restoration of a scaled and clipped render target. The captured original-font screen was visually inspected. This uses a synchronous readiness notification, so it needs neither a blocking future nor an extra rendering thread.

Artwork provenance, prompts, preserved concepts and the current native-resolution limitation are recorded in `res/graphics_source/loading_screens/README.md`.
