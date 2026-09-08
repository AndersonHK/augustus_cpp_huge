# Unified logging

`src/core/Logger.h` and `Logger.cpp` own application logging. The former Augustus logger, Crash Context implementation, compatibility aliases, and extractor logging/context stubs are removed. Application callers use one vocabulary: `Logger::info`, `warning`, `error`, `fatal`, and `Logger::Scope`. The `*f` methods support printf-style formatting; `report` accepts a runtime severity for external diagnostic adapters.

```cpp
Logger::Scope scope("save.load", filename);
Logger::warning("Repairing missing owner", detail.c_str());
Logger::infof("Loaded %d buildings", count);
```

Scopes are thread-local, noncopyable, and removed by their destructor. They support changing context and optional diagnostic callbacks. Resetting context cannot let an old scope destroy a newly created one. `log_context()` emits the current stack and invokes callbacks, with recursive callback invocation guarded. Warning/error/fatal messages include the active context; informational messages remain concise. Messages and context are dynamically sized, including the game's file/console sink.

The logger serializes output and informational repeat accounting. Only info messages are deduplicated, with a bounded cache and repeat totals emitted on flush, output changes, or exit. Every warning and error is emitted, including identical repeats, so validation counters cannot miss a later failure. Flush is reusable and does not repeat an already emitted summary.

Output adapters own their destinations, not another logging implementation. The game adapter retains its log file, backup, startup buffering, debugger output, and validation counters. Warnings/errors go to stderr in release and debug builds. SDL-originated diagnostics enter Logger through one adapter. Standalone tools use the same implementation with console output. The extractor DLL accepts an optional trailing logging callback in its size-versioned requests and forwards reports to the host logger; older request sizes remain accepted. Scoped adapters are restored before a request returns or its DLL unloads.

`fatal()` flushes and terminates with a nonzero status, optionally invoking the game's dialog callback. Tools have no dialog dependency, and headless game runs use the existing dialog suppression. Native crash handlers still collect platform stack traces/runtime dumps, but report through Logger. The old archival asset packer has also had its private logger removed and its source renamed to `.cpp`; it has no build target in the current solution.

Validation for this change:

- Release solution build, including game, parser, logger test, launcher, and both extractor tools: `out/logger-final-build.log` (exit 0).
- `LoggerTest`: distinct severities, repeated-warning visibility, informational summaries, 12,000-character detail preservation, thread isolation, scope/callback lifetime, recursive callbacks, context reset, repeat flush, formatted zero values. Its fatal subprocess exits 1 without a dialog (`out/logger-contract.log`, `out/logger-fatal.err`).
- Definition contracts: `out/logger-definitions.log` (negative fixtures intentionally report errors).
- Native runtime contracts, deliberate payload repairs and failed-load rollback, then 3,000 rendered frames: `out/logger-runtime1.log`. Exactly three intentional repair warnings and two intentional model errors are asserted before the clean soak.
- Consul native and original campaign save roundtrips plus 3,000 frames each: `out/logger-save-gate.log`. Native import is clean; campaign import records 45 relationship repairs, then canonical reload and soak have zero warnings/errors.
- Julius-only Aedile save, 3,000 frames with empty stderr: `out/logger-julius-gate.log`.
- Extractor DLL logging callback: `out/logger-dll-test.py` supplies malformed XML under `out/logger-dll-fixture`, receives the expected Error, and verifies the request fails without producing graphics.
- Installed final executable: `out/logger-deployed.log` passes a 3,000-frame Consul soak with empty stderr.

The final executable and DLLs are installed in the game directory and verified by SHA-256. Previous installed binaries are preserved under `out/pre-logger-deployment`. Source/tools/project backups taken during migration are under `out/logger-migration-backup`.

Unrelated working changes are preserved. No commit or upstream ancestry merge is part of this cleanup.
