#include "game/file.h"
#include "translation/translation.h"
#include "game/file_editor.h"
#include "game/state.h"
#include "game/tick.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/runtime_overlay_images.h"
#include "window/logo.h"
#include "window/main_menu.h"

#include "core/hotkey_config.h"
#include "editor/editor.h"
#include "window/editor/map.h"
#include "game.h"

#include "building/building_runtime.h"
#include "building/building_type_registry_internal.h"
#include "game/performance_tracker.h"
#include "graphics/declarative_window.h"
#include "startup/startup_parser_abi.h"

#include "game/settings.h"
#include "game/campaign.h"
#include "game/mod_manager.h"
#include "scenario/scenario.h"
#include "platform/prefs.h"
#include "platform/user_path.h"
#include "sound/system.h"
#include "assets/assets.h"
#include "assets/image_group_payload_api.h"
#include "building/properties.h"
#include "city/view.h"
#include "core/config.h"
#include "core/image.h"
#include "core/locale.h"
#include "core/log.h"
#include "core/random.h"
#include "core/string.h"
#include "figure/type.h"
#include "game/Animation.h"
#include "game/speed.h"
#include "graphics/font.h"
#include "graphics/text.h"
#include "graphics/video.h"
#include "graphics/window.h"
#include "map/tile_runtime_api.h"
#include "platform/file_manager.h"
#include "scenario/property.h"
#include "sound/city.h"

#include <stdio.h>
#include <filesystem>
#include <string>

static char init_failure_message[512];

static void errlog(const char *msg)
{
    log_error(msg, 0, 0);
}

static void clear_init_failure_message(void)
{
    init_failure_message[0] = '\0';
}

static void set_init_failure_message(const char *message, const char *detail)
{
    if (detail && *detail) {
        snprintf(init_failure_message, sizeof(init_failure_message), "%s\n\n%s", message, detail);
    } else {
        snprintf(init_failure_message, sizeof(init_failure_message), "%s", message);
    }
}

static int augustus_graphics_extract_is_available()
{
    std::string graphics_path = mod_manager::augustus_graphics_path();
    while (!graphics_path.empty() && (graphics_path.back() == '/' || graphics_path.back() == '\\')) {
        graphics_path.pop_back();
    }
    if (graphics_path.empty()) {
        return 0;
    }

    std::error_code error;
    return std::filesystem::is_directory(graphics_path, error) &&
        std::filesystem::is_regular_file(graphics_path + ".graphics_extract.stamp", error);
}

static encoding_type update_encoding(void)
{
    language_type language = locale_determine_language();
    encoding_type encoding = encoding_determine(language);
    log_info("Detected encoding:", 0, encoding);
    font_set_encoding(encoding);
    translation_load(language);
    return encoding;
}

int game_pre_init(void)
{
    settings_load();
    config_load();
    performance_tracker_init(config_get(CONFIG_DEBUG_PERFORMANCE_TRACKER));
    hotkey_config_load();
    scenario_settings_init();
    game_campaign_clear();
    game_state_unpause();

    if (!lang_load(0)) {
        errlog("'c3.eng' or 'c3_mm.eng' files not found or too large.");
        return 0;
    }
    update_encoding();
    random_init();
    return 1;
}

static int is_unpatched(void)
{
    const uint8_t *delete_game = lang_get_string("main_strings.1.6");
    const uint8_t *option_menu = lang_get_string("main_strings.2.0");
    const uint8_t *difficulty_option = lang_get_string("main_strings.2.6");
    const uint8_t *help_menu = lang_get_string("main_strings.3.0");
    // Without patch, the difficulty option string does not exist and
    // getting it "falls through" to the next text group, or, for some
    // languages (pt_BR): delete game falls through to option menu
    return difficulty_option == help_menu || delete_game == option_menu;
}

static int load_initial_climate_graphics(void)
{
    if (!Image::load_climate(CLIMATE_CENTRAL, 0, 1, 1, 1)) {
        return 0;
    }
    if (!Image::load_climate(CLIMATE_NORTHERN, 0, 1, 1, 1)) {
        return 0;
    }
    if (!Image::load_climate(CLIMATE_DESERT, 0, 1, 1, 1)) {
        return 0;
    }
    return Image::load_climate(CLIMATE_CENTRAL, 0, 1, 0, 0);
}

int game_init(void)
{
    clear_init_failure_message();

    if (!load_initial_climate_graphics()) {
        const char *asset_failure_reason = assets_get_failure_reason();
        if (asset_failure_reason && *asset_failure_reason) {
            set_init_failure_message("Failed to load graphics assets.", asset_failure_reason);
        } else {
            set_init_failure_message("Failed to load main graphics.", 0);
        }
        errlog("unable to load main graphics");
        return 0;
    }
    if (!Image::load_enemy_graphics(ENEMY_0_BARBARIAN)) {
        set_init_failure_message("Failed to load enemy graphics.", 0);
        errlog("unable to load enemy graphics");
        return 0;
    }
    int missing_fonts = 0;
    if (!Image::load_fonts(encoding_get())) {
        set_init_failure_message("Failed to load the selected language font graphics.", 0);
        errlog("unable to load font graphics");
        if (encoding_get() == ENCODING_KOREAN || encoding_get() == ENCODING_JAPANESE) {
            missing_fonts = 1;
        } else {
            return 0;
        }
    }
    if (!font_load_mod_font_pack()) {
        const char *font_failure_reason = font_get_failure_reason();
        set_init_failure_message("Failed to load mod font pack.", font_failure_reason && *font_failure_reason ? font_failure_reason : 0);
        errlog("unable to load mod font pack");
        return 0;
    }
    if (!declarative_window_registry_load()) {
        const char *failure_reason = declarative_window_registry_get_failure_reason();
        set_init_failure_message("Failed to load UI window definitions.", failure_reason);
        errlog("unable to load UI window definitions");
        if (failure_reason && *failure_reason) {
            errlog(failure_reason);
        }
        return 0;
    }

    startup_parser_request_v1 startup_request = {};
    startup_request.struct_size = sizeof(startup_request);
    startup_request.abi_version = STARTUP_PARSER_ABI_VERSION;
    startup_parser_result_v1 startup_result = {};
    startup_result.struct_size = sizeof(startup_result);
    if (startup_parser_run_v1(&startup_request, &startup_result) != STARTUP_PARSER_STATUS_SUCCEEDED) {
        const std::string failure_step = startup_result.failure_step;
        const std::string failure_message = startup_result.failure_message;
        const std::string summary = failure_step.empty()
            ? "Failed to load startup definitions."
            : "Failed to load " + failure_step + ".";
        set_init_failure_message(summary.c_str(),
            failure_message.empty() ? nullptr : failure_message.c_str());
        errlog("unable to load startup definitions");
        return 0;
    }
    for (int type = 0; type < BUILDING_TYPE_MAX; ++type) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(static_cast<building_type>(type));
        if (!definition || !definition->has_race() || !definition->race().betting.enabled) continue;
        if (!declarative_window_definition(definition->race().betting.window)) {
            set_init_failure_message("Race betting window is unresolved.", definition->race().betting.window.c_str());
            errlog("race betting window is unresolved");
            return 0;
        }
    }
    building_runtime_reset();
    load_augustus_messages();
    sound_system_init();
    game_state_init();
    int actions = ACTION_NONE;
    if (missing_fonts) {
        actions |= ACTION_SHOW_MESSAGE_MISSING_FONTS;
    }
    if (is_unpatched()) {
        actions |= ACTION_SHOW_MESSAGE_MISSING_PATCH;
    }
    if (!augustus_graphics_extract_is_available()) {
        actions |= ACTION_SHOW_MESSAGE_MISSING_EXTRA_ASSETS;
    }
    if (config_must_configure_user_directory()) {
        actions |= ACTION_SETUP_USER_DIR;
    } else if (!platform_file_manager_is_directory_writeable(pref_user_dir())) {
        actions |= ACTION_SHOW_MESSAGE_USER_DIR_NOT_WRITABLE;
    } else {
        platform_user_path_create_subdirectories();
    }
    if (config_get(CONFIG_UI_SHOW_INTRO_VIDEO)) {
        actions |= ACTION_SHOW_INTRO_VIDEOS;
    }

    window_logo_show(actions);
    return 1;
}

const char *game_get_init_failure_message(void)
{
    return init_failure_message;
}

static int reload_language(int is_editor, int reload_images)
{
    if (!lang_load(is_editor)) {
        if (is_editor) {
            errlog("'c3_map.eng' or 'c3_map_mm.eng' files not found or too large.");
        } else {
            errlog("'c3.eng' or 'c3_mm.eng' files not found or too large.");
        }
        return 0;
    }
    encoding_type encoding = update_encoding();
    if (!is_editor) {
        load_augustus_messages();
    }

    if (!Image::load_fonts(encoding)) {
        errlog("unable to load font graphics");
        return 0;
    }
    if (!font_load_mod_font_pack()) {
        errlog("unable to load mod font pack");
        return 0;
    }
    if (!Image::load_climate(scenario_property_climate(), is_editor, reload_images, 0, 0)) {
        errlog("unable to load main graphics");
        return 0;
    }

    resource_init();

    return 1;
}

int game_init_editor(void)
{
    if (!reload_language(1, 0)) {
        return 0;
    }

    game_file_editor_clear_data();
    game_file_editor_create_scenario(2);

    if (city_view_is_sidebar_collapsed()) {
        city_view_toggle_sidebar();
    }

    editor_set_active(1);
    window_editor_map_show();
    return 1;
}

void game_exit_editor(void)
{
    if (!reload_language(0, 0)) {
        return;
    }
    editor_set_active(0);
    window_main_menu_show(1);
}

int game_reload_language(void)
{
    return reload_language(editor_is_active(), 1);
}

void game_run(void)
{
    Animation::update_timers();
    int num_ticks = game_speed_get_elapsed_ticks();
    int processed_ticks = 0;
    for (int i = 0; i < num_ticks; i++) {
        {
            PerformanceTrackerScope tick_scope(PERFORMANCE_TRACKER_BUCKET_TICK);
            game_tick_run();
            game_file_write_mission_saved_game();
        }
        processed_ticks++;

        if (window_is_invalid()) {
            break;
        }
    }
    performance_tracker_record_ticks_processed(processed_ticks);
}

void game_draw(void)
{
    window_draw(0);
    sound_city_play();
}

void game_display_fps(int fps)
{
    int x_offset = 8;
    int y_offset = 24;
    int width = 24;
    int height = 20;
    graphics_draw_rect(x_offset, y_offset, width + 2, height + 2, COLOR_BLACK);
    graphics_fill_rect(x_offset + 1, y_offset + 1, width, height, COLOR_WHITE);
    text_draw_number_centered_colored(fps, x_offset, y_offset + 6, width, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height), COLOR_BLACK);
}

void game_exit(void)
{
    // Tear down runtime-managed image groups before payload storage so shutdown does not
    // depend on C++ static destruction order between the two caches.
    building_runtime_reset();
    tile_runtime_reset();
    image_group_payload_clear_all();
    runtime_overlay_images_reset();
    image_manager().clear();
    font_reset_mod_font_pack();

    video_shutdown();
    settings_save();
    config_save();
    performance_tracker_shutdown();
    sound_system_shutdown();
}
