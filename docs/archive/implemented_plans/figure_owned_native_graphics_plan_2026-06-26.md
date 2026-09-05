# Figure-Owned Native Graphics Plan

Historical implementation plan retained for the migration record. Its interim attribute, pattern, selector, and fallback descriptions are superseded by `docs/figure_owned_native_graphics_plan.md` and the strict runtime schema.

Snapshot: 2026-06-26

## Intent

Make figure graphics owned by `Figure`/`FigureType` in the same architectural
shape that native buildings now use. The city draw loop should ask the figure
object for its resolved graphic and draw request. It should not reconstruct
image ids from figure type, action state, direction, cart state, corpse rows,
or bespoke animation branches.

This is a prerequisite for Vespasian figure XML that points at the same pixel
art as Augustus/Julius but declares half-size logical dimensions, and later for
high-definition figure assets whose source pixels intentionally differ from
their logical game size.

## Current Findings

- `Mods/Vespasian/FigureType/_README.md` documents a root-level
  `<graphics ... />` node, but the common path is still legacy-group flavored:
  `image_group`, `base_image_offset`, `max_image_offset`,
  `static_frame_count`, and corpse offsets.
- Some FigureType XML already uses `path_pattern` / `image_pattern`, for
  example `barkeep.xml` and `work_camp_architect.xml`, but this is still a
  pattern bridge rather than the building-style target/layer/policy model.
- `src/figure/FigureGraphics.cpp` now owns cached default/action/corpse
  target bindings, legacy atlas image-id formulas, carried-resource cart
  imagery, and XML-authored overlay layer construction. `src/figure/figure_runtime_native.cpp` still uses
  `GenericFigureGraphics` as the runtime bridge, but it now asks
  `FigureTypeDefinition` / `FigureGraphics` for cached bindings instead of
  rebuilding path strings per draw.
- `src/widget/city_figure.cpp` has a native-slice fast path, then falls back to
  carts, horses, fort standards, map flags, enemy images, and `Image::from_id`.
  That keeps graphics policy split between runtime controllers and draw code.
- `src/figure/image.cpp` still owns general direction and frame arithmetic. The
  duplicate cart-attachment and corpse timing tables have been deleted. Migrated
  overlay geometry lives in strict FigureType XML; corpse source/frame-count data
  remains FigureType-owned, and `FigureGraphics` resolves the universal
  death-lifecycle clock directly from its exact transition boundaries. Remaining
  direction policy should become reusable figure graphics data or named
  save-compatible animation state.
- `runtime_texture_draw_request(...)` already accepts explicit logical width and
  height, but city figure drawing normally calls `runtime_texture_draw(...)`,
  which derives logical size from source pixel size.

## Progress Checkpoint

- [x] Establish `GraphicsDefinition`, `BuildingGraphics`, `FigureGraphics`, and `ResourceGraphics` as the intended graphics class split.
- [x] Retire the separate `figure_graphics.h` direction and keep FigureType graphics data on `FigureGraphics`.
- [x] Materialize cached default/action/corpse FigureType graphics bindings through `FigureGraphics`.
- [x] Add structured FigureType graphics child-node parsing for default/action/corpse/cart targets.
- [x] Move many legacy direction/frame/corpse formulas behind `FigureGraphics` helpers instead of local controller arithmetic.
- [x] Delete the 128-entry corpse timing table and global corpse-offset API; preserve every legacy wait-tick transition through one bounded `FigureGraphics` lifecycle policy while keeping corpse source and frame count in FigureType data.
- [x] Migrate stationary ballista direction/frame presentation as the first bounded general-direction family: strict idle/firing state layers own the atlas and view transform, the firing state consumes the authored missile-launcher schedule, and the behavior controller no longer selects images.
- [x] Move figure info-window draw bodies onto `Figure`/figuretype child `draw(c)` methods, reducing one legacy central switch surface.
- [~] Move cart/resource/flag/enemy overlay assembly behind `FigureGraphics`; the hardcoded cart-attachment table and overlay fallback are gone, while raw `image_id`, save-compatible resource `cart_image_id`, and flag bridges remain.
- [x] Make figure draw requests consume each payload slice's assetlist-owned fixed logical size and offsets, including independently sized auxiliary layers. FigureType logical-dimension and sprite-offset attributes are rejected; the shared 26,29 walker anchor is stored on the 428 referenced animation, action, and death assetlists, while portrait assetlists remain unanchored.
- [~] Make converted FigureType graphics use real payload-managed file path references as the authored norm, such as nested `Walkers/<file>` paths. All active Vespasian FigureTypes now have zero legacy source attributes, and the 35 shared definitions touched by the Augustus conversion carry matching graphics blocks without changing Vespasian profiles or behavior. Lower-layer extracted `Group_*` paths still need replacement by sensible authored assetlists containing only asset facts.
- [x] Make city figure drawing ask the figure object for a complete draw request, with `city_figure.cpp` only handling placement/submission and no image-id fallback.
- [ ] Delete controller-owned image mutation and direct `f->image_id` authority for converted figures.
- [ ] Add Vespasian half-size FigureType XML only after correctly scoped authored assetlist ownership and every renderer seam prerequisite are complete. The rejected wrapper/module pass has been removed.

## Target Shape

### Shared Graphics Class Boundary

The graphics hierarchy is explicit:

```cpp
class GraphicsDefinition;
class BuildingGraphics : public GraphicsDefinition;
class FigureGraphics : public GraphicsDefinition;
class ResourceGraphics : public GraphicsDefinition;
```

`BuildingGraphics` owns building graphics selection and warehouse/resource
storage stack refs. `FigureGraphics` owns FigureType draw policy, figure draw
requests, carried-load/cart refs, and figure overlay composition. `ResourceGraphics`
is intentionally narrow and owns only resource icon presentation.

`GraphicsDefinition` owns vocabulary that is shared across those children:
definition kind, comparison operators, normalized eight-way direction helpers,
orientation/point/frame value objects, and canonical target role names such as
default, action, corpse, overlay, icon, resource storage, and resource cart.

The old `src/figure/figure_graphics.h` facade is retired. Figure draw request
types now live beside the native figure runtime API, and FigureType graphics
data lives on `FigureGraphics` instead of a generic policy bag.

### Authored Graphics Versus Extracted Legacy Art

Path-only native targets are the desired authored-asset contract. A Vespasian
authored figure group should be able to use meaningful file names and a single
default entry, such as `<path value="Walkers\lion_tamer_walk_ne_01" />`, without
redeclaring an arbitrary legacy image id.

The current legacy graphics extractor is intentionally crude and often loses
source metadata while splitting atlas content into generated files. When using
that extracted output as migration input, FigureType graphics may still need
explicit `<image>` children or other redeclared metadata to recover meaning the
extractor did not preserve. That is an extractor shortcoming, not a limitation
of the target FigureType graphics schema.

### Figure Graphics Definition

Replace the current root `<graphics ... />` attribute bag with a structured
FigureType graphics model that mirrors BuildingType graphics:

```xml
<graphics>
    <default>
        <path value="Walkers\Barkeep_NE" />
        <image value="Barkeep NE 01" />
    </default>
    <animation state="walking" direction="ne" frame_count="12">
        <path value="Walkers\Barkeep_NE_{frame}" />
        <image value="Barkeep NE {frame}" />
    </animation>
    <corpse frame_count="8">
        <path value="Walkers\Barkeep_death_{frame}" />
        <image value="Barkeep death {frame}" />
    </corpse>
</graphics>
```

The exact XML shape can evolve, but the important contract is:

- FigureType owns all image group paths, image ids, and animation policies.
- C++ owns selection concepts such as direction, action state, corpse state,
  carried resource, selected highlight, and tile-progress placement.
- XML can define logical width/height or logical scale per target/animation.
- Authored logical dimensions should use the final renderer's fine-grained
  integer/fixed-point unit rather than floats, so common ratios such as
  half-size, third-size, sixth-size, 6x source art, and non-round source/logical
  relationships stay deterministic.
- The grain should be chosen for city-view needs, not UI convenience. A
  transitional six-units-per-pixel bridge is acceptable while wiring requests,
  but the final XML unit may need to be much finer so 0.5, 0.33, 6.67, and
  similar relationships remain integer-authored.
- XML can define sprite offset overrides where extracted metadata is missing.
- The legacy one-line graphics node becomes a migration input, not the final
  schema.

### Figure Graphics Runtime Object

Runtime figure draw resolution should continue moving behind the
`FigureGraphics` definition and native figure runtime API. Do not reintroduce a
parallel `figure_graphics.h` facade; that path was retired so `FigureGraphics`
can be the graphics-definition child, not a separate draw helper.

`FigureGraphicDrawRequest` should contain:

- base `RuntimeDrawSlice`;
- optional overlay/cart/resource slices;
- color mask/highlight input;
- sprite offsets;
- logical width/height;
- renderer scaling policy;
- whether the image is elevated, enemy, selected, or hidden;
- enough debug identity to report missing XML paths without falling back
  silently.

### Figure Object Surface

The draw loop should become:

```cpp
FigureDrawContext ctx { x, y, scale, highlight };
figure.draw(ctx);
```

or, during coexistence:

```cpp
if (const FigureGraphicDrawRequest *request = Figure(f).graphic_request(ctx)) {
    draw_request(*request);
} else {
    draw_legacy_figure(f, ctx);
}
```

The important point is that `src/widget/city_figure.cpp` stops knowing which
rows or special offsets belong to a figure type. It should only apply city-view
placement, tile-progress movement offset, and renderer submission.

## Data Model

### FigureType Graphics Definition

Move from `FigureGraphics` as a transitional holder of legacy fields to a small
definition graph:

- `targets`: default, action-specific, corpse, attack, idle, cart, carried load,
  mounted/animal, formation standard, map flag.
- `direction_policy`: eight-way, four-way, static, view-relative, fixed.
- `frame_policy`: loop, ping-pong, static variant, wait-tick driven,
  attack-offset driven, corpse-decay driven.
- `logical_size`: optional width/height or scale factor.
- `sprite_offset`: extracted metadata first, XML override second.
- `layers`: optional attached overlays such as carts, carried resources, buckets,
  animals, standards, flags, and projectiles.

### Runtime State

Figure graphics should read existing save-compatible fields at first:

- `image_offset`
- `attack_image_offset`
- `wait_ticks`
- `direction`
- `previous_tile_direction`
- `action_state`
- `cart_image_id` (resource-cart save bridge only)
- `resource_id`
- `loads_sold_or_carrying`

Over time, direct image ids should stop being the authoritative state. The save
fields can remain as compatibility cursors until save migration can replace
them with named animation state.

## Slice Plan

### Slice 1: Graphics Definition Boundary

- Retire the standalone `figure_graphics.h` facade and keep `FigureGraphics`
  as the FigureType graphics-definition child of `GraphicsDefinition`.
- Keep `BuildingGraphics` as the building-specific graphics-definition child,
  so figure XML migration can share target/path/layer contracts without copying
  building-only state assumptions.
- Promote shared target-role, comparison, direction/orientation, point/offset,
  and frame vocabulary into `GraphicsDefinition`; leave child-specific storage
  and runtime selection behavior on the concrete child classes.
- Keep the narrow draw request type beside the native figure runtime API so
  runtime rendering can represent one base slice plus optional overlay slices.
- Keep debug counters and once-per-type info logging focused on which figures
  still fall back to `Image::from_id` while converted paths resolve through
  `FigureGraphics`.

### Slice 2: Native Payload Cache

- Give runtime-bound figures a cached graphics binding similar to
  `building_runtime::CachedGraphicsBindings`.
- Cache resolved `ImageGroupPayload` and `ImageGroupEntry` pointers by
  FigureType, action target, direction, frame, and logical-size signature.
- Replace repeated path-pattern expansion in `GenericFigureGraphics` with
  load-time materialized target data where possible.
- First safe cache starter: `GenericFigureGraphics` memoized expanded
  native path/image target strings by pattern, direction, and frame, and asks
  the existing `ImageGroupPayload` registry before calling load.
- Cached binding slice: FigureType load now materializes default/action/corpse
  bindings on `FigureGraphics`, storing expanded path/image text plus resolved
  `ImageGroupPayload` and `ImageGroupEntry` pointers. `GenericFigureGraphics`
  reads those cached bindings by target role, direction, and frame instead of
  rebuilding strings and payload lookups during every draw.
- Cached binding cleanup: default/action/corpse role selection and corpse-frame
  clamping now live behind `FigureTypeDefinition::graphics_binding_for_state(...)`
  instead of a runtime-local frame helper.
- Figure-state target lookup now lives on `FigureGraphics`, including the
  current/previous direction choice, orientation-normalized target direction,
  and corpse-frame offset used by cached bindings.
- Follow-up consolidation: hardcoded native figure entries and
  `GenericFigureGraphics` now share the registry-first payload lookup and the
  native-entry-to-draw-request base slice/sprite offset assembly. The remaining
  `city_figure.cpp` `Image::from_id` fallback stays until unconverted
  image-id figures and the legacy `image_id >= 10000` offset hack are retired.
- Cleanup pass: the one-use `GenericFigureGraphics::resolve_entry` wrapper was
  deleted; draw-request assembly now calls the shared `native_entry(...)`
  helper directly while preserving the same missing-target diagnostics.
- Cleanup pass: legacy base draw-request setup moved out of the retired
  standalone draw facade and into the private native helper layer, where it now
  returns base-slice validity for depot/cart draw request builders.
- Use `runtime_texture_draw_request(...)` so logical size can differ from
  source pixel size.

### Slice 3: Structured FigureType Graphics XML

- Add structured child-node graphics parsing beside the existing legacy
  one-line node.
- Parser slice: FigureType now accepts strict child targets under `<graphics>`:
  `<default>`, `<action>`, `<corpse>`, and `<cart>`, plus nested `<path>` and
  optional `<image>` nodes for default/action/corpse path targets. Path-only
  child targets bind to the image group's default entry; flat legacy
  `path_pattern` attributes still require explicit `image_pattern` during
  migration.
- Extracted legacy groups can remain verbose until the extractor preserves more
  metadata. Authored Vespasian graphics should prefer meaningful one-group
  files with default entries and no duplicate image-id declarations.
- Load-time validation now expands native default/action/corpse targets through
  `FigureGraphics` and requires each referenced `ImageGroupPayload` plus either
  its explicit entry or default entry to resolve, with figure/profile/context
  details in startup failures.
- Support default, direction, corpse, action-state, static-frame, and cart/load
  targets.
- Keep legacy attributes as a temporary migration input, but do not add new
  features to them.
- `Mods/Vespasian/FigureType/_README.md` and `_template.xml.example` now show
  the nested path-only authored shape.

### Slice 4: Retire Controller Image Mutation

- Replace direct `f->image_id = ...` and `figure_image_update(...)` calls in
  native controllers with calls that advance or select a named graphics state.
- Start with simple service walkers, then transient walkers, then market and
  depot carts, then entertainment/animal cases.
- Keep movement and behavior controllers responsible for behavior only. They
  may set state like `walking`, `returning`, `attacking`, `corpse`, or
  `carrying_resource`; they should not calculate image ids.

#### Remaining Controller-Owned Image Mutation Map

The remaining `src/figure/figure_runtime_native.cpp` mutations are controller
state, not city-draw-only fallbacks. Move these only after `FigureGraphics` can
resolve legacy `image_group` graphics into draw requests without relying on
`f->image_id`:

- `update_legacy_figure_graphics_image_state`: legacy XML graphics fallback for
  action/corpse/direction/image-offset state. Needs a draw-request resolver for
  non-native `image_group` policies, including corpse base. Directional default
  rows now share the `FigureGraphics` directional-frame helper and preserve
  `direction_frame_stride`. Native path/image policies now skip the synthetic
  draw-request fallback check and validate their cached `FigureGraphics`
  binding directly. Generic legacy default/static/corpse/directional image-id
  formulas now live on `FigureGraphics`; controllers pass current animation
  state and keep only the legacy `f->image_id` mutation until draw requests can
  resolve old `image_group` policies directly. Prefect, charioteer, and
  gladiator special attack/base rows now also ask `FigureGraphics` for named
  legacy row ids instead of rebuilding `legacy_image_base() + 104` locally.
- `RoamingServiceFigure`: service walkers and labor seekers now delegate legacy
  image-state selection through `FigureGraphics::update_legacy_image_state`.
  Still needs `max_image_offset` and animation cursor ownership moved out of the
  controller.
- `TransientWandererFigure`: ownerless ambient walkers now delegate corpse,
  static-frame, and default legacy image-state selection through
  `FigureGraphics::update_legacy_image_state`. Still needs animation cursor
  ownership moved out of the controller.
- `MarketSupplierFigure`: market supplier base/corpse image-state selection now
  delegates through `FigureGraphics::update_legacy_image_state`. Still needs
  carried-resource/follower state represented as graphics state before the city
  fallback can stop reading `f->image_id`.
- `DeliveryFollowerFigure`: delivery boy and supplier followers now delegate
  base/corpse/direction/image-offset selection through the legacy
  `FigureGraphics` helper while preserving the previous-tile-direction fallback.
  Still needs follow/dead action naming and cart/resource clearing represented
  as graphics state rather than `cart_image_id = 0`.
- `EngineerServiceFigure`: simple service animation now delegates legacy
  image-state selection through `FigureGraphics::update_legacy_image_state`.
  Still needs animation cursor ownership moved out of the controller.
- `PrefectServiceFigure`: default and corpse image-state selection delegates
  through the legacy `FigureGraphics` helper. Going-to-fire and at-fire bucket
  artwork now uses strict named `<state>` layers with the exact legacy
  movement/attack direction sources, two view adjustments, atlas rows, and frame
  divisors; the controller no longer calculates or stores those bucket image ids.
  Prefect attack artwork still needs a named action target.
- `EntertainmentFigureBase`: plain default/corpse image-state rows now delegate
  through the legacy `FigureGraphics` helper where the current XML is plain
  `image_group` data. Charioteer corpse/attack, lion-tamer whip, and gladiator
  attack rows now use the `FigureGraphics` directional-frame helper, but still
  need action targets represented in data. The lion-tamer animal is now a strict
  named `<overlay>` whose atlas group, direction/frame policy, cart-style offset,
  draw order, and corpse suppression are owned by `FigureGraphics`; its
  controllers no longer choose or store that animal in `cart_image_id`.

### Slice 5: Overlay And Cart Ownership

- Move cart/resource overlays out of `city_figure.cpp` into figure graphics
  layers.
- Replace `cart_image_id` finalization with FigureGraphics-owned overlay
  helpers first, including explicit clear/no-overlay states, then move those
  overlays into draw request layers.
- Depot-cart draw requests now use FigureGraphics helpers for direction-major
  row arithmetic and legacy base-image request setup. XML-owned resource-load
  completeness, cart layer offsets, and carried-resource slice selection now
  live on `FigureGraphics`; the remaining work is replacing the legacy
  `f->image_id` base-image bridge with direct old-`image_group` draw requests.
- Map-flag animation, category-icon selection, stacking, and one-based number
  policy now come from strict FigureType `<map_flag>`/`<marker>` data.
  `FigureGraphics` assembles both image layers and supplies the resolved number
  and authored offset to the narrow text renderer; the controller no longer
  calculates or stores `image_id`/`cart_image_id` graphics bridges.
- Enemy-atlas figure drawing now resolves through a colorless
  `FigureGraphics` layer when no cart overlay is present; city draw no longer
  calls `Image::enemy(...)` directly, but enemy image-id selection still lives
  in the enemy/native controllers.
- After the map-flag and enemy draw migrations, remaining city draw fallbacks
  collapse to one `Image::from_id` path, so the fallback debug counter now
  treats every legacy fallback draw as an image-id fallback.
- The old `prepared_legacy_image(...)` bridge now lives behind
  `FigureGraphics::legacy_image(...)`, preserving unpacked-asset and
  external-image loading for unconverted fallbacks. Raw `f->image_id`,
  `cart_image_id`, and legion flag id storage remains until those overlays have
  named graphics targets.
- Enemy-atlas sprite offsets are now also supplied by the `FigureGraphics` draw
  request. The city fallback offset path no longer selects `Image::enemy(...)`
  and only mirrors the remaining `Image::from_id` fallback.
- `FigureGraphicsLayer` now owns legacy image layers, stacked flag/map overlays,
  enemy colorless layers, and cart overlay layer offsets; runtime draw requests
  only adapt those layers. Raw `f->image_id`, `cart_image_id`, and
  `legion_flag_id` bridges remain until named overlay targets are authored.
- Lion-tamer whip frames now come from a structured FigureType `<action><path value="Walkers\\<group>" /></action>` legacy action source instead of a
  runtime-hardcoded atlas group. The controller still owns the timed
  `wait_ticks_missile >= 96` selection until action-state/frame policy can
  represent that condition.
- Resource carts should resolve their resource payload through the same graphics
  request rather than mutate `cart_image_id`. Depot cart XML-owned
  resource-load completeness, layer offsets, and carried-resource cart slice
  selection live on `FigureGraphics`. The lighthouse supplier now completes one
  exact generic-cart family: its graphics-only FigureType policy owns the empty
  cart group, returning action, collecting-item resource source, fixed-one load,
  all eight attachment offsets, draw order, corpse suppression, and the paired
  food-lift threshold/shift fields. Its controller no longer mutates or finalizes
  `cart_image_id`; resource identity remains in `collecting_item_id`. Production,
  warehouse, and docker transitions publish their one-tick presentation through
  owner-bound `FigureGraphicsState`, while `cart_image_id` is synthesized and
  hydrated only at the existing save boundary.
- Lion-tamer animals are named FigureType graphics layers. Roman fort standards
  now author their moving/halted unit-banner frames and stacked legion badge in
  `<standard>` data, eliminating their `cart_image_id` bridge while preserving
  the existing auxiliary infantry/archer pole-only fallback. Prefect bucket
  states and complete map-flag stacks are also authored layers. Hippodrome
  horse/cart team imagery, direction transforms, cart attachment, draw order,
  and all four wait-tick world-offset schedules now live in strict
  `<hippodrome_race>` data. Missile-launcher pose schedules now live in strict
  graphics-only FigureType `<missile_launcher>` definitions. Those definitions
  select either the saved `attack_image_offset` cursor or transient
  `wait_ticks_missile`, author the divisor, exact leading frames, and stable
  after-frame, and are published only through `FigureGraphics` so they cannot
  replace legacy runtime profiles. Fishing-boat ordinary/fishing rows and trade-ship dock-facing presentation now use generic strict `<directional>`/`<pose>` policy driven by saved action and direction fields; their controllers no longer calculate or clear images.

- The legacy cart-attachment family has been deleted rather than hidden behind
  another facade. Actor, gladiator, tourist, immigrant, emigrant, native-trader,
  lion-tamer, and hippodrome overlays author exact eight-direction offsets;
  ordinary overlays additionally author action visibility and optional
  per-resource atlas stride. `FigureGraphics` consumes those definitions
  directly, and no controller or renderer path calls the removed cart-offset
  setter or cart-image finalizers. The saved cart offset bytes remain layout
  compatibility fields but no longer drive these overlay layers.

#### Graphics-only FigureType definitions

`graphics_only="true"` is the narrow seam for figures whose presentation must
be authored before their behavior is migrated to native FigureType profiles.
Such a definition must contain graphics and must not contain profiles. It is
layered and validated like an ordinary FigureType, but it is excluded from
`definition_for(...)`; `FigureGraphics::for_type(...)` can still resolve it.
This keeps action-dependent legacy pathing intact for figures such as tower
sentries while removing controller-owned launcher tables. Missile-launcher
definitions are policy-only: they intentionally have no default image source,
because native-warrior and enemy-atlas code still owns the base sprite family.
Projectile type, target, lifetime, and saved in-flight cursor fields are not
part of this presentation contract and remain unchanged.

### Slice 6: Native File-Path Payload Ownership

- Move all converted figure graphics to the image group payload manager as the
  authoritative source of draw slices.
- FigureType graphics XML must use real relative file paths with nested
  `<graphics><default><path value="Walkers\\<file>" /></default></graphics>`
  style nodes instead of legacy group/image-id references as the authored
  contract.
- Keep legacy group/image-id bridges only for unconverted fallbacks and old save
  hydration. Converted figures should draw from resolved payload entries.
- This is one required gate before Vespasian half-size FigureType XML overrides,
  because logical-size metadata has to attach to resolved payload-backed figure
  graphics rather than legacy atlas ids.
- The full Renderer Scaling Seams plan is also a hard prerequisite. Half-size
  figure XML must wait until every seam checklist item is complete, especially
  the split between source pixel dimensions and fixed-point logical image
  dimensions at the renderer boundary.

### Slice 7: Vespasian Half-Size Figure XML

- Add Vespasian FigureType XML overrides for every resized figure only after
  the figure payload gate and every Renderer Scaling Seams item are complete.
- Point each override at the same extracted pixel art initially.
- Define logical width/height in the final fixed-point logical-size unit, using
  half the source-pixel dimensions as the first Vespasian validation target.
- Validate that tile-progress offsets, busy-road offsets, carts, corpses, and
  selection coordinate reporting still line up at half logical size.
- Do not resize the source PNGs for this slice. The point is to prove source
  pixels and logical size are decoupled.

### Slice 8: Delete Legacy Branches

- Remove figure-specific image-id arithmetic from `city_figure.cpp`.
- Collapse `figure_runtime_has_native_graphics`, `figure_runtime_graphic_slice`,
  and `figure_runtime_graphic_sprite_offset` into the new object request API.
- Delete local duplicate corpse/direction/cart tables once the policy graph owns
  them. Corpse timing and cart attachment are complete; general direction remains.
- Keep only narrow compatibility readers for old saves and unconverted figures.

## Validation Plan

- Static validation: every structured FigureType graphics target resolves to an
  `ImageGroupPayload` entry at load time.
- Runtime validation: enable a debug counter for native figure draw requests,
  legacy figure fallbacks, missing payloads, and logical-size overrides.
- Visual validation: compare normal, overlay, and selected-city draws for
  service walkers, carts, entertainment walkers, animals, boats, enemies, and
  corpses.
- Scaling validation: at zoom `0.5`, `1`, `2`, and `3`, verify Vespasian
  half-size figures keep correct tile anchoring while source pixels remain
  sampled from the original art.
- Save/load validation: old saves with legacy `image_id`, `image_offset`, and
  action states rebind to the same visible animation state after load.

## Non-Goals

- Do not make XML own movement, pathfinding, attack logic, or service effects.
- Do not require high-definition replacement art in the half-size XML slice.
- Do not keep adding one-off action graphics attributes to the legacy
  one-line `<graphics ... />` node.
- Do not preserve `f->image_id` as the authoritative draw contract once a figure
  has native graphics.

## Acceptance Direction

- The city draw loop asks a figure for a draw request and submits it.
- FigureType XML owns the graphic source and logical size.
- Figure controllers update behavior state, not image ids.
- Vespasian can halve figure logical size by XML override while using the same
  pixel art.
- Remaining legacy figure draw paths are obvious, counted, and shrinking.
