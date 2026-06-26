# Build Menu Layout Audit

This maps the building menu button layout from upstream Julius and upstream Augustus, then compares those references with the XML-authored menu state in this repo's bundled `Mods` profiles.

## Sources

- Julius GitHub reference: [`bvschaik/julius` `src/building/menu.c`](https://github.com/bvschaik/julius/blob/016d5254c2b734dac5c56abccac05c0ba74cb934/src/building/menu.c) and [`src/building/menu.h`](https://github.com/bvschaik/julius/blob/016d5254c2b734dac5c56abccac05c0ba74cb934/src/building/menu.h).
- Augustus GitHub reference: [`Keriew/augustus` `src/building/menu.c`](https://github.com/Keriew/augustus/blob/46cb8980b1bf23667c30250a7a7e4c54a54145aa/src/building/menu.c) and [`src/building/menu.h`](https://github.com/Keriew/augustus/blob/46cb8980b1bf23667c30250a7a7e4c54a54145aa/src/building/menu.h).
- XML as-is state: `Mods/{Julius,Augustus,Vespasian}/{BuildingType,BuildingTypeMenu,Tiles}/*.xml`.
- Local XML menu runtime: `src/building/menu.h` and `src/building/menu.cpp`.

## Reading Notes

- Upstream C enum names are normalized from `BUILDING_*` to lowercase ids, for example `BUILDING_MENU_FARMS` becomes `menu_farms`.
- XML entries are shown as `type(order)` using the `<building type="...">` id and `<button order="...">`.
- XML `group="tools"` is accepted by the runtime as an alias for `clear_land`.
- Roads, highways, roadblocks, bridges, and vacant lots are direct-tool or smart-tool special cases. The BuildingType README says they should not declare generated `<button>` nodes, so their absence from XML is not automatically a bug.
- `Tiles/plaza.xml` does declare a `<button>`, and the loader includes `Tiles`, so plaza is included in the XML as-is lists.
- There is no upstream Vespasian repository reference. The Vespasian target below is an inferred review target from the local Vespasian/Augustus-style menu model, not an external source.

## High-Confidence Findings From Pre-Approval Snapshot

1. Fort submenus are present, but fort buildings are not assigned to them.
   - `Mods/*/BuildingTypeMenu/fort.xml` defines the top-level fort submenu expander under `security`.
   - `fort_legionaries`, `fort_javelin`, `fort_mounted`, `fort_archers`, and `fort_swords` currently use `group="security"` in all three mods.
   - Result: forts appear directly under `security`; `forts` is empty.

2. The `gardens` submenu is structurally empty in Augustus and Vespasian.
   - `all_gardens` has a `group="gardens"` self-button.
   - `gardens` and `overgrown_gardens` are currently under `parks`, so opening the `gardens` submenu only cycles back to `all_gardens`.

3. Julius XML currently contains several Augustus-style submenu expanders.
   - Julius upstream has only `farms`, `raw_materials`, `workshops`, `small_temples`, `large_temples`, and `forts` as submenus.
   - Julius XML currently also has `trees`, `paths`, `parks`, `statues`, and `governor_home` under `administration`, with matching submenu groups.

4. Augustus and Vespasian XML are currently identical for menu button layout.
   - Both have 173 button entries across `BuildingType`, `BuildingTypeMenu`, and `Tiles`.
   - Differences from upstream Augustus are therefore shared unless Vespasian intentionally diverges.

## Applied High-Confidence Fixes

These approved fixes have been applied after the pre-approval snapshot below:

- Julius, Augustus, and Vespasian fort children now use `group="forts"` instead of `group="security"`:
  `fort_legionaries`, `fort_javelin`, `fort_mounted`, `fort_archers`, and `fort_swords`.
- Augustus and Vespasian `gardens` and `overgrown_gardens` now use `group="gardens"` instead of `group="parks"`.
- Julius Augustus-style submenu expander buttons were removed from `trees`, `paths`, `parks`, `statues`, and `governor_home`; those menu definitions remain as XML identities/event-data records.
- Julius governor residences, statues, triumphal arch, gardens, plaza, engineer post, shipyard, dock, wharf, and mission post now have Julius-style button groups/orders:
  - `administration`: `forum(10)`, `senate(20)`, `governors_house(30)`, `governors_villa(40)`, `governors_palace(50)`, `small_statue(60)`, `medium_statue(70)`, `large_statue(80)`, `triumphal_arch(90)`
  - `education`: `school(10)`, `academy(20)`, `library(30)`, `mission_post(40)`
  - `engineering`: `gardens(10)`, `plaza(20)`, `engineers_post(30)`, `shipyard(60)`, `dock(70)`, `wharf(80)`
- Julius-only extra fort/tool buttons from the second review pass were hidden by removing their `<button>` entries:
  `fort_archers`, `fort_swords`, `repair_land`, and `clear_trees`.
- Julius `industry` and `security` now match upstream Julius order.
- Augustus and Vespasian now apply the second-pass Augustus fixes in matching layouts:
  - `mission_post` moved to `education`; `city_mint` and `triumphal_arch` moved to `administration`.
  - `small_mausoleum`, `large_mausoleum`, and `nymphaeum` moved to `temples`.
  - `grand_temples`, `water`, `health`, `forts`, `paths`, `industry`, `entertainment`, `security`, and `statues` were reordered to match the approved Augustus target where content overlaps.
  - `palisade_gate` is placed immediately after `palisade` as the local extra in the security order.

The detailed `XML As-Is` sections below reflect the current XML state after both approved high-confidence batches.

## Current Recheck Plan

Status after the approved high-confidence fixes:

1. Refresh upstream references from GitHub `master`: done. Julius is still `016d5254c2b734dac5c56abccac05c0ba74cb934`; Augustus is still `46cb8980b1bf23667c30250a7a7e4c54a54145aa`.
2. Parse current XML as-is state from `BuildingType`, `BuildingTypeMenu`, and `Tiles`: done.
3. Compare current XML to upstream Julius/Augustus hardcoded menu arrays: done.
4. Separate direct placement/order candidates from content additions and direct-tool exclusions: done.
5. Apply approved high-confidence second-batch XML edits: done.
6. Validate XML parse state and Augustus/Vespasian layout parity: done.

## Current XML As-Is (Post-Approval Snapshot)

Julius current XML:

- `administration`: `forum(10)`, `senate(20)`, `governors_house(30)`, `governors_villa(40)`, `governors_palace(50)`, `small_statue(60)`, `medium_statue(70)`, `large_statue(80)`, `triumphal_arch(90)`
- `education`: `school(10)`, `academy(20)`, `library(30)`, `mission_post(40)`
- `engineering`: `gardens(10)`, `plaza(20)`, `engineers_post(30)`, `shipyard(60)`, `dock(70)`, `wharf(80)`
- `entertainment`: `theater(10)`, `amphitheater(20)`, `colosseum(30)`, `hippodrome(40)`, `gladiator_school(50)`, `lion_house(60)`, `actor_colony(70)`, `chariot_maker(80)`
- `farms`: `wheat_farm(10)`, `vegetable_farm(20)`, `fruit_farm(30)`, `olive_farm(40)`, `vines_farm(50)`, `pig_farm(60)`
- `forts`: `fort_legionaries(100)`, `fort_javelin(110)`, `fort_mounted(120)`
- `health`: `barber(10)`, `bathhouse(20)`, `doctor(30)`, `hospital(40)`
- `industry`: `farms(0)`, `raw_materials(100)`, `workshops(200)`, `market(300)`, `granary(400)`, `warehouse(500)`
- `large_temples`: `large_temples(0)`, `large_temple_ceres(10)`, `large_temple_neptune(20)`, `large_temple_mercury(30)`, `large_temple_mars(40)`, `large_temple_venus(50)`
- `raw_materials`: `clay_pit(10)`, `marble_quarry(20)`, `iron_mine(30)`, `timber_yard(40)`
- `security`: `wall(10)`, `tower(20)`, `gatehouse(30)`, `prefecture(40)`, `fort(50)`, `military_academy(60)`, `barracks(70)`
- `small_temples`: `small_temples(0)`, `small_temple_ceres(10)`, `small_temple_neptune(20)`, `small_temple_mercury(30)`, `small_temple_mars(40)`, `small_temple_venus(50)`
- `temples`: `small_temples(0)`, `large_temples(100)`, `oracle(500)`
- `tools`: `clear_land(10)`
- `water`: `draggable_reservoir(5)`, `aqueduct(20)`, `fountain(30)`, `well(40)`
- `workshops`: `wine_workshop(10)`, `oil_workshop(20)`, `weapons_workshop(30)`, `furniture_workshop(40)`, `pottery_workshop(50)`

Augustus current XML:

- `administration`: `all_gardens(0)`, `trees(100)`, `paths(200)`, `parks(300)`, `statues(400)`, `governor_home(500)`, `plaza(600)`, `forum(700)`, `senate(800)`, `city_mint(900)`, `triumphal_arch(1000)`
- `education`: `school(10)`, `academy(20)`, `library(30)`, `mission_post(40)`
- `engineering`: `engineers_post(10)`, `workcamp(20)`, `architect_guild(30)`, `shipyard(30)`, `dock(40)`, `lighthouse(40)`, `wharf(40)`
- `entertainment`: `theater(10)`, `amphitheater(20)`, `arena(30)`, `tavern(40)`, `colosseum(50)`, `hippodrome(60)`, `actor_colony(70)`, `gladiator_school(80)`, `lion_house(90)`, `chariot_maker(100)`
- `farms`: `wheat_farm(10)`, `vegetable_farm(20)`, `fruit_farm(30)`, `olive_farm(40)`, `vines_farm(50)`, `pig_farm(60)`
- `forts`: `fort_legionaries(100)`, `fort_javelin(110)`, `fort_mounted(120)`, `fort_swords(130)`, `fort_archers(140)`
- `gardens`: `all_gardens(0)`, `gardens(20)`, `overgrown_gardens(290)`
- `gov_res`: `governors_house(0)`, `governors_villa(100)`, `governors_palace(200)`
- `grand_temples`: `pantheon(10)`, `grand_temple_ceres(20)`, `grand_temple_neptune(30)`, `grand_temple_mercury(40)`, `grand_temple_mars(50)`, `grand_temple_venus(60)`
- `health`: `barber(10)`, `bathhouse(20)`, `doctor(30)`, `hospital(40)`, `latrines(50)`
- `industry`: `farms(0)`, `raw_materials(100)`, `workshops(200)`, `market(300)`, `granary(400)`, `warehouse(500)`, `depot(600)`, `caravanserai(700)`
- `large_temples`: `large_temples(0)`, `large_temple_ceres(10)`, `large_temple_neptune(20)`, `large_temple_mercury(30)`, `large_temple_mars(40)`, `large_temple_venus(50)`
- `parks`: `dolphin_fountain(80)`, `grand_garden(90)`, `pavilion(100)`, `hedge_dark(150)`, `hedge_light(160)`, `looped_garden_wall(170)`, `colonnade(180)`, `roofed_garden_wall(220)`, `garden_wall_gate(230)`, `hedge_gate_dark(240)`, `hedge_gate_light(250)`, `panelled_garden_wall(260)`, `panelled_garden_gate(270)`, `looped_garden_gate(280)`, `small_pond(700)`, `large_pond(800)`
- `paths`: `paths(0)`, `garden_path(10)`, `pine_path(20)`, `fir_path(30)`, `oak_path(40)`, `elm_path(50)`, `fig_path(60)`, `plum_path(70)`, `palm_path(80)`, `date_path(90)`
- `raw_materials`: `clay_pit(10)`, `marble_quarry(20)`, `iron_mine(30)`, `timber_yard(40)`, `gold_mine(50)`, `stone_quarry(60)`, `sand_pit(70)`
- `security`: `palisade(10)`, `palisade_gate(20)`, `watchtower(30)`, `wall(40)`, `tower(50)`, `gatehouse(60)`, `prefecture(70)`, `fort(80)`, `barracks(90)`, `mess_hall(100)`, `armoury(110)`, `military_academy(120)`
- `shrines`: `shrines(0)`, `shrine_ceres(10)`, `shrine_neptune(20)`, `shrine_mercury(30)`, `shrine_mars(40)`, `shrine_venus(50)`
- `small_temples`: `small_temples(0)`, `small_temple_ceres(10)`, `small_temple_neptune(20)`, `small_temple_mercury(30)`, `small_temple_mars(40)`, `small_temple_venus(50)`
- `statues`: `small_statue(10)`, `goddess_statue(20)`, `senator_statue(30)`, `gladiator_statue(40)`, `decorative_column(50)`, `medium_statue(60)`, `legion_statue(70)`, `obelisk(80)`, `large_statue(90)`, `horse_statue(100)`
- `temples`: `small_temples(0)`, `large_temples(100)`, `grand_temples(200)`, `shrines(300)`, `lararium(400)`, `oracle(500)`, `small_mausoleum(600)`, `large_mausoleum(610)`, `nymphaeum(620)`
- `tools`: `clear_land(10)`, `repair_land(20)`, `clear_trees(30)`
- `trees`: `trees(0)`, `pine_tree(10)`, `fir_tree(20)`, `oak_tree(30)`, `elm_tree(40)`, `fig_tree(50)`, `plum_tree(60)`, `palm_tree(70)`, `date_tree(80)`
- `water`: `draggable_reservoir(5)`, `aqueduct(20)`, `fountain(30)`, `well(40)`
- `workshops`: `wine_workshop(10)`, `oil_workshop(20)`, `weapons_workshop(30)`, `furniture_workshop(40)`, `pottery_workshop(50)`, `brickworks(70)`, `concrete_maker(80)`

Vespasian current XML:

- Same button layout as Augustus current XML.

## Second-Batch High-Confidence Candidates (Applied)

These were direct mismatches against the latest upstream menu arrays, excluding direct-tool omissions such as roads/vacant lots/bridges and excluding purely extra local content where the parent group is not obviously wrong. They were approved and applied in the second implementation pass.

### Julius Candidates

1. Remove or hide Julius-only extra fort buttons unless intentionally diverging from Julius.
   - Upstream Julius `forts`: `fort_legionaries`, `fort_javelin`, `fort_mounted`.
   - Before second batch Julius XML `forts`: `fort_legionaries`, `fort_javelin`, `fort_mounted`, `fort_archers`, `fort_swords`.

2. Remove or hide Julius `repair_land` and `clear_trees` buttons unless intentionally diverging from Julius.
   - Upstream Julius `clear_land`: `clear_land`.
   - Before second batch Julius XML `tools`: `clear_land`, `repair_land`, `clear_trees`.

3. Reorder Julius `industry` to match upstream.
   - Upstream: `farms`, `raw_materials`, `workshops`, `market`, `granary`, `warehouse`.
   - Before second batch XML: `farms`, `granary`, `market`, `warehouse`, `raw_materials`, `workshops`.

4. Reorder Julius `security` to match upstream.
   - Upstream: `wall`, `tower`, `gatehouse`, `prefecture`, `fort`, `military_academy`, `barracks`.
   - Before second batch XML: `prefecture`, `wall`, `gatehouse`, `tower`, `barracks`, `military_academy`, `fort`.

### Augustus Candidates

1. Move `mission_post` from `administration` to `education`, and reorder education as upstream.
   - Upstream `education`: `school`, `academy`, `library`, `mission_post`.
   - Before second batch XML `education`: `school`, `library`, `academy`; `mission_post` is under `administration`.

2. Move `city_mint` from `gov_res` to main `administration`.
   - Upstream `administration` contains `city_mint`; upstream `gov_res` contains only governor residences.
   - Before second batch XML has `city_mint` in `gov_res`.

3. Move `triumphal_arch` from `statues` to main `administration`.
   - Upstream `administration` contains `triumphal_arch`; upstream `statues` does not.
   - Before second batch XML has `triumphal_arch` in `statues`.

4. Move `small_mausoleum`, `large_mausoleum`, and `nymphaeum` from `parks` to `temples`.
   - Upstream `temples` contains all three.
   - Before second batch XML has all three in `parks`.

5. Reorder `grand_temples` so `pantheon` is first.
   - Upstream: `pantheon`, then the five named grand temples.
   - Before second batch XML: five named grand temples, then `pantheon`.

6. Reorder direct order-only groups to match upstream where content already matches or has only approved local extras:
   - `water`: `draggable_reservoir`, `aqueduct`, `fountain`, `well`
   - `health`: `barber`, `bathhouse`, `doctor`, `hospital`, `latrines`
   - `forts`: `fort_legionaries`, `fort_javelin`, `fort_mounted`, `fort_swords`/`fort_auxilia_infantry`, `fort_archers`
   - `paths`: `paths`, `garden_path`, `pine_path`, `fir_path`, `oak_path`, `elm_path`, `fig_path`, `plum_path`, `palm_path`, `date_path`
   - `industry`: `farms`, `raw_materials`, `workshops`, `market`, `granary`, `warehouse`, `cart_depot`, `caravanserai`
   - `entertainment`: `theater`, `amphitheater`, `arena`, `tavern`, `colosseum`, `hippodrome`, `actor_colony`, `gladiator_school`, `lion_house`, `chariot_maker`
   - `security`: `palisade`, `palisade_gate`, `watchtower`, `wall`, `tower`, `gatehouse`, `prefecture`, `fort`, `barracks`, `mess_hall`, `armoury`, `military_academy`.
   - `statues`: `small_statue`, `goddess_statue`, `senator_statue`, `gladiator_statue`, `decorative_column`, `medium_statue`, `legion_statue`, `obelisk`, `large_statue`, `horse_statue`.

### Vespasian Candidates

Vespasian has no upstream repository baseline, so these are high-confidence only if Vespasian should continue tracking Augustus menu structure where it shares content:

- Apply the same Augustus candidate set above to Vespasian.
- Leave decorative park-only extras such as the garden/hedge/panel/looped gates, `dolphin_fountain`, and `grand_garden` in their existing local order.

## Not High Confidence In This Pass

- Missing `road`, `highway`, `roadblock`, `house_vacant_lot`, `low_bridge`, `ship_bridge`, and `highway_station` XML buttons are not candidates here because local XML/runtime docs describe roads, bridges, vacant lots, and smart tools as direct-tool/sidebar-owned special cases.
- Extra decorative park content not present in upstream Augustus is not treated as wrong by itself; only its parent grouping/order should be reviewed.

## Julius GitHub Reference

Main buttons:

- `vacant_house`: `house_vacant_lot`
- `clear_land`: `clear_land`
- `road`: `road`
- `water`: `draggable_reservoir`, `aqueduct`, `fountain`, `well`
- `health`: `barber`, `bathhouse`, `doctor`, `hospital`
- `temples`: `menu_small_temples`, `menu_large_temples`, `oracle`
- `education`: `school`, `academy`, `library`, `mission_post`
- `entertainment`: `theater`, `amphitheater`, `colosseum`, `hippodrome`, `gladiator_school`, `lion_house`, `actor_colony`, `chariot_maker`
- `administration`: `forum`, `senate`, `governors_house`, `governors_villa`, `governors_palace`, `small_statue`, `medium_statue`, `large_statue`, `triumphal_arch`
- `engineering`: `gardens`, `plaza`, `engineers_post`, `low_bridge`, `ship_bridge`, `shipyard`, `dock`, `wharf`
- `security`: `wall`, `tower`, `gatehouse`, `prefecture`, `fort`, `military_academy`, `barracks`
- `industry`: `menu_farms`, `menu_raw_materials`, `menu_workshops`, `market`, `granary`, `warehouse`

Sub-buttons:

- `farms`: `wheat_farm`, `vegetable_farm`, `fruit_farm`, `olive_farm`, `vines_farm`, `pig_farm`
- `raw_materials`: `clay_pit`, `marble_quarry`, `iron_mine`, `timber_yard`
- `workshops`: `wine_workshop`, `oil_workshop`, `weapons_workshop`, `furniture_workshop`, `pottery_workshop`
- `small_temples`: `menu_small_temples`, `small_temple_ceres`, `small_temple_neptune`, `small_temple_mercury`, `small_temple_mars`, `small_temple_venus`
- `large_temples`: `menu_large_temples`, `large_temple_ceres`, `large_temple_neptune`, `large_temple_mercury`, `large_temple_mars`, `large_temple_venus`
- `forts`: `fort_legionaries`, `fort_javelin`, `fort_mounted`

## Julius XML As-Is (Pre-Approval Snapshot)

Main and submenu groups:

- `administration`: `forum(10)`, `senate(20)`, `mission_post(90)`, `trees(100)`, `paths(200)`, `parks(300)`, `statues(400)`, `governor_home(500)`, `plaza(600)`
- `education`: `school(10)`, `academy(20)`, `library(30)`
- `engineering`: `engineers_post(10)`, `shipyard(30)`, `dock(40)`, `wharf(40)`
- `entertainment`: `theater(10)`, `amphitheater(20)`, `colosseum(30)`, `hippodrome(40)`, `gladiator_school(50)`, `lion_house(60)`, `actor_colony(70)`, `chariot_maker(80)`
- `farms`: `wheat_farm(10)`, `vegetable_farm(20)`, `fruit_farm(30)`, `olive_farm(40)`, `vines_farm(50)`, `pig_farm(60)`
- `gov_res`: `governors_house(0)`, `governors_villa(100)`, `governors_palace(200)`
- `health`: `barber(10)`, `bathhouse(20)`, `doctor(30)`, `hospital(40)`
- `industry`: `farms(0)`, `granary(20)`, `market(30)`, `warehouse(30)`, `raw_materials(100)`, `workshops(200)`
- `large_temples`: `large_temples(0)`, `large_temple_ceres(10)`, `large_temple_neptune(20)`, `large_temple_mercury(30)`, `large_temple_mars(40)`, `large_temple_venus(50)`
- `parks`: `gardens(20)`
- `paths`: `paths(0)`
- `raw_materials`: `clay_pit(10)`, `marble_quarry(20)`, `iron_mine(30)`, `timber_yard(40)`
- `security`: `prefecture(10)`, `wall(20)`, `gatehouse(30)`, `tower(40)`, `barracks(50)`, `military_academy(60)`, `fort_legionaries(100)`, `fort_javelin(110)`, `fort_mounted(120)`, `fort_archers(130)`, `fort_swords(140)`, `fort(400)`
- `small_temples`: `small_temples(0)`, `small_temple_ceres(10)`, `small_temple_neptune(20)`, `small_temple_mercury(30)`, `small_temple_mars(40)`, `small_temple_venus(50)`
- `statues`: `small_statue(10)`, `medium_statue(20)`, `large_statue(30)`, `triumphal_arch(40)`
- `temples`: `small_temples(0)`, `large_temples(100)`, `oracle(500)`
- `tools`: `clear_land(10)`, `repair_land(20)`, `clear_trees(30)`
- `trees`: `trees(0)`
- `water`: `draggable_reservoir(5)`, `aqueduct(20)`, `fountain(30)`, `well(40)`
- `workshops`: `wine_workshop(10)`, `oil_workshop(20)`, `weapons_workshop(30)`, `furniture_workshop(40)`, `pottery_workshop(50)`

Julius review differences:

- Fort children should be reviewed: upstream has only `fort_legionaries`, `fort_javelin`, and `fort_mounted` under `forts`; XML has five fort variants directly under `security`.
- `mission_post` is under `administration` in XML but under `education` upstream.
- `trees`, `paths`, `parks`, `statues`, and `governor_home` are Augustus-style submenu expanders not present in Julius upstream.
- `repair_land` and `clear_trees` are extra XML clear/tools entries relative to Julius upstream.
- `gardens`, `plaza`, `low_bridge`, and `ship_bridge` are not all represented in `engineering` XML; plaza is under `administration`, and bridges are direct-tool exclusions.

## Augustus GitHub Reference

Main buttons:

- `vacant_house`: `house_vacant_lot`
- `clear_land`: `clear_land`, `repair_land`
- `road`: `road`, `highway`, `roadblock`
- `water`: `draggable_reservoir`, `aqueduct`, `fountain`, `well`
- `health`: `barber`, `bathhouse`, `doctor`, `hospital`, `latrines`
- `temples`: `menu_small_temples`, `menu_large_temples`, `menu_grand_temples`, `menu_shrines`, `lararium`, `oracle`, `small_mausoleum`, `large_mausoleum`, `nymphaeum`
- `education`: `school`, `academy`, `library`, `mission_post`
- `entertainment`: `theater`, `amphitheater`, `arena`, `tavern`, `colosseum`, `hippodrome`, `actor_colony`, `gladiator_school`, `lion_house`, `chariot_maker`
- `administration`: `menu_gardens`, `menu_trees`, `menu_paths`, `menu_parks`, `menu_statues`, `menu_gov_res`, `plaza`, `forum`, `senate`, `city_mint`, `triumphal_arch`
- `engineering`: `engineers_post`, `low_bridge`, `ship_bridge`, `shipyard`, `dock`, `wharf`, `workcamp`, `architect_guild`, `lighthouse`, `highway_station`
- `security`: `palisade`, `watchtower`, `wall`, `tower`, `gatehouse`, `prefecture`, `menu_fort`, `barracks`, `mess_hall`, `armoury`, `military_academy`
- `industry`: `menu_farms`, `menu_raw_materials`, `menu_workshops`, `market`, `granary`, `warehouse`, `depot`, `caravanserai`

Sub-buttons:

- `farms`: `wheat_farm`, `vegetable_farm`, `fruit_farm`, `olive_farm`, `vines_farm`, `pig_farm`
- `raw_materials`: `clay_pit`, `marble_quarry`, `iron_mine`, `timber_yard`, `gold_mine`, `stone_quarry`, `sand_pit`
- `workshops`: `wine_workshop`, `oil_workshop`, `weapons_workshop`, `furniture_workshop`, `pottery_workshop`, `brickworks`, `concrete_maker`
- `small_temples`: `menu_small_temples`, `small_temple_ceres`, `small_temple_neptune`, `small_temple_mercury`, `small_temple_mars`, `small_temple_venus`
- `large_temples`: `menu_large_temples`, `large_temple_ceres`, `large_temple_neptune`, `large_temple_mercury`, `large_temple_mars`, `large_temple_venus`
- `forts`: `fort_legionaries`, `fort_javelin`, `fort_mounted`, `fort_auxilia_infantry`, `fort_archers`
- `parks`: `colonnade`, `hedge_light`, `hedge_dark`, `looped_garden_wall`, `roofed_garden_wall`, `panelled_garden_wall`, `pavilion_blue`, `small_pond`, `large_pond`
- `trees`: `menu_trees`, `pine_tree`, `fir_tree`, `oak_tree`, `elm_tree`, `fig_tree`, `plum_tree`, `palm_tree`, `date_tree`
- `paths`: `menu_paths`, `garden_path`, `pine_path`, `fir_path`, `oak_path`, `elm_path`, `fig_path`, `plum_path`, `palm_path`, `date_path`
- `grand_temples`: `pantheon`, `grand_temple_ceres`, `grand_temple_neptune`, `grand_temple_mercury`, `grand_temple_mars`, `grand_temple_venus`
- `statues`: `small_statue`, `goddess_statue`, `senator_statue`, `gladiator_statue`, `decorative_column`, `medium_statue`, `legion_statue`, `obelisk`, `large_statue`, `horse_statue`
- `gov_res`: `governors_house`, `governors_villa`, `governors_palace`
- `shrines`: `menu_shrines`, `shrine_ceres`, `shrine_neptune`, `shrine_mercury`, `shrine_mars`, `shrine_venus`
- `gardens`: `menu_gardens`, `gardens`, `overgrown_gardens`

## Augustus XML As-Is (Pre-Approval Snapshot)

Main and submenu groups:

- `administration`: `all_gardens(0)`, `senate(10)`, `forum(20)`, `mission_post(90)`, `trees(100)`, `paths(200)`, `parks(300)`, `statues(400)`, `governor_home(500)`, `plaza(600)`
- `education`: `school(20)`, `library(30)`, `academy(40)`
- `engineering`: `engineers_post(10)`, `workcamp(20)`, `architect_guild(30)`, `shipyard(30)`, `dock(40)`, `lighthouse(40)`, `wharf(40)`
- `entertainment`: `colosseum(0)`, `theater(10)`, `actor_colony(20)`, `amphitheater(30)`, `gladiator_school(40)`, `lion_house(50)`, `arena(60)`, `tavern(70)`, `hippodrome(100)`, `chariot_maker(200)`
- `farms`: `wheat_farm(10)`, `vegetable_farm(20)`, `fruit_farm(30)`, `olive_farm(40)`, `vines_farm(50)`, `pig_farm(60)`
- `gardens`: `all_gardens(0)`
- `gov_res`: `governors_house(0)`, `governors_villa(100)`, `governors_palace(200)`, `city_mint(300)`
- `grand_temples`: `grand_temple_ceres(10)`, `grand_temple_neptune(20)`, `grand_temple_mercury(30)`, `grand_temple_mars(40)`, `grand_temple_venus(50)`, `pantheon(60)`
- `health`: `doctor(10)`, `hospital(20)`, `bathhouse(30)`, `barber(40)`, `latrines(50)`
- `industry`: `farms(0)`, `granary(20)`, `market(30)`, `warehouse(30)`, `cart_depot(40)`, `caravanserai(50)`, `raw_materials(100)`, `workshops(200)`
- `large_temples`: `large_temples(0)`, `large_temple_ceres(10)`, `large_temple_neptune(20)`, `large_temple_mercury(30)`, `large_temple_mars(40)`, `large_temple_venus(50)`
- `parks`: `gardens(20)`, `dolphin_fountain(80)`, `grand_garden(90)`, `pavilion(100)`, `hedge_dark(150)`, `hedge_light(160)`, `looped_garden_wall(170)`, `colonnade(180)`, `nymphaeum(190)`, `small_mausoleum(200)`, `large_mausoleum(210)`, `roofed_garden_wall(220)`, `garden_wall_gate(230)`, `hedge_gate_dark(240)`, `hedge_gate_light(250)`, `panelled_garden_wall(260)`, `panelled_garden_gate(270)`, `looped_garden_gate(280)`, `overgrown_gardens(290)`, `small_pond(700)`, `large_pond(800)`
- `paths`: `paths(0)`, `pine_path(10)`, `fir_path(20)`, `oak_path(30)`, `elm_path(40)`, `fig_path(50)`, `plum_path(60)`, `palm_path(70)`, `date_path(80)`, `garden_path(90)`
- `raw_materials`: `clay_pit(10)`, `marble_quarry(20)`, `iron_mine(30)`, `timber_yard(40)`, `gold_mine(50)`, `stone_quarry(60)`, `sand_pit(70)`
- `security`: `prefecture(10)`, `wall(20)`, `watchtower(20)`, `gatehouse(30)`, `tower(40)`, `armoury(50)`, `barracks(50)`, `military_academy(60)`, `mess_hall(70)`, `fort_legionaries(100)`, `fort_javelin(110)`, `fort_mounted(120)`, `fort_archers(130)`, `fort_swords(140)`, `palisade(150)`, `palisade_gate(160)`, `fort(400)`
- `shrines`: `shrines(0)`, `shrine_ceres(10)`, `shrine_neptune(20)`, `shrine_mercury(30)`, `shrine_mars(40)`, `shrine_venus(50)`
- `small_temples`: `small_temples(0)`, `small_temple_ceres(10)`, `small_temple_neptune(20)`, `small_temple_mercury(30)`, `small_temple_mars(40)`, `small_temple_venus(50)`
- `statues`: `small_statue(10)`, `medium_statue(20)`, `large_statue(30)`, `triumphal_arch(40)`, `goddess_statue(50)`, `senator_statue(60)`, `obelisk(70)`, `horse_statue(80)`, `legion_statue(90)`, `decorative_column(100)`, `gladiator_statue(110)`
- `temples`: `small_temples(0)`, `large_temples(100)`, `grand_temples(200)`, `shrines(300)`, `lararium(400)`, `oracle(500)`
- `tools`: `clear_land(10)`, `repair_land(20)`, `clear_trees(30)`
- `trees`: `trees(0)`, `pine_tree(10)`, `fir_tree(20)`, `oak_tree(30)`, `elm_tree(40)`, `fig_tree(50)`, `plum_tree(60)`, `palm_tree(70)`, `date_tree(80)`
- `water`: `draggable_reservoir(5)`, `aqueduct(20)`, `well(30)`, `fountain(40)`
- `workshops`: `wine_workshop(10)`, `oil_workshop(20)`, `weapons_workshop(30)`, `furniture_workshop(40)`, `pottery_workshop(50)`, `brickworks(70)`, `concrete_maker(80)`

Augustus review differences:

- `forts` is empty; fort children are directly under `security`.
- `gardens` only contains `all_gardens`; `gardens` and `overgrown_gardens` are under `parks`.
- `mission_post` is under `administration`, not `education`.
- `city_mint` is under `gov_res`, not main `administration`.
- `small_mausoleum`, `large_mausoleum`, and `nymphaeum` are under `parks`, not top-level `temples`.
- `grand_temples` puts `pantheon` last; upstream puts `pantheon` first.
- `water`, `health`, `entertainment`, `industry`, `parks`, `paths`, `security`, and `statues` have order drift from upstream.
- XML has extra authored entries not in upstream hardcoded Augustus: `clear_trees`, `palisade_gate`, garden/hedge/panel/looped gates, `dolphin_fountain`, and `grand_garden`.

## Vespasian Intended Review Target

This is the inferred Vespasian target I would use for manual approval, because Vespasian has no separate upstream hardcoded menu. It keeps the Vespasian XML content set, but applies the structural submenu intent visible in `src/building/menu.h` and `SUBMENU_EXPANDERS`.

Candidate groups:

- `administration`: `all_gardens`, `trees`, `paths`, `parks`, `statues`, `governor_home`, `plaza`, `forum`, `senate`, `city_mint`, `triumphal_arch`
- `education`: `school`, `academy`, `library`, `mission_post`
- `engineering`: `engineers_post`, `workcamp`, `architect_guild`, `shipyard`, `dock`, `wharf`, `lighthouse`
- `entertainment`: `theater`, `amphitheater`, `arena`, `tavern`, `colosseum`, `hippodrome`, `actor_colony`, `gladiator_school`, `lion_house`, `chariot_maker`
- `farms`: `wheat_farm`, `vegetable_farm`, `fruit_farm`, `olive_farm`, `vines_farm`, `pig_farm`
- `gardens`: `all_gardens`, `gardens`, `overgrown_gardens`
- `gov_res`: `governors_house`, `governors_villa`, `governors_palace`
- `grand_temples`: `pantheon`, `grand_temple_ceres`, `grand_temple_neptune`, `grand_temple_mercury`, `grand_temple_mars`, `grand_temple_venus`
- `health`: `barber`, `bathhouse`, `doctor`, `hospital`, `latrines`
- `industry`: `farms`, `raw_materials`, `workshops`, `market`, `granary`, `warehouse`, `cart_depot`, `caravanserai`
- `large_temples`: `large_temples`, `large_temple_ceres`, `large_temple_neptune`, `large_temple_mercury`, `large_temple_mars`, `large_temple_venus`
- `parks`: `colonnade`, `hedge_light`, `hedge_dark`, `looped_garden_wall`, `roofed_garden_wall`, `panelled_garden_wall`, `pavilion`, `garden_wall_gate`, `hedge_gate_dark`, `hedge_gate_light`, `panelled_garden_gate`, `looped_garden_gate`, `dolphin_fountain`, `grand_garden`, `small_pond`, `large_pond`
- `paths`: `paths`, `garden_path`, `pine_path`, `fir_path`, `oak_path`, `elm_path`, `fig_path`, `plum_path`, `palm_path`, `date_path`
- `raw_materials`: `clay_pit`, `marble_quarry`, `iron_mine`, `timber_yard`, `gold_mine`, `stone_quarry`, `sand_pit`
- `security`: `palisade`, `palisade_gate`, `watchtower`, `wall`, `tower`, `gatehouse`, `prefecture`, `fort`, `barracks`, `mess_hall`, `armoury`, `military_academy`
- `forts`: `fort_legionaries`, `fort_javelin`, `fort_mounted`, `fort_archers`, `fort_swords`
- `shrines`: `shrines`, `shrine_ceres`, `shrine_neptune`, `shrine_mercury`, `shrine_mars`, `shrine_venus`
- `small_temples`: `small_temples`, `small_temple_ceres`, `small_temple_neptune`, `small_temple_mercury`, `small_temple_mars`, `small_temple_venus`
- `statues`: `small_statue`, `goddess_statue`, `senator_statue`, `gladiator_statue`, `decorative_column`, `medium_statue`, `legion_statue`, `obelisk`, `large_statue`, `horse_statue`
- `temples`: `small_temples`, `large_temples`, `grand_temples`, `shrines`, `lararium`, `oracle`, `small_mausoleum`, `large_mausoleum`, `nymphaeum`
- `tools`: `clear_land`, `repair_land`, `clear_trees`
- `trees`: `trees`, `pine_tree`, `fir_tree`, `oak_tree`, `elm_tree`, `fig_tree`, `plum_tree`, `palm_tree`, `date_tree`
- `water`: `draggable_reservoir`, `aqueduct`, `fountain`, `well`
- `workshops`: `wine_workshop`, `oil_workshop`, `weapons_workshop`, `furniture_workshop`, `pottery_workshop`, `brickworks`, `concrete_maker`

Open Vespasian approval questions:

- Should `small_mausoleum`, `large_mausoleum`, and `nymphaeum` follow upstream Augustus under `temples`, or remain decorative/park entries?
- Should `city_mint` be a main `administration` button, or remain grouped under `gov_res`?
- Should `triumphal_arch` stay in `statues`, move to main `administration`, or appear in both?
- Should `mission_post` follow upstream under `education`, or remain in `administration`?
- Should `fort_swords` remain as a Vespasian/Augustus XML option even though upstream Augustus reference uses `fort_auxilia_infantry` instead?

## Vespasian XML As-Is (Pre-Approval Snapshot)

Main and submenu groups:

- `administration`: `all_gardens(0)`, `senate(10)`, `forum(20)`, `mission_post(90)`, `trees(100)`, `paths(200)`, `parks(300)`, `statues(400)`, `governor_home(500)`, `plaza(600)`
- `education`: `school(20)`, `library(30)`, `academy(40)`
- `engineering`: `engineers_post(10)`, `workcamp(20)`, `architect_guild(30)`, `shipyard(30)`, `dock(40)`, `lighthouse(40)`, `wharf(40)`
- `entertainment`: `colosseum(0)`, `theater(10)`, `actor_colony(20)`, `amphitheater(30)`, `gladiator_school(40)`, `lion_house(50)`, `arena(60)`, `tavern(70)`, `hippodrome(100)`, `chariot_maker(200)`
- `farms`: `wheat_farm(10)`, `vegetable_farm(20)`, `fruit_farm(30)`, `olive_farm(40)`, `vines_farm(50)`, `pig_farm(60)`
- `gardens`: `all_gardens(0)`
- `gov_res`: `governors_house(0)`, `governors_villa(100)`, `governors_palace(200)`, `city_mint(300)`
- `grand_temples`: `grand_temple_ceres(10)`, `grand_temple_neptune(20)`, `grand_temple_mercury(30)`, `grand_temple_mars(40)`, `grand_temple_venus(50)`, `pantheon(60)`
- `health`: `doctor(10)`, `hospital(20)`, `bathhouse(30)`, `barber(40)`, `latrines(50)`
- `industry`: `farms(0)`, `granary(20)`, `market(30)`, `warehouse(30)`, `cart_depot(40)`, `caravanserai(50)`, `raw_materials(100)`, `workshops(200)`
- `large_temples`: `large_temples(0)`, `large_temple_ceres(10)`, `large_temple_neptune(20)`, `large_temple_mercury(30)`, `large_temple_mars(40)`, `large_temple_venus(50)`
- `parks`: `gardens(20)`, `dolphin_fountain(80)`, `grand_garden(90)`, `pavilion(100)`, `hedge_dark(150)`, `hedge_light(160)`, `looped_garden_wall(170)`, `colonnade(180)`, `nymphaeum(190)`, `small_mausoleum(200)`, `large_mausoleum(210)`, `roofed_garden_wall(220)`, `garden_wall_gate(230)`, `hedge_gate_dark(240)`, `hedge_gate_light(250)`, `panelled_garden_wall(260)`, `panelled_garden_gate(270)`, `looped_garden_gate(280)`, `overgrown_gardens(290)`, `small_pond(700)`, `large_pond(800)`
- `paths`: `paths(0)`, `pine_path(10)`, `fir_path(20)`, `oak_path(30)`, `elm_path(40)`, `fig_path(50)`, `plum_path(60)`, `palm_path(70)`, `date_path(80)`, `garden_path(90)`
- `raw_materials`: `clay_pit(10)`, `marble_quarry(20)`, `iron_mine(30)`, `timber_yard(40)`, `gold_mine(50)`, `stone_quarry(60)`, `sand_pit(70)`
- `security`: `prefecture(10)`, `wall(20)`, `watchtower(20)`, `gatehouse(30)`, `tower(40)`, `armoury(50)`, `barracks(50)`, `military_academy(60)`, `mess_hall(70)`, `fort_legionaries(100)`, `fort_javelin(110)`, `fort_mounted(120)`, `fort_archers(130)`, `fort_swords(140)`, `palisade(150)`, `palisade_gate(160)`, `fort(400)`
- `shrines`: `shrines(0)`, `shrine_ceres(10)`, `shrine_neptune(20)`, `shrine_mercury(30)`, `shrine_mars(40)`, `shrine_venus(50)`
- `small_temples`: `small_temples(0)`, `small_temple_ceres(10)`, `small_temple_neptune(20)`, `small_temple_mercury(30)`, `small_temple_mars(40)`, `small_temple_venus(50)`
- `statues`: `small_statue(10)`, `medium_statue(20)`, `large_statue(30)`, `triumphal_arch(40)`, `goddess_statue(50)`, `senator_statue(60)`, `obelisk(70)`, `horse_statue(80)`, `legion_statue(90)`, `decorative_column(100)`, `gladiator_statue(110)`
- `temples`: `small_temples(0)`, `large_temples(100)`, `grand_temples(200)`, `shrines(300)`, `lararium(400)`, `oracle(500)`
- `tools`: `clear_land(10)`, `repair_land(20)`, `clear_trees(30)`
- `trees`: `trees(0)`, `pine_tree(10)`, `fir_tree(20)`, `oak_tree(30)`, `elm_tree(40)`, `fig_tree(50)`, `plum_tree(60)`, `palm_tree(70)`, `date_tree(80)`
- `water`: `draggable_reservoir(5)`, `aqueduct(20)`, `well(30)`, `fountain(40)`
- `workshops`: `wine_workshop(10)`, `oil_workshop(20)`, `weapons_workshop(30)`, `furniture_workshop(40)`, `pottery_workshop(50)`, `brickworks(70)`, `concrete_maker(80)`

Vespasian review differences:

- After the approved high-confidence passes, Vespasian has the same XML button layout as Augustus.
- Remaining possible review is limited to intentional direct-tool omissions and decorative park extras that do not have an upstream Augustus one-to-one reference.
