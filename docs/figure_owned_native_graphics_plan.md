# Figure-Owned Native Graphics Plan

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
- `src/figure/figure_runtime_native.cpp` has `GenericFigureGraphics`, which can
  resolve an `ImageGroupEntry` and return a `RuntimeDrawSlice`. That is useful
  as a bridge, but many native figure controllers still assign `f->image_id`
  directly.
- `src/widget/city_figure.cpp` has a native-slice fast path, then falls back to
  carts, horses, fort standards, map flags, enemy images, and `Image::from_id`.
  That keeps graphics policy split between runtime controllers and draw code.
- `src/figure/image.cpp` owns corpse, direction, frame, missile launcher, and
  cart offset tables. Some of this policy should become reusable figure graphics
  data; some should move into XML; some remains temporary save-compatible
  animation state.
- `runtime_texture_draw_request(...)` already accepts explicit logical width and
  height, but city figure drawing normally calls `runtime_texture_draw(...)`,
  which derives logical size from source pixel size.

## Target Shape

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

Add a figure graphics object, probably under `src/figure/figure_graphics.*` or
inside the runtime split if that stays cleaner:

```cpp
class FigureGraphics {
public:
    FigureGraphicDrawRequest resolve(const Figure &figure) const;
    void advance(Figure &figure) const;
    void invalidate(Figure &figure) const;
};
```

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

### FigureType Graphics Policy

Move from `GraphicsPolicy` as a bag of legacy fields to a small policy graph:

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
- `cart_image_id`
- `resource_id`
- `loads_sold_or_carrying`

Over time, direct image ids should stop being the authoritative state. The save
fields can remain as compatibility cursors until save migration can replace
them with named animation state.

## Slice Plan

### Slice 1: Facade Without Behavior Change

- Add `FigureGraphics` facade that wraps the existing `GenericFigureGraphics`,
  `figure_image_update`, and legacy `f->image_id` rules.
- Add a narrow draw request type that can represent one base slice plus optional
  overlay slices.
- Make `city_figure.cpp` call the facade first but keep every legacy fallback.
- Add debug counters and once-per-type info logging that report which figures
  still fall back to `Image::from_id`
  (`FigureGraphicsDebugCounters::facade_draw_requests`,
  `legacy_fallback_draws`, and `image_from_id_fallback_draws`).

### Slice 2: Native Payload Cache

- Give runtime-bound figures a cached graphics binding similar to
  `building_runtime::CachedGraphicsBindings`.
- Cache resolved `ImageGroupPayload` and `ImageGroupEntry` pointers by
  FigureType, action target, direction, frame, and logical-size signature.
- Replace repeated path-pattern expansion in `GenericFigureGraphics` with
  load-time materialized target data where possible.
- First safe cache starter: `GenericFigureGraphics` now memoizes expanded
  native path/image target strings by pattern, direction, and frame, and asks
  the existing `ImageGroupPayload` registry before calling load. Direct
  `ImageGroupEntry *` caching still waits for a payload generation/invalidation
  signal so game reset cannot leave stale entry pointers.
- Follow-up consolidation: hardcoded native figure entries and
  `GenericFigureGraphics` now share the registry-first payload lookup and the
  native-entry-to-draw-request base slice/sprite offset assembly. The remaining
  `city_figure.cpp` `Image::from_id` fallback stays until unconverted
  image-id figures and the legacy `image_id >= 10000` offset hack are retired.
- Cleanup pass: the one-use `GenericFigureGraphics::resolve_entry` wrapper was
  deleted; draw-request assembly now calls the shared `native_entry(...)`
  helper directly while preserving the same missing-target diagnostics.
- Cleanup pass: `FigureGraphics::set_legacy_base_draw_request_image(...)` was
  removed from the public facade and moved to the private native helper layer,
  where it now returns base-slice validity for depot/cart draw request builders.
- Use `runtime_texture_draw_request(...)` so logical size can differ from
  source pixel size.

### Slice 3: Structured FigureType Graphics XML

- Add structured child-node graphics parsing beside the existing legacy
  one-line node.
- Validate every path/image reference at FigureType load time.
- Support default, direction, corpse, action-state, static-frame, and cart/load
  targets.
- Keep legacy attributes as a temporary migration input, but do not add new
  features to them.
- Update `Mods/Vespasian/FigureType/_README.md` once the parser shape is real.

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

- `update_legacy_graphics_policy_image_state`: legacy XML graphics fallback for
  action/corpse/direction/image-offset state. Needs a draw-request resolver for
  non-native `image_group` policies, including corpse base. Directional default
  rows now share the `FigureGraphics` directional-frame helper and preserve
  `direction_frame_stride`.
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
- `PrefectServiceFigure`: default and corpse image-state selection now
  delegates through the legacy `FigureGraphics` helper with the existing
  direction/attack-direction choice preserved. Fire, bucket, and attack artwork
  now use the `FigureGraphics` directional-frame helper, but still need named
  action targets/layers.
- `EntertainmentFigureBase`: plain default/corpse image-state rows now delegate
  through the legacy `FigureGraphics` helper where the current XML is plain
  `image_group` data. Charioteer corpse/attack, lion-tamer whip, and gladiator
  attack rows now use the `FigureGraphics` directional-frame helper, but still
  need action targets represented in data. Legacy cart-overlay finalization now
  goes through `FigureGraphics`, but the controller still chooses the cart/animal
  base image and stores the final result in `cart_image_id`.

### Slice 5: Overlay And Cart Ownership

- Move cart/resource overlays out of `city_figure.cpp` into figure graphics
  layers.
- Replace `cart_image_id` finalization with FigureGraphics-owned overlay
  helpers first, including explicit clear/no-overlay states, then move those
  overlays into draw request layers.
- Depot-cart draw requests now use FigureGraphics helpers for direction-major
  row arithmetic and legacy base-image request setup; cart layer selection and
  load/resource slices still need named graphics state.
- Map-flag base and flag image layers now resolve through `FigureGraphics`;
  `city_figure.cpp` only keeps the flag number text overlay while
  `cart_image_id` remains the temporary storage bridge.
- Enemy-atlas figure drawing now resolves through a colorless
  `FigureGraphics` layer when no cart overlay is present; city draw no longer
  calls `Image::enemy(...)` directly, but enemy image-id selection still lives
  in the enemy/native controllers.
- After the map-flag and enemy draw migrations, remaining city draw fallbacks
  collapse to one `Image::from_id` path, so the fallback debug counter now
  treats every legacy fallback draw as an image-id fallback.
- Enemy-atlas sprite offsets are now also supplied by the `FigureGraphics` draw
  request. The city fallback offset path no longer selects `Image::enemy(...)`
  and only mirrors the remaining `Image::from_id` fallback.
- Resource carts should resolve their resource payload through the same graphics
  request rather than mutate `cart_image_id`.
- Lion tamer animals, hippodrome horses, fort standards, map flags, prefect
  buckets, missile launchers, and fishing-boat/dock-related overlays should each
  become named graphics policies or layers.

### Slice 6: Vespasian Half-Size Figure XML

- Add Vespasian FigureType XML overrides for every resized figure.
- Point each override at the same extracted pixel art initially.
- Define logical width/height in the final fixed-point logical-size unit, using
  half the source-pixel dimensions as the first Vespasian validation target.
- Validate that tile-progress offsets, busy-road offsets, carts, corpses, and
  selection coordinate reporting still line up at half logical size.
- Do not resize the source PNGs for this slice. The point is to prove source
  pixels and logical size are decoupled.

### Slice 7: Delete Legacy Branches

- Remove figure-specific image-id arithmetic from `city_figure.cpp`.
- Collapse `figure_runtime_has_native_graphics`, `figure_runtime_graphic_slice`,
  and `figure_runtime_graphic_sprite_offset` into the new object request API.
- Delete local duplicate corpse/direction/cart tables once the policy graph owns
  them.
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
