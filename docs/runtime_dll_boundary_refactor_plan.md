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
- conversion from serialized save records into current runtime-owned objects
- writing current save format

Does not own:

- normal runtime ownership rules
- tick-time repair scans
- city simulation behavior
- graphics or XML parsing internals

Runtime contract:

```cpp
LoadResult load_save_into_runtime(const SaveLoadRequest &request, RuntimeImportSink &runtime, ErrorSink &errors);
SaveResult write_runtime_save(const SaveWriteRequest &request, const RuntimeExportView &runtime, ErrorSink &errors);
```

Once a save is loaded, bridge-only compatibility data should be gone. The runtime should hold object references, typed registries, and current-version state.

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

## Load/Unload Discipline

Each one-shot DLL should be treated as a tool that runs and disappears:

1. Runtime/bootstrap loads the DLL.
2. Runtime calls one exported function with an explicit request.
3. DLL reports through the shared crash/error context sink.
4. DLL returns success plus data/output paths, or failure plus diagnostics.
5. Runtime copies/imports the returned data into runtime-owned objects.
6. Runtime unloads the DLL.

No runtime object should store pointers to DLL-owned memory. Returned data must either be copied into runtime-owned storage or transferred through an explicit ownership handle that remains valid after unload.

## Migration Slices

1. Add headless startup parser executable as the immediate safety harness.
2. Isolate startup parsing behind a narrow C++ facade so callers stop reaching into individual registries directly.
3. Convert the facade output into an explicit `StartupDefinitions` object owned by runtime startup.
4. Move parser-only helpers, schema walkers, and cross-reference validators behind that facade.
5. Build the parser facade as a static boundary first, then as a DLL once payload ownership is explicit.
6. Apply the same treatment to graphics extraction, starting from the standalone extractor executable.
7. Apply the same treatment to save/load migration after current-version runtime ownership is strong enough that bridge data can be discarded immediately after import.

## Test Harness Pattern

The small test executables should become contract callers for these DLLs, not alternate implementations:

- `AugustusGraphicsExtractor.exe` should eventually load/call the graphics extraction DLL, verify generated outputs, then exit.
- `StartupParserTest.exe` should eventually load/call the XML startup parser DLL, verify the returned `StartupDefinitions`, then exit.
- A future save bridge tester should load/call the save/load DLL against known-good saves and assert that they import into valid current runtime data.

That pattern keeps tests honest. If a DLL boundary changes, the game and its focused test executable both exercise the same exported contract. The testers remain small PowerShell-friendly smoke tests while the heavy compatibility code still lives in the one-shot DLL that can be unloaded after use.

## Quality Bar

- A DLL boundary is successful only if the runtime cannot accidentally call private implementation helpers after the DLL unloads.
- Namespaces should reflect ownership: `startup_parser`, `graphics_extractor`, `save_bridge`, and `runtime` should not bleed into each other.
- The runtime executable should get smaller and import fewer one-shot headers over time.
- Failing startup data must fail loudly before entering the game loop.
- Compatibility code belongs in the DLL that needs it, not in runtime fallback branches.
