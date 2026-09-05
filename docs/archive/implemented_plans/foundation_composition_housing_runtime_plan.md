# Foundation, Housing, and Composition Runtime Refactor

> Historical implementation plan. The foundation, housing, composition, placement, repair, and save-bridge slices described here are integrated. Use the current subsystem docs and source for runtime contracts.

## Summary

Implement in this order:

1. Introduce authoritative external foundation definitions and rotated geometry.
2. Implement native housing definition, state, and runtime modules.
3. Complete composition as a separate owner-bound module.
4. Rewrite placement and map interaction over composition members and their foundations.
5. Delete superseded policies, fields, branches, and compatibility paths.

## Foundation Architecture

- Add `Mods/<mod>/Foundations/*.xml`. Every buildable `BuildingType` references one required foundation.
- A foundation is authored once in canonical orientation with width, height, and dense symbolic rows.
- Every active cell declares required terrain, permitted existing blocking terrain, terrain changes, map binding, and optional owner-controlled road passage.
- Rotation transforms the complete cell definition: coordinates, requirements, generated terrain, passage positions, and dimensions.
- Non-rotatable buildings validate only their canonical orientation. Rotatable buildings evaluate all four rotated foundations through the default placement path.
- Placement retains the previously selected valid rotation. If it becomes invalid, rotations are tested clockwise from the preferred build rotation.
- Manual rotation changes the preferred rotation and immediately revalidates.
- The selected rotation is stored in the immutable placement result used by ghosts, construction, composition expansion, and map publication.
- `FoundationDef` exposes rotated dimensions and cells.
- `BuildingFoundation` binds a building and definition and owns validation, placement, removal, map rebinding, perimeter iteration, and walker passage.
- `FoundationState` records terrain changes made per cell so removal preserves permitted underlying terrain.
- `RoadblockState` stores mutable walker permissions. Default and configurable masks live in `FoundationDef`.
- Warehouse and granary flag animation state moves to `BuildingGraphicsState`.

Remove `<model size>`, scalar runtime geometry, inline foundation policies, `<roadblock>`, geometric roadblock kinds, and square footprint loops. `<model cost>` remains economics data during this initiative.

## Water and Docks

- A dock foundation has six required land cells and three required water cells in canonical orientation.
- Generic foundation rotation selects and retains the valid shoreline-facing orientation.
- Delete the shoreline policy, dedicated shoreline orientation path, and hidden extra-three-rows-of-water requirement.
- Navigable-water connectivity belongs to `WaterAccessDef`, evaluated from the selected foundation's rotated water cells.
- Docks explicitly require open-water connectivity. Other waterside buildings receive it only when declared.
- Foundation contains no shoreline or open-water semantic Boolean.

## Farm Profile Behavior

- Vespasian farms use a `land_2x2` owner foundation and five composed field children with `meadow_1x1` foundations, so every field must occupy meadow directly.
- Julius and Augustus retain the original nearby-meadow behavior: a `meadow_2x2` owner applies the site-level meadow-within-three-tiles requirement, while its five field children use `land_1x1` foundations.
- The Vespasian gameplay divergence is recorded in `docs/gameplay_divergences_from_augustus.md` and the BuildingType authoring README.

## Housing Architecture

- Rename the external `HousingType` concept to `HousingProfileDef`; it owns residents, requirements, prosperity, tax, and compatibility level.
- Add per-building `HousingDef` with profile, capacity, resolved transitions, and explicit balance values currently inferred from footprint size.
- Add `HousingState` for population and housing-owned service, sentiment, tax, access, crime, evolution, and spawning state.
- Add owner-bound `HousingModule` for population, evolution, merging, splitting, vacant-lot conversion, and housing identity.
- Keep workforce, figures, resources, generic water and road access, desirability, health, and sickness in their respective modules.
- Delete runtime `house_size`, `house_is_merged`, `subtype.house_level`, `has_house_size()`, and native behavior based on legacy housing levels.
- Replace classification with `HousingModule` presence and geometry with rotated foundation cells.
- Merging requires exact target-foundation coverage by unique qualifying housing objects whose `merge_to` resolves to the same result. Results may merge again.
- Shrinking uses the declared `split_to` for leftover cells; initially it must have a one-cell foundation.
- Demolition creates vacant-lot buildings over every occupied housing foundation cell.
- Housing transitions do not use composition.

## Composition Architecture

- Replace embedded composed-building behavior with `CompositionDef` and owner-bound `BuildingComposition`.
- XML declares child type, role, canonical offset relative to the owner, and orientation policy.
- Child offsets and foundations rotate with the selected owner rotation.
- Generic rotation is the default; complete per-rotation overrides remain only where stable role ordering is deliberate.
- Remove authored aggregate dimensions and main offsets; derive bounds from rotated owner and child foundations.
- `BuildingComposition` owns ordered child pointers; children retain a direct owner pointer and definition index.
- Expose one normalized layout iterator returning owner and children with world origin, rotation, role, and foundation.
- Reject nested composition, overlaps, duplicate roles, self-reference, unresolved types, and incomplete overrides.
- Creation is transactional: validate the whole layout, create and bind all objects, then publish map cells.
- Cost and global requirements apply once to the owner. Clearance is counted once per unique cell.
- Repair, destruction, deletion, mothballing, and type replacement enter through the owner and iterate all children.
- Child road cells delegate walker permissions to the composition owner.
- Dynamic bridge segment chains remain separate from fixed XML composition.

## Implementation Slices

1. Add the foundation registry, parser, XML definitions, rotated cell transforms, and startup validation. Migrate all building types away from model size and foundation policies, initially exposing geometry without changing placement.
2. In parallel, implement native housing and composition verticals. Root owns shared `Building`, runtime rebinding, save interfaces, integration, compilation, and deployment.
3. Refactor `ConstructionPlacementPlan` to expand composition layouts and validate every member foundation. Produce one immutable result containing rotation, world cells, clearing operations, costs, and failure reason. Ghosts, construction, repair, and force placement consume that result.
4. Replace terrain painters, binders, routing geometry, and removal code with foundation iteration. Delete scalar geometry, shoreline paths, composition footprint dimensions, roadblock compatibility, and geometric type checks.

## Save Compatibility

- Preserve existing byte positions through a legacy save DTO; removed fields do not remain in the runtime building record.
- Exact saved `BuildingType` identity is authoritative. Legacy size, merged, and level bytes only disambiguate old housing records.
- Stage housing, foundation, graphics, road-permission, and composition state before runtime materialization.
- Save synthesizes legacy scalar dimensions and housing values solely for compatibility.
- Resolve fixed composition chains into pointers once after records load and validate ownership, order, types, cycles, and duplicates.
- Reconstruct known missing legacy children but never adopt unrelated live buildings.
- Persist foundation terrain deltas at the stable integrated checkpoint and advance the save version then.

## Tests and Acceptance

All regression code belongs in `StartupParserTest.exe` or pure helpers linked into it, never Vespasian.

- Parser failures cover missing foundations, malformed rows, invalid masks and permissions, illegal rotations, unresolved children, and invalid housing transitions.
- Rotation fixtures cover rectangular and sparse foundations, preferred and retained rotation, deterministic fallback, and rotation of requirements, generated terrain, and passages.
- Dock fixtures cover the exact six-land/three-water footprint, automatic orientation, explicit open-water connectivity, and absence of the hidden extra-water rule.
- Composition fixtures cover farms, warehouse roles, hippodrome, and forts in every rotation.
- Housing fixtures cover mixed source types, repeated merging, rectangular and sparse targets, partial-coverage rejection, shrinking, and vacant lots.
- Placement invariants require identical ghost and placement cells, inverse placement and removal, preserved underlying terrain, correct ownership, and unique cost and clearance accounting.
- Save fixtures cover legacy housing footprints, ambiguous mappings, composition hydration, terrain deltas, road permissions, and synthesized round trips.
- Root alone builds `Release|x64`, runs `StartupParserTest.exe`, builds Vespasian, performs gameplay smoke tests, and deploys.

## Coordination

- Keep durable specialists assigned to foundations and placement, housing and state, and composition and save relationships.
- Specialists own their verticals with broad latitude but do not compile or deploy.
- Root owns cross-module APIs, overlapping runtime and save files, integration, compilation, testing, and deployment.
- Foundation always describes one building object. Composition only expands an owner into child objects and rotated offsets.
