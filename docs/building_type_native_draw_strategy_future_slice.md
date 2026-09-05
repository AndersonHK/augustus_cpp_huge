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
- build-rotation, orientation, connectable, storage-load, production-progress, storage-permission, and gatehouse-orientation options
- construction phase graphics
- conditional graphics selection
- payload-backed footprint, top, and animation slices
- composed building parts, rotation offsets, and authored placement footprints
- construction tool kinds, shoreline/open-water foundation cells, and water-access facts

That is enough for many normal buildings, but not enough for every placement preview. Some cases are not simply "pick an image and draw it"; they are tool behaviors with terrain, network, composition, or multi-part placement semantics.

## Current Source Audit, 2026-06-26

The current branch is well past the original "future slice" baseline. The data-contract side is mostly present:

- `building_type_registry_xml.cpp` parses `water_access`, `composed/main/part/offset`, `graphics/default/variant/layer/options/condition`, `construction/phase`, and `resource_storage`.
- `building_runtime_graphics.cpp` selects graphics options for stable variants, build rotation, connectable tiles, storage load, orientation, production progress, storage permissions, and gatehouse orientation.
- `BuildingType::resolve_graphics_target_for_image(...)` picks construction-phase graphics before normal graphics, and `GraphicsDefinition::draw_footprint/draw_top/draw_animation(...)` consumes native runtime slices.
- `ConstructionPlacementPlan::build()` uses composed parts and rotation-aware offsets for placement previews and desirability-range part traversal.
- Vespasian XML already authors representative data: hippodrome phased/rotated graphics plus composed parts, warehouse composed storage spaces, farm field parts with production-progress graphics, dock/wharf/shipyard orientation graphics, reservoir/aqueduct water-access facts, and draggable reservoir/aqueduct tool declarations.

The remaining work is not "add the first native strategy model"; it is to delete or collapse the compatibility draw paths that still bypass the generic data:

- Network tools retain procedural route and bridge geometry, but reservoir, aqueduct, and bridge pieces draw through native `BuildingGraphics`; road-surface projection is owned by the road/aqueduct strategy and garden-wall-to-gate projection is centralized behind the connectable strategy.
- `city_with_overlay.cpp` no longer bypasses native farm footprint/top drawing, `native_crops` no longer mutates `map_image` through `GROUP_BUILDING_FARM_CROPS`, and exceptional mothball/stockpile icon anchors are root-graphics XML data. Farm owners author `overlay_summary mode="composition_owner"`; child rendering resolves that owner policy through the composition relationship, so summary suppression no longer asks whether a building type is a farm.
- Storage is mostly native for warehouse/granary visible state: XML owns warehouse ornaments, granary resource layers, and Augustus/Vespasian storage permission flags, while Julius stays flagless to match upstream Julius. Remaining storage/tile work is `resource_storage` genericization plus decorative-gate/road-surface drawing.
- Gatehouse base and cap mapping are owned by XML `gatehouse_orientation` data and the `gatehouse_overlay` layer role; the legacy base-image fallback and literal cap dispatch are deleted. Low- and ship-bridge native option targets likewise no longer retain no-op type handlers.
- Small and medium statues now author the base and rotated art as two native `orientation` options in Julius, Augustus, and Vespasian. The generic orientation selector preserves the old `(building rotation + view rotation) % 2` choice for all sixteen rotation/view combinations, so the statue-specific fallback is deleted without changing foundation geometry.
- Hippodrome owner, middle, and end parts author all four finished build rotations in every stack. Augustus and Vespasian additionally author all four rotations for each of four construction phases on every part, while Julius intentionally remains instant-build. The phase/orientation/composition fallback in `building/image.cpp` is therefore deleted.
- `BuildingGraphics.cpp` still logs native draw-stage fallback to legacy rendering when a runtime slice is missing.
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

   Status: implemented at the parser/runtime contract. XML already uses orientation, build-rotation, connectable, stable, storage-load, production-progress, storage-permission, and gatehouse-orientation option selectors. Remaining work is caller cleanup: migrate any old image handlers that still bypass an existing BuildingType graphics target.

2. **Farms**

   Status: implemented at the draw-adjacent policy boundary. Farms are authored as composed buildings with field part types, field graphics use production-progress options, `native_crops` uses the same XML option selector, and owners author `overlay_summary mode="composition_owner"`. `BuildingGraphics::draws_overlay_summary_at` resolves that owner policy for both owner and child objects without farm classification. The exceptional mothball/stockpile icon anchor remains owner graphics data and its existing owner-only runtime rule is unchanged.

3. **Storage and tile composites**

   Status: partial. Warehouse storage spaces, warehouse ornaments, granary resource layers, Augustus/Vespasian storage permission flags, and gatehouse cap overlays are authored in BuildingType data, and `resource_storage` exists. Remaining work is to genericize the remaining `resource_storage` direct branch and decorative-gate/road-surface drawing without losing terrain validation.

4. **Multi-part previews**

   Status: implemented for the representative composed-preview path. `ConstructionPlacementPlan` consumes authored parts and rotation offsets, and Vespasian hippodrome/warehouse/farm XML proves the shape. Remaining work is compatibility deletion around callers, not first-time data modeling.

5. **Network and water tools**

   Status: implemented at the preview graphics boundary. Bridge length, road adjacency, aqueduct routing, and reservoir dragging remain procedural network algorithms; selected building graphics and bridge variants flow through native graphics or their owning map strategy rather than hardcoded ghost image dispatch.

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
