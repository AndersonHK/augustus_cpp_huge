#pragma once

#include "figure/figure.h"
#include "figure/PathingMode.h"

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
    const Building &building,
    const char *profile_id);

// Applies the bound/default XML movement profile to legacy action walkers.
int figure_runtime_apply_profile_movement(Figure *f);

const figure_type_registry_impl::PathingPolicy *figure_runtime_pathing_policy(Figure *f);
roadblock_permission figure_runtime_roadblock_permission(Figure *f);
figure_type_registry_impl::PathingMode::RoutePolicySelection figure_runtime_route_policy_selection(
    Figure *f,
    RouteNeighborhood neighborhood);

// Executes a native FigureType controller; returns zero when legacy action should handle it.
int figure_runtime_execute(Figure *f);

// Updates legacy-action figures from their XML graphics policy.
int figure_runtime_update_graphics(Figure *f);

// Lets XML pathing policies override a vanilla roaming direction at intersections.
int figure_runtime_choose_roaming_direction(
    Figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction);

// Records pathing-only road recency for smart service walkers.
void figure_runtime_record_road_service_visit(Figure *f);
