# Bound Runtime Module Extraction Plan

This plan identifies the lowest-friction modules to peel out of legacy records first. The target shape is owner-bound runtime modules:

```cpp
building.storage().reserve_output_loads();
building.production().tick();
building.religion().provide_access();
```

Each bound module should package:

- `Building &owner_`
- `const *Def` pointer loaded from XML/type definitions
- mutable `*State &` owned by the building instance or, during migration, temporarily viewed from the legacy record

The first slices should wrap existing record-backed state before moving storage out of `building`. That keeps save compatibility intact while callers migrate to module behavior.

## Draft Runtime Facade Files

These are planning sketches, not current files. When implementation starts, each class should get its own matching `.h/.cpp` pair under the owning domain folder. The exact folder can still be decided, but the class/file naming should stay one-to-one.

Each class follows this ownership shape:

```cpp
class BuildingSomething {
public:
    class State;

    BuildingSomething(Building &owner, const SomethingDef *definition, State &state);
    void tick_or_do_domain_behavior();

private:
    Building &owner_;
    const SomethingDef *definition_ = nullptr;
    State &state_;
};
```

The `definition_` member is a pointer rather than a reference because a building object can change type during gameplay. The definition object itself remains immutable after startup.

### BuildingWaterAccess.h

Information comes from two places today:

- `Mods/*/WaterAccessType/*.xml` defines the access vocabulary, such as fountain, well, reservoir, or future modded water kinds.
- `Mods/*/BuildingType/<building>.xml` currently owns the building-specific provider/requirement nodes, ranges, origins, and node geometry.

Future XML shape: peel the building-specific definition into a new folder such as `Mods/*/WaterAccess/*.xml` or `Mods/*/BuildingWaterAccess/*.xml`, then make `BuildingType` reference that definition by path. `WaterAccessType` remains a vocabulary registry consumed by those definitions.

```cpp
#pragma once

#include "building/building_fwd.h"

struct map_point;

namespace building_type_registry_impl {
class WaterAccessDefinition;
}

class BuildingWaterAccess {
public:
    class State;

    BuildingWaterAccess(Building &owner,
        const building_type_registry_impl::WaterAccessDefinition *definition,
        State &state);

    int has_definition() const;
    int provides_access() const;
    int requires_access() const;
    int has_required_access() const;
    int cached_road_access_point(map_point *road) const;
    void register_provider();
    void unregister_provider();
    void dirty_neighborhood();

private:
    Building &owner_;
    const building_type_registry_impl::WaterAccessDefinition *definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Move `WaterAccessDefinition` out of `BuildingType` or make its extraction path explicit.
- Decide the new XML root name and whether one definition file can be shared by many buildings.
- Keep climate/range modifiers represented as data instead of helper fallbacks.

### BuildingCulture.h

Information comes from:

- `Mods/*/CultureModule/*.xml` for the culture/entertainment concept.
- `BuildingType` culture bindings for capacity, upgrade bonus capacity, count mode, and which buildings use which module.

Future XML shape: keep `CultureModule` as reusable domain identity, and add a richer `BuildingCulture/*.xml` binding definition if the building-specific data keeps growing. Then `BuildingType` should reference the building culture definition rather than embedding capacity/count-mode details.

```cpp
#pragma once

#include "building/building_fwd.h"

namespace building_type_registry_impl {
class BuildingType;
class CultureModule;
struct BuildingCultureModule;
}

class Figure;

class BuildingCulture {
public:
    class State;

    BuildingCulture(Building &owner,
        const building_type_registry_impl::BuildingType *building_definition,
        State &state);

    int has_definition() const;
    int can_host_show() const;
    int needs_performer() const;
    int has_performer() const;
    int should_animate() const;
    void record_performance();
    void clear_performer_if_matches(const Figure &figure);

    const building_type_registry_impl::CultureModule *primary_definition() const;
    const building_type_registry_impl::BuildingCultureModule *primary_binding() const;

private:
    Building &owner_;
    const building_type_registry_impl::BuildingType *building_definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Decide whether the facade is named `BuildingCulture` or `BuildingEntertainment`.
- Separate venue/provider state from mixed legacy figure ids.
- Make animation gates use this module so composed children ask the bound owner/module state consistently.

### BuildingReligion.h

Information mostly already comes from:

- `Mods/*/Religions/*.xml` for tier, gods, capacity, and presentation.
- `Mods/*/Gods/*.xml` for god identity.
- `BuildingType` as the binder saying which temple uses which religion definition.

Future XML shape: mostly keep as-is. `BuildingType` can continue to reference a religion definition by path; the runtime change is to make all temple behavior route through `building.religion()`.

```cpp
#pragma once

#include "building/building_fwd.h"
#include "city/constants.h"

namespace building_type_registry_impl {
class Religion;
enum class ReligionTier;
}

class BuildingReligion {
public:
    class State;

    BuildingReligion(Building &owner,
        const building_type_registry_impl::Religion *definition,
        State &state);

    int has_definition() const;
    int is_temple() const;
    int serves_god(god_type god) const;
    int is_tier(building_type_registry_impl::ReligionTier tier) const;
    int provides_access_to(const Building &house) const;
    int capacity() const;
    const building_type_registry_impl::Religion *definition() const;

private:
    Building &owner_;
    const building_type_registry_impl::Religion *definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Identify record-backed temple/grand-temple state that belongs to the building module rather than city god state.
- Keep city-wide favor/blessing state outside this module.

### BuildingHousing.h

Information comes from:

- `Mods/*/HousingType/*.xml` for resident/evolution requirements and resident class.
- `BuildingType` for capacity, footprint, vacant-lot fill type, and transitions such as merge/split/evolve/devolve.

This is an important distinction: `HousingType` is not the complete building module. It describes the resident class/level and requirements. A building of the same resident type may still differ by footprint, capacity, merge/split shape, and transition target.

Future XML shape: add a `BuildingHousing/*.xml` folder for capacity, footprint-sensitive policy, and merge/split/evolution binding. That definition can reference `HousingType` as the resident definition. `BuildingType` should eventually reference the building-housing definition rather than embedding capacity and transition ids.

```cpp
#pragma once

#include "building/building_fwd.h"

namespace building_type_registry_impl {
class BuildingType;
class HousingType;
}

class BuildingHousing {
public:
    class State;

    BuildingHousing(Building &owner,
        const building_type_registry_impl::BuildingType *building_definition,
        const building_type_registry_impl::HousingType *resident_definition,
        State &state);

    int has_definition() const;
    int population() const;
    int population_room() const;
    int capacity() const;
    int resident_level() const;
    int can_evolve() const;
    int can_devolve() const;
    int can_merge() const;
    int can_split() const;
    void consume_goods();
    void merge_or_split();
    void refresh_definition_bindings();

    const building_type_registry_impl::BuildingType *building_definition() const;
    const building_type_registry_impl::HousingType *resident_definition() const;

private:
    Building &owner_;
    const building_type_registry_impl::BuildingType *building_definition_ = nullptr;
    const building_type_registry_impl::HousingType *resident_definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Define `BuildingHousingState` over current record fields without moving save layout first.
- Decide how 1x1/2x2 merge ownership is represented so rapid tent merge/unmerge cannot be split across scattered callers.
- Move capacity out of ad hoc callers and into this module facade.

### BuildingProduction.h

Information comes from:

- `Mods/*/ProductionMethod/*.xml` for output, inputs, output source, output destination, cart loads, batch size, climate bonuses, and treasury output.
- `BuildingType` for which production method or methods a building uses.

Future XML shape: mostly keep `ProductionMethod` as the immutable definition. Only add a separate `BuildingProduction/*.xml` folder if buildings need binding metadata beyond a list of production methods.

```cpp
#pragma once

#include "building/building_fwd.h"
#include "game/resource.h"

namespace building_type_registry_impl {
class BuildingType;
class ProductionMethod;
}

class Figure;

class BuildingProduction {
public:
    class State;

    BuildingProduction(Building &owner,
        const building_type_registry_impl::BuildingType *building_definition,
        State &state);

    int has_definition() const;
    int can_start_cycle() const;
    int has_inputs() const;
    int is_blocked() const;
    int output_cart_capacity(resource_type resource) const;
    int reserve_output_cart(resource_type *out_resource, int *out_loads);
    void release_output_cart_if_matches(const Figure &figure);
    void tick();

    const building_type_registry_impl::ProductionMethod *primary_definition() const;

private:
    Building &owner_;
    const building_type_registry_impl::BuildingType *building_definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Define ownership of output cart ids, production progress, stockpiling, and native production flags.
- Keep actual resource slot math delegated to `BuildingStorage`.
- Avoid spawning replacement carts merely because a current cart is idle or waiting for a valid destination.

### BuildingStorage.h

Information comes from:

- `Mods/*/StorageType/*.xml` for role, resource list, and capacity.
- `BuildingType` for which storage types apply to a building.
- Runtime record/storage helpers for actual slots, reservations, accept/get permissions, and cart interactions.

Future XML shape: probably keep `StorageType` as reusable storage definitions. Add `BuildingStorage/*.xml` only if per-building slot policy, reservation policy, or accept/get defaults become too rich for simple references.

```cpp
#pragma once

#include "building/building_fwd.h"
#include "building/storage_type.h"
#include "game/resource.h"

class Figure;

namespace building_type_registry_impl {
class BuildingType;
class StorageType;
}

class BuildingStorage {
public:
    class State;

    BuildingStorage(Building &owner,
        const building_type_registry_impl::BuildingType *building_definition,
        State &state);

    int has_definition() const;
    int accepts(resource_type resource) const;
    int amount(resource_type resource, building_type_registry_impl::StorageRole role) const;
    int available_space(resource_type resource, building_type_registry_impl::StorageRole role) const;
    int reserve_input(resource_type resource, int loads, const Figure &figure);
    int receive(resource_type resource, int loads, const Figure &figure);
    int reserve_output(resource_type resource, int loads, const Figure &figure);
    void release_reservations_for(const Figure &figure);

    const building_type_registry_impl::StorageType *definition_for(
        resource_type resource, building_type_registry_impl::StorageRole role) const;

private:
    Building &owner_;
    const building_type_registry_impl::BuildingType *building_definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Define `BuildingStorageState` over legacy storage slots and reservation arrays.
- Centralize warehouse/granary/dock/market slot semantics before moving save fields.
- Preserve the rule that a warehouse slot committed to one resource cannot mix another resource into the same slot.

### BuildingFormation.h

Information comes from:

- `Mods/*/FormationType/*.xml` for formation identity.
- `Mods/*/UnitType/*.xml` for combat unit definitions.
- Fort `BuildingType` for the formation reference.

Future XML shape: mostly already correct. Forts should reference the formation type they own; formations reference their unit composition; units reference figure/combat definitions.

```cpp
#pragma once

#include "building/building_fwd.h"
#include "figure/type.h"

class FormationType;

class BuildingFormation {
public:
    class State;

    BuildingFormation(Building &owner, const FormationType *definition, State &state);

    int has_definition() const;
    int can_muster() const;
    int active_formation_id() const;
    figure_type recruit_figure_type() const;
    void muster();
    void spawn_recruit();
    void layout_slots();

    const FormationType *definition() const;

private:
    Building &owner_;
    const FormationType *definition_ = nullptr;
    State &state_;
};
```

Missing before implementation:

- Remove legacy assumptions that formations are always 4x4 or 16 soldiers.
- Make slot spacing dynamic so larger formations still fit the fortress mustering ground.
- Keep this behind the module facade before changing save/load bridges for legacy formation enums.

## XML Ownership Map

This is the intended split between existing XML, future XML, and the runtime module class.

| Runtime facade | Existing XML source | Future XML source | Notes |
| --- | --- | --- | --- |
| `BuildingWaterAccess` | `WaterAccessType` plus embedded `BuildingType/<building>.xml` `<water_access>` nodes | `WaterAccess/*.xml` or `BuildingWaterAccess/*.xml`, referenced by `BuildingType` | `WaterAccessType` is only the vocabulary of water kinds. The complete building-specific provider/requirement definition is currently embedded in `BuildingType` and must be peeled out. |
| `BuildingCulture` | `CultureModule` plus embedded `BuildingType` culture bindings | `BuildingCulture/*.xml`, referencing `CultureModule/*.xml` | `CultureModule` names the culture/entertainment concept. Capacity, count mode, upgrade bonus, and venue binding currently still live in `BuildingType`. |
| `BuildingReligion` | `Religions/*.xml`, `Gods/*.xml`, referenced by `BuildingType` | Mostly already correct | Religion is closer to complete XML ownership than most modules. `BuildingType` should remain the binder saying which religion definition a temple uses. |
| `BuildingHousing` | `HousingType/*.xml` plus `BuildingType` capacity/footprint/transitions | `BuildingHousing/*.xml` for footprint/capacity/merge/split policy, still referencing `HousingType` resident definitions | `HousingType` is the resident/evolution model, not the complete building module. A small tent, large tent, and 2x2 grouping can share or transition between resident definitions while differing in capacity, footprint, and merge/split rules. |
| `BuildingProduction` | `ProductionMethod/*.xml`, referenced by `BuildingType` | Mostly already correct, with optional `BuildingProduction/*.xml` only if bindings become richer | Production methods are good immutable definitions. Runtime ownership still needs to move progress, output cart ownership, stockpiling, and blocked-state behavior behind `BuildingProduction`. |
| `BuildingStorage` | `StorageType/*.xml`, referenced by `BuildingType` | Mostly already correct, possibly `BuildingStorage/*.xml` if per-building slot policies outgrow type refs | `StorageType` defines roles/resources/capacity ingredients. Runtime slot state, reservations, accept/get permissions, and save repair should route through `BuildingStorage`. |
| `BuildingFormation` | `FormationType/*.xml`, `UnitType/*.xml`, referenced by fort `BuildingType` | Mostly already correct | Formation definitions are new and clean. Runtime fort ownership, mustering state, dynamic spacing, and legacy 16-soldier assumptions belong behind this facade. |
| `BuildingGraphics` | embedded `BuildingType` `<graphics>` nodes | `Graphics/Buildings/*.xml` referenced by `BuildingType` | Building graphics already has the right class direction, but the XML should be peeled out of building type so graphics can evolve independently. |
| `FigureGraphics` | embedded `FigureType` `<graphics>` nodes | `Graphics/Walkers/*.xml`, `Graphics/Figures/*.xml`, or equivalent path-based folders referenced by `FigureType` | Figure graphics should follow the same node system as building graphics and use real file-path references like `Walkers/<file>`. |
| `ResourceGraphics` | embedded `Resources/*.xml` graphics metadata | `Graphics/Resources/*.xml` for warehouse/cart/icon image groups, referenced by `Resources` | Resource graphics should keep resource identity separate from figure/building draw policy, while still letting resources reference their associated cart, warehouse, and icon graphics. |

## First Extraction Candidates

### 1. Water Access

XML basis: `Mods/Vespasian/WaterAccessType`.

Why first:

- Small definition set: aqueduct, fountain, latrines, reservoir, well.
- Already conceptually separated from building identity.
- Mostly policy/access behavior rather than high-volume resource inventory.

Target shape:

```cpp
building.water_access().has_required_access();
building.water_access().register_provider();
building.water_access().dirty_neighborhood();
```

Initial state source:

- Wrap existing building/map water-access fields and cached road/access flags.
- Do not move save storage yet.

Good first slice:

- Add `BuildingWaterAccess` facade.
- Bind it from `Building` only when `BuildingType` has a water-access definition.
- Move water-access checks currently keyed by building type into the bound facade.

### 2. Culture / Entertainment Access

XML basis: `Mods/Vespasian/CultureModule`.

Why early:

- The XML already names culture/entertainment policies.
- The recent actor/gladiator/venue regressions showed this logic wants a coherent module.
- UI, overlays, venue seeker routing, and animation gates all currently orbit the same concept.

Target shape:

```cpp
building.culture().can_host_show();
building.culture().needs_actor();
building.culture().record_performance();
building.culture().draw_overlay_state();
```

Initial state source:

- Wrap existing venue/show state in the building record.
- Keep venue figure ids and mixed destination ids untouched until relationship types are planned.

Good first slice:

- Add `BuildingCulture` or `BuildingEntertainment` facade.
- Move "has plays/shows", animation gating, and venue provider eligibility behind it.
- Leave route spawning and figure ownership as callers until the relationship model is ready.

### 3. Religion

XML basis: `Mods/Vespasian/Religions` and `Mods/Vespasian/Gods`.

Why early:

- Religion definitions are already XML-owned.
- Runtime concepts are clear: god, tier, access, coverage, blessings/curses, temple module.
- It is less entangled with logistics than storage/distribution.

Target shape:

```cpp
building.religion().god();
building.religion().tier();
building.religion().provides_access_to(house);
building.religion().tick_festival_effects();
```

Initial state source:

- Wrap record-backed temple/grand-temple state where present.
- Keep city-wide god favor state in the city/god system; the building module should expose temple participation, not own the whole religion simulation.

Good first slice:

- Add `BuildingReligion` facade.
- Move temple tier/god/type checks from string/enum helpers to the facade.
- Make culture/religion UI ask `building.religion()` instead of reconstructing temple meaning from type ids.

### 4. Housing

XML basis: `Mods/Vespasian/HousingType`.

Why early but not first:

- Housing XML is mature and already close to module-shaped.
- Housing state is large and important, but the domain is coherent.
- The merge/unmerge tent bug points to value in one owner-bound housing lifecycle.

Target shape:

```cpp
building.housing().population();
building.housing().can_evolve();
building.housing().merge_or_split();
building.housing().consume_goods();
```

Initial state source:

- Wrap existing house fields from the record.
- Do not immediately move population, inventory, or evolution fields out of `building`; first route callers through `BuildingHousing`.

Good first slice:

- Add `BuildingHousing` facade.
- Move house-size/evolution/desirability queries behind it.
- Centralize merge/split decisions so the 2x2 tent oscillation has one owner.

### 5. Production

XML basis: `Mods/Vespasian/ProductionMethod`.

Why medium-risk:

- Production definitions are already externalized.
- Native production has existing object work.
- It touches workers, inputs, outputs, carts, progress, and stockpiling, so it should follow water/culture/religion.

Target shape:

```cpp
building.production().tick();
building.production().has_inputs();
building.production().reserve_output_cart();
building.production().is_blocked();
```

Initial state source:

- Wrap existing production progress, output cart ids, and native production state.
- Keep storage reservations in storage until `BuildingStorage` is ready.

Good first slice:

- Add `BuildingProduction` facade bound to production method definitions.
- Move producer output-cart ownership and stuck-cart cleanup into the facade.
- Keep actual resource slot math delegated to storage.

### 6. Storage

XML basis: `Mods/Vespasian/StorageType`.

Why important but later:

- It already has strong XML definitions.
- It is the heart of many previous regressions: reservations, cart pushers, warehouses, granaries, markets, docks, inputs, outputs, and save repair.
- It should be extracted deliberately after the facade pattern is proven on smaller modules.

Target shape:

```cpp
building.storage().reserve_input(resource, figure);
building.storage().receive(resource, loads, figure);
building.storage().reserve_output(resource, figure);
building.storage().accepted_resources();
```

Initial state source:

- Wrap existing `resources[]`, storage ids, accepted goods, permissions, reservations, stockpiling, and slot state.
- Do not split the save layout until callers stop reading record fields directly.

Good first slice:

- Add `BuildingStorage` facade around current storage runtime helpers.
- Move raw `resources[]` and storage-id callers behind facade methods in touched files.
- Add typed runtime indexes for input/output storage by resource only after mutation points are centralized.

### 7. Formation / Unit Ownership

XML basis: `Mods/Vespasian/FormationType` and `Mods/Vespasian/UnitType`.

Why medium-risk:

- Definitions are clean and new.
- Runtime formation code still has legacy constants and many figure relationships.
- Forts eventually need a bound module, but larger formation sizes depend on figure logical-size/render work.

Target shape:

```cpp
building.formation().muster();
building.formation().spawn_recruit();
building.formation().layout_slots();
```

Initial state source:

- Wrap current fort/formation ids and formation object links.
- Do not change save conversion for legacy formation enums until module bindings are stable.

Good first slice:

- Add `BuildingFormation` facade for forts.
- Move fort-to-formation ownership and dynamic mustering-ground spacing into the facade.

## Recommended Order

1. `BuildingWaterAccess`
2. `BuildingCulture` / `BuildingEntertainment`
3. `BuildingReligion`
4. `BuildingHousing`
5. `BuildingProduction`
6. `BuildingStorage`
7. `BuildingFormation`

This order starts with small, XML-shaped modules and delays the high-risk resource/storage split until the pattern is proven.

## Migration Rule

Each module should first become a behavior facade over existing state. Only after callers route through the facade should the legacy record fields be physically peeled into module-owned state structs.

Do not move save fields first. Move call ownership first, then state ownership.
