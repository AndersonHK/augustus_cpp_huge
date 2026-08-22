#include "file.h"
#include "translation/translation.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_id_bridge.h"
#include "building/building_runtime.h"
#include "building/construction.h"
#include "building/granary.h"
#include "building/house_population.h"
#include "building/maintenance.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/storage.h"
#include "building/water_access_runtime.h"
#include "city/data.h"
#include "city/emperor.h"
#include "city/map.h"
#include "city/message.h"
#include "city/military.h"
#include "city/mission.h"
#include "city/victory.h"
#include "city/view.h"
#include "core/config.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/image.h"
#include "core/io.h"
#include "core/log.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/trade_prices.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/formation.h"
#include "figure/name.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "figuretype/animal.h"
#include "figuretype/water.h"
#include "game/Animation.h"
#include "game/campaign.h"
#include "game/difficulty.h"
#include "game/file_io.h"
#include "game/resource_id_bridge.h"
#include "game/settings.h"
#include "game/state.h"
#include "game/time.h"
#include "game/tutorial.h"
#include "game/undo.h"
#include "graphics/weather.h"
#include "map/aqueduct.h"
#include "map/bookmark.h"
#include "map/building.h"
#include "map/desirability.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/image_context.h"
#include "map/natives.h"
#include "map/orientation.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_network.h"
#include "map/soldier_strength.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/tile_runtime_api.h"
#include "map/tiles.h"
#include "map/water_navigation.h"
#include "platform/file_manager.h"
#include "scenario/criteria.h"
#include "scenario/custom_messages.h"
#include "scenario/demand_change.h"
#include "scenario/distant_battle.h"
#include "scenario/earthquake.h"
#include "scenario/emperor_change.h"
#include "scenario/empire.h"
#include "scenario/event/controller.h"
#include "scenario/gladiator_revolt.h"
#include "scenario/invasion.h"
#include "scenario/map.h"
#include "scenario/price_change.h"
#include "scenario/property.h"
#include "scenario/request.h"
#include "scenario/scenario.h"
#include "sound/city.h"
#include "sound/music.h"
#include "window/plain_message_dialog.h"

#include <string>
#include <string.h>

static const char MISSION_SAVED_GAMES[][32] = {
    "Citizen.svv",
    "Clerk.svv",
    "Engineer.svv",
    "Architect.svv",
    "Quaestor.svv",
    "Procurator.svv",
    "Aedile.svv",
    "Praetor.svv",
    "Consul.svv",
    "Proconsul.svv",
    "Caesar.svv",
    "Caesar2.svv"
};

void game_file_show_loaded_save_mod_mismatch_warning(void)
{
    if (!game_file_io_last_loaded_save_has_mod_mismatch()) {
        return;
    }

    static char save_mod_name[FILE_NAME_MAX + 16];
    static char active_mod_name[FILE_NAME_MAX + 16];

    snprintf(save_mod_name, sizeof(save_mod_name), "Save mod: %s", game_file_io_last_loaded_save_mod_name());
    snprintf(active_mod_name, sizeof(active_mod_name), "Active mod: %s", game_file_io_last_loaded_active_mod_name());

    window_plain_message_dialog_show_with_extra("TR_SAVEGAME_MOD_MISMATCH_TITLE",
        "TR_SAVEGAME_MOD_MISMATCH_MESSAGE", (const uint8_t *) save_mod_name, (const uint8_t *) active_mod_name);
}

static void clear_scenario_data(void)
{
    // clear data
    city_victory_reset();
    building_construction_clear_type();
    city_data_init();
    city_message_init_scenario();
    game_state_init();
    Animation::init();
    sound_city_init();
    building_menu_enable_all();
    building_clear_all();
    building_storage_clear_all();
    Figure::init_scenario();
    enemy_armies_clear();
    figure_name_init();
    formations_clear();
    building_monument_initialize_deliveries();
    Route::clearAll();
    figure_visited_buildings_init();
    scenario_events_clear();
    custom_messages_clear_all();

    game_time_init(2098);

    // Runtime tile graphics are scoped to the loaded world, just like the map grids.
    // Clear them before loading a new scenario so authored surface graphics cannot
    // survive at grid offsets that are ordinary terrain in the next world.
    tile_runtime_reset();

    // clear grids
    map_image_clear();
    map_building_clear();
    map_terrain_clear();
    map_aqueduct_clear();
    map_figure_clear();
    map_property_clear();
    map_sprite_clear();
    map_random_clear();
    map_desirability_clear();
    map_elevation_clear();
    map_soldier_strength_clear();
    map_road_network_clear();

    map_image_context_init();
    map_random_init();
}

void game_file_clear_scenario_data_for_save_load(void)
{
    clear_scenario_data();
}

static void initialize_scenario_data(const uint8_t *scenario_name)
{
    water_navigation::begin_world_load();
    water_access_runtime_begin_world_load();
    scenario_set_name(scenario_name);
    scenario_map_init();

    // initialize grids
    map_tiles_update_all_elevation();
    map_tiles_update_all_water();
    map_tiles_update_all_earthquake();
    map_tiles_update_all_rocks();
    map_tiles_add_entry_exit_flags();
    map_tiles_update_all_empty_land();
    map_tiles_update_all_meadow();
    map_tiles_update_all_rubble();
    map_tiles_update_all_roads();
    map_tiles_update_all_highways();
    map_tiles_update_all_plazas();
    map_tiles_update_all_walls();
    map_tiles_update_all_aqueducts(0);

    // Load climate before to prevent climate related images blinking
    image_load_climate(scenario_property_climate(), 0, 0, 0, 0);

    map_natives_init();

    city_view_init();

    figure_create_fishing_points();
    figure_create_herds();
    figure_create_flotsam();
    figure_visited_buildings_init();

    Route::updateAllTerrain();

    scenario_map_init_entry_exit();

    map_point entry = scenario_map_entry();
    map_point exit = scenario_map_exit();

    city_map_set_entry_point(entry.x, entry.y);
    city_map_set_exit_point(exit.x, exit.y);

    game_time_init(scenario_property_start_year());

    // set up events
    scenario_earthquake_init();
    scenario_gladiator_revolt_init();
    scenario_emperor_change_init();
    scenario_criteria_init_max_year();

    empire_init_scenario();
    traders_clear();
    scenario_invasion_init();
    city_military_determine_distant_battle_city();
    scenario_request_init();
    scenario_demand_change_init();
    scenario_price_change_init();
    building_menu_update();
    image_load_enemy(scenario_property_enemy());

    city_data_init_scenario();

    setting_set_default_game_speed();
    game_state_unpause();

    weather_reset();
    if (!building_hydrate_loaded_compositions(SAVE_GAME_CURRENT_VERSION)) log_error("Scenario building composition initialization failed", 0, 0);
    // After new city/scenario init, every live building instance is rebound to its runtime wrapper and rebuilds native
    // graphics/storage/production state.
    building_runtime_initialize_city_graphics_cache();
    water_navigation::finish_world_load();
    figure_runtime_initialize_city();
}

static void load_empire_data(int is_custom_scenario, int empire_id)
{
    empire_load(is_custom_scenario, empire_id);
    scenario_distant_battle_set_roman_travel_months();
    scenario_distant_battle_set_enemy_travel_months();
}

static int load_scenario_data(const char *scenario_file)
{
    if (!game_file_io_read_scenario(scenario_file)) {
        return 0;
    }

    trade_prices_reset();
    load_empire_data(1, scenario_empire_id());
    city_view_reset_orientation();
    return 1;
}

static int load_custom_scenario(const uint8_t *scenario_name, const char *scenario_file)
{
    clear_scenario_data();
    if (!load_scenario_data(scenario_file)) {
        return 0;
    }
    initialize_scenario_data(scenario_name);
    return 1;
}

/**
 * search for hippodrome buildings, all three pieces should have the same subtype.orientation
 */
static bool validate_loaded_hippodrome_composition(Building b)
{
    const building *record = b.record();
    if (record && record->next_part_building_id != 0 && record->prev_part_building_id != 0) {
        Building *next = Building::get(static_cast<unsigned int>(record->next_part_building_id));
        Building *prev = Building::get(static_cast<unsigned int>(record->prev_part_building_id));
        if (!next || !prev) {
            log_error("Loaded save contains an incomplete hippodrome composition", 0, b.id);
            return false;
        }
        if (b.orientation() != next->orientation() || b.orientation() != prev->orientation()) {
            log_error("Loaded save contains inconsistent hippodrome orientations", 0, b.id);
            return false;
        }
    }
    return true;
}

static bool validate_loaded_hippodromes(void)
{
    const building_type hippodrome = building_type_id_bridge_runtime_from_text("hippodrome");
    if (hippodrome == BUILDING_NONE) {
        return true;
    }
    for (Building *building = Building::first_of_type(hippodrome); building; building = building->next_of_type()) {
        if (!validate_loaded_hippodrome_composition(*building)) {
            return false;
        }
    }
    return true;
}

static bool initialize_saved_game(void)
{
    water_navigation::begin_world_load();
    water_access_runtime_begin_world_load();
    load_empire_data(!game_campaign_is_original(), scenario_empire_id());
    if (resource_id_bridge_mapping_joins_meat_and_fish()) {
        empire_city_migrate_legacy_fishing_production();
    }
    empire_city_update_trading_data(scenario_empire_id());

    map_image_context_init();
    map_image_clear();
    map_image_update_all();

    scenario_map_init();

    city_view_init();

    Route::updateAllTerrain();

    map_orientation_update_buildings();
    Route::clean();
    map_road_network_update();
    Route::updateLandTerrain();
    building_granaries_calculate_stocks();
    building_menu_update();
    city_message_init_problem_areas();

    sound_city_init();

    building_construction_clear_type();
    game_undo_disable();
    game_state_reset_overlay();

    city_mission_tutorial_set_fire_message_shown(1);
    city_mission_tutorial_set_disease_message_shown(1);

    image_load_climate(scenario_property_climate(), 0, 0, 0, 0);
    image_load_enemy(scenario_property_enemy());
    city_military_determine_distant_battle_city();

    map_natives_check_land(0);

    city_message_clear_scroll();

    setting_set_default_game_speed();

    game_state_unpause();

    weather_reset();
    // This is where restored saved buildings rebuild renderer/runtime state again after the world has finished loading.
    building_runtime_initialize_city_graphics_cache();
    map_tiles_update_all_gardens();
    map_tiles_update_all_roads();
    map_tiles_update_all_highways();
    map_tiles_update_all_plazas();
    map_tiles_update_all_aqueducts(0);
    map_building_rebind_runtime_references();
    Route::updateAllTerrain();
    map_road_network_update();
    Route::updateLandTerrain();
    water_navigation::finish_world_load();
    building_maintenance_check_rome_access();
    house_population_update_room();
    if (!Figure::resolve_loaded_building_references(SAVE_GAME_CURRENT_VERSION)) {
        log_error("World initialization failed strict figure/building reference validation", 0, 0);
        return false;
    }
    figure_runtime_initialize_city();
    map_tiles_update_all_gardens();
    map_tiles_update_all_plazas();
    map_building_rebind_runtime_references();
    city_view_restore_lookup();
    return true;
}

static int start_scenario(const uint8_t *scenario_name, const char *scenario_file)
{
    int mission = scenario_campaign_mission();
    int rank = scenario_campaign_rank();
    map_bookmarks_clear();
    int is_save_game = 0;
    const char *full_scenario_file = dir_get_file_at_location(scenario_file, PATH_LOCATION_SCENARIO);
    if (!full_scenario_file) {
        return 0;
    }
    if (!load_custom_scenario(scenario_name, full_scenario_file)) {
        uint8_t scenario_mapx_name[FILE_NAME_MAX];
        string_copy(scenario_name, scenario_mapx_name, FILE_NAME_MAX);
        if (game_file_load_saved_game(full_scenario_file) == FILE_LOAD_SUCCESS) {
            is_save_game = 1;
            scenario_set_name(scenario_mapx_name);
        } else {
            return 0;
        }
    }
    scenario_set_player_name(setting_player_name());

    scenario_set_campaign_mission(mission);
    scenario_set_campaign_rank(rank);

    scenario_settings_init_mission();
    city_emperor_init_scenario(rank);

    tutorial_init();

    if (!is_save_game) {
        scenario_events_init();
    }
    building_menu_update();
    city_message_init_scenario();

    return 1;
}

static const char *get_scenario_filename(const uint8_t *scenario_name, const char *extension, int decomposed)
{
    static char filename[FILE_NAME_MAX];
    encoding_to_utf8(scenario_name, filename, FILE_NAME_MAX, decomposed);
    if (!file_has_extension(filename, extension)) {
        file_append_extension(filename, extension, FILE_NAME_MAX);
    }
    return filename;
}

int game_file_start_scenario_from_buffer(uint8_t *data, int length, int is_save_game)
{
    buffer buf;
    buffer_init(&buf, data, length);
    int mission = scenario_campaign_mission();
    int rank = scenario_campaign_rank();
    map_bookmarks_clear();

    if (is_save_game) {
        if (game_file_io_read_save_game_from_buffer(&buf) != FILE_LOAD_SUCCESS) {
            return 0;
        }
    } else {
        clear_scenario_data();
        if (!game_file_io_read_scenario_from_buffer(&buf)) {
            return 0;
        }
        trade_prices_reset();
        load_empire_data(1, scenario_empire_id());
        city_view_reset_orientation();
    }
    if (mission == 0) {
        scenario_set_player_name(setting_player_name());
        setting_set_personal_savings_for_mission(0, 0);
    } else {
        scenario_restore_campaign_player_name();
    }

    if (is_save_game) {
        if (!validate_loaded_hippodromes() || !initialize_saved_game()) {
            return 0;
        }
        building_storage_reset_building_ids();
        scenario_set_name(game_campaign_get_scenario(mission)->name);
        city_data_init_campaign_mission();
    } else {
        initialize_scenario_data(game_campaign_get_scenario(mission)->name);
    }
    scenario_set_custom(game_campaign_is_original() ? 0 : 2);
    scenario_set_campaign_mission(mission);
    scenario_set_campaign_rank(rank);
    scenario_restore_campaign_player_name();

    if (game_campaign_is_original()) {
        scenario_settings_init_mission();
    } else {
        scenario_settings_init_favor();
        scenario_set_starting_personal_savings(setting_personal_savings_for_mission(0));
    }

    city_emperor_init_scenario(rank);

    tutorial_init();

    if (!is_save_game) {
        scenario_events_init();
    }
    building_menu_update();
    city_message_init_scenario();

    return 1;
}

int game_file_start_scenario_by_name(const uint8_t *scenario_name)
{
    if (start_scenario(scenario_name, get_scenario_filename(scenario_name, "map", 0))) {
        return 1;
    }
    if (start_scenario(scenario_name, get_scenario_filename(scenario_name, "mapx", 0))) {
        return 1;
    }
    if (start_scenario(scenario_name, get_scenario_filename(scenario_name, "map", 1))) {
        return 1;
    }
    return start_scenario(scenario_name, get_scenario_filename(scenario_name, "mapx", 1));
}

int game_file_load_saved_game(const char *filename)
{
    game_campaign_suspend();
    int result = game_file_io_read_saved_game(filename, 0);
    if (result != FILE_LOAD_SUCCESS) {
        game_campaign_restore();
        return result;
    }
    if (!game_campaign_is_active()) {
        game_campaign_clear();
    }
    if (!validate_loaded_hippodromes() || !initialize_saved_game()) {
        game_campaign_restore();
        return FILE_LOAD_WRONG_FILE_FORMAT;
    }
    building_storage_reset_building_ids();
    sound_music_update(1);
    return 1;
}

int game_file_write_saved_game(const char *filename)
{
    return game_file_io_write_saved_game(filename);
}

int game_file_make_yearly_autosave(void)
{
    int next_autosave_slot = config_get(CONFIG_GENERAL_NEXT_AUTOSAVE_SLOT);
    if (next_autosave_slot >= config_get(CONFIG_GP_CH_MAX_AUTOSAVE_SLOTS)) {
        next_autosave_slot = 0;
    }

    char current_save_name[FILE_NAME_MAX];
    char backup_save_name[FILE_NAME_MAX];
    const std::string savegame_directory = platform_file_manager_get_directory_for_location(PATH_LOCATION_SAVEGAME);

    snprintf(current_save_name, FILE_NAME_MAX, "%s%s",
        savegame_directory.c_str(), "autosave-year.svv");
    snprintf(backup_save_name, FILE_NAME_MAX, "%s%s%d%s",
        savegame_directory.c_str(), "autosave-year-bak-",
        next_autosave_slot, ".svv");

    if (file_exists(current_save_name, NOT_LOCALIZED) && !platform_file_manager_copy_file(current_save_name, backup_save_name)) {
        log_error("Unable to preserve the previous yearly autosave", backup_save_name, 0);
        return 0;
    }
    int result = game_file_write_saved_game(current_save_name);

    if (result) {
        next_autosave_slot++;
        config_set(CONFIG_GENERAL_NEXT_AUTOSAVE_SLOT, next_autosave_slot);
        config_save();
    }

    return result;
}

int game_file_delete_saved_game(const char *filename)
{
    return game_file_io_delete_saved_game(filename);
}

void game_file_write_mission_saved_game(void)
{
    if (!city_mission_should_save_start() || !game_campaign_is_active()) {
        return;
    }
    const char *filename = 0;
    char localized_filename[FILE_NAME_MAX];
    if (game_campaign_is_original()) {
        int rank = scenario_campaign_rank();
        if (rank < 0) {
            rank = 0;
        } else if (rank > 11) {
            rank = 11;
        }
        filename = MISSION_SAVED_GAMES[rank];
        if (locale_translate_rank_autosaves()) {
            encoding_to_utf8(lang_get_string(current_string_key(32, rank)), localized_filename, FILE_NAME_MAX,
                encoding_system_uses_decomposed());
            strncat_s(localized_filename, FILE_NAME_MAX, ".svv", _TRUNCATE);
            filename = localized_filename;
        }
    } else {
        uint8_t encoded_filename[FILE_NAME_MAX];
        uint8_t *cursor = string_copy(game_campaign_get_info()->name, encoded_filename, FILE_NAME_MAX);
        cursor = string_copy(string_from_ascii(" - "), cursor, FILE_NAME_MAX - (cursor - encoded_filename));
        cursor += string_from_int(cursor, scenario_campaign_mission() + 1, 0);
        cursor = string_copy(string_from_ascii(" - "), cursor, FILE_NAME_MAX - (cursor - encoded_filename));
        cursor = string_copy(game_campaign_get_scenario(scenario_campaign_mission())->name, cursor,
            FILE_NAME_MAX - (cursor - encoded_filename));
        string_copy(string_from_ascii(".svv"), cursor, FILE_NAME_MAX - (cursor - encoded_filename));
        encoding_to_utf8(encoded_filename, localized_filename, FILE_NAME_MAX, encoding_system_uses_decomposed());
        filename = localized_filename;
    }
    if (!dir_get_file_at_location(filename, PATH_LOCATION_SAVEGAME)) {
        game_file_io_write_saved_game(dir_append_location(filename, PATH_LOCATION_SAVEGAME));
    }
}
