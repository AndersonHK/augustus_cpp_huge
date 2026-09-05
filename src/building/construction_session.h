#pragma once

#include "building/building_type.h"
#include "input/keys.h"
#include "map/point.h"

inline int construction_session_retains_resolved_type(
    const building_type_registry_impl::BuildingType &definition)
{
    return definition.bridge().is_bridge() || definition.tool().is_roadblock();
}

class ConstructionToolSession {
public:
    building_type type = BUILDING_NONE;
    building_type selected_type = BUILDING_NONE;
    building_type compatibility_alias_type = BUILDING_NONE;
    map_tile raw_start = { 0, 0, 0 };
    map_tile raw_end = { 0, 0, 0 };
    map_tile start = { 0, 0, 0 };
    map_tile end = { 0, 0, 0 };

    void clear();
    void select_requested_type(building_type requested_type, key_modifier_type modifiers);
    building_type resolve_type(key_modifier_type modifiers) const;
    building_type resolve_type(key_modifier_type modifiers, int construction_in_progress) const;
    int sync_type(key_modifier_type modifiers, int construction_in_progress);
    void sync_drag_points(key_modifier_type modifiers);
    int selection_is_drag_tool() const;
    void set_raw_start(int x, int y, int grid_offset);
    void set_raw_end(int x, int y, int grid_offset);
    void force_type(building_type new_type);

private:
    static void set_tile(map_tile *tile, int x, int y);
};
