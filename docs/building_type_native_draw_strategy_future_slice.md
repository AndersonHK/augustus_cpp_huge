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
- construction phase graphics
- conditional graphics selection
- payload-backed footprint, top, and animation slices

That is enough for many normal buildings, but not enough for every placement preview. Some cases are not simply "pick an image and draw it"; they are tool behaviors with terrain, network, composition, or multi-part placement semantics.

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

   Add the missing BuildingType graphics selection needed for normal oriented buildings. Migrate simple old image handlers first.

2. **Farms**

   Make farm crop/farmhouse composition data-driven enough for Vespasian farm changes. This should remove farm-specific image branching from ghost and live rendering where possible.

3. **Storage and tile composites**

   Move warehouse, granary, roadblock, plaza, gatehouse, and decorative gate logic into reusable strategy data. Keep terrain validation in construction/map code, but stop naming concrete building types in draw code.

4. **Multi-part previews**

   Model forts and hippodrome as composed preview strategies with authored part types, offsets, sizes, and draw order.

5. **Network and water tools**

   Leave bridge length, road adjacency, aqueduct rules, reservoir dragging, and waterside terrain checks in C++. Move their selected graphics and BuildingType facts onto native strategy data.

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
