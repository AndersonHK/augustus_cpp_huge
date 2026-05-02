#ifndef FIGURE_RUNTIME_API_H
#define FIGURE_RUNTIME_API_H

#include "figure/figure.h"

#ifdef __cplusplus
extern "C" {
#endif

void figure_runtime_reset(void);
void figure_runtime_initialize_city(void);
void figure_runtime_on_created(figure *f);
void figure_runtime_on_deleted(figure *f);

/* Creates a native FigureType walker using an explicit profile-owned start contract. */
figure *figure_runtime_create_profiled(
    figure_type type,
    int x,
    int y,
    direction_type dir,
    unsigned int building_id,
    const char *profile_id);

/* Rebinds an existing native figure to a profile after legacy creation/load code sets owner state. */
int figure_runtime_bind_profile(figure *f, const char *profile_id);

/* Executes a native FigureType controller; returns zero when legacy action should handle it. */
int figure_runtime_execute(figure *f);

/* Lets XML pathing policies override a vanilla roaming direction at intersections. */
int figure_runtime_choose_roaming_direction(
    figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction);

/* Records pathing-only road recency for smart service walkers. */
void figure_runtime_record_road_service_visit(figure *f);

#ifdef __cplusplus
}
#endif

#endif // FIGURE_RUNTIME_API_H
