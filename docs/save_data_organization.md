# Save Data Organization

This document maps how Vespasian `.svv` save data is allocated, written, loaded, and handed back to runtime systems. It is a live-game save reference first. Scenario files use the same file-piece machinery, but their layout is covered separately in the scenario appendix. For the post-read bridge layer that resolves save-local ids into runtime objects, BuildingType definitions, and legacy structs, see `docs/save_load_runtime_bridges.md`. For water access type identity and mask propagation after the save table resolves, see `docs/water_access_runtime.md`.

Current save version in this checkout is `SAVE_GAME_CURRENT_VERSION = 0xc9`. Current scenario version is `SCENARIO_CURRENT_VERSION = 23`.

The dynamic city payload now appends named monument-gift awards after the existing city fields (0xc8), followed by trade/resource/finance history (0xc9). The history stores stable resource text ids, wide quantities and visit identities, current and seven previous accounting years, and partial-period metadata. The writer rejects overflowing pieces before replacing an existing save. These native gates do not decode post-fork Augustus SVX layouts; see the current sync ledger's save-bridge audit.

## Top-Level Flow

The save/load entry points are in `src/game/file_io.cpp`.

- Saving starts in `game_file_io_write_saved_game(filename)`.
- It sets the resource mapping to `RESOURCE_CURRENT_VERSION`, calls `init_savegame_data(SAVE_GAME_CURRENT_VERSION)`, and then calls `savegame_save_to_state(&savegame_data.state)`.
- `init_savegame_data()` allocates a sequence of `file_piece` buffers. The order of those buffers is the on-disk order.
- Each subsystem writer receives one or more `buffer *` pieces and writes its owned runtime data into them.
- `savegame_write_to_file()` writes the same piece sequence to disk. Dynamic pieces get a 32-bit piece-size prefix before their payload. Compressed pieces are written through `write_compressed_chunk()`.

Loading is the reverse:

- `game_file_io_read_saved_game(filename, offset)` or `game_file_io_read_save_game_from_buffer(buf)` peeks the save/resource versions first.
- Unsupported newer save/resource versions are rejected before any state load.
- `resource_set_mapping(resource_version)` installs the legacy resource id mapping needed by old saves; newer saves replace it during load with the resource text-id table.
- `init_savegame_data(save_version)` allocates the exact piece sequence for that save version.
- `savegame_read_from_file()` or `savegame_read_from_buffer()` fills those pieces in order.
- `savegame_load_from_state(&savegame_data.state, save_version)` calls subsystem loaders.
- `clear_savegame_pieces()` resets and frees every allocated file-piece buffer after the save has been consumed.

The file extension policy lives outside the payload format. Vespasian saves use `.svv`; old `.savf` files can be renamed because the bytes did not change during the extension rename.

## File-Piece Contract

`file_piece` is the outer allocation unit:

```c
typedef struct {
    buffer buf;
    int compressed;
    int dynamic;
} file_piece;
```

`create_savegame_piece(size, compressed)` appends one `file_piece` to `savegame_data.pieces` and returns the piece's `buffer`. Fixed-size pieces allocate a zeroed buffer immediately. Dynamic pieces start with `data = 0` and `size = 0`; on save, the subsystem writer replaces the buffer with a payload it allocates, and on load the file reader allocates the buffer after reading the piece-size prefix.

All scalar `buffer_write_*` and `buffer_read_*` functions use little-endian byte order. Reads past the end return zero and set `overflow`; writes past the end set `overflow` and skip the write. `buffer_init_dynamic()` allocates a payload with an internal 32-bit size header. That internal header is part of the piece contents; it is separate from the file-level dynamic piece-size prefix. `buffer_init_dynamic_array()` adds a standard dynamic-array header: total dynamic size, a version/skip field, array size, and element size.

Compression is per piece. A compressed piece writes a 32-bit compressed-size prefix followed by compressed bytes; if compression fails, that prefix is `UNCOMPRESSED` (`0x80000000`) and the raw payload follows. Saves after `SAVE_GAME_LAST_ZIP_COMPRESSION` read compressed chunks through zlib.

Save-version gates are append/order gates. Adding, removing, resizing, or reordering any persisted data must update `SAVE_GAME_CURRENT_VERSION` and add a `SAVE_GAME_LAST_*` boundary in `src/game/save_version.h`.

## Live Save Pieces

This table follows `init_savegame_data()` order exactly. "Current" means the piece is present in newly written `0xc6` saves. Old-only pieces are still allocated while reading older versions so the stream stays aligned. The `C` column means the file piece is compressed on disk.

| # | Piece | Gate | Size allocation | C | Writer | Loader / consumer | Runtime data |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `scenario_campaign_mission` | current | 4 | no | `scenario_settings_save_state` | `scenario_settings_load_state` | Campaign mission id |
| 2 | `file_version` | current | 4 | no | `buffer_write_i32(SAVE_GAME_CURRENT_VERSION)` | version peek before load | Save format version |
| 3 | `resource_version` | `> SAVE_GAME_LAST_STATIC_RESOURCES` | 4 | no | `buffer_write_u32(RESOURCE_CURRENT_VERSION)` | version peek before load | Resource id mapping version |
| 4 | `resource_type_table` | `> SAVE_GAME_LAST_NO_RESOURCE_TYPE_TABLE` | dynamic | no | `resource_id_bridge_save_table_save_state` | `resource_id_bridge_save_table_load_state` | Resource save id/text id table |
| 5 | `scenario_version` | `> SAVE_GAME_LAST_NO_SCENARIO_VERSION` | 4 | no | `buffer_write_i32(SCENARIO_CURRENT_VERSION)` | `save_version_to_scenario_version` | Embedded scenario format version |
| 6 | `image_grid` | `<= SAVE_GAME_LAST_STORED_IMAGE_IDS` | `image_grid` version size | yes | old saves only | `map_terrain_load_state` legacy image migration | Old stored map image ids |
| 7 | `edge_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_property_save_state` | `map_property_load_state` | Draw-tile / edge flags |
| 8 | `building_grid` | current | `GRID_SIZE_BUF_U16` or `GRID_SIZE_BUF_U32` | yes | `map_building_save_state` | `map_building_load_state` | Grid cell -> building id |
| 9 | `terrain_grid` | current | `GRID_SIZE_BUF_U16` or `GRID_SIZE_BUF_U32` | yes | `map_terrain_save_state` | `map_terrain_load_state` | Terrain bitfield grid |
| 10 | `aqueduct_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_aqueduct_save_state` | `map_aqueduct_load_state` | Aqueduct grid |
| 11 | `figure_grid` | current | `GRID_SIZE_BUF_U16` | yes | `map_figure_save_state` | `map_figure_load_state` | Grid cell -> first figure id |
| 12 | `bitfields_grid` | current | `GRID_SIZE_BUF_U8` or `GRID_SIZE_BUF_U16` | yes | `map_property_save_state` | `map_property_load_state` / `_u8` | Map property flags |
| 13 | `sprite_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_sprite_save_state` | `map_sprite_load_state` | Sprite grid |
| 14 | `random_grid` | current | `GRID_SIZE_BUF_U8` | no | `map_random_save_state` | `map_random_load_state` | Per-tile random bytes |
| 15 | `desirability_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_desirability_save_state` | `map_desirability_load_state` | Desirability grid |
| 16 | `elevation_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_elevation_save_state` | `map_elevation_load_state` | Elevation grid |
| 17 | `building_damage_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_building_save_state` | `map_building_load_state` | Building damage overlay grid |
| 18 | `aqueduct_backup_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_aqueduct_save_state` | `map_aqueduct_load_state` | Aqueduct backup grid |
| 19 | `sprite_backup_grid` | current | `GRID_SIZE_BUF_U8` | yes | `map_sprite_save_state` | `map_sprite_load_state` | Sprite backup grid |
| 20 | `figures` | current | dynamic after `SAVE_GAME_LAST_STATIC_VERSION` | yes | `figure_save_state` | `figure_load_state` | `array(figure)` records |
| 21 | `route_figures` | current | dynamic after `SAVE_GAME_LAST_STATIC_VERSION` | yes | `figure_route_save_state` | `figure_route_load_state` | Route id -> figure id table |
| 22 | `route_paths` | current | dynamic after `SAVE_GAME_LAST_STATIC_VERSION` | yes | `figure_route_save_state` | `figure_route_load_state` | Packed figure route directions |
| 23 | `formations` | current | dynamic after `SAVE_GAME_LAST_STATIC_VERSION` | yes | `formations_save_state` | `formations_load_state` | Military formations |
| 24 | `formation_totals` | current | 12 | no | `formations_save_state` | `formations_load_state` | Formation counters |
| 25 | `city_data` | current | dynamic after `SAVE_GAME_LAST_NO_KEYED_RESOURCE_STATE`; old resource-version-sized block before that | yes | `city_data_save_state` | `city_data_load_state` | Main city globals and keyed city resource state |
| 26 | `city_faction_unknown` | `<= SAVE_GAME_LAST_UNKNOWN_UNUSED_CITY_DATA` | 2 | no | old saves only | skipped by old layout | Removed city faction payload |
| 27 | `player_name` | current | 64 | no | `scenario_settings_save_state` | `scenario_settings_load_state` | Player name |
| 28 | `city_faction` | `<= SAVE_GAME_LAST_UNKNOWN_UNUSED_CITY_DATA` | 4 | no | old saves only | skipped by old layout | Removed city faction payload |
| 29 | `building_type_table` | `> SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE` | dynamic | no | `building_type_id_bridge_save_table_save_state` | `building_type_id_bridge_save_table_load_state` | Save-local building type id map |
| 30 | `water_access_type_table` | `> SAVE_GAME_LAST_NO_WATER_ACCESS_TYPE_TABLE` | dynamic | no | `water_access_type_id_bridge_save_table_save_state` | `water_access_type_id_bridge_save_table_load_state` | Save-local water access type id map |
| 31 | `buildings` | current | dynamic after `SAVE_GAME_LAST_STATIC_VERSION` | yes | `building_save_state` | `building_load_state` | `array(building)` records |
| 31a | `building_resource_state` | `> SAVE_GAME_LAST_NO_KEYED_RESOURCE_STATE` | dynamic | yes | `building_resource_state_save` | `building_resource_state_load` | Table-backed per-building resource ids, quantities, and accepted goods |
| 32 | `city_view_orientation` | current | 4 | no | `city_view_save_state` | `city_view_load_state` | City view orientation |
| 33 | `game_time` | current | 20 | no | `game_time_save_state` | `game_time_load_state` | Calendar/tick state |
| 34 | `building_extra_highest_id_ever` | current | 8 | no | `building_save_state` | not consumed by current load | Legacy building high-water data |
| 35 | `random_iv` | current | 8 | no | `random_save_state` | `random_load_state` | RNG state |
| 36 | `city_view_camera` | current | 8 | no | `city_view_save_state` | `city_view_load_state` | Camera position |
| 37 | `building_count_culture1` | `<= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | legacy count size | no | old saves only | old count migration | Static building counts |
| 38 | `city_graph_order` | current | 4 or 8 | no | `city_data_save_state` | `city_data_load_state` | Graph display order |
| 39 | `emperor_change_time` | current | 8 | no | `scenario_emperor_change_save_state` | `scenario_emperor_change_load_state` | Emperor-change timer |
| 40 | `empire` | current | 12 | no | `empire_save_state` | `empire_load_state` | Basic empire state |
| 41 | `empire_map` | `> SAVE_GAME_LAST_NO_CUSTOM_EMPIRE_MAP_IMAGE` | dynamic | no | `empire_save_custom_map` | `empire_load_custom_map` | Custom empire map image |
| 42 | `empire_cities` | current | dynamic after static scenario objects | yes | `empire_city_save_state` | `empire_city_load_state` | `array(empire_city)` records |
| 43 | `building_count_industry` | `<= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | legacy count size | no | old saves only | old count migration | Static industry counts |
| 44 | `trade_prices` | current | `8 * resource_total_mapped()` | no | `trade_prices_save_state` | `trade_prices_load_state` | Buy/sell prices per resource |
| 45 | `figure_names` | current | 84 | no | `figure_name_save_state` | `figure_name_load_state` | Figure name state |
| 46 | `culture_coverage` | current | 60 | no | `city_culture_save_state` | `city_culture_load_state` | Culture coverage counters |
| 47 | `scenario` | current | scenario buffer size for save version | no | `scenario_save_state` | `scenario_load_state` | Embedded scenario state |
| 48 | `requests` | `> SAVE_GAME_LAST_NO_EXTENDED_REQUESTS` | dynamic | no | `scenario_request_save_state` | `scenario_request_load_state` | Scenario requests |
| 49 | `invasions` | `> SAVE_GAME_LAST_STATIC_SCENARIO_ORIGINAL_DATA` | dynamic | yes | `scenario_invasion_save_state` | `scenario_invasion_load_state` | Scenario invasions |
| 50 | `demand_changes` | same as above | dynamic | yes | `scenario_demand_change_save_state` | `scenario_demand_change_load_state` | Demand-change events |
| 51 | `price_changes` | same as above | dynamic | yes | `scenario_price_change_save_state` | `scenario_price_change_load_state` | Price-change events |
| 52 | `allowed_buildings` | same as above | dynamic | yes | `scenario_allowed_building_save_state` | `scenario_allowed_building_load_state` | Scenario build permissions |
| 53 | `custom_variables` | same as above | dynamic | yes | `scenario_custom_variable_save_state` | `scenario_custom_variable_load_state` | Scenario custom variables |
| 54 | `scenario_events` | `> SAVE_GAME_LAST_NO_EVENTS` | dynamic | no | `scenario_events_save_state` | `scenario_events_load_state` | Event info records |
| 55 | `scenario_formulas` | `> SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA` | dynamic | no | `scenario_events_save_state` | `scenario_events_load_state` | Scenario formula records |
| 56 | `scenario_conditions` | `> SAVE_GAME_LAST_NO_EVENTS` | dynamic | no | `scenario_events_save_state` | `scenario_events_load_state` | Event condition groups |
| 57 | `scenario_actions` | `> SAVE_GAME_LAST_NO_EVENTS` | dynamic | no | `scenario_events_save_state` | `scenario_events_load_state` | Event actions |
| 58 | `custom_messages` | `> SAVE_GAME_LAST_NO_CUSTOM_MESSAGES` | dynamic | no | `custom_messages_save_state` | `custom_messages_load_state` | Custom message records |
| 59 | `custom_media` | same as above | dynamic | no | `custom_media_save_state` | `custom_messages_load_state` | Custom media records |
| 60 | `message_media_text_blob` | same as above | dynamic | no | `message_media_text_blob_save_state` | `message_media_text_blob_load_state` | Shared media text bytes |
| 61 | `message_media_metadata` | same as above | dynamic | no | `message_media_text_blob_save_state` | `message_media_text_blob_load_state` | Shared media text metadata |
| 62 | `building_model_data` | `> SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA` | dynamic | no | `model_save_model_data` | `model_load_model_data` | Saved building model overrides |
| 63 | `max_game_year` | current | 4 | no | `scenario_criteria_save_state` | `scenario_criteria_load_state` | Scenario max year |
| 64 | `earthquake` | current | 60 | no | `scenario_earthquake_save_state` | `scenario_earthquake_load_state` | Earthquake state |
| 65 | `emperor_change_state` | current | 4 | no | `scenario_emperor_change_save_state` | `scenario_emperor_change_load_state` | Emperor-change state |
| 66 | `messages` | current | 16000 | yes | `city_message_save_state` | `city_message_load_state` | City message records |
| 67 | `message_extra` | current | 12 | no | `city_message_save_state` | `city_message_load_state` | Message extra counters |
| 68 | `population_messages` | current | 10 | no | `city_message_save_state` | `city_message_load_state` | Population message flags |
| 69 | `message_counts` | current | 80 | no | `city_message_save_state` | `city_message_load_state` | Message category counts |
| 70 | `message_delays` | current | 80 | no | `city_message_save_state` | `city_message_load_state` | Message delays |
| 71 | `building_list_burning_totals` | current | 4 or 8 | no | `building_list_save_state` | `building_list_load_state` | Burning-list total count |
| 72 | `figure_sequence` | current | 4 | no | `figure_save_state` | `figure_load_state` | Next figure created-sequence |
| 73 | `scenario_settings` | current | 12 | no | `scenario_settings_save_state` | `scenario_settings_load_state` | Scenario settings block |
| 74 | `invasion_warnings` | current | dynamic after static scenario original data | yes | `scenario_invasion_warning_save_state` | `scenario_invasion_warning_load_state` | Invasion warning records |
| 75 | `scenario_is_custom` | current | 4 | no | `scenario_settings_save_state` | `scenario_settings_load_state` | Custom-scenario flag |
| 76 | `city_sounds` | current | 8960 | no | no current writer | no current loader | Reserved legacy sound-state block |
| 77 | `building_extra_highest_id` | current | 4 | no | `building_save_state` | not consumed by current load | Legacy highest building id |
| 78 | `figure_traders` | current | resource-version-sized trader block | no | `traders_save_state` | `traders_load_state` | Trader resource state |
| 79 | `building_list_burning` | current | dynamic after static version | yes | `building_list_save_state` | `building_list_load_state` | Burning building id list |
| 80 | `building_list_small` | current | dynamic after static version | yes | `building_list_save_state` | `building_list_load_state` | Small-building id list |
| 81 | `building_list_large` | current | dynamic after static version | yes | `building_list_save_state` | `building_list_load_state` | Large-building id list |
| 82 | `tutorial_part1` | current | 32 | no | `tutorial_save_state` | `tutorial_load_state` | Tutorial state |
| 83 | `building_count_military` | `<= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | legacy count size | no | old saves only | old count migration | Static military counts |
| 84 | `enemy_army_totals` | current | 20 | no | `enemy_armies_save_state` | `enemy_armies_load_state` | Enemy army totals |
| 85 | `building_storages` | current | dynamic after static version | no | `building_storage_save_state` | `building_storage_load_state` | `array(storage)` records |
| 86 | `building_count_culture2` | `<= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | legacy count size | no | old saves only | old count migration | Static culture counts |
| 87 | `building_count_support` | `<= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | legacy count size | no | old saves only | old count migration | Static support counts |
| 88 | `tutorial_part2` | current | 4 | no | `tutorial_save_state` | `tutorial_load_state` | Tutorial state |
| 89 | `gladiator_revolt` | current | 16 | no | `scenario_gladiator_revolt_save_state` | `scenario_gladiator_revolt_load_state` | Gladiator revolt state |
| 90 | `trade_routes` | `> SAVE_GAME_LAST_NO_EMPIRE_EDITOR` | dynamic | yes | `trade_routes_save_state` | `trade_routes_load_state` | Empire trade route records |
| 91 | `trade_route_limit` | `<= SAVE_GAME_LAST_NO_EMPIRE_EDITOR` | legacy resource-sized block | yes | old saves only | `trade_routes_migrate_to_buys_sells` | Old route import/export limits |
| 92 | `trade_route_traded` | same old gate | legacy resource-sized block | yes | old saves only | `trade_routes_migrate_to_buys_sells` | Old route traded quantities |
| 93 | `building_barracks_tower_sentry` | `<= SAVE_GAME_LAST_BARRACKS_TOWER_SENTRY_REQUEST` | 4 | no | old saves only | old migration | Retired sentry request state |
| 94 | `building_extra_sequence` | current | 4 | no | `building_save_state` | `building_load_state` | Next building created-sequence |
| 95 | `routing_counters` | current | 16 | no | `map_routing_save_state` | `map_routing_load_state` | Routing counters |
| 96 | `building_count_culture3` | `<= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | legacy count size | no | old saves only | old count migration | Static culture counts |
| 97 | `enemy_armies` | current | version-sized army block | no | `enemy_armies_save_state` | `enemy_armies_load_state` | Enemy army records |
| 98 | `city_entry_exit_xy` | current | 16 | no | `city_data_save_state` | `city_data_load_state` | Entry/exit x/y pairs |
| 99 | `last_invasion_id` | current | 2 | no | `scenario_invasion_warning_save_state` | `scenario_invasion_warning_load_state` | Last invasion warning id |
| 100 | `building_extra_corrupt_houses` | current | 8 | no | `building_save_state` | `building_load_state` | House corruption counters |
| 101 | `scenario_name` | current | 65 | no | `scenario_settings_save_state` | `scenario_settings_load_state` | Scenario filename/name |
| 102 | `bookmarks` | current | 32 | no | `map_bookmark_save_state` | `map_bookmark_load_state` | Map bookmarks |
| 103 | `tutorial_part3` | current | 4 | no | `tutorial_save_state` | `tutorial_load_state` | Tutorial state |
| 104 | `city_entry_exit_grid_offset` | current | 8 | no | `city_data_save_state` | `city_data_load_state` | Entry/exit grid offsets |
| 105 | `campaign_name` | `> SAVE_GAME_LAST_NO_CUSTOM_CAMPAIGNS` | dynamic | no | `scenario_settings_save_state` | `scenario_settings_load_state` | Custom campaign name |
| 106 | `mod_metadata` | `> SAVE_GAME_LAST_NO_MOD_METADATA` | dynamic | no | `savegame_mod_metadata_save_state` | `update_loaded_save_mod_metadata` | Saved mod name and metadata version |
| 107 | `end_marker` | current | 284 | no | reserved zero bytes | skipped on load | Reserved historical padding |
| 108 | `deliveries` | `> SAVE_GAME_LAST_NO_DELIVERIES_VERSION` | dynamic after static monument-delivery version | no | `building_monument_delivery_save_state` | `building_monument_delivery_load_state` | Monument delivery records |
| 109 | `custom_empire` | `> SAVE_GAME_LAST_UNVERSIONED_SCENARIOS` | dynamic | yes | `empire_object_save` | `empire_object_load` | Custom empire object graph |
| 110 | `visited_buildings` | `> SAVE_GAME_LAST_GLOBAL_BUILDING_INFO` | dynamic | yes | `figure_visited_buildings_save_state` | `figure_visited_buildings_load_state` | Figure visited-building lists |
| 111 | `rubble_grid` | `> SAVE_GAME_LAST_U16_GRIDS` | `GRID_SIZE_BUF_U32` | yes | `map_building_save_state` | `map_building_load_state` | Rubble original-building grid |
| 112 | `production_rates` | `> SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA` | dynamic | yes | `production_rates_save` | `production_rates_load` | Custom production rates |
| 113 | `road_service_history` | `> SAVE_GAME_LAST_NO_ROAD_SERVICE_HISTORY` | dynamic | yes | `map_road_service_history_save_state` | `map_road_service_history_load_state` | Smart-service road recency grids |
| 114 | `local_workforce_allocations` | `> SAVE_GAME_LAST_NO_LOCAL_WORKFORCE` | dynamic | yes | `building_local_workforce_save_state` | `building_local_workforce_load_state` | Local workforce house/workplace allocations |

## Allocated Runtime Structures

The file-piece buffers above are temporary. Subsystem loaders usually rebuild permanent runtime storage from the pieces.

- `savegame_data.pieces`: fixed array inside the static `savegame_data` object. Each active element owns one `buffer.data` pointer until `clear_savegame_pieces()` frees it.
- `buffer_init_dynamic` payloads: allocated by the subsystem writer and then owned by the corresponding `file_piece`; freed by `clear_savegame_pieces()`.
- `buffer_init_dynamic_array` payloads: same ownership as dynamic payloads, with a standardized array header for event/formula-like data.
- `array_init` / `array_expand` runtime arrays: loaders allocate or expand permanent runtime arrays, such as figures, buildings, storages, formations, route paths, empire objects, empire cities, trade routes, visited-building lists, scenario events, and scenario formulas.
- Per-record `malloc` allocations inside loaders are permanent runtime data until their owning subsystem clears them. Examples include route direction arrays and custom empire object buffers.

Important owned payloads:

- Figures: `figure_save_state()` allocates `4 + figure_count * 170` bytes, writes the per-record byte size (`170`), then serializes every `figure`. `figure_load_state()` resets figure runtime/profile state, reads old or current record sizes, initializes `array(figure)`, expands it, reads every record, tracks the highest nonzero state, and trims `data.figures.size`. Figure records use resource remapping and version gates for image ids, building ids, routing ids, visited-building index, and last destination.
- City data: current saves write `city_data` as a dynamic payload. Resource arrays inside it are keyed by resource save id and value, using the resource type table for text-id resolution on load. Saves at or before `SAVE_GAME_LAST_NO_KEYED_RESOURCE_STATE` still read the old fixed city-data block and flat resource arrays through the resource bridge.
- Buildings: `building_save_state()` allocates `4 + building_count * BUILDING_STATE_CURRENT_BUFFER_SIZE`, writes the current building record size, then calls `building_state_save_to_buffer()` for every `building`. `building_load_state()` initializes the current compatibility building array, reads records through `building_state_load_from_buffer()`, rebuilds building lists, and restores created-sequence/corrupt-house counters. Building type ids are saved through `building_type_id_bridge_save_id_from_runtime()`, not raw runtime enum values. Current saves then write `building_resource_state`, a dynamic keyed payload that replaces the flat compatibility resource mirrors with table-backed output resource ids, warehouse-space resource ids, fetch/depot resource ids, per-resource quantities, and accepted goods. The flat building payload still includes the legacy graphics variant byte, but runtime code owns it in `BuildingGraphicsState` and accesses it through `Building::graphics().variant()`: load copies `Record.variant` into graphics state, and save copies graphics state back into `Record.variant`. Rubble type data stores only the original BuildingType save id, logical origin grid offset, and orientation; dimensions come from the resolved BuildingType. Saves through `0xbd` migrate from main-grid-offset plus square size, and version `0xbe` migrates from the short-lived `x/y/width/height` rectangle shape.
- Resource type table: `resource_id_bridge_save_table_save_state()` writes dynamic format version `1`, entry count, and each save id/text id pair. The loader installs it before resource-bearing payloads are decoded and falls back to legacy raw-id maps when absent or invalid. Current keyed city/building resource payloads store numeric save ids from this table, not inline text ids.
- Building type table: `building_type_id_bridge_save_table_save_state()` writes dynamic format version `1`, entry count, and each save id/text id pair. The loader uses the table when present and falls back to legacy enum migration when absent or invalid. BuildingType construction data is runtime XML state and is not persisted in this table.
- Water access type table: `water_access_type_id_bridge_save_table_save_state()` writes dynamic format version `1`, entry count, and each save id/text id pair. The loader resolves text ids against the active mod's `WaterAccessType` XML before current numeric ids are used. Older saves synthesize the shared legacy ids for `well`, `fountain`, `reservoir`, `aqueduct`, and `latrines`.
- Building storages: `building_storage_save_state()` writes a record-size header and one storage record per `array(storage)` item. Current records include storage id/building id, permissions, per-resource state, and per-resource quantity data. The loader supports original/static/current record sizes and remaps resources.
- Building lists: `building_list_save_state()` writes small, large, and burning id lists as dynamic integer arrays plus a burning-total piece. The loader reads 16-bit ids for old static saves and 32-bit ids for newer dynamic saves.
- Monument deliveries: `building_monument_delivery_save_state()` writes a record-size header and 16-byte `monument_delivery` records containing walker id, destination id, resource, and cartloads. The loader rebuilds the delivery array and remaps resources.
- Figure routes: `figure_route_save_state()` writes one `uint32_t` figure id per path into `route_figures`, then writes route count and per-route direction byte arrays into `route_paths`. The loader reads old fixed 500-byte paths for old saves or current variable route lengths for new saves, allocates direction arrays, and restores each path's current step from the owning figure.
- Formations: `formations_save_state()` writes a record-size header and fixed-size formation records plus a 12-byte totals piece. The loader supports original, 10-legion, and current formation record sizes, expands the formation array, and skips trailing bytes from future record sizes.
- Empire cities: `empire_city_save_state()` writes a record-size header and one city record per `array(empire_city)` item. The loader expands the city array, remaps resources, and runs compatibility fixes for old campaign data.
- Custom empire objects: `empire_object_save()` writes a single zero if the scenario does not use a custom empire. For custom empires it writes object count, then each used or unused object. City objects write a larger record because they include per-resource trade fields. `empire_object_load()` rebuilds the object array and fixes image/resource compatibility.
- Scenario events: `scenario_events_save_state()` writes event info as a dynamic array, condition groups as a dynamic byte stream, actions as a dynamic array, and formulas as a dynamic array. Live savegames store the four event pieces in the historical order `scenario_events`, `scenario_formulas`, `scenario_conditions`, `scenario_actions`; scenario files store formulas later with model data. Keep those piece orders separate when editing `init_savegame_data()` or `init_scenario_data()`. `scenario_events_load_state()` recreates events, links conditions/actions back to event ids, loads formulas for newer versions, and runs migration helpers for older formats.
- City messages: `city_message_save_state()` splits message records, extra counters, counts, delays, and population-message flags across five pieces. The loader reconstructs the message subsystem from those pieces.
- Road service history: `map_road_service_history_save_state()` writes dynamic format version `1`, effect count, and a full `uint32_t` grid for each nonzero service effect. The loader clears history first, handles missing old saves by leaving zeros, reads only effects supported by the save version, skips unknown/future effect grids, and leaves newly appended religion, entertainment, market, medicine, or tax effects zeroed.
- Local workforce allocations: `building_local_workforce_save_state()` writes dynamic format version `1`, record count, then `(workplace_id, house_id, workers)` triples. The loader clears allocations first, validates the format, clamps record count to payload size, and ignores invalid building ids through later reconciliation.
- Dynamic model and production data: `model_save_model_data()` and `production_rates_save()` own their dynamic payload contents. Savegame load applies building type XML/model overrides after model load, then initializes resources and reapplies production-method throughput overrides.

## Save/Load Ordering Dependencies

Some pieces are independent byte dumps, but several order dependencies are intentional:

- Resource mapping is installed before any load that remaps resources. New saves load `resource_type_table` before resource-bearing scenario, figure, city, building, trade, price, and empire payloads; old saves synthesize that table from legacy raw-id maps. Current city and building resource payloads are keyed by save-local numeric resource ids and resolve to runtime numeric ids through that table.
- Scenario settings and scenario data load before map/city systems that depend on scenario dimensions or climate.
- `building_type_table` and `water_access_type_table` load before `buildings`, because building records store save-local building type ids and water compatibility mirrors may need text-id resolution as water state migrates. If a saved building type is not present in the active mod stack, the building loader removes that record and its associated figures instead of accepting a typeless live building. `building_resource_state` loads immediately after `buildings`, once the building ids it patches exist.
- `figure_load_state()` runs before route, formation, trader, visited-building, and building-slot cleanup logic can safely refer to figure ids.
- `model_load_model_data()` runs after buildings and before `building_type_registry_apply_model_overrides()`.
- `resource_init()` runs before production-rate overrides and many economy systems are used after load.
- Road service history and local workforce load after resources/model data and before gameplay resumes; both tolerate missing old-save pieces by clearing to empty state.

Preview loading for savegame thumbnails uses the same piece reader, but it does not run the full live-game loader. It reads enough city/scenario/building/map data to render minimap info; full record-to-object hydration remains part of the live save bridge path.

Compatibility helpers that do not own standalone pieces still matter to the load contract. `building_type_id_bridge_prepare_new_save_table()` prepares the table before writing; `map_property_load_state_u8()` reads old 8-bit map bitfields; `figure_visited_buildings_migrate()` synthesizes visited-building state for old saves; `map_terrain_migrate_old_bridges()` and `map_terrain_migrate_old_walls()` repair old terrain encodings; `scenario_events_migrate_to_formulas()`, `scenario_events_migrate_to_resolved_display_names()`, `scenario_events_migrate_to_grid_slices()`, `scenario_events_min_max_migrate_to_formulas()`, and `scenario_events_migrate_to_buys_sells()` upgrade old scenario-event data after the owning pieces load.

## Scenario File Appendix

Scenario files use `scenario_data`, `scenario_state`, and `create_scenario_piece()`. Their pieces are written by `scenario_save_to_state()` and loaded by `scenario_load_from_state()`. They are not `.svv` live-game pieces, but the same fixed/dynamic/compressed file-piece rules apply.

| # | Scenario piece | Gate | Size allocation | C | Writer | Loader / consumer | Runtime data |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `resource_version` | `> SCENARIO_LAST_NO_STATIC_RESOURCES` | 4 | no | `buffer_write_u32(RESOURCE_CURRENT_VERSION)` | `resource_set_mapping` | Scenario resource mapping |
| 2 | `graphic_ids` | current | `GRID_SIZE_BUF_U16` | no | `map_image_save_state_legacy` | `map_image_load_state_legacy` / terrain load | Legacy scenario image ids |
| 3 | `edge` | current | `GRID_SIZE_BUF_U8` | no | `map_property_save_state` | `map_property_load_state` | Draw-tile / edge flags |
| 4 | `terrain` | current | `GRID_SIZE_BUF_U16` | no | `map_terrain_save_state_legacy` | `map_terrain_load_state` | Terrain grid |
| 5 | `bitfields` | current | `GRID_SIZE_BUF_U8` or `GRID_SIZE_BUF_U16` | no | `map_property_save_state` | `map_property_load_state` / `_u8` | Map property flags |
| 6 | `random` | current | `GRID_SIZE_BUF_U8` | no | `map_random_save_state` | `map_random_load_state` | Per-tile random bytes |
| 7 | `elevation` | current | `GRID_SIZE_BUF_U8` | no | `map_elevation_save_state` | `map_elevation_load_state` | Elevation grid |
| 8 | `random_iv` | current | 8 | no | `random_save_state` | `random_load_state` | RNG state |
| 9 | `camera` | current | 8 | no | `city_view_save_scenario_state` | `city_view_load_scenario_state` | Scenario editor camera |
| 10 | `scenario` | current | version-specific scenario buffer | no | `scenario_save_state` | `scenario_load_state` | Main scenario data |
| 11 | `requests` | `> SCENARIO_LAST_NO_EXTENDED_REQUESTS` | dynamic | yes | `scenario_request_save_state` | `scenario_request_load_state` | Scenario requests |
| 12 | `invasions` | `> SCENARIO_LAST_STATIC_ORIGINAL_DATA` | dynamic | yes | `scenario_invasion_save_state` | `scenario_invasion_load_state` | Scenario invasions |
| 13 | `demand_changes` | same as above | dynamic | yes | `scenario_demand_change_save_state` | `scenario_demand_change_load_state` | Demand changes |
| 14 | `price_changes` | same as above | dynamic | yes | `scenario_price_change_save_state` | `scenario_price_change_load_state` | Price changes |
| 15 | `allowed_buildings` | same as above | dynamic | yes | `scenario_allowed_building_save_state` | `scenario_allowed_building_load_state` | Allowed building table |
| 16 | `custom_variables` | same as above | dynamic | yes | `scenario_custom_variable_save_state` | `scenario_custom_variable_load_state` | Custom variables |
| 17 | `scenario_events` | `> SCENARIO_LAST_NO_EVENTS` | dynamic | yes | `scenario_events_save_state` | `scenario_events_load_state` | Scenario event info |
| 18 | `scenario_conditions` | same as above | dynamic | yes | `scenario_events_save_state` | `scenario_events_load_state` | Event conditions |
| 19 | `scenario_actions` | same as above | dynamic | yes | `scenario_events_save_state` | `scenario_events_load_state` | Event actions |
| 20 | `custom_messages` | `> SCENARIO_LAST_NO_CUSTOM_MESSAGES` | dynamic | yes | `custom_messages_save_state` | `custom_messages_load_state` | Custom messages |
| 21 | `custom_media` | same as above | dynamic | yes | `custom_media_save_state` | `custom_messages_load_state` | Custom media |
| 22 | `message_media_text_blob` | same as above | dynamic | yes | `message_media_text_blob_save_state` | `message_media_text_blob_load_state` | Media text bytes |
| 23 | `message_media_metadata` | same as above | dynamic | yes | `message_media_text_blob_save_state` | `message_media_text_blob_load_state` | Media text metadata |
| 24 | `empire` | `> SCENARIO_LAST_UNVERSIONED` | dynamic | yes | `empire_object_save` | `empire_object_load` | Custom empire object graph |
| 25 | `empire_map` | `> SCENARIO_LAST_NO_CUSTOM_EMPIRE_MAP_IMAGE` | dynamic | no | `empire_save_custom_map` | `empire_load_custom_map` | Custom empire map image |
| 26 | `model_data` | `> SCENARIO_LAST_NO_FORMULAS_AND_MODEL_DATA` | dynamic | no | `model_save_model_data` | `model_load_model_data` | Scenario model overrides |
| 27 | `scenario_formulas` | same as above | dynamic | yes | `scenario_events_save_state` | `scenario_events_load_state` | Scenario formulas |
| 28 | `production_rates` | same as above | dynamic | yes | `production_rates_save` | `production_rates_load` | Scenario production rates |
| 29 | `end_marker` | current | 4 | no | reserved skip | reserved skip | Scenario end marker |

