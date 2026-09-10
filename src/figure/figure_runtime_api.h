#pragma once

#include "figure/figure.h"
#include "figure/PathingMode.h"

#include <cstdio>

namespace figure_type_registry_impl {
class FigureGraphicsState;
}

void figure_runtime_reset();
void figure_runtime_initialize_city();
void figure_runtime_on_created(Figure *f);
void figure_runtime_on_deleted(Figure *f);

// Resolves a saved owner id only after checking the exact figure profile's ownership contract.
// Older formats may translate their legacy action/owner representation once; current formats must serialize the profile id.
bool figure_runtime_resolve_loaded_owner(Figure *f, unsigned int saved_owner_id, bool allow_legacy_profile_translation, bool allow_legacy_owner_reference_repair, bool allow_delayed_owner_binding_bridge, bool allow_land_trade_profile_bridge, Building **resolved_owner);

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

// Runtime-owned semantic presentation for XML-authored figure graphics.
// cart_image_id is maintained only as the existing save/load bridge.
void figure_runtime_graphics_begin_update(Figure *f);
void figure_runtime_graphics_select_default_entry(Figure *f, const char *image_id);
void figure_runtime_graphics_select_default_entry_frame(Figure *f, const char *image_id, int one_based_frame);
void figure_runtime_graphics_set_default_offset(Figure *f, int x, int y);
void figure_runtime_graphics_add_required_layer(Figure *f, const char *role, const char *image_id, int one_based_frame, int x, int y, int draw_before_base);
void figure_runtime_graphics_select_directional_entry_frame(Figure *f, const char *state_id, int direction, int one_based_frame);
void figure_runtime_graphics_select_corpse_entry(Figure *f, const char *image_id);
void figure_runtime_graphics_hide_default_entry(Figure *f);
void figure_runtime_graphics_show_empty_cart(Figure *f);
void figure_runtime_graphics_show_resource_cart(Figure *f);
void figure_runtime_graphics_hide_cart(Figure *f);
const figure_type_registry_impl::FigureGraphicsState *figure_runtime_graphics_state(Figure *f);

// Lets XML pathing policies override a vanilla roaming direction at intersections.
int figure_runtime_choose_roaming_direction(
    Figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction);

// Records pathing-only road recency for smart service walkers.
void figure_runtime_record_road_service_visit(Figure *f);

void figure_runtime_debug_dump(FILE *file);
