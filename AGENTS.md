# Repository Coding Practices

- Prefer readable function declarations and function calls on one line. Split them across lines only when the expression is genuinely too complex to understand comfortably in one-line form.
- `rg` and `gh` are not installed in this workspace. Use PowerShell `Get-ChildItem` and `Select-String`, plus ordinary `git` commands.
- On Windows, invoke MSBuild and other tools that spawn child processes through `Start-Process -UseNewEnvironment -NoNewWindow -Wait -PassThru` with an absolute executable path, and propagate the exit code. The Codex host can expose duplicate `PATH`/`Path` keys; do not attempt in-process case-only normalization.
- CLI and headless tests must suppress operating-system crash dialogs and report failures through stderr plus a nonzero exit code.
- The startup gate must load a representative set of recent `.svv` and legacy `.sav` files, advance and render each city headlessly for several thousand frames, and fail on any warning or error emitted during load or the soak.
- Save bridges must keep older and recoverably inconsistent saves loadable. Log every repair as a warning, repair the serialized/runtime relationship at load, and fix the runtime producer so the next save is clean; do not strand a usable save merely because the warning was persisted.
- Renderer compatibility fallbacks are temporary bug indicators. Converted paths require a zero-fallback validation threshold; do not normalize fallback counters or logs as successful output.

## Proprietary graphics boundary

- Never extract, copy, stage, commit, archive, or otherwise materialize Caesar 3 graphics or bulk runtime-extracted Augustus graphics inside this repository's `Mods` tree. Caesar 3 graphics are proprietary. Augustus graphics are redistribution-compatible with this project, but generated XML, cropped PNGs, atlases, manifests, and extraction stamps remain runtime/build artifacts rather than authored source assets.
- Runtime extraction may write only outside the checkout, normally beneath the installed game at `<game install path>\Mods\Julius\Graphics` and `<game install path>\Mods\Augustus\Graphics`, or beneath the ignored `extracted_graphics_sample` validation directory.
- Source XML should refer to runtime-extracted logical groups directly when that fully represents the asset. Authored bridge/backport XMLs may live under `Mods/*/Graphics` when no clean direct-reference representation exists; the Julius low/ship bridge definitions are intentional examples. Do not add wrappers that only duplicate an extracted group.
- Authored graphics directories are source directories, not extraction destinations. They may contain Vespasian-owned graphics, redistribution-safe backport fixes, and necessary authored bridge compositions. Verify provenance before adding any bitmap.
- Keep editable high-resolution masters, layer files, generation inputs, and other source images under `res/graphics_source`, never under a mod's shipped `Graphics` directory. `Mods/*/Graphics` may contain only the authored runtime assets and XML definitions the game actually loads; do not create a `Source` subdirectory there.
- Before any graphics-related commit, inspect the staged paths and reject generated `Group_*` trees, extraction stamps/manifests, bulk graphics XML/PNG output, and any asset whose redistribution rights are not established.
- The historical Augustus graphics import beginning at commit `fb138c6440c81a4aa5ad44cc12601484e2089b0d` is covered by a compatible license and is not a proprietary Caesar 3 incident. Any history cleanup is deferred repository hygiene, not an emergency rights-remediation task; do not use historical generated output as a substitute for validating the current extractor.
