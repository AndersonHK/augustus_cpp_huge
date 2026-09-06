#include "game/mod_settings_runtime.h"
#include "game/mod_content.h"
#include "game/file.h"
#include "game/file_io.h"
#include "game/state.h"
#include "scenario/map.h"
#include "startup/startup_definition_loader.h"
#include "graphics/window.h"
#include "core/config.h"
#include "game/defines.h"
#include "city/population.h"
#include "city/finance.h"
#include "platform/mod_options_win32.h"
#include "window/config.h"
#include "window/city.h"
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include "../../tools/catch_up_test/ui_windows.h"

namespace {
bool persist_changes = true;
void load_definitions()
{
    startup_definition_loader::Request request;
    request.load_config = false; request.load_localization = false; request.validate_mod_layout = false; request.prepare_graphics_validation = false;
    auto result = startup_definition_loader::load(request);
    if (!result.succeeded) throw std::runtime_error(result.failure_step + ": " + result.failure_message);
}
}
void mod_settings_apply(const std::string &key, int value)
{
    auto candidate = mod_content::runtime(); candidate.set(key, value);
    auto previous = mod_content::runtime();
    const bool has_city = scenario_map_size() > 0;
    const bool paused = game_state_is_paused() != 0;
    const int overlay = game_state_overlay();
    auto snapshot = std::filesystem::temp_directory_path() / ("vespasian-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".svv");
    const std::string filename = mod_content::path_text(snapshot);
    if (has_city && !game_file_io_write_saved_game(filename.c_str())) throw std::runtime_error("Cannot snapshot the current city; the setting was not changed.");
    try {
        // Owners must release old definition references before any registry is replaced.
        if (has_city) game_file_clear_scenario_data_for_save_load();
        mod_content::runtime() = std::move(candidate);
        load_definitions();
        if (has_city && game_file_load_saved_game(filename.c_str()) != FILE_LOAD_SUCCESS) throw std::runtime_error("The city could not be restored with this setting.");
        if (persist_changes) mod_content::runtime().save();
    } catch (const std::exception &error) {
        const std::string reason = error.what();
        if (has_city) game_file_clear_scenario_data_for_save_load();
        mod_content::runtime() = std::move(previous);
        load_definitions();
        if (has_city && game_file_load_saved_game(filename.c_str()) != FILE_LOAD_SUCCESS) throw std::runtime_error("City recovery failed. Recovery save: " + filename);
        if (paused) game_state_pause(); else game_state_unpause(); game_state_set_overlay(overlay);
        std::error_code ignored; std::filesystem::remove(snapshot, ignored);
        throw std::runtime_error(reason + " The previous setting and city were restored.");
    }
    if (paused) game_state_pause(); else game_state_unpause(); game_state_set_overlay(overlay);
    std::error_code ignored; std::filesystem::remove(snapshot, ignored); window_invalidate();
}
void mod_settings_validate_live_changes()
{
    const auto settings = mod_content::runtime().settings();
    const int population = city_population(), treasury = city_finance_treasury();
    struct NoPersistence { NoPersistence() { persist_changes = false; } ~NoPersistence() { persist_changes = true; } } guard;
    window_config_validate_mod_settings();
    window_city_show();
    int checked = 0;
    for (const auto &setting : settings) {
        if (!setting.effective) continue;
        const int alternate = setting.boolean ? !setting.value : setting.value == setting.maximum ? setting.minimum : setting.maximum;
        mod_settings_apply(setting.key(), alternate);
        if (city_population() != population || city_finance_treasury() != treasury) throw std::runtime_error("Live setting changed city population or treasury");
        if (setting.key() == "Augustus:RETIREMENT_AGE" && game_defines_retirement_age() != alternate) throw std::runtime_error("Retirement age did not apply immediately");
        mod_settings_apply(setting.key(), setting.value);
        if (city_population() != population || city_finance_treasury() != treasury) throw std::runtime_error("Live setting restoration changed the city");
        ++checked;
    }
      if (!checked && !settings.empty()) throw std::runtime_error("No effective mod settings were tested");
      validate_in_game_ui_windows();
}
void mod_settings_show(const std::function<void(const char *, int)> &apply_hardcoded)
{
#ifdef _WIN32
    native_options::show_dialog(GetActiveWindow(), [&apply_hardcoded] {
        std::vector<native_options::Row> rows;
        native_options::append_hardcoded_rows(rows, [](const char *key, int) { return config_get(config_key_from_name(key)); }, apply_hardcoded);
        native_options::append_mod_rows(rows, mod_content::runtime(), mod_settings_apply);
        return rows;
    });
#endif
}
