# Save/Load Runtime Bridges

This document follows save data after the `.svv` file-piece layer has already been read. For the byte-level piece order, allocation sizes, compression flags, and writer/loader table, start with `docs/save_data_organization.md`. This note focuses on the bridge systems that turn save-local data back into runtime objects, runtime structs, module state, and compatibility records. For the water access simulation that consumes the resolved water access type table, see `docs/water_access_runtime.md`.

Current live-save version in this checkout is `SAVE_GAME_CURRENT_VERSION = 0xb9`. Current scenario version is `SCENARIO_CURRENT_VERSION = 22`.

Long-term migration direction: hardcoded legacy-id bridges should eventually move into mod-owned XML declarations, described in `docs/mod_owned_compatibility_bridge_plan.md`. That future bridge belongs to startup/save-load boundaries; normal runtime should continue to consume resolved objects and string-owned definitions.

## Load Timeline

The live-save entry points are in `src/game/file_io.cpp`.

1. `game_file_io_read_saved_game()` or `game_file_io_read_save_game_from_buffer()` peeks the save version and, for saves after `SAVE_GAME_LAST_STATIC_RESOURCES`, the resource version.
2. Unsupported newer save/resource versions are rejected before subsystem state is loaded.
3. `resource_set_mapping(resource_version)` installs the legacy resource-id map that old saves need before any resource-bearing payload is decoded.
4. `init_savegame_data(save_version)` allocates the exact `file_piece` sequence for that version.
5. `savegame_read_from_file()` or `savegame_read_from_buffer()` fills those pieces in on-disk order.
6. `savegame_load_from_state(&savegame_data.state, save_version)` loads `resource_id_bridge_save_table_load_state()` after scenario core data and before scenario requests, figures, city data, buildings, or other resource-bearing payloads are decoded.
7. `clear_savegame_pieces()` frees the temporary file-piece buffers after the data has been consumed.

`savegame_load_from_state()` does not finish all runtime rebinding by itself. The larger game-load path in `src/game/file.c` calls `building_runtime_initialize_city_graphics_cache()` and `figure_runtime_initialize_city()` after the world has finished loading. That second phase currently rebuilds lazy C++ `Building`/`building_runtime` objects, native graphics bindings, native storage/production objects, and native figure controllers over the compatibility arrays restored by `savegame_load_from_state()`.

This is transitional. The target save bridge is not "runtime mutates the save record forever." The target is:

1. Load reads exactly one save record shape for the save version.
2. That one record is converted into exactly one current `Building` object plus its owned runtime/module state.
3. If the record cannot produce a valid object, the bridge discards that building, logs the saved id/type/position/module context, and prevents the bad record from entering runtime.
4. Once object hydration finishes, the read save records are temporary compatibility input and should be discarded; normal runtime should hold only valid building objects and their modules.
5. Runtime functions may assume any building fetched from runtime is a complete, valid object in a known state.
6. Save reconstructs the current save record shape from the runtime object plus every required module state.
7. If a savable runtime object lacks required module state, the save bridge should still finish the save with the safest partial/default payload it can, then report an error with object id/type/module context after the save has been flushed.

`Building.id` is special identity data and remains the stable bridge key. Ordinary peeled fields should move out of the runtime record when their module owns them; they should then be appended back into the save record only by the save bridge.

The important live-save order is:

1. Scenario settings and embedded scenario data.
2. `resource_id_bridge_save_table_load_state()`.
3. Map grids.
4. Figure records, routes, and formations.
5. City globals.
6. `building_type_id_bridge_save_table_load_state()`.
7. `water_access_type_id_bridge_save_table_load_state()`.
8. `building_load_state()`.
9. `building_resource_state_load()` for keyed resource state in current saves.
10. View/time/random and legacy model data.
11. XML model overrides through `building_type_registry_apply_model_overrides()`.
12. Resource and production-rate data.
13. Road service history and local workforce allocations.
14. Empire, messages, building lists, building storage, deliveries, visited-building data, and old-version migrations.

This order matters. Resource-bearing scenario requests, figures, city arrays, building records, trade routes, prices, and empire objects use save-local resource ids, so the Resource bridge must load before those payloads are decoded. Building records store save-local building type ids, so the BuildingType bridge must load before `building_state_load_from_buffer()` reads any building. The current bridge stages per-building runtime state in an id-keyed load packet while compatibility arrays are read; the target is to consume that packet while creating the building object, then drop the save-record input instead of keeping it as runtime truth.

## Version Gates

Save versions are append/order gates, not schema negotiation. `src/game/save_version.h` is the authority for version constants and the rule: if the persisted read/write order, size, or behavior changes, `SAVE_GAME_CURRENT_VERSION` must increase and a descriptive `SAVE_GAME_LAST_*` boundary must be added.

Recent runtime-bridge gates:

| Gate | Last save without | Loader effect |
| --- | --- | --- |
| `SAVE_GAME_LAST_NO_ROAD_SERVICE_HISTORY = 0xaf` | Road service history dynamic payload | `map_road_service_history_load_state()` starts from zeroed history. |
| `SAVE_GAME_LAST_NO_RELIGION_ROAD_SERVICE_HISTORY = 0xb0` | Appended religion service effects | Older payloads read only the effects present in that version; religion grids remain zero. |
| `SAVE_GAME_LAST_NO_LOCAL_WORKFORCE = 0xb1` | Local workforce allocation payload | `building_local_workforce_load_state()` clears allocations and rebuilds counters from zero. |
| `SAVE_GAME_LAST_NO_ENTERTAINMENT_ROAD_SERVICE_HISTORY = 0xb2` | Appended entertainment service effects | Entertainment grids remain zero for older saves. |
| `SAVE_GAME_LAST_LEGACY_ENTERTAINMENT_SHOW_HALF_DAYS = 0xb3` | Active-day entertainment show counters | `read_type_data()` doubles legacy half-day counters on load. |
| `SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE = 0xb4` | Building type save table | Bridge synthesizes a legacy table from enum/text migration data. |
| `SAVE_GAME_LAST_NO_MARKET_ROAD_SERVICE_HISTORY = 0xb5` | Appended market service effect | Market goods history remains zero for older saves. |
| `SAVE_GAME_LAST_NO_WATER_ACCESS_TYPE_TABLE = 0xb7` | Water access type save table | Bridge synthesizes the shared legacy water access text ids and resolves them through the active mod's XML numeric ids. |
| `SAVE_GAME_LAST_NO_RESOURCE_TYPE_TABLE = 0xb8` | Resource save table | Bridge synthesizes the legacy raw resource-id maps and resolves them through active `Resources` XML text ids. |
| `SAVE_GAME_LAST_NO_KEYED_RESOURCE_STATE = 0xb8` | Keyed city/building resource state | Current saves use table-backed resource save ids in dynamic city/building resource payloads; old flat arrays remain load-only compatibility. |

Other broad gates still shape the bridge path:

- `SAVE_GAME_LAST_STATIC_VERSION` decides whether arrays such as figures and buildings include a per-record buffer-size prefix.
- `SAVE_GAME_LAST_STATIC_RESOURCES` controls resource mapping and several old fixed-size resource payloads.
- `SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA` controls saved model data, formula data, production rates, and related scenario migrations.
- `SAVE_GAME_LAST_MONUMENT_TYPE_DATA` separates old monument fields embedded in type data from newer building-state monument fields.
- `SAVE_GAME_LAST_NO_DELIVERIES_VERSION` and `SAVE_GAME_LAST_STATIC_MONUMENT_DELIVERIES_VERSION` gate monument delivery allocation and its element-size prefix.

## Resource Save Bridge

`src/game/resource_id_bridge.cpp` owns resource identity conversion between persisted save ids and active `Mods/<Mod>/Resources/*.xml` definitions.

There are three identities in play:

| Identity | Owner | Meaning |
| --- | --- | --- |
| Text id | Resource XML | Stable semantic id such as `wheat`, `gold`, or `bricks`. |
| Runtime id | Current process | `resource_type` slot from the active Resource XML `slot` attribute. |
| Save id | One save file | Numeric id used inside saved resource arrays and resource-bearing records. |

New saves write a dynamic table with format version `1`, entry count, and each save id/text id pair. Save id `0` remains `none`. Runtime code still uses the active numeric `resource_type`; save payloads use the save-local numeric id, and the table is the only place where that numeric id is tied back to the stable XML text id.

On load, `resource_id_bridge_save_table_load_state()` resolves saved text ids against the active Resource XML before scenario requests, figures, city data, buildings, trade routes, prices, empire objects, or other resource-bearing payloads are decoded. If the save lacks the table, the bridge synthesizes the old raw-id maps for `RESOURCE_ORIGINAL_VERSION` through `RESOURCE_HAS_NEW_MONUMENT_ELEMENTS` and then resolves those legacy ids through the same XML text ids.

For saves after `SAVE_GAME_LAST_NO_KEYED_RESOURCE_STATE`, city resource arrays and per-building resource state are dynamic keyed payloads. Each keyed entry stores a resource save id and the associated value; it does not embed resource strings inline and does not trust the active runtime slot as stable disk identity. For saves at or before that gate, old flat resource arrays are decoded through the bridge and copied into current runtime slots.

The legacy raw resource id tables belong only to this bridge. Runtime resource facts such as special status, food/storable/inventory flags, trade defaults, XML attribute names, and graphics are read from Resource XML.

## BuildingType Save Bridge

`src/building/building_type_id_bridge.cpp` owns the memory bridge between persisted building type identity and the active runtime registry.

There are three identities in play:

| Identity | Owner | Meaning |
| --- | --- | --- |
| Text id | BuildingType XML/legacy migration | Stable semantic id such as `grand_temple_mars` or `warehouse_space`. |
| Runtime id | Current process | `building_type` runtime slot assigned from active BuildingType XML definitions and compatibility migration. |
| Save id | One save file | Compact 16-bit id used inside saved building records and rubble/original-type fields. |

### Runtime Table

`building_type_id_bridge_reset_for_runtime()` clears bridge state and rebuilds the runtime table from the active process. The intended live source is active BuildingType XML in `g_building_types`; legacy numeric ids are compatibility-only input for old saves that did not persist a BuildingType table.

Dynamic BuildingTypes such as `BUILDING_THEATER` and `BUILDING_WELL` are process-local runtime ids. They must not be trusted as stable persisted values. The bridge persists their text ids, then resolves them back to whatever runtime id the current registry assigned.

Native housing BuildingTypes follow the same text-id rule. Vespasian, Augustus, and Julius define the full house chain from `house_small_tent` through `house_luxury_palace`, while a housing compatibility layer maps those text ids to legacy `house_level` values only for old runtime fields that still need levels. Whole-building residential capacity is BuildingType XML data and is not persisted in the save table. The bridge skips seeding a legacy house enum slot when an active XML BuildingType owns the same text id so active XML ids win at runtime.

For old saves without a BuildingType save table, unambiguous raw legacy house ids are translated through the housing compatibility table instead of through the old enum/event-attr identity. After each building record is read, the loader normalizes the legacy house level plus the saved footprint to the active native BuildingType, so occupied small tents, merged 2x2 houses, and larger villa/palace footprints resolve to matching XML ids. Empty legacy vacant lots stay on the vacant-lot compatibility type until migrants occupy them.

### Save Table On Write

`savegame_save_to_state()` calls:

1. `building_type_id_bridge_prepare_new_save_table()`.
2. `building_type_id_bridge_save_table_save_state(state->building_type_table)`.
3. `building_save_state(...)`.

The prepare step creates a save-local table from the current runtime table. Save id `0` remains none/empty. Each real entry records:

- save id
- text id

The current table format is `SAVE_TABLE_VERSION = 1`. It stores text identity only. BuildingType graphics, construction, labor, storage, and production facts come from active XML/runtime definitions, not from the save table.

### Save Table On Load

`building_type_id_bridge_save_table_load_state(buf, has_save_table)` loads the active save-local id table.

If `has_save_table` is false, or the dynamic table cannot be read, the bridge calls the legacy-table path. That path reconstructs save ids from old raw enum values and the migration table.

For each version-1 entry, the loader resolves runtime identity by saved text id against the current runtime table. XML-owned text ids without a current definition are marked missing. `building_type_id_bridge_save_id_is_missing()` lets the building loader drop those building instances safely.

If a building record references a save id that is not present in the loaded table, the bridge treats that id as a legacy raw enum and recovers its canonical text id through `building_type_legacy_migration_text_id_for_enum()`. This preserves old raw-id records such as `BUILDING_ORACLE = 98` without overriding explicit save-table entries.

Preview loading does not mutate the active bridge. It reads enough city/scenario/building/map data to render minimap info, while full record-to-object hydration remains part of the live save bridge path.

## WaterAccessType Save Bridge

`src/building/water_access_type_id_bridge.cpp` mirrors the BuildingType bridge for XML-owned water access identity.

Water access definitions live in `Mods/<Mod>/WaterAccessType/*.xml`. Each file declares a stable text id plus a numeric id from `0` through `7`; the runtime stores coverage and provider state as `uint8_t` masks where bit `1 << number_id` is the active access type. The registry rejects missing definitions, duplicate text ids, duplicate numeric ids, ids outside `0..7`, and more than eight active types.

The shared bundled ids are:

| Text id | Numeric id | Notes |
| --- | --- | --- |
| `well` | `0` | Present in Julius, Augustus, and Vespasian. |
| `fountain` | `1` | Present in Julius, Augustus, and Vespasian. |
| `reservoir` | `2` | Present in Julius, Augustus, and Vespasian. |
| `aqueduct` | `3` | Present in Julius, Augustus, and Vespasian. |
| `latrines` | `4` | Present only in Augustus and Vespasian. Julius must not reference it. |

New saves write a dynamic table with format version `1`, entry count, and each save id/text id pair. Save id `0` remains none/empty. On load, the bridge resolves saved text ids against the active mod registry, so save-local ids do not depend on the current XML numeric ids.

Old saves without the table synthesize the legacy shared mapping through text ids:

| Legacy raw id | Text id |
| --- | --- |
| `1` | `well` |
| `2` | `fountain` |
| `3` | `reservoir` |
| `4` | `aqueduct` |
| `5` | `latrines` |

The current building record still persists compatibility mirror bytes such as `has_water_access`, `has_well_access`, and `has_latrines_access`. Gameplay and graphics decisions should use typed water masks and BuildingType water rules; those bytes remain save/load mirrors for old code paths while the migration continues.

## Building Records

`building_load_state()` in `src/building/building.cpp` still owns the compatibility `array(building)` allocation. This is the current migration surface, not the desired runtime ownership model.

1. It reads the per-building buffer size for saves after `SAVE_GAME_LAST_STATIC_VERSION`.
2. It computes the number of building records from the buffer size.
3. It calls `array_init(data.buildings, BUILDING_ARRAY_SIZE_STEP, initialize_new_building, building_in_use)` and `array_expand(data.buildings, buildings_to_load)`.
4. It clears `data.first_of_type` and `data.last_of_type`.
5. Each record is read through `building_state_load_from_buffer()`.
6. A live building that cannot resolve to a BuildingType is quarantined by the save bridge: the mismatch is logged, figures that point at the unsupported building are removed, map tile ownership is cleared, and the record becomes `BUILDING_STATE_UNUSED` / `BUILDING_NONE`.
7. Non-unused buildings are inserted back into per-type linked lists through `fill_adjacent_types()`.
8. Current saves then load `building_resource_state`, which replaces compatibility flat resource mirrors with table-backed resource values.
9. The array is trimmed to the highest id still in use.

`building_state_load_from_buffer()` in `src/building/state.cpp` is where save-local type identity becomes compatibility runtime input:

- The saved type field is read as `uint16_t saved_building_type`.
- `building_type_id_bridge_save_id_is_missing(saved_building_type)` checks whether the save id refers to a missing XML-owned type.
- `building_type_id_bridge_runtime_from_save_id(saved_building_type)` writes the current runtime `building_type` into `b->type`.
- Native BuildingType graphics stage the saved legacy graphics variant byte into the per-building load packet as `BuildingGraphicsState`. This is a field peel, not a save-format change: load copies `Record.variant` into `BuildingGraphicsState::variant`, and save copies the state value back into the record byte.
- If a live record resolves to no runtime type, the loader reports the save/load mismatch with the saved id, runtime id, text ids, position, state, and link fields, then removes that record as unsupported content. This is required for cross-mod compatibility, such as loading an Augustus save in Julius where Augustus-only building ids have no active definition.
- After load, the runtime invariant is still strict: any live `Building` object asked for `Building::type()` must already have a valid BuildingType definition. The bridge quarantine exists so unsupported saved content never reaches that runtime path as a glitched live record.

After identity is resolved, the loader fills legacy struct fields exactly in save order:

- coordinates, grid offset, size, state, faction, road network, creation sequence, workers, population, tax/storage values, risks, sentiment, figure slots, formation id, storage id, accepted goods, tourism fields, native meeting center fields, workforce counters, and resource arrays
- `subtype` union data, including warehouse-space resource ids and fort figure type migration
- type-specific union data through `read_type_data()`
- `building.monument` fields such as upgrades, progress, and phase
- dynamic resource arrays and old resource remaps through `resource_remap()`

`read_type_data()` is a compatibility choke point. It always consumes the old type-data byte count for the save version: 42 bytes at or before `SAVE_GAME_LAST_STATIC_RESOURCES`, 26 bytes after that, except for the old caravanserai offset bug. It decodes house service fields, warehouse/granary/depot/dock supplier fields, entertainment show counters, monument compatibility fields, and rubble/original-type fields. Rubble and warehouse-space original building types also go through `building_type_id_bridge_runtime_from_save_id()`.

The building record is still the current serialized compatibility shape, but it should not be treated as the final runtime ownership model. As modules are peeled out, the runtime-side object should lose those fields and the save-side building record should be reconstructed only at the save/load bridge. The bridge must know which modules are required for each savable building type. If a building cannot supply the state its type declares, the bridge should write conservative/default values for the missing module slice, finish the save, and then report the error so the player gets a partial save instead of losing the whole save attempt.

### Native Graphics Variants

The save record still contains the legacy per-building variant byte for compatibility, but runtime code now reaches it through `BuildingGraphicsState` and `Building::graphics().variant()` instead of a field on `struct building`. Older systems such as rotated pavilions and decorative variants keep using the same graphics-owned path. Native BuildingType graphics also use the same state byte when the resolved graphics target has `<options selection="stable_variant">`.

The runtime rule is:

1. Conditional `<variant>` targets are resolved from live building state.
2. If the winning target has options, `Building::graphics().variant() % option_count` chooses the option.
3. The saved byte is preserved as module state. Any modulo behavior belongs to graphics option selection, not to the save/load bridge.

The saved byte is staged after the whole record has been read, then consumed by `building_runtime_initialize_city_graphics_cache()` while the `building_runtime` and its `BuildingGraphicsState` are materialized. That order matters because future conditional graphics targets may depend on fields such as water access, desirability, resources, or figure slots. The save/load bridge still writes and reads the legacy byte in the flat building payload, but combines it with runtime module state instead of keeping it in the runtime record.

## BuildingType Runtime Fan-Out

`src/building/building_type_registry.cpp` owns the process-wide BuildingType registry:

- `g_building_types` is a `std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX>`.
- `BuildingType` stores identity, model, foundation, build-button, sound, event data, flags, water access, graphics, construction, labor, spawn groups, storage references, and production method references.
- `BUILDING_THEATER` and `BUILDING_WELL` are dynamic runtime ids refreshed from text ids by `refresh_known_building_type_ids()`.
- Spawn groups can now carry probability gates. Residential beggar and patrician walkers are ordinary BuildingType spawns that use per-house figure slots plus current XML probability data rather than save-backed global counters.

Saved model data and XML model overrides have a deliberate order:

1. `model_reset()` restores the legacy model table to defaults.
2. Saves after `SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA` call `model_load_model_data(state->building_model_data)`.
3. `building_type_registry_apply_model_overrides()` reapplies active XML/registry model facts over the loaded legacy table.

That final fan-out writes BuildingType data back into legacy systems:

- `model_get_building(definition->type())` receives cost, desirability, and labor values.
- `building_properties_apply_xml_model_size()` updates legacy footprint size fields.
- `building_properties_apply_xml_event_attr()` updates legacy event attrs.
- `building_properties_apply_xml_sound_id()` updates city sound fields.
- XML flags update legacy property fields such as fireproof, desirability-range drawing, and Venus bonus.

C callers read registry data through `src/building/building_type_api.h`, while legacy code that still expects `building_properties` or `model_building` sees the bridged values above.

Post-load, `building_runtime_initialize_city_graphics_cache()` clears C++ building runtime state and rebuilds it over live legacy buildings:

- `building_runtime_reset()` clears `g_runtime_instances` and resets native production/storage runtimes.
- `building_local_workforce_initialize_city()` clamps saved allocation data and rebuilds house/workplace workforce counters.
- Each in-use, mothballed, or created building receives a `building_runtime` wrapper through `building_runtime_impl::get_or_create_instance(b)`.
- Each wrapper owns a `Building` object over the restored record plus the current `BuildingType` definition for `b->type`.
- `set_building_graphic()` precomputes native image-group bindings.
- `storage_runtime_initialize_city()` creates native `StorageSlot` objects for definitions with native storage.
- `production_runtime_initialize_city()` creates native `Production` objects for definitions with native production methods.

## Monuments

Monuments have legacy-shaped runtime construction truth, with XML data bridged into that shape when needed.

`src/building/monument.cpp` still contains hardcoded `monument_type` resource tables for older monuments such as Pantheon, Colosseum, Hippodrome, mausoleums, Nymphaeum, and City Mint. These hardcoded tables win first so retained legacy monuments keep their original behavior.

XML-declared phased monuments keep their old monument membership through a text-id vector populated from the active `BuildingType` registry, then project their current XML construction data into a local `monument_type` cache in `monument.cpp`:

- `monument_type_for(type)` returns a hardcoded legacy table first.
- Retained legacy monument ids seed the mutable text-id vector, and active XML definitions with construction nodes append their canonical ids at runtime.
- Runtime callers resolve text ids through `building_type_id_bridge_text_from_runtime()`.
- Save-load callers can resolve text ids directly from the save table through `building_type_id_bridge_text_from_save_id()`, which lets `read_type_data()` choose the monument branch before relying only on runtime enum identity.
- If no hardcoded table exists, `xml_monument_type(type)` reads `BuildingType::ConstructionDefinition` through the registry facade and caches `phases` plus `[phase][resource]` requirements.
- Phase counts, resource requests, delivery targeting, and load-time resource clamping all use this legacy-shaped bridge.

This keeps construction definitions out of the save table. Save-loaded building records keep their persisted instance state, while current XML supplies the monument phase requirements that the legacy code path expects.

Per-instance monument state lives in each loaded `building` record:

- `b->monument.upgrades`
- `b->monument.progress`
- `b->monument.phase`
- resource counts in `b->resources`
- normal building state such as mothballing, workers, road access, and multipart links

`normalize_monument_phase_after_load()` turns zero or terminal numeric phase values into `MONUMENT_FINISHED`. For unfinished monuments, `building_state_load_from_buffer()` clamps saved resource counts down to the requirements returned by `building_monument_resources_needed_for_monument_type()`, so an old or changed definition cannot leave impossible surplus requirements on the instance.

Runtime graphics refresh follows the same authority split:

- Registry-backed phased monuments call `Building::refresh_graphic()` and render from cached XML payload slices.
- Legacy monuments redraw through `map_building_tiles_add(..., building_image_get(b), TERRAIN_BUILDING)`.
- Runtime-owned XML buildings use `map_image` only for neutral tile bookkeeping; the actual footprint/top/animation comes from `ImageGroupPayload`, not from a saved or reconstructed integer image group id.
- Live city rendering asks `Building::draw_footprint(...)`, `draw_top(...)`, and `draw_animation(...)` for those payload slices. Direct `building *` use in draw code should be treated as a remaining legacy boundary, not as the preferred runtime API.

Monument deliveries are separate dynamic state. `building_monument_delivery_load_state()` allocates `array(monument_delivery)`, reads an optional element-size prefix for newer saves, remaps resource ids, and preserves only active delivery rows. Old saves without a deliveries piece call `building_monument_initialize_deliveries()` and start with an empty delivery array.

## Figure Runtime Bridge

Figures persist as legacy `array(figure)` records. `figure_load_state()` resets `figure_runtime`, reads the saved array record size when present, allocates the figure array, and fills each `figure` with legacy state.

Native FigureType runtime state is not persisted as C++ objects. It is rebuilt after load:

- `figure_runtime_initialize_city()` clears `g_runtime_entries`, walks live figures, and calls the internal binder.
- The binder finds the `FigureTypeDefinition` for `f->type`.
- Saved figures generally lack exact XML profile bindings, so `infer_profile_id()` recovers a profile from legacy fields.
- Labor seekers infer `acquisition` or `validation` from `collecting_item_id`.
- Priests infer god profiles from the owner building type.
- Residential walkers infer required house-spawn profiles: patricians use `house_roamer`, and beggars use `unemployment_wanderer`.
- Entertainment walkers infer school/venue/service profiles from action state and type.
- If no inferred profile is available, the definition's default profile is used.
- New runtime-created walkers use `figure_runtime_create_profiled()` and `figure_runtime_bind_profile()` so they do not depend on inference.

The beggar `unemployment_wanderer` profile currently uses `stand_still`. That bridge accepts old action state zero and the temporary roaming action state used by the first native conversion; in the roaming case it removes route state and resumes stationary lifetime countdown from the loaded `wait_ticks`.

Residential spawn policy itself is not persisted. Houses keep their legacy figure slots in the building record; new XML-owned residential spawns use `figure_id4` as the one-active-per-house slot. When an older save has an active beggar or patrician with only `building_id` set, the runtime spawn slot check can adopt that live owned figure before deciding whether to create another one.

This is the same bridge pattern as buildings: legacy records are persisted, while runtime controllers are reconstructed from legacy fields plus current XML definitions.

## Road Service History

`src/map/road_service_history.cpp` owns pathing-only telemetry for smart service walkers.

Runtime storage is:

- `std::array<grid_u32, ROAD_SERVICE_EFFECT_MAX> g_history`
- `uint32_t g_last_visit_stamp`

The local-workforce payload is dynamic and starts with:

1. `kSaveFormatVersion = 1`
2. effect count
3. one full `GRID_SIZE * GRID_SIZE` `uint32_t` grid per saved effect id

Effect ids are append-only. Removed meanings must stay reserved so old save columns do not shift. The loader receives five gates from `savegame_load_from_state()`:

- `has_saved_state`
- `has_religion_effects`
- `has_entertainment_effects`
- `has_market_effects`
- `has_medicine_tax_effects`

Missing payloads or missing appended effect groups start from zeroed history. Unsupported payloads reset to zero. Future extra grids are consumed so the ordinal payload remains aligned. Vespasian's doctor, surgeon, and tax collector smart-service effects append after `market_goods`, so older payloads simply leave those grids zeroed. `update_last_visit_stamp_from_history()` rebuilds the current generation counter from loaded grids.

Road service history does not provide coverage or mutate buildings. It only gives smart service walkers recency data for path selection.

## Local Workforce Allocations

`src/building/local_workforce.cpp` owns local workforce allocation state.

Runtime storage is `std::vector<WorkforceAllocation> g_allocations`, where each record stores:

- workplace building id
- house building id
- worker count

The save payload is dynamic and starts with:

1. `kSaveFormatVersion = 1`
2. record count
3. `workplace_id`, `house_id`, `workers` for each record

`building_local_workforce_load_state()` clears allocations first. If the save has no payload, an invalid payload, or an unsupported format version, it clamps/rebuilds from an empty vector. If a payload exists, it reads positive allocations, then calls:

- `clamp_allocation_table()` to remove allocations for dead/non-workforce buildings and trim over-assigned workplaces/houses.
- `rebuild_counters_from_allocations()` to rewrite legacy `building` fields such as `local_workforce_assigned`, `local_workforce_unemployed`, and `local_workforce_validation_delay`.
- `g_preserve_allocations_on_next_city_initialize = 1`, so the later `building_runtime_initialize_city_graphics_cache()` call preserves the loaded vector when it calls `building_local_workforce_initialize_city()`.

The allocation vector is runtime-owned, but its endpoints are legacy building ids. All validity after load is therefore checked against the restored `array(building)` and current BuildingType labor policy.

## Building Storage, Lists, Routes, And Formations

These payloads are still legacy subsystem state, but they depend on building/figure ids restored earlier:

- `building_list_load_state()` restores small, large, and burning building lists after city messages/traders and before building storage. The lists refer back to restored building ids and legacy building categories.
- `building_storage_load_state()` restores storage records after buildings and lists. Building records carry `storage_id`; storage records keep detailed warehouse/granary-like inventory state.
- `figure_route_load_state()` restores route-figure and route-path payloads after figure records. Figure records carry current route/path ids and movement state; routes give walkers their packed path directions.
- `formations_load_state()` restores military formations after route data. Buildings and figures reference formations through ids such as `formation_id`.

The bridge rule here is simple: save data restores id-addressed legacy arrays first, then later runtime systems use those ids to rebind wrappers or behavior.

## Model, Production, Scenario, Message, And Empire Data

Several other save-backed structures fan out into runtime state after load:

- `model_load_model_data()` reads the saved `model_building[BUILDING_TYPE_MAX]` table; `building_type_registry_apply_model_overrides()` then reapplies active BuildingType XML model data over it.
- `production_rates_load()` restores dynamic production-rate settings after `resource_init()`, applying them to production methods while resource mappings are already active.
- `scenario_events_load_state()` restores events, conditions, actions, and formulas according to the embedded scenario version. Old saves run migration helpers such as `scenario_events_migrate_to_formulas()`, resolved-name migration, grid-slice migration, and buys/sells migration.
- `scenario_load_state()` and related scenario loaders restore settings, requests, invasions, demand/price changes, allowed buildings, and custom variables before map/runtime post-load work.
- `city_message_load_state()`, `custom_messages_load_state()`, `custom_media_load_state()`, and `message_media_text_blob_load_state()` restore message queues and custom media/text payloads.
- `empire_load_state()`, `empire_load_custom_map()`, `empire_city_load_state()`, and `empire_object_load()` restore empire runtime state. Older resource mappings can trigger fish/meat production recomputation after custom empire data loads.

These systems are not all C++ wrapper bridges, but they follow the same compatibility shape: read versioned legacy payloads into subsystem-owned memory, then run migrations or registry/resource fan-out after the prerequisites are loaded.

## Save-Side Mirror

On save, `savegame_save_to_state()` writes the runtime bridge data before the records that depend on it:

1. File/resource/scenario versions.
2. Scenario settings and mod metadata.
3. `resource_id_bridge_save_table_save_state()`.
4. `building_type_id_bridge_prepare_new_save_table()` and `building_type_id_bridge_save_table_save_state()`.
5. `water_access_type_id_bridge_prepare_new_save_table()` and `water_access_type_id_bridge_save_table_save_state()`.
6. Map, figure, route, formation, city, and building records.
7. `building_resource_state_save()` for table-backed per-building resource values.
8. Legacy model data.
9. Scenario, message, empire, list, storage, route, delivery, visited-building, production-rate, road-history, and workforce payloads.

Building records write `building_type_id_bridge_save_id_from_runtime(b->type)` instead of raw runtime enum ids. Type-data rubble/original-type fields use the same bridge. The resource type table is written before resource-bearing records so saved resource ids resolve through XML text ids instead of current runtime slots. Current city and building resource values are saved as resource save ids plus amounts/states, while old flat arrays remain only as compatibility mirrors for older save versions. The water access type table is written immediately after the BuildingType table so future persisted water-type references can resolve through text ids before current XML numeric ids are used. This is what lets the save survive dynamic BuildingType ids, XML-owned resource definitions, XML-owned water access type ids, and old enum migrations without treating the current process enum as stable disk data.
