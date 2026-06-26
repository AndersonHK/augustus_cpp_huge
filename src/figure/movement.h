#pragma once

#include "figure/figure.h"

#define FIGURE_REROUTE_DESTINATION_TICKS 120

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
