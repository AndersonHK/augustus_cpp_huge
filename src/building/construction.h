#pragma once

#include "building/building_type.h"
#include "city/warning.h"
#include "figure/type.h"
#include "map/grid.h"


int building_construction_cycle_forward(void);

int building_construction_cycle_back(void);

int building_construction_type_can_cycle(building_type type);

int building_construction_type_num_cycles(building_type type);

int building_construction_type_cycle_steps(building_type type);

void building_construction_set_cost(int cost);
void building_construction_set_force_place_clear_cost(int cost);

void building_construction_set_type(const building_type_registry_impl::BuildingType *type, int setup_rotation);

void building_construction_clear_type(void);

int building_construction_is_auto_cycling(void);

void building_construction_toggle_auto_cycle(void);

int building_construction_can_rotate(void);

building_type building_construction_type(void);

building_type building_construction_selection_type(void);

void building_construction_set_hover_tile(int x, int y, int grid_offset);

int building_construction_cost(void);
int building_construction_force_place_clear_cost(void);
int building_construction_can_place(void);

int building_construction_size(int *x, int *y);

int building_construction_in_progress(void);

void building_construction_start(int x, int y, int grid_offset);

int building_construction_is_updatable(void);
int building_construction_force_place_active(void);

void building_construction_cancel(void);

void building_construction_update(int x, int y, int grid_offset);

figure_type building_construction_nearby_enemy_type(grid_slice *slice);

void building_construction_offset_start_from_orientation(int *x, int *y, int size);

void building_construction_place(void);
void building_construction_set_can_place(int can_place);

int building_construction_can_place_on_terrain(int x, int y, warning_type *warning, translation_key *text_key);

void building_construction_record_view_position(int view_x, int view_y, int grid_offset);
void building_construction_get_view_position(int *view_x, int *view_y);
int building_construction_get_start_grid_offset(void);
int building_construction_get_reservoir_aqueduct_preview_route(grid_slice *route, building_type *aqueduct_type);

void building_construction_reset_draw_as_constructing(void);
int building_construction_draw_as_constructing(void);

int building_construction_is_land_work_type(building_type type);
/** @brief to place a single wall tile at the given grid offset. since walls are being moved to building category,
* Every tile should be handled separately with individual building IDs
*/
int building_construction_place_wall(int grid_offset);

