#pragma once

#include "figure/figure.h"

#define FIGURE_REROUTE_DESTINATION_TICKS 120

constexpr int FIGURE_LEGACY_TILE_PROGRESS_MAX = 15;
constexpr int FIGURE_TILE_PROGRESS_MAX = FIGURE_LEGACY_TILE_PROGRESS_MAX;
constexpr int FIGURE_CROSS_COUNTRY_TILE_UNITS = FIGURE_LEGACY_TILE_PROGRESS_MAX;

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

void figure_movement_init_roaming(Figure *f);

void figure_movement_move_ticks(Figure *f, int num_ticks);

void figure_movement_move_ticks_with_percentage(Figure *f, int num_ticks, int tick_percentage);

void figure_movement_move_ticks_tower_sentry(Figure *f, int num_ticks);

void figure_movement_roam_ticks(Figure *f, int num_ticks);

void figure_movement_follow_ticks(Figure *f, int num_ticks);

void figure_movement_follow_ticks_with_percentage(Figure *f, int num_ticks, int tick_percentage);

void figure_movement_advance_attack(Figure *f);

void figure_movement_set_cross_country_direction(
    Figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile);

void figure_movement_set_cross_country_destination(Figure *f, int x_dst, int y_dst);

int figure_movement_move_ticks_cross_country(Figure *f, int num_ticks);

int figure_movement_can_launch_cross_country_missile(int x_src, int y_src, int x_dst, int y_dst);
