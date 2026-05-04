# Save/Load Runtime Bridges

This document follows save data after the `.svv` file-piece layer has already been read. For the byte-level piece order, allocation sizes, compression flags, and writer/loader table, start with `docs/save_data_organization.md`. This note focuses on the bridge systems that turn save-local data back into runtime objects, legacy structs, and C++ wrappers.

Current live-save version in this checkout is `SAVE_GAME_CURRENT_VERSION = 0xb6`. Current scenario version is `SCENARIO_CURRENT_VERSION = 22`.

## Load Timeline

The live-save entry points are in `src/game/file_io.cpp`.

1. `game_file_io_read_saved_game()` or `game_file_io_read_save_game_from_buffer()` peeks the save version and, for saves after `SAVE_GAME_LAST_STATIC_RESOURCES`, the resource version.
2. Unsupported newer save/resource versions are rejected before subsystem state is loaded.
3. `resource_set_mapping(resource_version)` installs the resource-id map that old saves need before any resource-bearing payload is decoded.
4. `init_savegame_data(save_version)` allocates the exact `file_piece` sequence for that version.
5. `savegame_read_from_file()` or `savegame_read_from_buffer()` fills those pieces in on-disk order.
6. `savegame_load_from_state(&savegame_data.state, save_version)` fans the buffers into subsystem-owned runtime data.
7. `clear_savegame_pieces()` frees the temporary file-piece buffers after the data has been consumed.

`savegame_load_from_state()` does not finish all runtime rebinding by itself. The larger game-load path in `src/game/file.c` calls `building_runtime_initialize_city_graphics_cache()` and `figure_runtime_initialize_city()` after the world has finished loading. That second phase rebuilds lazy C++ runtime wrappers, native graphics bindings, native storage/production objects, and native figure controllers over the legacy arrays restored by `savegame_load_from_state()`.

The important live-save order is:

1. Scenario settings and embedded scenario data.
2. Map grids.
3. Figure records, routes, and formations.
4. City globals.
5. `building_type_id_bridge_save_table_load_state()`.
6. `building_load_state()`.
7. View/time/random and legacy model data.
8. XML model overrides through `building_type_registry_apply_model_overrides()`.
9. Resource and production-rate data.
10. Road service history and local workforce allocations.
11. Empire, messages, building lists, building storage, deliveries, visited-building data, and old-version migrations.

This order matters. The `0xb6` building heap and legacy building records store save-local building type ids, so the BuildingType bridge must load before `building_load_state()` materializes any building. Runtime wrappers load later because they need the restored legacy arrays, linked lists, and registry definitions to be stable.

## Version Gates

Save versions are append/order gates, not schema negotiation. `src/game/save_version.h` is the authority for version constants and the rule: if the persisted read/write order, size, or behavior changes, `SAVE_GAME_CURRENT_VERSION` must increase and a descriptive `SAVE_GAME_LAST_*` boundary must be added.

Recent runtime-bridge gates:

| Gate | Last save without | Loader effect |
| --- | --- | --- |
| `SAVE_GAME_LAST_NO_ROAD_SERVICE_HISTORY = 0xaf` | Road service history dynamic payload | `map_road_service_history_load_state()` starts from zeroed history. |
| `SAVE_GAME_LAST_NO_RELIGION_ROAD_SERVICE_HISTORY = 0xb0` | Appended religion service effects | Older payloads read only the effects present in that version; religion grids remain zero. |
| `SAVE_GAME_LAST_NO_LOCAL_WORKFORCE = 0xb1` | Local workforce allocation payload | `building_local_workforce_load_state()` clears allocations and rebuilds counters from zero. |
| `SAVE_GAME_LAST_NO_ENTERTAINMENT_ROAD_SERVICE_HISTORY = 0xb2` | Appended entertainment service effects | Entertainment grids remain zero for older saves. |
| `SAVE_GAME_LAST_LEGACY_ENTERTAINMENT_SHOW_HALF_DAYS = 0xb3` | Active-day entertainment show counters | `read_legacy_type_data()` doubles legacy half-day counters on load. |
| `SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE = 0xb4` | Building type save table | Bridge synthesizes a legacy table from enum/text migration data. |
| `SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE = 0xb5` | BuildingType object table and owned building heap before the repaired `0xb6` contract | Older saves reconstruct legacy monument construction and raw building records from compatibility data. `0xb6` requires BuildingType table format `3` and the owned building heap; v1/v2 tables or legacy building records are fatal. |

Other broad gates still shape the bridge path:

- `SAVE_GAME_LAST_STATIC_VERSION` decides whether arrays such as figures and buildings include a per-record buffer-size prefix.
- `SAVE_GAME_LAST_STATIC_RESOURCES` controls resource mapping and several old fixed-size resource payloads.
- `SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA` controls saved model data, formula data, production rates, and related scenario migrations.
- `SAVE_GAME_LAST_MONUMENT_TYPE_DATA` separates old monument fields embedded in type data from newer building-state monument fields.
- `SAVE_GAME_LAST_NO_DELIVERIES_VERSION` and `SAVE_GAME_LAST_STATIC_MONUMENT_DELIVERIES_VERSION` gate monument delivery allocation and its element-size prefix.

## BuildingType Save Bridge

`src/building/building_type_id_bridge.cpp` owns the memory bridge between persisted building type identity and the active runtime registry.

There are three identities in play:

| Identity | Owner | Meaning |
| --- | --- | --- |
| Text id | BuildingType XML/legacy migration | Stable semantic id such as `grand_temple_mars` or `warehouse_space`. |
| Runtime id | Current process | `building_type` enum/dynamic slot used by legacy structs and runtime arrays. |
| Save id | One save file | Compact 16-bit id used inside saved building records and rubble/original-type fields. |

### Runtime Table

`building_type_id_bridge_reset_for_runtime()` clears bridge state and rebuilds the runtime table from the active process. It seeds legacy enum slots through `building_type_legacy_migration_text_id_for_enum()` and then records active registry definitions from `g_building_types`.

Dynamic BuildingTypes such as `BUILDING_THEATER` and `BUILDING_WELL` are process-local runtime ids. They must not be trusted as stable persisted values. The bridge persists their text ids, then resolves them back to whatever runtime id the current registry assigned.

### Save Table On Write

`savegame_save_to_state()` calls:

1. `building_type_id_bridge_prepare_new_save_table()`.
2. `building_type_id_bridge_save_table_save_state(state->building_type_table)`.
3. `building_save_state(...)`.

The prepare step creates a save-local table from the current runtime table. Save id `0` remains none/empty. Each real entry records:

- save id
- text id
- legacy enum hint
- default graphics path/image
- phased construction definition, when present or synthesized from legacy monument data

The current written table format is `SAVE_TABLE_MONUMENT_CONSTRUCTION_VERSION = 3`. The older `SAVE_TABLE_TEXT_VERSION = 1` stored text identity only, and the previous object-table version `2` is retained as an old-save compatibility reader. Version `3` is mandatory for `0xb6`: the object table must carry monument construction data instead of depending on legacy monument reconstruction during load.

### Save Table On Load

`building_type_id_bridge_save_table_load_state(buf, has_save_table, save_version)` calls the internal table loader with definition migration enabled.

If `has_save_table` is false, or the dynamic table cannot be read, older saves call the legacy-table path. That path reconstructs save ids from old raw enum values and the migration table. For `0xb6`, absent, invalid, v1, or v2 tables are fatal because current saves must not reach hidden legacy reconstruction.

For each table entry, the loader resolves runtime identity in this order:

1. Resolve by saved text id against the current runtime table.
2. If missing, resolve by legacy enum hint through `building_type_legacy_migration_enum_for_text_id()` / legacy table data.
3. For object tables, fall back to the raw legacy hint when it is still inside `BUILDING_TYPE_MAX`.

Then the loader applies runtime definition migration:

- Object-table entries with saved construction call `apply_saved_construction_entry()`, which writes phased construction data into the current `BuildingType` definition and preserves default graphics with `ensure_default_graphics()`.
- Version `3` entries that correspond to known monument construction but do not carry construction are treated as malformed/missing rather than silently reconstructed from old fallback data.
- v1 entries, absent tables, invalid tables, and old saves call `apply_legacy_monument_entry()`, which applies compatibility monument definitions only for saves at or before `SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE`.
- XML-owned text ids without a current definition are marked missing. `building_type_id_bridge_save_id_is_missing()` lets the building loader drop those building instances safely.

Preview loading uses `building_type_id_bridge_save_table_preview_load_state()`. It snapshots the active bridge, loads the save table without applying definition migration, stamps preview building footprints, and then `building_type_id_bridge_save_table_preview_restore()` restores the previous bridge state.

## Building Heap and Legacy Records

`building_load_state()` owns the permanent `array(building)` allocation, but `0xb6` no longer reads the old fixed building record stream. Current saves use the private singleton in `src/building/building_save_heap.cpp`.

For `0xb6`, `building_save_state()` snapshots each live `building` through `building_save_heap_capture_building()`, then writes a heap stream with `building_save_heap_write_current()`. The heap payload begins with `BSHP`, heap version, building count, and object count. Each following object has `{kind, version, object_id, payload_length, next_offset}` and a variable-length payload, so the save behaves like an object heap rather than a vector of fixed records.

The heap's save object classes are private to the `.cpp` file. Public code can only use the facade functions. The current object set is:

- `BuildingInstanceSaveObject`: common per-instance state, save-local BuildingType id, resources, accepted goods, workers, figures, population, risks, tax/storage, tourism, sickness, strike, variant, and multipart links.
- Payload objects: house, supplier, dock, depot, industry, distribution, rubble, roadblock, entertainment, monument, native storage, native production, and extension.
- `MonumentSaveObject`: explicit per-instance monument state (`upgrades`, `progress`, `phase`, `secondary_frame`). Monument instance state is not read from legacy type-data in `0xb6`.

For `0xb6`, `building_load_state()` calls `building_save_heap_load_current()`, allocates `array(building)`, then materializes each id through `building_save_heap_materialize_building()`. Materialization resolves save-local BuildingType ids through the bridge before applying payload objects. Unknown object kinds, duplicate objects, bad lengths, unresolved active BuildingType ids, missing instance objects, missing monument payloads for monument buildings, v1/v2 BuildingType tables, or legacy building bytes all report a fatal save-data error.

For `0xb5` and older, the old fixed-record path remains as legacy import only. `building_state_load_from_buffer()` reads the per-record size, resolves save-local/legacy type ids through the bridge, reads old type-specific union bytes through `read_legacy_type_data()`, applies old resource and monument gates, then `building_load_state()` rebuilds `first_of_type` / `last_of_type`. `building_state_legacy_save_to_buffer()` and `read_legacy_type_data()` are not valid current-save paths.

## BuildingType Runtime Fan-Out

`src/building/building_type_registry.cpp` owns the process-wide BuildingType registry:

- `g_building_types` is a `std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX>`.
- `BuildingType` stores identity, model, foundation, build-button, sound, event data, flags, water access, graphics, construction, labor, spawn groups, storage references, and production method references.
- `BUILDING_THEATER` and `BUILDING_WELL` are dynamic runtime ids refreshed from text ids by `refresh_known_building_type_ids()`.

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
- Each wrapper points at the legacy `building *` plus the current `BuildingType` definition for `b->type`.
- `set_building_graphic()` precomputes native image-group bindings.
- `storage_runtime_initialize_city()` creates native `StorageSlot` objects for definitions with native storage.
- `production_runtime_initialize_city()` creates native `Production` objects for definitions with native production methods.

## Monuments

Monuments have legacy compatibility data and BuildingType-backed construction truth. New `0xb6` saves make the BuildingType object table authoritative for construction definitions.

`src/building/monument.cpp` still contains hardcoded `monument_type` resource tables for older monuments such as Pantheon, Colosseum, Hippodrome, mausoleums, Nymphaeum, and City Mint. These remain compatibility/runtime fallback data for types that do not have BuildingType phased construction, but `0xb6` serialization converts their construction into the BuildingType object-table payload before writing.

Newer BuildingType-backed monuments use `BuildingType::ConstructionDefinition`:

- `building_type_registry_has_phased_construction(type)` decides whether construction comes from the registry.
- `building_type_registry_get_construction_phase_count(type)` returns the number of saved/XML construction phases.
- `building_type_registry_get_construction_requirement(type, resource, phase)` provides per-phase resource needs.
- `building_monument_resources_needed_for_monument_type()` chooses registry construction first, then falls back to the hardcoded legacy monument table.

The bridge's v3 save table preserves object-level construction so saved monument definitions can survive active XML/runtime changes. Each saved construction entry carries:

- whether phased construction exists
- default graphics path and image
- road update radius
- phase index
- phase graphics path and image
- per-phase resource/amount requirements

For saves at or before `SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE`, `apply_legacy_monument_entry()` uses `LEGACY_MONUMENT_CONSTRUCTION` in `building_type_id_bridge.cpp`. That table covers grand temples, large temples, oracle, lighthouse, caravanserai, Pantheon, Colosseum, Hippodrome, Nymphaeum, mausoleums, and City Mint compatibility definitions, including default graphics and phase requirements. It skips Oracle when the active mod is Julius.

There is a separate old-save per-building state hazard. Legacy building records contain the old fixed-size type-data block, and saves at or before `SAVE_GAME_LAST_MONUMENT_TYPE_DATA` stored monument instance fields inside that block for specific legacy monument types. `read_legacy_type_data()` must decide that old layout from an explicit legacy type list, not from current XML `building_monument_is_monument()` status; otherwise a type that became a monument through XML can have unrelated old union bytes interpreted as `b->monument.upgrades`, `progress`, and `phase`. `0xb6` avoids this path entirely by requiring `MonumentSaveObject`.

Per-instance monument state lives in each loaded `building` record:

- `b->monument.upgrades`
- `b->monument.progress`
- `b->monument.phase`
- resource counts in `b->resources`
- normal building state such as mothballing, workers, road access, and multipart links

`normalize_monument_phase_after_load()` turns zero or terminal numeric phase values into `MONUMENT_FINISHED`. For unfinished monuments, `building_state_load_from_buffer()` clamps saved resource counts down to the requirements returned by `building_monument_resources_needed_for_monument_type()`, so an old or changed definition cannot leave impossible surplus requirements on the instance.

Runtime graphics refresh follows the same authority split:

- Registry-backed phased monuments call `building_runtime_apply_graphic()`.
- Legacy monuments redraw through `map_building_tiles_add(..., building_image_get(b), TERRAIN_BUILDING)`.

Monument deliveries are separate dynamic state. `building_monument_delivery_load_state()` allocates `array(monument_delivery)`, reads an optional element-size prefix for newer saves, remaps resource ids, and preserves only active delivery rows. Old saves without a deliveries piece call `building_monument_initialize_deliveries()` and start with an empty delivery array.

## Figure Runtime Bridge

Figures persist as legacy `array(figure)` records. `figure_load_state()` resets `figure_runtime`, reads the saved array record size when present, allocates the figure array, and fills each `figure` with legacy state.

Native FigureType runtime state is not persisted as C++ objects. It is rebuilt after load:

- `figure_runtime_initialize_city()` clears `g_runtime_entries`, walks live figures, and calls the internal binder.
- The binder finds the `FigureTypeDefinition` for `f->type`.
- Saved figures generally lack exact XML profile bindings, so `infer_profile_id()` recovers a profile from legacy fields.
- Labor seekers infer `acquisition` or `validation` from `collecting_item_id`.
- Priests infer god profiles from the owner building type.
- Entertainment walkers infer school/venue/service profiles from action state and type.
- If no inferred profile is available, the definition's default profile is used.
- New runtime-created walkers use `figure_runtime_create_profiled()` and `figure_runtime_bind_profile()` so they do not depend on inference.

This is the same bridge pattern as buildings: legacy records are persisted, while runtime controllers are reconstructed from legacy fields plus current XML definitions.

## Road Service History

`src/map/road_service_history.cpp` owns pathing-only telemetry for smart service walkers.

Runtime storage is:

- `std::array<grid_u32, ROAD_SERVICE_EFFECT_MAX> g_history`
- `uint32_t g_last_visit_stamp`

The save payload is dynamic and starts with:

1. `kSaveFormatVersion = 1`
2. effect count
3. one full `GRID_SIZE * GRID_SIZE` `uint32_t` grid per saved effect id

Effect ids are append-only. Removed meanings must stay reserved so old save columns do not shift. The loader receives three gates from `savegame_load_from_state()`:

- `has_saved_state`
- `has_religion_effects`
- `has_entertainment_effects`

Missing payloads or missing appended effect groups start from zeroed history. Unsupported payloads reset to zero. Future extra grids are consumed so the ordinal payload remains aligned. `update_last_visit_stamp_from_history()` rebuilds the current generation counter from loaded grids.

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
- `production_rates_load()` restores dynamic production-rate settings after `resource_init()`, so resource mappings are already active.
- `scenario_events_load_state()` restores events, conditions, actions, and formulas according to the embedded scenario version. Old saves run migration helpers such as `scenario_events_migrate_to_formulas()`, resolved-name migration, grid-slice migration, and buys/sells migration.
- `scenario_load_state()` and related scenario loaders restore settings, requests, invasions, demand/price changes, allowed buildings, and custom variables before map/runtime post-load work.
- `city_message_load_state()`, `custom_messages_load_state()`, `custom_media_load_state()`, and `message_media_text_blob_load_state()` restore message queues and custom media/text payloads.
- `empire_load_state()`, `empire_load_custom_map()`, `empire_city_load_state()`, and `empire_object_load()` restore empire runtime state. Older resource mappings can trigger fish/meat production recomputation after custom empire data loads.

These systems are not all C++ wrapper bridges, but they follow the same compatibility shape: read versioned legacy payloads into subsystem-owned memory, then run migrations or registry/resource fan-out after the prerequisites are loaded.

## Save-Side Mirror

On save, `savegame_save_to_state()` writes the runtime bridge data before the records that depend on it:

1. File/resource/scenario versions.
2. Scenario settings and mod metadata.
3. `building_type_id_bridge_prepare_new_save_table()` and `building_type_id_bridge_save_table_save_state()`.
4. Map, figure, route, formation, city, and building records.
5. Legacy model data.
6. Scenario, message, empire, list, storage, route, delivery, visited-building, production-rate, road-history, and workforce payloads.

The `0xb6` building heap writes `building_type_id_bridge_save_id_from_runtime(b->type)` instead of raw runtime enum ids. Legacy records and old type-data rubble/original-type fields use the same bridge while importing old saves. This is what lets the save survive dynamic BuildingType ids, XML-owned definitions, and old enum migrations without treating the current process enum as stable disk data.
