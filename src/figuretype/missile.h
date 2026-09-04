#pragma once

#include "figure/figure.h"

void figure_create_explosion_cloud(int x, int y, int size, int alt_sound);
void figure_create_missile(int figure_id, int x, int y, int x_dst, int y_dst, figure_type type);

void figure_explosion_cloud_action(Figure *f);
void figure_explosion_cloud_update_graphics(Figure *f);
void figure_projectile_action(Figure *f);

