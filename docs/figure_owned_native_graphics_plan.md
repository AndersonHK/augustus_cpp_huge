# Figure-Owned Native Graphics: Remaining Work

Snapshot: 2026-08-31

## Current Contract

Figure graphics now follow the same ownership direction as building graphics. `FigureGraphics` inherits shared graphics-definition semantics, resolves logical asset XML, and produces complete draw requests. City drawing submits those requests; it does not reconstruct figure images from type-specific enums or fall back to the legacy city-draw switch.

FigureType XML uses this strict shape:

```xml
<graphics>
    <default>
        <path value="Health_Culture\Academy" />
    </default>
</graphics>
```

The path names a logical asset XML and never includes `.xml` or names a PNG. A conceptual asset XML may contain its default image, animations, corpse presentation, named entries, and composite layers together. FigureType should retain selection data such as `max_image_offset` only when the same fact is not present or inferable in the referenced asset data.

Runtime-extracted Julius and Augustus assets are the default source. A `Category\Group_*` path is valid when that extracted group fully represents the conceptual model. Do not author wrapper XML merely to rename or duplicate it. A redistribution-safe bridge XML belongs in the lowest relevant mod only when an essential legacy sequence, offset, or composition cannot be extracted or inferred; it contains semantics and references, never extracted proprietary pixels.

An unresolved request is an error and must say `ERROR: Figure graphics request is unresolved; figure_type=...`. It is not a disabled fallback: the old path is being migrated and deleted.

## Implemented Baseline

- [x] Strict path-only FigureType graphics parsing rejects legacy source attributes, direct image selectors, patterns, and figure-specific schema such as `runtime_selected_image_group`.
- [x] Generic native draw requests cover default/action/corpse, directional, state, overlay, missile, resource-cart, standard, map-flag, and hippodrome presentation through asset data.
- [x] `city_figure.cpp` asks the figure runtime for a complete request and has no legacy image-id drawing fallback for converted figures.
- [x] The startup gate synthetically materializes every configured figure alive and dead, including every direction and applicable state, and rejects unresolved bindings.
- [x] Julius-only, Julius+Augustus, and Vespasian startup/save gates load their inherited graphical layers without requiring duplicate upper-mod definitions.

## Remaining Migration

- [ ] Delete controller-owned `image_id` mutation wherever it no longer carries simulation or save-bridge state. Remove generic “update graphics” stubs when the centralized runtime already refreshes every figure.
- [ ] Finish separating save-compatible legacy image fields from draw authority. Persist or migrate them only where old saves require the data; native rendering must consume object state plus resolved asset definitions.
- [ ] Add Vespasian logical half-size figure overrides only after the renderer has an explicit fixed-point logical-size contract and consistent tile anchoring. Logical/source dimensions belong in asset data, not FigureType XML.
- [ ] Continue toward atlas residency in GPU VRAM and Vulkan submission driven by object state and shader-visible material/animation data. This is a renderer follow-on, not a reason to weaken the current XML contract.

## Validation Gates

- Strict parser fixtures must reject every removed FigureType graphics schema and direct bitmap/XML-suffixed path.
- Synthetic alive/dead coverage must resolve every configured type without fallback counters or unresolved-request logs.
- Startup must pass for Julius, Julius+Augustus, and Vespasian after clean runtime extraction.
- Required save cohorts must round-trip and run 3,000 ticks at 1,000% speed with only allowlisted migration-repair warnings; steady-state throughput should exceed 1,000 TPS by the third second.
- Manual validation must include wolves, zebra live/corpse presentation, chariots with horses/carriage/rider layers, resource carts, flags, missiles, boats, and composed-building scenes before the migration is considered visually closed.
