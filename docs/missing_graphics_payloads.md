# Missing Graphics Payloads

## 2026-05-04 Colosseum Runtime Fallback

- Source truth: `res/assets/Graphics/monuments.xml` defines Colosseum finished-state graphics under the source `Monuments` assetlist. During packing/extraction those source group names are remapped; the extracted runtime group is `Health_Culture\Colosseum`.
- Missing extracted dependency: `extracted_graphics_sample/Augustus/Graphics/Health_Culture/Colosseum.xml` builds `Coloseum ON` from `Image_0000`, `Image_0014`, and `Coloseum_ON_Layer_04`, but the extracted `Health_Culture/Colosseum` folder only contains `Image_0000` through `Image_0013` plus `Coloseum_ON_Layer_04.png`. `Image_0014` is not present, so loading the full extracted Colosseum group fails before BuildingType graphics can use `Col Glad Fight`, `Col Naumachia`, `Col Imp Games`, or `Col Exec`.
- Runtime fallback used: Augustus and Vespasian now point BuildingType graphics and the build-menu icon at `Health_Culture\Colosseum_Runtime/Colosseum_Base`, a small mod graphics assetlist that references only the resolvable Julius extracted base entry `Health_Culture\Colosseum/Image_0000`.
- Behavior gap: festival-game Colosseum graphics variants are intentionally omitted from Augustus/Vespasian XML until the extracted `Image_0014` dependency, or an equivalent fully resolvable extracted Colosseum ON/base composition, is available.

## 2026-05-04 Native Meeting Hut Central Fallback

- Source truth: `res/assets/Graphics/terrain_maps.xml` defines the central native meeting hut from source group `159` image `112` plus `Native_Meeting_Hut_Central_01_Mask`.
- Extracted runtime mapping: the generated `Terrain_Maps\Native_Meeting_Hut_Central_01` assetlist points that base layer at `UI\Message_Images/Image_0112` and uses `Native_Meeting_Hut_Central_01_Layer_02` as the top alpha layer.
- Missing extracted dependency: `extracted_graphics_sample/Augustus/Graphics/UI/Message_Images.xml` only exposes `HoldGames Banner` and does not expose `Image_0112`; there is also no `Image_0112.png` in the extracted `UI/Message_Images` folder. Loading `Terrain_Maps\Native_Meeting_Hut_Central_01` therefore fails.
- Runtime fallback used: Augustus and Vespasian now use the Julius extracted base-game meeting hut `Aesthetics\Native/Image_0002` for the default/central native meeting hut. Northern and desert variants continue to use the resolvable extracted groups `Terrain_Maps\Native_Meeting_Hut_Northern_01` and `Terrain_Maps\Native_Meeting_Hut_Southern_01`.
- Behavior gap: the central Augustus/Vespasian overlay/mask variant is not represented until `UI\Message_Images/Image_0112` or an equivalent extracted base footprint is available.
