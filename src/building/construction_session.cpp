#include "building/construction_session.h"

extern "C" {
#include "building/tool_mode.h"
#include "map/grid.h"
}

void ConstructionToolSession::clear()
{
    type = BUILDING_NONE;
    selected_type = BUILDING_NONE;
    compatibility_alias_type = BUILDING_NONE;
    raw_start = { 0, 0, 0 };
    raw_end = { 0, 0, 0 };
    start = { 0, 0, 0 };
    end = { 0, 0, 0 };
}

void ConstructionToolSession::select_requested_type(building_type requested_type, key_modifier_type modifiers)
{
    selected_type = building_tool_mode_selection_type(requested_type);
    compatibility_alias_type = requested_type;
    type = resolve_type(modifiers);
    raw_start = { 0, 0, 0 };
    raw_end = { 0, 0, 0 };
    start = { 0, 0, 0 };
    end = { 0, 0, 0 };
}

building_type ConstructionToolSession::resolve_type(key_modifier_type modifiers) const
{
    if (!selected_type) {
        return type;
    }
    if (!building_tool_mode_handles_selection(selected_type)) {
        return selected_type;
    }
    return building_tool_mode_resolve(selected_type, compatibility_alias_type, modifiers);
}

int ConstructionToolSession::sync_type(key_modifier_type modifiers)
{
    const building_type resolved_type = resolve_type(modifiers);
    if (resolved_type == type) {
        return 0;
    }
    type = resolved_type;
    sync_drag_points(modifiers);
    return 1;
}

void ConstructionToolSession::sync_drag_points(key_modifier_type modifiers)
{
    if (!raw_start.grid_offset) {
        return;
    }

    const int raw_end_x = raw_end.grid_offset ? raw_end.x : raw_start.x;
    const int raw_end_y = raw_end.grid_offset ? raw_end.y : raw_start.y;
    int start_x = raw_start.x;
    int start_y = raw_start.y;
    int end_x = raw_end_x;
    int end_y = raw_end_y;

    if (selected_type && building_tool_mode_handles_selection(selected_type)) {
        building_tool_mode_resolve_drag_points(
            selected_type,
            compatibility_alias_type,
            modifiers,
            raw_start.x,
            raw_start.y,
            raw_end_x,
            raw_end_y,
            &start_x,
            &start_y,
            &end_x,
            &end_y);
    }

    set_tile(&start, start_x, start_y);
    set_tile(&end, end_x, end_y);
}

int ConstructionToolSession::selection_is_drag_tool() const
{
    return selected_type &&
        building_tool_mode_handles_selection(selected_type) &&
        building_tool_mode_is_drag_tool(selected_type);
}

void ConstructionToolSession::set_raw_start(int x, int y, int grid_offset)
{
    raw_start = { x, y, grid_offset };
    raw_end = raw_start;
}

void ConstructionToolSession::set_raw_end(int x, int y, int grid_offset)
{
    raw_end = { x, y, grid_offset };
}

void ConstructionToolSession::force_type(building_type new_type)
{
    type = new_type;
}

void ConstructionToolSession::set_tile(map_tile *tile, int x, int y)
{
    tile->x = x;
    tile->y = y;
    tile->grid_offset = map_grid_offset(x, y);
}
