#include "settings.h"

#include "city/constants.h"
#include "core/buffer.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/dir.h"
#include "core/file.h"
#include "game/speed.h"
#include "sound/device.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace {

constexpr std::size_t kInfSize = 560;
constexpr std::size_t kMaxPersonalSavings = 100;
constexpr std::size_t kMaxPlayerName = 32;
constexpr std::size_t kMaxPlayerNameLength = kMaxPlayerName - 1;

static constexpr std::array<set_sound, SOUND_TYPE_MAX> k_default_sound_settings = {{
    {1, 100}, // Speech
    {1, 100}, // Effects
    {1, 100}, // City
    {1, 80},  // Music
}};
static constexpr std::array<int, 3> kEarlySoundTypes = {SOUND_TYPE_EFFECTS, SOUND_TYPE_MUSIC, SOUND_TYPE_SPEECH};
static constexpr std::array<int, 4> kAllSoundTypes = {SOUND_TYPE_EFFECTS, SOUND_TYPE_MUSIC, SOUND_TYPE_SPEECH, SOUND_TYPE_CITY};

class SettingsData {
public:
    int fullscreen{1};
    int window_width{800};
    int window_height{600};
    std::array<set_sound, SOUND_TYPE_MAX> sound_settings{k_default_sound_settings};
    int game_speed{90};
    int scroll_speed{70};
    set_difficulty difficulty{DIFFICULTY_NORMAL};
    set_tooltips tooltips{TOOLTIPS_FULL};
    int monthly_autosave{0};
    int warnings{1};
    int gods_enabled{1};
    int victory_video{0};
    int last_advisor{ADVISOR_LABOR};
    std::array<std::uint8_t, kMaxPlayerName> player_name{};
    std::array<int, kMaxPersonalSavings> personal_savings{};
    std::array<std::uint8_t, kInfSize> inf_file{};

    void set_player_name(std::string_view name)
    {
        const auto copy_len = std::min(name.size(), kMaxPlayerNameLength);
        player_name.fill(0);
        std::copy_n(name.data(), copy_len, player_name.data());
    }

    void sanitize_player_name()
    {
        const auto player_name_end = std::find(player_name.begin(), player_name.end(), std::uint8_t{0});
        const auto safe_len = std::min<std::size_t>(static_cast<std::size_t>(player_name_end - player_name.begin()), kMaxPlayerNameLength);
        player_name[safe_len] = 0;
        if (safe_len + 1 < player_name.size()) {
            std::fill(player_name.begin() + safe_len + 1, player_name.end(), std::uint8_t{0});
        }
    }
};

static SettingsData data;

template <typename Fn, std::size_t N>
static void for_sound_types(const std::array<int, N> &types, Fn &&action){ for (const int type : types) { action(type); } }

static bool valid_sound_type(int type){ return type >= 0 && type < SOUND_TYPE_MAX; }
static set_sound *sound_by_type(int type){ return valid_sound_type(type) ? &data.sound_settings[static_cast<size_t>(type)] : nullptr; }

static void set_player_name(std::string_view player_name)
{ data.set_player_name(player_name); }

static std::string_view bounded_string_view(const std::uint8_t *text, std::size_t max_size)
{ if (!text) { return {}; } const auto end = std::find(text, text + max_size, std::uint8_t{0}); return {reinterpret_cast<const char *>(text), static_cast<std::size_t>(end - text)}; }

static void load_default_settings(void)
{ data = SettingsData{}; }

static void load_settings(buffer *buf)
{
    buffer_skip(buf, 4);
    data.fullscreen = buffer_read_i32(buf);
    buffer_skip(buf, 3);
    for_sound_types(kEarlySoundTypes,
                    [buf](const int type) { data.sound_settings[type].enabled = buffer_read_u8(buf); });
    buffer_skip(buf, 6);
    data.game_speed = buffer_read_i32(buf);
    data.scroll_speed = buffer_read_i32(buf);
    buffer_read_raw(buf, data.player_name.data(), kMaxPlayerName);
    data.sanitize_player_name();
    buffer_skip(buf, 16);
    data.last_advisor = buffer_read_i32(buf);
    buffer_skip(buf, 4); //int save_game_mission_id;
    data.tooltips = static_cast<set_tooltips>(buffer_read_i32(buf));
    buffer_skip(buf, 4); //int starting_favor;
    buffer_skip(buf, 4); //int personal_savings_last_mission;
    buffer_skip(buf, 4); //int current_mission_id;
    buffer_skip(buf, 4); //int is_custom_scenario;
    data.sound_settings[SOUND_TYPE_CITY].enabled = buffer_read_u8(buf);
    data.warnings = buffer_read_u8(buf);
    data.monthly_autosave = buffer_read_u8(buf);
    buffer_skip(buf, 1); //unsigned char autoclear_enabled;
    for_sound_types(kAllSoundTypes,
                    [buf](const int type) { data.sound_settings[type].volume = buffer_read_i32(buf); });
    buffer_skip(buf, 8); // ram
    data.window_width = buffer_read_i32(buf);
    data.window_height = buffer_read_i32(buf);
    buffer_skip(buf, 8); //int max_confirmed_resolution;
    for (auto &savings : data.personal_savings) {
        savings = buffer_read_i32(buf);
    }
    data.victory_video = buffer_read_i32(buf);

    if (buffer_at_end(buf)) {
        // Settings file is from unpatched C3, use default values
        data.difficulty = DIFFICULTY_HARD;
        data.gods_enabled = 1;
    } else {
        data.difficulty = static_cast<set_difficulty>(buffer_read_i32(buf));
        data.gods_enabled = buffer_read_i32(buf);
    }
}

} // namespace

void settings_load(void)
{
    load_default_settings();

    const char *settings_file = dir_get_file_at_location("c3.inf", PATH_LOCATION_CONFIG);
    if (!settings_file) {
        return;
    }
    FILE *fp = file_open(settings_file, "rb");
    if (!fp) {
        return;
    }
    const size_t bytes_read = fread(data.inf_file.data(), 1, kInfSize, fp);
    file_close(fp);
    if (!bytes_read) {
        return;
    }

    buffer buf;
    buffer_init(&buf, data.inf_file.data(), bytes_read);
    load_settings(&buf);

    if (data.window_width + data.window_height < 500) {
        // most likely migration from Caesar 3
        data.window_width = 800;
        data.window_height = 600;
    }
    if (data.last_advisor <= ADVISOR_NONE || data.last_advisor > ADVISOR_CHIEF) {
        data.last_advisor = ADVISOR_LABOR;
    }
}

void settings_save(void)
{
    data.sanitize_player_name();

    buffer b;
    buffer *buf = &b;
    buffer_init(buf, data.inf_file.data(), kInfSize);

    buffer_skip(buf, 4);
    buffer_write_i32(buf, data.fullscreen);
    buffer_skip(buf, 3);
    for_sound_types(kEarlySoundTypes, [buf](const int type) {
        buffer_write_u8(buf, static_cast<uint8_t>(data.sound_settings[type].enabled));
    });
    buffer_skip(buf, 6);
    buffer_write_i32(buf, data.game_speed);
    buffer_write_i32(buf, data.scroll_speed);
    buffer_write_raw(buf, data.player_name.data(), kMaxPlayerName);
    buffer_skip(buf, 16);
    buffer_write_i32(buf, data.last_advisor);
    buffer_skip(buf, 4); //int save_game_mission_id;
    buffer_write_i32(buf, static_cast<int>(data.tooltips));
    buffer_skip(buf, 4); //int starting_favor;
    buffer_skip(buf, 4); //int personal_savings_last_mission;
    buffer_skip(buf, 4); //int current_mission_id;
    buffer_skip(buf, 4); //int is_custom_scenario;
    buffer_write_u8(buf, static_cast<uint8_t>(data.sound_settings[SOUND_TYPE_CITY].enabled));
    buffer_write_u8(buf, static_cast<uint8_t>(data.warnings));
    buffer_write_u8(buf, static_cast<uint8_t>(data.monthly_autosave));
    buffer_skip(buf, 1); //unsigned char autoclear_enabled;
    for_sound_types(kAllSoundTypes, [buf](const int type) { buffer_write_i32(buf, data.sound_settings[type].volume); });
    buffer_skip(buf, 8); // ram
    buffer_write_i32(buf, data.window_width);
    buffer_write_i32(buf, data.window_height);
    buffer_skip(buf, 8); //int max_confirmed_resolution;
    for (const auto savings : data.personal_savings) {
        buffer_write_i32(buf, savings);
    }
    buffer_write_i32(buf, data.victory_video);
    buffer_write_i32(buf, static_cast<int>(data.difficulty));
    buffer_write_i32(buf, data.gods_enabled);

    // Find existing file to overwrite
    const char *settings_file = dir_append_location("c3.inf", PATH_LOCATION_CONFIG);
    FILE *fp = file_open(settings_file, "wb");
    if (!fp) {
        return;
    }
    fwrite(data.inf_file.data(), 1, kInfSize, fp);
    file_close(fp);
}

int setting_fullscreen(void)
{
    return data.fullscreen;
}

void setting_window(int *width, int *height)
{
    *width = data.window_width;
    *height = data.window_height;
}

void setting_set_display(int fullscreen, int width, int height)
{
    data.fullscreen = fullscreen;
    if (!fullscreen) {
        data.window_width = width;
        data.window_height = height;
    }
}

const set_sound *setting_sound(int type)
{
    return sound_by_type(type);
}

int setting_sound_is_enabled(int type)
{
    const set_sound *sound = sound_by_type(type);
    return sound ? sound->enabled : 0;
}

void setting_toggle_sound_enabled(int type)
{
    set_sound *sound = sound_by_type(type);
    if (!sound) {
        return;
    }
    sound->enabled = sound->enabled ? 0 : 1;
}

void setting_set_sound_volume(int type, int volume)
{
    set_sound *sound = sound_by_type(type);
    if (!sound) {
        return;
    }
    sound->volume = calc_bound(volume, 0, 100);
}

void setting_reset_sound(int type, int enabled, int volume)
{
    set_sound *sound = sound_by_type(type);
    if (!sound) {
        return;
    }
    sound->enabled = enabled;
    sound->volume = calc_bound(volume, 0, 100);
}

int setting_game_speed(void)
{
    return data.game_speed;
}

int setting_set_game_speed(int speed)
{
    game_speed_get_index(speed); // validate speed
    data.game_speed = speed;
    return 1;
}

void setting_decrease_game_speed(void)
{
    int index = game_speed_get_index(data.game_speed);
    if (index > 0) {
        --index;
    }
    data.game_speed = game_speed_get_speed(index);
}

void setting_increase_game_speed(void)
{
    int index = game_speed_get_index(data.game_speed);
    if (index < TOTAL_GAME_SPEEDS - 1) {
        ++index;
    }
    data.game_speed = game_speed_get_speed(index);
}

void setting_set_default_game_speed(void)
{
    data.game_speed = game_speed_get_speed(config_get(CONFIG_GP_CH_DEFAULT_GAME_SPEED)); //previously hardcoded 70
}

int setting_scroll_speed(void)
{
    return data.scroll_speed;
}

void setting_increase_scroll_speed(void)
{
    data.scroll_speed = calc_bound(data.scroll_speed + 10, 0, 100);
}

void setting_decrease_scroll_speed(void)
{
    data.scroll_speed = calc_bound(data.scroll_speed - 10, 0, 100);
}

void setting_reset_speeds(int game_speed, int scroll_speed)
{
    data.game_speed = game_speed;
    data.scroll_speed = scroll_speed;
}

set_tooltips setting_tooltips(void)
{
    return data.tooltips;
}

void setting_cycle_tooltips(void)
{
    switch (data.tooltips) {
        case TOOLTIPS_NONE: data.tooltips = TOOLTIPS_SOME; break;
        case TOOLTIPS_SOME: data.tooltips = TOOLTIPS_FULL; break;
        default: data.tooltips = TOOLTIPS_NONE; break;
    }
}

int setting_warnings(void)
{
    return data.warnings;
}

void setting_toggle_warnings(void)
{
    data.warnings = data.warnings ? 0 : 1;
}

int setting_monthly_autosave(void)
{
    return data.monthly_autosave;
}

void setting_toggle_monthly_autosave(void)
{
    data.monthly_autosave = data.monthly_autosave ? 0 : 1;
}

int setting_gods_enabled(void)
{
    return data.gods_enabled;
}

void setting_toggle_gods_enabled(void)
{
    data.gods_enabled = data.gods_enabled ? 0 : 1;
}

set_difficulty setting_difficulty(void)
{
    return data.difficulty;
}

void setting_increase_difficulty(void)
{
    if (data.difficulty >= DIFFICULTY_VERY_HARD) {
        data.difficulty = DIFFICULTY_VERY_HARD;
    } else {
        data.difficulty = static_cast<set_difficulty>(static_cast<int>(data.difficulty) + 1);
    }
}

void setting_decrease_difficulty(void)
{
    if (data.difficulty <= DIFFICULTY_VERY_EASY) {
        data.difficulty = DIFFICULTY_VERY_EASY;
    } else {
        data.difficulty = static_cast<set_difficulty>(static_cast<int>(data.difficulty) - 1);
    }
}

int setting_victory_video(void)
{
    data.victory_video = data.victory_video ? 0 : 1;
    return data.victory_video;
}

int setting_last_advisor(void)
{
    return data.last_advisor;
}

void setting_set_last_advisor(int advisor)
{
    data.last_advisor = advisor;
}

const uint8_t *setting_player_name(void)
{
    data.sanitize_player_name();
    return data.player_name.data();
}

void setting_set_player_name(const uint8_t *player_name)
{
    set_player_name(bounded_string_view(player_name, kMaxPlayerName));
}

int setting_personal_savings_for_mission(int mission_id)
{
    return data.personal_savings[mission_id];
}

void setting_set_personal_savings_for_mission(int mission_id, int savings)
{
    data.personal_savings[mission_id] = savings;
}

void setting_clear_personal_savings(void)
{
    data.personal_savings.fill(0);
}
