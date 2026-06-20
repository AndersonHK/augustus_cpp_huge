#pragma once

#include "figure/figure.h"
#include "graphics/runtime_texture.h"

void figure_runtime_reset();
void figure_runtime_initialize_city();
void figure_runtime_on_created(Figure *f);
void figure_runtime_on_deleted(Figure *f);

// Creates a native FigureType walker using an explicit profile-owned start contract.
Figure *figure_runtime_create_profiled(
    figure_type type,
    int x,
    int y,
    direction_type dir,
    Building &building,
    const char *profile_id);

// Rebinds an existing native figure to a profile after legacy creation/load code sets owner state.
int figure_runtime_bind_profile(Figure *f, const char *profile_id);

// Executes a native FigureType controller; returns zero when legacy action should handle it.
int figure_runtime_execute(Figure *f);

// Updates legacy-action figures from their XML graphics policy.
int figure_runtime_update_graphics(Figure *f);
int figure_runtime_has_native_graphics(const Figure *f);
const RuntimeDrawSlice *figure_runtime_graphic_slice(const Figure *f);
int figure_runtime_graphic_sprite_offset(const Figure *f, int *x, int *y);

// Lets XML pathing policies override a vanilla roaming direction at intersections.
int figure_runtime_choose_roaming_direction(
    Figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction);

// Records pathing-only road recency for smart service walkers.
void figure_runtime_record_road_service_visit(Figure *f);
