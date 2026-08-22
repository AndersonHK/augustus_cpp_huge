#pragma once

#include "figure/figure.h"

#define FIGURE_REROUTE_DESTINATION_TICKS 120

constexpr int FIGURE_LEGACY_TILE_PROGRESS_MAX = 15;
constexpr int FIGURE_TILE_PROGRESS_MAX = 128;
constexpr int FIGURE_NORMAL_MOVEMENT_SPEED = 9;
constexpr int FIGURE_CROSS_COUNTRY_TILE_UNITS = FIGURE_TILE_PROGRESS_MAX;
constexpr int FIGURE_MOVEMENT_NORMALIZED_ONE = 65536;

constexpr int figure_movement_legacy_progress_to_runtime(int progress)
{
    return progress >= FIGURE_LEGACY_TILE_PROGRESS_MAX ? FIGURE_TILE_PROGRESS_MAX : (progress * FIGURE_TILE_PROGRESS_MAX + FIGURE_LEGACY_TILE_PROGRESS_MAX / 2) / FIGURE_LEGACY_TILE_PROGRESS_MAX;
}

constexpr int figure_movement_runtime_progress_to_legacy(int progress)
{
    return progress >= FIGURE_TILE_PROGRESS_MAX ? FIGURE_LEGACY_TILE_PROGRESS_MAX : (progress * FIGURE_LEGACY_TILE_PROGRESS_MAX + FIGURE_TILE_PROGRESS_MAX / 2) / FIGURE_TILE_PROGRESS_MAX;
}

constexpr int figure_movement_advance_tile_progress(int progress)
{
    const int legacy_progress = figure_movement_runtime_progress_to_legacy(progress);
    return figure_movement_legacy_progress_to_runtime(legacy_progress + 1);
}

constexpr int figure_movement_legacy_ticks_to_runtime(int ticks)
{
    return (ticks * FIGURE_TILE_PROGRESS_MAX + FIGURE_LEGACY_TILE_PROGRESS_MAX / 2) / FIGURE_LEGACY_TILE_PROGRESS_MAX;
}

constexpr int figure_movement_normalized_progress(int progress)
{
    return progress >= FIGURE_TILE_PROGRESS_MAX ? FIGURE_MOVEMENT_NORMALIZED_ONE : progress * FIGURE_MOVEMENT_NORMALIZED_ONE / FIGURE_TILE_PROGRESS_MAX;
}

constexpr int figure_movement_normalized_cross_country_offset(int value)
{
    int offset = value % FIGURE_CROSS_COUNTRY_TILE_UNITS;
    if (offset < 0) offset += FIGURE_CROSS_COUNTRY_TILE_UNITS;
    return offset * FIGURE_MOVEMENT_NORMALIZED_ONE / FIGURE_CROSS_COUNTRY_TILE_UNITS;
}

constexpr int figure_movement_tile_to_cross_country(int tile)
{
    return tile * FIGURE_CROSS_COUNTRY_TILE_UNITS;
}

constexpr int figure_movement_tile_center_cross_country(int tile)
{
    return figure_movement_tile_to_cross_country(tile) + FIGURE_CROSS_COUNTRY_TILE_UNITS / 2;
}

constexpr int figure_movement_cross_country_to_tile(int value)
{
    return value / FIGURE_CROSS_COUNTRY_TILE_UNITS;
}

constexpr int figure_movement_cross_country_tile_offset(int value)
{
    return value % FIGURE_CROSS_COUNTRY_TILE_UNITS;
}

constexpr bool figure_movement_tile_progress_complete(int progress)
{
    return progress >= FIGURE_TILE_PROGRESS_MAX;
}

constexpr bool figure_movement_legacy_progress_round_trip_is_exact()
{
    for (int progress = 0; progress <= FIGURE_LEGACY_TILE_PROGRESS_MAX; progress++) {
        if (figure_movement_runtime_progress_to_legacy(figure_movement_legacy_progress_to_runtime(progress)) != progress) return false;
    }
    return true;
}

constexpr bool figure_movement_legacy_tick_count_is_preserved()
{
    int progress = 0;
    for (int tick = 0; tick < FIGURE_LEGACY_TILE_PROGRESS_MAX - 1; tick++) progress = figure_movement_advance_tile_progress(progress);
    if (figure_movement_tile_progress_complete(progress)) return false;
    return figure_movement_tile_progress_complete(figure_movement_advance_tile_progress(progress));
}

static_assert(figure_movement_legacy_progress_round_trip_is_exact(), "128-unit progress must reproduce every legacy render position");
static_assert(figure_movement_legacy_tick_count_is_preserved(), "128-unit progress must preserve the legacy 15-tick tile duration");

void figure_movement_init_roaming(Figure *f);

void figure_movement_move_ticks(Figure *f, int num_ticks);

void figure_movement_move_ticks_with_percentage(Figure *f, int num_ticks, int tick_percentage);

void figure_movement_move_ticks_tower_sentry(Figure *f, int num_ticks);

void figure_movement_roam_ticks(Figure *f, int num_ticks);

void figure_movement_follow_ticks(Figure *f, int num_ticks);

void figure_movement_follow_ticks_with_percentage(Figure *f, int num_ticks, int tick_percentage);

void figure_movement_advance_attack(Figure *f);

void figure_movement_set_cross_country_direction(Figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile);

void figure_movement_set_cross_country_destination(Figure *f, int x_dst, int y_dst);

int figure_movement_move_ticks_cross_country(Figure *f, int num_ticks);

int figure_movement_can_launch_cross_country_missile(int x_src, int y_src, int x_dst, int y_dst);
