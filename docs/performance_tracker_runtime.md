# Vespasian Performance Tracker Runtime Plan

## Summary
Add an opt-in, low-overhead performance tracker controlled by `debug_performance_tracker=0/1` in `Vespasian.ini`. When enabled, it writes one readable key/value line per second to a new `vespasian-performance.log` sidecar file. The first version stays coarse: enough to locate hot paths without adding per-tile or per-figure timing overhead.

## Key Changes
- Add `CONFIG_DEBUG_PERFORMANCE_TRACKER` / `debug_performance_tracker`, default `0`, loaded from `Vespasian.ini` by the existing config path.
- Add `src/game/performance_tracker.h/.cpp` with:
  - cached enabled flag
  - fixed bucket enum and fixed counter arrays
  - C++ RAII scope helper for coarse timing
  - C-callable record functions where needed
  - no dynamic allocation or string formatting in hot scopes
- Add project entries to `Vespasian.vcxproj` and `.filters`.
- Use SDL high-resolution counters for timed scopes; flush only once per second.
- Keep off cost to a cheap enabled check at coarse call sites; no file open, no counter reads, and no per-frame formatting when disabled.

## Tracking Contract
- Output file: `vespasian-performance.log` in the same logging directory as `augustus-log.txt`.
- Format: one line per sample, readable and parseable key/value fields, for example:
  `perf sample=12 elapsed_ms=1003 speed=1000 target_ticks=1003 actual_ticks=997 frames=61 zero_tick_frames=0 speed_wait_ms=0.0 run_ms=410.2 tick_ms=320.5 draw_ms=180.4 present_ms=470.1 buckets=figure:120.3,advance:90.1,building_state:18.4,map_desirability:22.7,city_draw:150.8`
- Tick goal is computed from the existing speed-timer math, not guessed from the UI label.
- "Idle waiting for speed timer" means active frames where `game_speed_get_elapsed_ticks()` returned zero because not enough speed-timer time had accumulated.
- Initial buckets:
  - outer frame/run/draw/present
  - total tick processing
  - scheduled tick work in `advance_tick`
  - `figure_action_handle`
  - selected scheduled hot candidates: road network, resources, water/building refreshes, production, labor, desirability, building state, house evolution, figure generation, culture/trade/formation updates
  - city draw total plus coarse city draw passes for footprint, main row pass, deletion/elevated pass, overlays, weather/clouds

## Test Plan
- Build `Release|x64` with the root `Vespasian.sln`.
- With `debug_performance_tracker=0`, confirm no performance log is created and no tracker lines appear in `augustus-log.txt`.
- With `debug_performance_tracker=1`, launch a city and confirm one line per second appears in `vespasian-performance.log`.
- Run at 1000 speed on a large city and confirm `target_ticks`, `actual_ticks`, `speed_wait_ms`, `tick_ms`, `draw_ms`, `present_ms`, and bucket totals are plausible.
- Confirm shutdown closes the log cleanly and normal config save preserves the new ini key.

## Assumptions
- The first implementation is intentionally coarse v1; no per-tile, per-figure, or per-draw-call probes.
- The tracker is configured through the ini only, with no config-menu UI in this pass.
- VSync wait is reported separately as present time so it is not mistaken for simulation cost.
