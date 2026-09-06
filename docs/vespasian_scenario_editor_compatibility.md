# Built-in scenario editor: Vespasian compatibility report

## Identity and installed support

The main-menu editor is the built-in Julius/Augustus-derived editor, not a separate GitHub executable. Its missing-support prompt points to the Julius map-editor installation instructions. The installed construction-kit files supply the original Caesar III editor strings and graphics (`c3_map.eng`, `C3map.sg2`, climate `.555` data and map panels). The downloaded package is 12,088,266 bytes, MD5 `dd72e34599551560b8f4683636d7d364`; the installer payload is retained outside the checkout. Original `c32.emp` was preserved; the original editor executable was not substituted for Vespasian.

The observed startup crash was an empty invasion-list sidebar dereference. That is repaired. Native placement and MAPX loading also needed fixes: the terminator width and explicit serialized identities for the seven native scenery tools.

## Implemented in this slice

- Startup, map, attributes, models and event dialogs render through the game's own graphics system.
- House model and construction-resource editors resolve current mod definitions rather than fixed upstream building indexes.
- Event metadata exposes the new native actions and conditions. Copies have independent formula/text storage and correct parent identities.
- Scenario overlays survive save/load and settings recomposition, and reset on replacement by another scenario.
- Empire XML supports button icons, encoded names, resource opening prices and hidden routes; zero denarii is a valid cost.
- The demand table distinguishes buying from selling; increases from zero activate the resource. Legacy direction inference waits for the new empire.
- MAPX roundtrip keeps native hut, alternate hut, meeting place, crop, decoration, monument and watchtower identities.

## Remaining compatibility work

| Gap | Why it matters / next work |
| --- | --- |
| Foreign binary MAPX and SVX schemas | Our post-fork versions and records diverge. SVX family/layout identification is implemented, semantic conversion is not. Add producer-aware foreign DTO readers and conversions before calling foreign scenarios/saves supported. MAPX needs the same explicit-family treatment. Never dispatch purely by extension or matching version numbers. |
| Upstream housing model assumptions | Upstream capacities may be per tile; native values are per building. Native water requirements are none/well/fountain, with other services separate. Do not reinterpret an upstream latrine/water value 3 as a native enum. Conversion must reconcile this against mod-authored housing profiles and services. |
| XML dialect compatibility | Native identifiers preserve mod-defined buildings/resources. Some upstream selectors and formulas use fixed legacy enums or different area representations. Native XML roundtrip is not evidence that every upstream exported event/model XML imports identically. Add explicit dialect conversion fixtures. |
| Editor UX and declarative layouts | New capabilities are exposed in the current native editor. Several editor dialogs still use imperative layout code; figure-category combinations use a numeric mask rather than a checkbox composer. Event copy is within the current scenario; safe cross-scenario copying requires remapping building/resource/city/variable identities. These are candidates for the proposed editor UI work. |
| Authoring arbitrary mod content | Native scenery tools cover seven supported identities, not an arbitrary mod brush palette. Terrain/tool assumptions and native image-grid compatibility still constrain new mod-defined editor objects. A data-driven palette is preferable to adding tool enums for every asset. |
| D14 shallow water | Intentionally awaiting the user's terrain/pathing/save-format decision. The editor does not paint the new upstream shallow-water bit. |
| Localization | Source ships English additions with original extracted catalogs supplying original strings. The new labels are not a complete reconciliation of all upstream translated catalogs; CJK and other-language visual acceptance remains open. |
| Platforms | This editor has been tested on Windows x64. Android/iOS/Switch 2 eligibility does not establish editor input, packaging or renderer compatibility on those devices. |

## Fork recommendation for next turn

Keep the editor attached to Vespasian's definition, scenario and save owners while deciding the UI architecture. A standalone copy of upstream's editor would still need bridges for every divergent building/resource/figure/terrain identity and scenario overlay. A dedicated editor front end over a stable authoring API could be useful; duplicating the simulation/schema owners would multiply compatibility work. No editor fork or new repository was created.

The final deployed editor test passes with empty stderr, including all 13 new action dialogs, every variable color, native MAPX scenery/event preservation, and empire XML identity/cost/visibility roundtrip. Test logs and exact acceptance results are linked from [the D08 review record](augustus_sync_d08_plan.md). Gaps above remain visible even when native roundtrip tests pass.
