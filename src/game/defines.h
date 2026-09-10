#pragma once


int game_defines_load(void);
const char *game_defines_get_failure_reason(void);

int game_defines_ticks_per_day(void);
int game_defines_days_in_month(int month);
int game_defines_days_per_year(void);
int game_defines_ticks_per_month(int month);
int game_defines_ticks_per_year(void);
int game_defines_is_last_day_of_month(int month, int day);
int game_defines_is_last_day_of_year(int month, int day);
int game_defines_default_building_hit_points(void);
int game_defines_building_damage_extra_hit(void);
int game_defines_retirement_age(void);
int game_defines_fixed_workers(void);
int game_defines_fixed_worker_percentage(void);
int game_defines_enemy_retreat_speed_multiplier(void);
int game_defines_enemy_low_morale_combat_divisor(void);
int game_defines_legacy_figure_logical_units_per_source_pixel(void);
bool game_defines_ui_feature(const char *name);

int game_defines_mortality_percentage(int health_bucket, int age_decennium);
int game_defines_birth_percentage(int age_decennium);

