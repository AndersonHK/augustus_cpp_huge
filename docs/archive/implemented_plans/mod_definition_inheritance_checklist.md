# Mod Definition Inheritance Checklist

> Completed migration checklist. Current mod metadata and inheritance behavior are documented in `docs/mod_metadata.md`.

- [x] Confirm the ordered mod stack is metadata-backed and accepts a sparse fourth mod depending on Vespasian.
- [x] Confirm registry-backed definition folders enumerate every active layer and merge by stable string identity.
- [x] Confirm empty intermediate definition folders do not interrupt inheritance.
- [x] Confirm whole-file resources resolve from the nearest configured layer.
- [x] Delete exact non-Graphics copies from Augustus when Julius is the inherited winner.
- [x] Delete exact non-Graphics copies from Vespasian when Augustus or Julius is the inherited winner.
- [x] Retain distinct upper-layer overrides and tombstones.
- [x] Add a startup-harness failure for exact non-Graphics duplicates of the nearest inherited file.
- [x] Add four-layer BuildingType, FigureType, generic enumeration, and dependency-order tests.
- [x] Document sparse definition inheritance and transitive fourth-mod dependencies.
- [x] Leave authored Graphics exceptions, runtime-extracted Graphics, `mod.xml`, and `.gitignore` untouched by the duplicate scrub.
- [x] Cleanly re-extract Julius and Augustus graphics from the installed game/package inputs.
- [x] Build Release parser and extractor targets with zero compiler warnings/errors.
- [x] Deploy the Release executable and sparse authored mod trees.
- [x] Pass Julius-only, Julius plus Augustus, and Julius plus Augustus plus Vespasian startup tests.
- [x] Pass all 56 required and representative save migrations and 3,000-tick soaks at 1000% speed.
- [x] Confirm zero unallowlisted runtime warnings/errors and steady-state throughput above 1,000 TPS.
- [x] Confirm the installed stack contains zero exact redundant upper-layer non-Graphics files.
- [x] Confirm the deployed executable SHA-256 matches the Release build.

The proprietary-history scrub remains plan-only in `temp/proprietary_graphics_history_scrub_plan.md`; it was not executed.
