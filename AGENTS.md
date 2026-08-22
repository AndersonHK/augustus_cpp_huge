# Repository Coding Practices

- Prefer readable function declarations and function calls on one line. Split them across lines only when the expression is genuinely too complex to understand comfortably in one-line form.
- `rg` and `gh` are not installed in this workspace. Use PowerShell `Get-ChildItem` and `Select-String`, plus ordinary `git` commands.
- CLI and headless tests must suppress operating-system crash dialogs and report failures through stderr plus a nonzero exit code.
- The startup gate must load a representative set of recent `.svv` and legacy `.sav` files, advance and render each city headlessly for several thousand frames, and fail on any warning or error emitted during load or the soak.
- Save bridges must keep older and recoverably inconsistent saves loadable. Log every repair as a warning, repair the serialized/runtime relationship at load, and fix the runtime producer so the next save is clean; do not strand a usable save merely because the warning was persisted.
- Renderer compatibility fallbacks are temporary bug indicators. Converted paths require a zero-fallback validation threshold; do not normalize fallback counters or logs as successful output.
