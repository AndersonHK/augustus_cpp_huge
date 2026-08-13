# Runtime DLL Boundary Refactor Plan

## Intent

Split one-shot startup and bridge code out of the always-loaded runtime executable. The runtime should stay clean and lean: simulation, rendering, input, and current game state. Extraction, XML parsing, and save migration are setup/bridge jobs that should run, return typed runtime data or generated files, then leave memory and the runtime namespace.

The target model is strict DLL/plugin-style boundaries:

- load the DLL
- call a narrow exported entry point
- receive validated output or a fatal diagnostic
- unload the DLL
- never call or reference its internals from normal runtime code

The shared exception is the crash/error context logger and the smallest shared core ABI needed to report fatal startup diagnostics.

Separate focused executables may share implementation through a shared DLL or static module boundary. For example, `JuliusGraphicsExtractor.exe` and `AugustusGraphicsExtractor.exe` should remain separate contract tests, but the legacy image decoding, filesystem helpers, PNG writing, and extraction reports should live in a shared graphics-extraction library. The same rule applies to runtime, XML startup parsing, and save/load bridging: keep executable entry points narrow and independent, share only through explicitly owned module contracts, and avoid duplicated private helper code.

## Ownership Contract

The target architecture is a handoff between layers, not a shared reset/rebuild system.

- XML startup creates immutable type definitions: `BuildingType`, `FigureType`, resources, units, formations, graphics definitions, pathing definitions, and other registries. Once startup has produced those objects, startup is done until program shutdown.
- Runtime consumes those resolved definitions and live objects. It creates, uses, mutates, and removes live runtime objects, but it does not reset, recreate, or reload type registries.
- Save/load bridging is the only layer that knows save records. It reads records and produces current live objects, serializes current live objects back into records, and applies old-save migrations. Runtime should not know that legacy records exist.
- The bridge does not own the game loop. It should not run ticks, route walkers as gameplay, render, or repair runtime behavior beyond import/export and compatibility migration.
- Graphics extraction only extracts legacy assets into native files. It does not own runtime rendering, XML gameplay meaning, or save migration.

## Boundary Smells

The dependency graph is part of the design feedback. If a focused tester like `StartupParserTest` needs to compile against most runtime files, the boundary is wrong or still too porous. The tester should call the same small startup-parser contract the game calls; it should not need city simulation, rendering, save migration, extraction helpers, or record-era gameplay internals just to validate XML startup.

Use compile dependencies as a partitioning guide:

- code needed only to decode legacy graphics belongs behind the graphics extraction DLL
- code needed only to parse and validate XML belongs behind the startup parser DLL
- code needed only to read old saves, migrate records, or bridge legacy ids belongs behind the save/load bridge DLL
- code needed by the live game loop belongs in runtime, and should consume current-version objects

The target runtime contract is object-owned. Runtime code should not know raw save records; it should receive current-state objects. Save/load should not know XML parser internals. XML startup should not know save bridge internals. Graphics extraction should be completely self-contained and unrelated to runtime rendering after generated assets exist.

### Startup Parser Test Dependency Audit

Current direct tester shape:

| Dependency surface | Current state | First extraction target |
| --- | --- | --- |
| Runtime reset internals | `StartupParserTest` no longer includes `building_runtime.h` or calls `building_runtime_reset()` directly. Startup parser cleanup must remain parser-owned; registry load should produce immutable definitions, not reset runtime bridge state. | Keep tester cleanup limited to parser-owned teardown until the startup facade returns a typed `StartupDefinitions` object. |
| Registry load order | `StartupParserTest` now calls `startup_parser::parse_startup_definitions()` and prints returned step diagnostics instead of calling each registry directly. The facade still uses global registries internally as the static-boundary first step. | Convert the facade's `StartupDefinitions` from a summary into the immutable definition payload runtime startup will consume. |
| Startup environment reporting | `StartupParserTest` now asks the facade for game root, mod stack, and selected mod path through `startup_parser::inspect_startup_environment()` instead of including `game/mod_manager.h` directly. | Keep moving tester-visible startup facts behind the facade until the executable can link only shared-core/parser sources. |
| Graphics validation inputs | The startup parser facade now has an explicit pre-registry graphics-validation preparation step, so `StartupParserTest` installs the minimal headless renderer needed to validate generated image-group payloads. The test executable checks extraction stamp files before parsing any XML and fails with exact `JuliusGraphicsExtractor` / `AugustusGraphicsExtractor` commands when deployed Mods have not been regenerated. | Replace runtime image-cache loading with parser/graphics-owned image-group validation data after graphics extraction exposes a self-contained generated-asset manifest. |
| Project compile graph | `StartupParserTest.vcxproj` now uses curated startup/static-boundary source groups instead of `src\**\*.cpp`. The list keeps startup, XML registries, generated image-group materialization, core file/XML helpers, and minimal platform file/log support while leaving out window/widget/input/editor/sound/app-shell sources and extractor implementation files. `GraphicsDefinition` is now the shared graphics base in `src\graphics\GraphicsDefinition.h/.cpp`; `BuildingGraphics` data/selection methods live in `src\building\BuildingGraphics.h/.cpp`, `FigureGraphics` owns FigureType draw policies in `src\figure\FigureGraphics.h/.cpp`, and `ResourceGraphics` is limited to icon presentation in `src\game\ResourceGraphics.h/.cpp`. XML graphics path/source resolution now lives in `src\assets\xml_path_resolution.cpp`, generated image-copy primitives now live in `src\assets\image_copy.cpp`, pathing mode XML metadata now lives in `src\figure\PathingMode_metadata.cpp`, parser-visible `ProductionMethod` data now stays in `src\building\production_method.cpp` while live production eligibility/progress lives in `src\building\production_method_runtime.cpp`, and parser-visible `Distribution` rules stay in `src\building\distribution.cpp` while live source lookup/acceptance behavior lives in `src\building\distribution_runtime.cpp`. Parser-test builds now fence out live `BuildingGraphics` condition evaluation and construction-phase building-state selection instead of supplying fake live building state, and no longer need runtime `PathingMode` terrain probes. | Split the remaining parser-test shim surfaces into parser-owned sources: generated image materialization still depends on runtime image/atlas loader hooks, and registry cleanup still calls menu/monument cache invalidation hooks until definition loading returns immutable payloads. |
| Platform/UI shims | `platform_shims.cpp` supplies parser-test no-ops for menu/monument cache invalidation, runtime image/atlas entry points, legacy image lookups, and building/scenario validation bridges. The project no longer links SDL_mixer or SDL_ttf; stale timing, resize/fullscreen, folder-dialog, exit, external-pixel loader, runtime building reset, and terrain/pathing probe hooks are no longer present; SDL2 remains for the current file/log platform helpers. Production-only city finance, mothball, game calendar, shipyard water-spawn, distribution source lookup, storage permission, stockpile, acceptance, live graphics-condition building state, climate/festival, and runtime PathingMode shims are no longer needed by parser-time loading. | Move file/log support into shared-core/parser-owned code so the parser test can eventually drop SDL2 entirely; replace runtime image loader stubs only after generated graphics validation reads a self-contained generated-asset manifest. |

## Candidate DLLs

### Graphics Extraction DLL

Owns:

- legacy graphics extraction
- atlas/source-file decoding needed only for extraction
- generated PNG/XML/stamp/manifest output
- extractor-specific compatibility tables and source heuristics

Does not own:

- runtime draw submission
- city image selection
- runtime image cache
- BuildingType/FigureType gameplay semantics

Runtime contract:

```cpp
ExtractionResult extract_graphics(const ExtractionRequest &request, ErrorSink &errors);
```

After extraction succeeds, the runtime loads generated native assets through the normal image/resource pipeline. Extractor implementation details must not stay reachable from the game loop.

Future extractor revisions should preserve and emit more meaningful metadata
from the legacy source files: stable group names, useful default entries,
source dimensions, offsets, layer intent, and any recoverable animation/action
structure. The current extractor output is a migration artifact that often
forces FigureType or BuildingType graphics XML to redeclare metadata that was
lost during extraction. Authored Vespasian graphics are not constrained by that
shape and should remain free to use clean one-group-per-file names and default
entries.

### XML Startup Parser DLL

Owns:

- strict XML schema parsing
- startup dependency ordering between definitions
- key/string/id resolution into serialized runtime definition blobs
- validation that cross-references resolve
- diagnostics for missing or invalid mod data

Does not own:

- live simulation ticks
- save/load migration
- rendering
- fallback behavior that hides invalid data

Runtime contract:

```cpp
StartupDefinitions parse_startup_definitions(const StartupParseRequest &request, ErrorSink &errors);
```

The runtime receives already-validated definition objects or a compact immutable data blob. Normal gameplay code should not include parser-private headers, XML node walkers, schema helpers, or temporary compatibility lookup tables.

### Save/Load Bridge DLL

Owns:

- reading every supported legacy and current save format
- migration between save versions
- legacy enum/id/text bridges needed only to understand stored data
- mod-owned compatibility bridge declarations, once those replace hardcoded legacy-id tables
- conversion from serialized save records into current runtime-owned objects
- writing current save format

Does not own:

- normal runtime ownership rules
- tick-time repair scans
- city simulation behavior
- graphics or XML parsing internals

Runtime contract:

```cpp
LoadedGameObjects load_save_objects(const SaveLoadRequest &request, ErrorSink &errors);
SaveResult write_save_from_objects(const SaveWriteRequest &request, const RuntimeObjectExportView &objects, ErrorSink &errors);
```

Once a save is loaded, bridge-only compatibility data should be gone. The runtime should hold object references, typed registries, and current-version state.

Long-term bridge declarations are tracked in `docs/mod_owned_compatibility_bridge_plan.md`. The save/load DLL should be the eventual owner of those declarations at migration time, while runtime continues to see only resolved objects.

## Shared Core Boundary

Allowed shared dependencies:

- crash/error context logging
- platform-neutral file path primitives needed by the exported ABI
- stable plain data structures used as DLL boundary payloads
- versioned ABI structs

Disallowed shared dependencies:

- parser-private XML helper APIs in runtime gameplay code
- extractor-private image tables in runtime draw code
- save-migration enums or bridge tables in runtime simulation code
- broad global registries that let DLL internals leak back into the executable

Headers at the DLL boundary should be small, versioned, and mostly plain data. Prefer opaque handles and exported functions over sharing implementation classes across the boundary.

The first concrete shared boundary is `startup/startup_parser_abi.h`. It is a
C-compatible, versioned request/result contract with explicit flags, reserved
field validation, fixed caller-owned failure diagnostics, transient step and
ordered-mod callbacks, and caller-owned environment buffers with complete
source lengths for truncation detection. Unknown versions, truncated result
structures, reserved values, and unknown flags fail before parser state
changes. Both the game bootstrap and `StartupParserTest` call this ABI. The ABI
adapter delegates to the internal `startup_definition_loader` module; runtime
callers cannot include registry orchestration or parser-owned result types.
The current implementation remains a static boundary and still publishes the
validated definitions into global registries internally. Replacing that final
static bridge with a transferred immutable definition payload is the remaining
ownership prerequisite for moving the same ABI implementation into a DLL.

## Load/Unload Discipline

Each one-shot DLL should be treated as a tool that runs and disappears:

1. Runtime/bootstrap loads the DLL.
2. Runtime calls one exported function with an explicit request.
3. DLL reports through the shared crash/error context sink.
4. DLL returns success plus owned definitions, generated paths, or live object bundles; otherwise it returns failure plus diagnostics.
5. Runtime takes ownership of the returned startup definitions or live objects through an explicit result contract.
6. Runtime unloads the DLL.

No runtime object should store pointers to DLL-owned memory. Returned data must either be copied into runtime-owned storage or transferred through an explicit ownership handle that remains valid after unload.

## Migration Slices

- [x] Add headless startup parser executable as the immediate safety harness.
- [x] Isolate startup parsing behind a narrow C++ facade so callers stop reaching into individual registries directly.
- [~] Convert the facade output into an explicit `StartupDefinitions` object owned by runtime startup.
  - Current state: the facade returns step diagnostics and startup environment facts, but still relies on global registries as the static-boundary bridge.
- [~] Move parser-only helpers, schema walkers, and cross-reference validators behind that facade.
  - Current state: several parser-visible/runtime-visible seams were split, but generated image materialization and cleanup hooks still expose runtime-shaped dependencies.
- [~] Build the parser facade as a static boundary first, then as a DLL once payload ownership is explicit.
  - Current state: the versioned C ABI is exercised by both production startup and `StartupParserTest`; immutable definition payload transfer and DLL load/unload remain.
- [~] Apply the same treatment to graphics extraction, starting from the standalone Julius and Augustus extractor executables and a shared extraction implementation module.
  - Current state: a versioned C ABI owns Augustus extraction and climate-atlas bootstrap requests/results. Runtime climate loading and the standalone Augustus extractor call only that boundary; Julius reaches the same bootstrap ABI after its atlas load. Concrete extractor classes are private to the implementation module. Dynamic DLL loading/unloading remains future work.
- [ ] Apply the same treatment to save/load migration after current-version runtime ownership is strong enough that bridge data can be discarded immediately after import.

## Test Harness Pattern

The small test executables should become contract callers for these DLLs, not alternate implementations:

- `JuliusGraphicsExtractor.exe` should eventually load/call the shared graphics extraction DLL for legacy atlas extraction, verify generated Julius outputs, then exit.
- `AugustusGraphicsExtractor.exe` should eventually load/call the same graphics extraction DLL for Augustus image-group extraction, verify generated Augustus outputs, then exit.
- `StartupParserTest.exe` should require those generated graphics outputs, load/call the XML startup parser DLL, verify the returned `StartupDefinitions`, then exit.
- A future save bridge tester should load/call the save/load DLL against known-good saves and assert that they import into valid current runtime data.

That pattern keeps tests honest. If a DLL boundary changes, the game and its focused test executable both exercise the same exported contract. The testers remain small PowerShell-friendly smoke tests while the heavy compatibility code still lives in the one-shot DLL that can be unloaded after use.

## Quality Bar

- A DLL boundary is successful only if the runtime cannot accidentally call private implementation helpers after the DLL unloads.
- Namespaces should reflect ownership: `startup_parser`, `graphics_extractor`, `save_bridge`, and `runtime` should not bleed into each other.
- The runtime executable should get smaller and import fewer one-shot headers over time.
- Failing startup data must fail loudly before entering the game loop.
- Compatibility code belongs in the DLL that needs it, not in runtime fallback branches.
