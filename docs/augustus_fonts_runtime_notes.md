# Augustus Fonts Runtime Notes

## Current state in the repo

- The game can ingest vector fonts through `src/graphics/font_vector_runtime.cpp`.
- Legacy encoded text is converted to UTF-8 at the text boundary, then measured and drawn by the vector runtime when it is active.
- Rich-text layout exists in `src/graphics/rich_text.cpp` and now sends explicit integer pixel sizes to the vector text path.
- UTF-8 already exists at the localization boundary in:
  - `src/translation/localization_runtime.cpp`
  - `src/core/encoding.c`
- The renderer has moved substantially toward per-image rendering; vector glyphs are rendered as runtime textures and use the high-quality scaling policy.

## What files we would ship

For the new vector-font path, the practical asset formats to bundle are:

- `.ttf` for most Latin, Greek, and Cyrillic families
- `.otf` or `.ttf` for CJK families

Formats to avoid as the first runtime target:

- `.woff`
- `.woff2`
- `.ttc`
- `.otc`

`woff` and `woff2` are web-delivery formats, not the best native runtime target here.
`ttc` and `otc` font collections are possible later, but they add extra loader complexity.

## Recommended folder shape

Suggested mod-local layout:

```text
Mods/<mod>/Fonts/
  Latin/
    Marcellus-Regular.ttf
    OldStandard-Regular.ttf
    OldStandard-Italic.ttf
    OldStandard-Bold.ttf
  Greek/
    GFSDidot-Regular.ttf
  Cyrillic/
    OldStandard-Regular.ttf
    OldStandard-Italic.ttf
    OldStandard-Bold.ttf
  CJK/
    SourceHanSerifSC-Regular.otf
    SourceHanSerifTC-Regular.otf
    SourceHanSerifJP-Regular.otf
    SourceHanSerifK-Regular.otf
```

## Recommended font roles

- Headings and building names: `Marcellus`
- Body text, sidebars, help text, rich-text paragraphs: `Old Standard TT`
- Greek headings: `GFS Didot`
- Cyrillic body/headings: `Old Standard TT`
- CJK: region-specific `Source Han Serif`

## What the engine needs to ingest

The new text runtime should load, at minimum:

- font file path
- font family role
- face/style selection:
  - regular
  - bold
  - italic
  - bold-italic
- locale or script mapping
- fallback order for missing glyphs
- requested UI pixel size
- shaping/layout input from UTF-8 text
- glyph raster cache for DPI-scaled rendering

## Recommended ingest model

The clean runtime path is:

1. Load `.ttf` / `.otf` font files from `Mods/<mod>/Fonts/...`
2. Resolve the requested font role by UI usage and locale/script
3. Shape UTF-8 text into glyph runs
4. Rasterize glyphs at the requested integer pixel size supplied by the caller
5. Cache glyphs or text runs for reuse
6. Submit the resulting glyph images through the renderer

## Runtime size contract

Text callers are responsible for deciding the final integer pixel size they want rendered. For UI text, that usually means applying the UI scale at the widget/source call site, for example:

```c
screen_ui_to_pixel(font_definition_for(font)->line_height)
```

The text layer should not infer a separate `scale` or size delta after the call. It receives the requested pixel size, rasterizes glyphs at that size, and returns layout widths in the active render domain. This keeps each text draw or measure call self-contained.

## Important implementation note

If `Marcellus` is used as the primary Latin heading face, it should not be the only Latin family in the stack.

Reason:

- `Marcellus` is the closest visual match to Caesar 3
- but it is a regular-only family
- so body text and rich-text transforms are better handled by `Old Standard TT`

That makes the most practical mixed Latin stack:

- `Marcellus` for titles
- `Old Standard TT` for paragraph/UI text

## Relevant code references

- `src/graphics/font.cpp`
- `src/graphics/font.h`
- `src/graphics/font_vector_runtime.cpp`
- `src/graphics/rich_text.cpp`
- `src/graphics/text.cpp`
- `src/translation/localization_runtime.cpp`
- `src/core/encoding.c`

