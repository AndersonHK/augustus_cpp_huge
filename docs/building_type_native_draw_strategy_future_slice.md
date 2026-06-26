# BuildingType Native Draw Strategy Future Slice

## Intent

The current BuildingType migration gives every building a runtime id and moves identity/model/graphics references into XML, but it does not yet make the native engine capable of owning every draw and placement-preview case. That leaves `city_building_ghost.cpp`, legacy image helpers, and other seams carrying compatibility branches on top of the new runtime path.

This future slice should move the engine from "XML ids plus compatibility band-aids" to a smaller native draw strategy model where BuildingType owns the facts and the renderer/preview code consumes generic strategy data.

This is a prerequisite for Vespasian work that changes farms and adds orientation support broadly across buildings.

## Problem Statement

The refactor has grown code because it currently preserves legacy behavior while adding native runtime layers. Since the core native path cannot yet handle every special draw case, each migration step risks becoming one more adapter around older image-group and placement logic.

The desired end state is smaller code:

- no concrete `BUILDING_*` ids for normal building dispatch
- no enum-order fallbacks
- no ad hoc image lookup when a BuildingType graphics target exists
- no special-case ghost renderer unless the behavior is truly procedural
- all ordinary building orientation/variant/farm graphics choices expressed through BuildingType-owned metadata

## Current Boundary

BuildingType graphics already has a useful normal-rendering contract:

- default graphics target
- variant graphics targets
- stable graphics options
- build-rotation, orientation, connectable, storage-load, and production-progress options
- construction phase graphics
- conditional graphics selection
- payload-backed footprint, top, and animation slices
- composed building parts, rotation offsets, and authored placement footprints
- construction tool kinds, shoreline/open-water foundation cells, and water-access facts

That is enough for many normal buildings, but not enough for every placement preview. Some cases are not simply "pick an image and draw it"; they are tool behaviors with terrain, network, composition, or multi-part placement semantics.

## Current Source Audit, 2026-06-26

The current branch is well past the original "future slice" baseline. The data-contract side is mostly present:

- `building_type_registry_xml.cpp` parses `water_access`, `composed/main/part/offset`, `graphics/default/variant/layer/options/condition`, `construction/phase`, and `resource_storage`.
- `building_runtime_graphics.cpp` selects graphics options for stable variants, build rotation, connectable tiles, storage load, orientation, and production progress.
- `BuildingType::resolve_graphics_target_for_image(...)` picks construction-phase graphics before normal graphics, and `GraphicsDefinition::draw_footprint/draw_top/draw_animation(...)` consumes native runtime slices.
- `ConstructionPlacementPlan::build()` uses composed parts and rotation-aware offsets for placement previews and desirability-range part traversal.
- Vespasian XML already authors representative data: hippodrome phased/rotated graphics plus composed parts, warehouse composed storage spaces, farm field parts with production-progress graphics, dock/wharf/shipyard orientation graphics, reservoir/aqueduct water-access facts, and draggable reservoir/aqueduct tool declarations.

The remaining work is not "add the first native strategy model"; it is to delete or collapse the compatibility draw paths that still bypass the generic data:

- `city_building_ghost.cpp` still has hardcoded preview branches for draggable reservoir, aqueduct, bridge, and road/garden-gate transforms.
- `city_with_overlay.cpp` and `city_without_overlay.cpp` still branch on farm drawing rules; `city_without_overlay.cpp` still carries a gatehouse top branch.
- Storage is partially native, but warehouse/granary overlay ornaments and `resource_storage` direct branches are still compatibility code rather than fully generic strategy handling.
- `animations.cpp` still logs native draw-stage fallback to legacy rendering when a runtime slice is missing.
- `building_type_registry_xml.cpp` still accepts metadata-only BuildingType XML temporarily, so parser strictness is not yet at the final data-owned state.

## Strategy Families

Add a small set of native draw/preview strategies rather than turning graphics XML into a giant rules engine.

Likely strategy families:

- `simple`: one BuildingType target, normal `Building::draw_footprint`, `draw_top`, and `draw_animation`
- `oriented`: XML-authored orientation selectors for buildings whose pixels vary by facing
- `stable_options`: existing random/stable variant options, retained for decorative variants
- `farm_composite`: farm-specific crop/farmhouse composition without hardcoded building ids
- `storage_composite`: warehouse/granary footprint and sub-tile composition rules
- `connectable_tile`: gatehouse, roadblock, plaza, garden gates, and similar road-surface overlays
- `multi_part`: fort and hippodrome style multi-building preview layouts
- `waterside`: dock, shipyard, wharf orientation and water-front validation
- `network_tool`: roads, highways, bridges, aqueducts, and draggable reservoirs, where C++ should still own the terrain/network algorithm while BuildingType owns graphics and model facts

## Suggested Slice Order

1. **Orientation and simple variants**

   Status: implemented at the parser/runtime contract. XML already uses orientation, build-rotation, connectable, stable, storage-load, and production-progress option selectors. Remaining work is caller cleanup: migrate any old image handlers that still bypass an existing BuildingType graphics target.

2. **Farms**

   Status: data side exists. Farms are authored as composed buildings with field part types, and field graphics use production-progress options. Remaining work is deletion-focused: remove or shrink farm-specific footprint/top/overlay branches while preserving the old drawable-corner behavior.

3. **Storage and tile composites**

   Status: partial. Warehouse storage spaces and granary resource layers are authored in BuildingType data, and `resource_storage` exists. Remaining work is to genericize warehouse/granary ornaments and gatehouse/decorative-gate/road-surface drawing without losing terrain validation.

4. **Multi-part previews**

   Status: implemented for the representative composed-preview path. `ConstructionPlacementPlan` consumes authored parts and rotation offsets, and Vespasian hippodrome/warehouse/farm XML proves the shape. Remaining work is compatibility deletion around callers, not first-time data modeling.

5. **Network and water tools**

   Status: model facts are partially native. Tool kind, water-access, shoreline/open-water foundation, and selected graphics facts are in BuildingType XML for several water/network buildings. Remaining work is the preview renderer: bridge length, road adjacency, aqueduct rules, and reservoir dragging can stay procedural, but their selected graphics/model facts should stop living in hardcoded ghost branches.

## Non-Goals

- Do not reintroduce legacy building enums.
- Do not make XML encode pathfinding, terrain search, or bridge/aqueduct algorithms.
- Do not require every procedural placement tool to become a pure graphics node.
- Do not hide missing BuildingType definitions behind fallback guards for real buildings.

## Acceptance Direction

A future implementation should be able to answer these questions from BuildingType/native strategy data:

- What renderer strategy does this type use?
- Which graphics target should this instance draw for its current state?
- Does this type need orientation-specific graphics?
- Is this preview a single footprint or a composed set of parts?
- Which behavior remains a terrain/network algorithm, and which data has moved out of hardcoded type branches?

Success is not just "more XML." Success is deleting compatibility branches while keeping Augustus, Julius, and Vespasian behavior legible and testable.
