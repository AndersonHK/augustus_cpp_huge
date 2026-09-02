# Documentation Map

Top-level documents in this directory describe the current runtime, active migrations, or maintained contributor guidance. Completed plans, superseded audits, and obsolete handoffs live under [`archive/`](archive/README.md).

## Current Runtime Contracts

- [`mod_metadata.md`](mod_metadata.md): `mod.xml`, dependency ordering, and sparse definition inheritance.
- [`graphics_extraction_pipeline.md`](graphics_extraction_pipeline.md): runtime extraction, logical asset paths, and the proprietary-graphics boundary.
- [`save_data_organization.md`](save_data_organization.md) and [`save_load_runtime_bridges.md`](save_load_runtime_bridges.md): persisted layout and runtime hydration/repair.
- [`resource_runtime.md`](resource_runtime.md): layered resource definitions and runtime identity.
- [`walker_pathing_runtime.md`](walker_pathing_runtime.md) and [`water_access_runtime.md`](water_access_runtime.md): figure routing and building water access.
- [`performance_tracker_runtime.md`](performance_tracker_runtime.md): current performance tracking behavior and validation limits.

## Active Migration And Validation Work

- [`deep_refactor_implementation_progress.md`](deep_refactor_implementation_progress.md): consolidated implementation and manual-validation tracker.
- [`figure_owned_native_graphics_plan.md`](figure_owned_native_graphics_plan.md): remaining figure-graphics ownership work.
- [`renderer_scaling_seam_plan.md`](renderer_scaling_seam_plan.md), [`render_performance_plans.md`](render_performance_plans.md), and [`renderer_ui_vertical_slice_design.md`](renderer_ui_vertical_slice_design.md): renderer transition work.
- [`routing_cost_map_scalability_plan.md`](routing_cost_map_scalability_plan.md), [`runtime_dll_boundary_refactor_plan.md`](runtime_dll_boundary_refactor_plan.md), and [`unit_and_formation_xml_plan.md`](unit_and_formation_xml_plan.md): active architectural work.
- [`../temp/proprietary_graphics_history_scrub_plan.md`](../temp/proprietary_graphics_history_scrub_plan.md): deferred cleanup of licensed historical Augustus extraction output; this is not a proprietary Caesar 3 incident.

## Maintenance Rules

- Update runtime-contract documents when the schema or ownership boundary changes.
- Keep machine-specific paths out of tracked documentation; use placeholders such as `<game install path>`.
- Archive completed plans instead of leaving them beside active work.
- Treat archived documents as historical evidence, not implementation authority.
