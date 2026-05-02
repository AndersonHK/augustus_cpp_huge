#pragma once

#include "core/buffer.h"
#include "figure/figure.h"

void figure_route_clear_all(void);

void figure_route_clean(void);

void figure_route_add(figure *f);

void figure_route_remove(figure *f);

int figure_route_get_current_direction(int path_id);

void figure_route_advance_tile(int path_id);

void figure_route_save_state(buffer *figures, buffer *buf_paths);

void figure_route_load_state(buffer *figures, buffer *buf_paths, int version);
