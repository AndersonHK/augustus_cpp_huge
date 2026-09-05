# Vespasian Performance Tracker Runtime

## Summary
The runtime provides an opt-in, low-overhead performance tracker controlled by `debug_performance_tracker=0/1` in `Vespasian.ini`. When enabled, it writes one readable key/value line per second to `vespasian-performance.log`. The tracker stays coarse enough to locate hot paths without adding per-tile or per-figure timing overhead.

## Implementation

- `CONFIG_DEBUG_PERFORMANCE_TRACKER` / `debug_performance_tracker` defaults to `0` and is loaded from `Vespasian.ini` by the existing config path.
- `src/game/performance_tracker.h/.cpp` provides:
  - cached enabled flag
  - fixed bucket enum and fixed counter arrays
  - C++ RAII scope helper for coarse timing
  - C-callable record functions where needed
  - no dynamic allocation or string formatting in hot scopes
- SDL high-resolution counters for timed scopes, with one flush per second.
- A cheap enabled check at coarse call sites; disabled mode performs no file open or per-frame formatting.

## Tracking Contract
- Output file: `vespasian-performance.log` in the same logging directory as `vespasian-log.txt`. On Windows, that directory is `%APPDATA%\augustus\Vespasian\`.
- Format: one line per sample, readable and parseable key/value fields, for example:
  `perf sample=12 elapsed_ms=1003 speed=1000 target_ticks=1003 actual_ticks=997 frames=61 zero_tick_frames=0 speed_wait_ms=0.0 run_ms=410.2 tick_ms=320.5 draw_ms=180.4 present_ms=470.1 buckets=figure:120.3,advance:90.1,building_state:18.4,map_desirability:22.7,city_draw:150.8`
- Tick goal is computed from the existing speed-timer math, not guessed from the UI label.
- "Idle waiting for speed timer" means active frames where `game_speed_get_elapsed_ticks()` returned zero because not enough speed-timer time had accumulated.
- Tracked buckets include:
  - outer frame/run/draw/present
  - total tick processing
  - scheduled tick work in `advance_tick`
  - `figure_action_handle`
  - selected scheduled hot candidates: road network, resources, water/building refreshes, production, labor, desirability, building state, house evolution, figure generation, culture/trade/formation updates
  - city draw total plus coarse city draw passes for footprint, main row pass, deletion/elevated pass, overlays, weather/clouds

## Validation

- With `debug_performance_tracker=0`, no performance log is created.
- With `debug_performance_tracker=1`, a running city emits approximately one line per second.
- Large-city 1000% runs should report plausible `target_ticks`, `actual_ticks`, `speed_wait_ms`, `tick_ms`, `draw_ms`, `present_ms`, render metrics, route metrics, and bucket totals.
- Shutdown closes the sidecar cleanly, and normal config save preserves the setting.

## Boundaries

- The tracker intentionally avoids per-tile, per-figure, and per-draw-call timers.
- It is configured through the ini and has no config-menu UI.
- VSync wait is reported separately as present time so it is not mistaken for simulation cost.
