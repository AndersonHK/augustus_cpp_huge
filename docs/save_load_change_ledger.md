# Save/Load Change Ledger

This ledger tracks narrow save/load bridge changes that may need to be walked back, cherry-picked, or audited separately from the larger owned-save-heap work.

## 2026-05-04 - FigureType Building Lookup Uses BuildingType Bridge

- Files:
  - `src/figure/figure_type_registry.cpp`
- Change:
  - `parse_building_type_name()` now asks `building_type_id_bridge_runtime_from_text(attr)` before falling back to the old `building_properties_for_type(type)->event_data.attr` scan.
- Why:
  - FigureType XML venue targets such as `building="hippodrome"` failed with `FigureType venue has an invalid building or show_slot`.
  - The `show_slot="days1"` value was valid; the failure came from `hippodrome` resolving to `BUILDING_NONE`.
  - The old lookup only saw C static building properties. XML-owned BuildingTypes can be valid runtime types without a static `event_data.attr` row.
- Expected effect:
  - FigureType XML can reference XML-owned BuildingTypes through the same runtime text bridge used by the save/load BuildingType table.
  - Existing legacy/static property names still work through the fallback loop.
- Walkback note:
  - Revert the include of `building/building_type_id_bridge.h` and the five-line bridge lookup block in `parse_building_type_name()`.

## 2026-05-04 - Missing XML-Owned Legacy Building Text IDs

- Files:
  - `src/building/building_type_legacy_migration.cpp`
- Change:
  - Added these mappings to `XML_OWNED_BUILDING_TYPE_IDS`:
    - `BUILDING_COLOSSEUM` -> `colosseum`
    - `BUILDING_HIPPODROME` -> `hippodrome`
    - `BUILDING_NATIVE_MONUMENT` -> `native_monument`
- Why:
  - These BuildingTypes have XML definitions but were missing from the legacy enum/text migration table.
  - Save/load bridge code that starts with a legacy enum id needs the XML text id to resolve or reconstruct the correct runtime BuildingType.
  - `BUILDING_PANTHEON` was already mapped; these three were the obvious missing XML-owned legacy ids found during the FigureType lookup failure.
- Expected effect:
  - Legacy enum ids for those buildings can round-trip through the BuildingType text bridge instead of depending on absent static property text.
- Walkback note:
  - Remove only the three added rows from `XML_OWNED_BUILDING_TYPE_IDS`.

## 2026-05-04 - Building Heap Version Bumped After Attribute Contract Change

- Files:
  - `src/building/building_save_heap.cpp`
- Change:
  - Bumped the private `BSHP` heap version from `1` to `2`.
  - Added a specific fatal error for retired v1 heaps: they do not guarantee explicit per-building monument attribute payloads.
- Why:
  - The heap contract changed so every active building has a `MonumentAttributesSaveObject`, independent of legacy monument classification.
  - Leaving the heap version at `1` made older local/dev `0xb6` saves fail later as `building N` missing an attribute payload, which made it look like a specific building was corrupt.
- Expected effect:
  - Current writes use heap v2.
  - Old v1 dev saves fail at the heap header with a clearer compatibility message.
- Walkback note:
  - Revert `kHeapVersion` to `1` only if also reverting the "every active building has monument attributes" invariant.

## 2026-05-04 - Monument Identity Reads Loaded Instance Attributes

- Files:
  - `src/building/monument.cpp`
  - `src/building/building_save_heap.cpp`
  - `src/building/building_type_id_bridge.cpp`
  - `src/building/building_type_id_bridge.h`
- Change:
  - `building_monument_is_monument()` now treats non-empty loaded monument attributes (`phase`, `progress`, `upgrades`, or `secondary_frame`) as monument identity before consulting the old type-level classifier.
  - `building_monument_type_is_monument()` can also detect a loaded type whose live instances carry monument attributes.
  - The 0xb6 heap now refuses to save or load non-empty monument attributes unless the saved BuildingType table entry carries phased construction.
  - Added `building_type_id_bridge_save_id_has_phased_construction()` for that heap validation.
- Why:
  - XML-declared monument instances were still being demoted/voided after load while legacy monuments survived through hardcoded type classification.
  - The saved instance attributes were present, but old monument identity was still being derived from `building_type_registry_has_phased_construction(type) || legacy_monument_type(type)`.
- Expected effect:
  - Loaded instance data is authoritative for whether that building has monument state.
  - If the BuildingType table failed to preserve the construction definition for a saved monument instance, the save/load path now fails loudly instead of silently degrading the building.
- Walkback note:
  - Revert the two monument identity helper changes and remove the heap validation plus `building_type_id_bridge_save_id_has_phased_construction()`.
