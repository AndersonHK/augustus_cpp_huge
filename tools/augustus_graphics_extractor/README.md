# Augustus Graphics Extractor Harness

This folder contains the standalone test harness for the generated Julius plus Augustus graphics extraction pipeline.

Build project:

```text
AugustusGraphicsExtractor.vcxproj
```

Runtime entrypoint:

```text
tools\augustus_graphics_extractor\main.cpp
```

The harness is intentionally thin:

- `HarnessCli` owns CLI parsing, defaults, and path normalization.
- `--extract-julius-first` calls `image_load_climate(..., extract_legacy_graphics = 1)`, which reaches the runtime `RuntimeGraphicsExtractionService` bridge.
- The final configured Augustus pass calls the C++ `AugustusExtractor` API directly with `ExtractorPaths` and `ExtractorOptions`.

Full design, CLI usage, expected output counts, and validation commands are documented in:

```text
docs\graphics_extraction_pipeline.md
```

Recommended clean test command from the repo root:

```powershell
.\x64\Release\AugustusGraphicsExtractor.exe `
    --game-root 'D:\Games\GOG Games\Caesar 3' `
    --julius-graphics 'x64\Release\Mods\Julius\Graphics' `
    --output 'x64\Release\Mods\Augustus\Graphics' `
    --extract-julius-first
```

`--extract-julius-first` is important when simulating runtime. Augustus extraction depends on the extracted Julius template files for numeric reference translation and generated image numbering.
That mode runs the runtime Julius plus Augustus bootstrap through `image_load_climate()`, then verifies the configured Augustus paths by stamp instead of forcing a duplicate extraction.
