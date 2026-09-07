# D13/D14 implementation and validation

Approved on 2026-09-07. The scope is shared mint production-rate control, production event arithmetic, XML sidebar housing information, and terrain definitions/identity bridges/foundation requirements. The user explicitly deferred further worker-control changes; the existing Augustus/Vespasian settings remain as implemented. Production-average rounding and the 102% efficiency presentation also remain deferred. The user subsequently authorized committing completed D13 as `Commit ledger part 4 - D13`, then continuing Terrain/D14. No ancestry adjustment is authorized by that commit instruction.

## Implementation sequence

1. Preserve the existing worker controls and their 45%/38% defaults. The proposed single disabled-to-50% slider is deferred to a future slice.
2. Add a generic production-method rate-source reference, resolved and cycle-checked at definition load. The Augustus mint's reverse recipe follows the denarii-producing method; resource production events mutate the controlling method once. Preserve scenario overrides as explicit deltas and verify set/add/clamping/reset behavior. The existing event dispatcher already distinguishes set and add; audit and test its native consumers before marking the upstream bug fix addressed.
3. Render housing/population sidebar information through UI window XML. Julius receives a version using its own extracted strings/assets; Augustus owns the extended view and Vespasian inherits it. Leave efficiency arithmetic untouched.
4. Design the object ownership and consumer API before changing terrain code. Replace `src/map/terrain.h/.cpp` with capitalized `Terrain.h/.cpp`. Load one terrain declaration per XML from each mod's `Terrain-Types` directory. The object itself is its runtime identity; there is no numeric terrain ID or lookup by integer/mask. Terrain operations become methods. Consumers and map tiles hold terrain definition object references resolved at startup. Compile foundation string requirements and traversal properties once; numeric masks exist only in legacy serialization.
5. Add a readable native terrain identity ledger and explicit reference lists, including map cells, foundation rollback deltas and scenario terrain parameters. Archive-only IDs identify names and shared reference sets; they are never runtime terrain handles. Legacy SAV/SVX/SVV masks decode through fixed bridge tables. Inspect archive compression and expose an inspectable text payload without claiming a binary/compressed whole save can be parsed with regex.
6. Move sawmill/tree, mine/rock, reservoir/water and related placement requirements onto generic foundation rules using terrain text IDs. Add shallow-water terrain and the editor/routing behavior required by D14 through those definitions.

## Terrain ownership and binding contract

The 2026-09-07 follow-up supersedes assigning integer IDs to runtime terrain objects. Authored strings identify definitions across loads; object identity identifies them during a run. Numbers identify archive records only; fixed bit positions remain confined to old formats. No private storage ID or numeric terrain handle is assigned to the objects.

Startup has three ordered phases: merge terrain XML declarations and construct the winning objects; resolve their relationships and validate traversal policies; load foundations, buildings, figure movement definitions and editor tools, binding their terrain names directly to those objects. For example, a requirement naming `rocks` becomes a reference to the instantiated rocks definition. Placement calls the bound object's query methods; it never resolves `rocks` again. Missing references fail startup with source provenance, rather than falling back to a legacy terrain constant.

The registry owns noncopyable, address-stable terrain objects. Required single references use `const Terrain&` (or `reference_wrapper` in assignable definition records); optional references use explicitly nullable pointers. Collections contain bound terrain objects. There are no integer constructors, integer conversions, public `mask()`/`number_id()` accessors, private numeric terrain identities, or compatibility overloads accepting legacy numeric terrain values. The runtime header does not expose a string-to-terrain lookup either: binding is a definition-loader operation. Removing the old enum and free-function API is intentional: compilation must expose unmigrated consumers.

Terrain queries and edits become methods. Single-definition queries belong to the definition; operations over combinations belong to a typed bound collection; map-wide backup, restore and persistence belong to the terrain map owner. Collection membership comes from object references, never from numbers supplied by a caller. The user later allowed a storage tradeoff based on simplicity, maintainability and inspection, with arbitrary terrain counts as the underlying goal. The chosen implementation remains shared immutable collections of actual terrain references: tiles reference these collections directly, avoiding per-tile allocations and a fixed bit-width limit. Archive IDs never become runtime composition handles. Loading speed must be measured rather than assumed.

Mod reload must first release the current city's owners and every definition containing terrain references, then release the terrain registry. Rebuild in the opposite order. Rollback reconstructs the previous stack and reloads the city snapshot; it must not reuse references into the discarded registry. This extends the existing live-settings reload lifecycle rather than relying on dangling pointers to reveal mistakes.

The save bridge resolves archive ID → stable name → terrain object once while loading the ledger, then binds shared reference lists. It translates cells, scenario parameters and foundation rollback deltas using that bound table. New saves generate explicit lists rather than terrain membership masks. Legacy fixed bit assignments are confined to the bridge. Duplicate archive IDs, malformed ledgers and unavailable terrain identities need explicit diagnostics and repair rules; an unknown saved identity must never acquire the meaning of a different active terrain by coincidence.

The initial inventory finds terrain-related references in 123 source/test files. The migration includes foundations and their serialized deltas, routing and navigation invalidation, figure movement, construction previews and rollback, renderer tile selection, editor tools, scenario terrain actions, cached counts and debug inspection. `WaterAccessType` demonstrates layered loading and archive ledgers, but its public numeric accessors are **not** a template for Terrain's consumer API.

Scenario terrain conditions (`scenario_condition_type_terrain_count_area_met`) and city-property terrain counts currently pass their generic integer parameters directly into terrain queries. They need their own bound runtime references, rebuilt when a scenario is loaded or an editor changes its parameters. Translating those integers inside each condition evaluation would leave an unmigrated consumer and is not acceptable. Serialized generic parameters remain archive DTO fields only for these terrain-bearing cases.

The first prototype must demonstrate the complete binding path with a foundation requirement: instantiate terrain → construct the requirement with its required reference → query the terrain object during placement. Test unknown names and registry reload lifetimes before expanding to the other consumers. The full migration must then remove the old API rather than leaving this prototype beside it.

### Integration order found in the code audit

`startup/startup_definition_loader.cpp` currently loads FigureType definitions before BuildingType definitions; the latter loads foundations internally. Terrain loading therefore belongs in the startup definition loader before FigureType loading, not merely at the start of the building registry. Rebuild order is terrain → movement/figure and foundation definitions → buildings → remaining cross-definition references. Teardown must reverse these dependencies, including construction previews, editor brush selections and map backups containing bound references.

`game/file_io.cpp` owns the archive pieces for water-access and god identity tables. The terrain ledger belongs alongside those dynamic pieces, but must be bound **before** the terrain grid and building foundation deltas are decoded. The load/save DLL's opaque archive pieces remain serialization payloads; native object references must not cross its ABI. Scenario archives require the same ledger boundary and an explicit version transition.

`map/water_navigation.cpp` caches topology and dock destination fields. Terrain mutations must invalidate topology when authored sea traversal changes, including a shallow-water overlay changing on a tile that remains water. The current water-membership-only invalidation is insufficient for D14. Clear/restore/undo and live definition reload need the same invalidation contract.

`PathingMode::TerrainAccess` describes traversal policies, rather than identifying a terrain definition. Preserve that distinction: replacing terrain identity does not require turning every unrelated policy enum into a terrain object. Its concrete terrain requirements and traversal queries must nevertheless use the bound Terrain definitions.

## Validation requirements

- Content contracts for inherited defaults, disabled workforce migration, rate-source lookup/cycles, duplicate terrain identities and unknown foundation terrain references. Duplicate archive IDs are a ledger validation case, not runtime terrain identity.
- Runtime contracts for both mint directions, independent gold mines, scenario set versus add, reset on scenario change and explicit override save/reload.
- UI captures and interactions for Julius and Augustus/Vespasian, with no Augustus-only locale or image dependencies in Julius XML.
- Terrain ledger permutation tests covering multi-terrain cells, old saves, foundation rollback and roundtrip identity. Runtime routing, tile storage and placement use object references; numeric masks are confined to legacy archive conversion and no string lookup occurs in per-tile traversal.
- Representative recent SVV and legacy SAV cities loaded and rendered for thousands of frames, strict zero warning/error/fallback checks after any explicit migration repair and canonical rewrite.

## Status

D13 is implemented. Terrain/D14 remains planned, not implemented by this commit.

The reverse mint declares `<output resource="gold" rate_from="city_mint_basic" destination="building_storage" />`. Binding occurs after overlay resolution; a missing source or cycle fails startup. An explicit recipe scenario override takes precedence; otherwise the recipe follows its source. Resource events modify independent controlling producers, so denarii events affect both mint directions once and gold events remain independent.

`sidebar_ratings.xml` owns layout and localization dependencies: Julius uses original extracted labels only, Augustus adds capacity and the population/housing tooltip strings, and Vespasian inherits Augustus. Both panel heights reserve space for the renderer's initial padding and final text row. No graphics were added.

Validation on 2026-09-07:

- Release game and StartupParserTest builds passed. Definition contracts, including inherited source rates, missing sources, cycles and conflicting output attributes, passed. The optional `--definitions-only` rerun mode explicitly skips executable validation; the default startup gate is unchanged.
- The full startup gate ran 70 recent/legacy cities for 3,000 ticks each. 69 passed. `Praetor 2 10.svv` missed the existing 1,000 steady simulation TPS threshold at 972.4; an isolated rerun measured 940.6. Both completed with zero post-migration warnings/errors. The full gate therefore remains a timing failure, not a pass. Its original seven migration repairs were logged and the canonical rewrite was clean. Logs: ignored `out/d13-startup.*` and `out/d13-praetor.*`.
- Vespasian native contracts passed shared mint rate, independent gold production, set/add, reset and keyed delta roundtrip, together with live mod settings and the existing catch-up regressions. The final UI rerun repeats these contracts after the panel height correction.
- Julius legacy import/canonical roundtrip and a separate 3,000-tick in-game settings/UI run passed without warnings after rewrite. Julius and Vespasian sidebar captures were visually inspected; the first Augustus capture exposed a panel-height error, corrected in the final XML. Captures/logs remain ignored validation artifacts under `out`; no extracted assets were staged.

Production-average rounding, the 102-based efficiency formula, the combined workforce slider and upstream ancestry remain unchanged.
