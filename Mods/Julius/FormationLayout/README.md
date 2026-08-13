# FormationLayout definitions

`FormationLayout` owns tactical figure positions. It is separate from `FormationType`, which owns a fort's unit composition and capacity.

Each active definition requires:

- a unique string `key` used by runtime formation, combat, and UI code;
- a unique `legacy_id` in the original `0..12` save/scenario range;
- exactly 16 ordered `<position x="..." y="..."/>` entries reproducing the original layouts.

Each active definition also owns enemy-army spacing in one of two forms:

- four `<army orientation="0..3">` nodes, each containing exactly seven ordered positions; or
- one `army_from="other_layout"` reference to a layout that authors those four orientation lists.

`army_from` references are direct aliases: the target must exist, remain active, and author its own offsets. Self-references and reference chains are rejected. Army orientation is the cardinal direction index (`formation.orientation / 2`), and the ordered position is selected by the enemy legion index. Indices after the seven authored army positions resolve to `(0, 0)`, preserving the zero-filled tail of the legacy table.

Example:

```xml
<layout key="example" legacy_id="0">
    <!-- 16 tactical positions -->
    <army orientation="0">
        <position x="0" y="0"/>
        <!-- six more army positions -->
    </army>
    <!-- orientations 1, 2, and 3 -->
</layout>
```

Layouts with identical army spacing should use `army_from` instead of copying positions:

```xml
<layout key="example_alias" legacy_id="1" army_from="example">
    <!-- 16 tactical positions -->
</layout>
```

Runtime formations bind the resolved definition directly. Slots after the authored compatibility positions are computed from the resolved `FormationType` footprint. Save records synthesize `legacy_id`; load and scenario creation resolve it once back to the definition.

Definitions layer through the normal Julius -> Augustus -> Vespasian stack. An upper mod only needs to author layouts it changes. Suppression uses an identity-only tombstone:

```xml
<layout key="example" disabled="true" />
```
