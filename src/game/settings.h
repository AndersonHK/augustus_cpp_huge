#pragma once

#include <stdint.h>

typedef enum {
    TOOLTIPS_NONE = 0,
    TOOLTIPS_SOME = 1,
    TOOLTIPS_FULL = 2
} set_tooltips;

typedef enum {
    DIFFICULTY_VERY_EASY = 0,
    DIFFICULTY_EASY = 1,
    DIFFICULTY_NORMAL = 2,
    DIFFICULTY_HARD = 3,
    DIFFICULTY_VERY_HARD = 4
} set_difficulty;

typedef struct {
    int enabled;
    int volume;
} set_sound;

void settings_load(void);

void settings_save(void);

int setting_fullscreen(void);
void setting_window(int *width, int *height);
void setting_set_display(int fullscreen, int width, int height);

const set_sound *setting_sound(int type);

int setting_sound_is_enabled(int type);
void setting_toggle_sound_enabled(int type);
void setting_set_sound_volume(int type, int volume);
void setting_reset_sound(int type, int enabled, int volume);

int setting_game_speed(void);
int setting_set_game_speed(int speed);
void setting_increase_game_speed(void);
void setting_decrease_game_speed(void);
void setting_set_default_game_speed(void);

int setting_scroll_speed(void);
void setting_increase_scroll_speed(void);
void setting_decrease_scroll_speed(void);
void setting_reset_speeds(int game_speed, int scroll_speed);

set_tooltips setting_tooltips(void);
void setting_cycle_tooltips(void);

int setting_warnings(void);
void setting_toggle_warnings(void);

int setting_monthly_autosave(void);
void setting_toggle_monthly_autosave(void);

int setting_gods_enabled(void);
void setting_toggle_gods_enabled(void);

set_difficulty setting_difficulty(void);
void setting_increase_difficulty(void);
void setting_decrease_difficulty(void);

int setting_victory_video(void);

int setting_last_advisor(void);
void setting_set_last_advisor(int advisor);

const uint8_t *setting_player_name(void);
void setting_set_player_name(const uint8_t *player_name);

int setting_personal_savings_for_mission(int mission_id);
void setting_set_personal_savings_for_mission(int mission_id, int savings);
void setting_clear_personal_savings(void);

#include <cstdint>
#include <string>
#include <string_view>

namespace settings {

enum class Tooltips : std::uint8_t { None = TOOLTIPS_NONE, Some = TOOLTIPS_SOME, Full = TOOLTIPS_FULL };
enum class Difficulty : std::uint8_t {
    VeryEasy = DIFFICULTY_VERY_EASY,
    Easy = DIFFICULTY_EASY,
    Normal = DIFFICULTY_NORMAL,
    Hard = DIFFICULTY_HARD,
    VeryHard = DIFFICULTY_VERY_HARD
};

struct Sound {
    bool enabled;
    int volume;
};

struct WindowSize {
    int width;
    int height;
};

inline Tooltips tooltips()
{
    return static_cast<Tooltips>(setting_tooltips());
}

inline Difficulty difficulty()
{
    return static_cast<Difficulty>(setting_difficulty());
}

inline Sound sound(int type)
{
    if (const set_sound *data = setting_sound(type)) {
        return {data->enabled != 0, data->volume};
    }
    return {false, 0};
}

inline bool is_fullscreen()
{
    return setting_fullscreen() != 0;
}

inline WindowSize window()
{
    WindowSize result{};
    setting_window(&result.width, &result.height);
    return result;
}

inline std::string_view player_name_view()
{
    const auto *const value = setting_player_name();
    return value ? std::string_view(reinterpret_cast<const char *>(value)) : std::string_view{};
}

inline std::string player_name()
{
    return std::string(player_name_view());
}

inline void set_player_name(std::string_view value)
{
    const std::string copy(value);
    setting_set_player_name(reinterpret_cast<const uint8_t *>(copy.c_str()));
}

} // namespace settings
