#pragma once

#include "building/building_type.h"
#include "input/keys.h"


int building_tool_mode_handles_requested_type(building_type requested_type);

int building_tool_mode_handles_selection(building_type selection_type);

building_type building_tool_mode_selection_type(building_type requested_type);

int building_tool_mode_is_drag_tool(building_type selection_type);

building_type building_tool_mode_resolve(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers);

building_type building_tool_mode_resolve_for_tile(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers,
    int x,
    int y,
    int grid_offset,
    int construction_in_progress);

void building_tool_mode_resolve_drag_points(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers,
    int raw_start_x,
    int raw_start_y,
    int raw_end_x,
    int raw_end_y,
    int *start_x,
    int *start_y,
    int *end_x,
    int *end_y);

