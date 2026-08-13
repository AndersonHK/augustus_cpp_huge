#include "map/bridge.h"
#include "tool_mode.h"

#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "city/view.h"
#include "core/direction.h"
#include "map/terrain.h"

namespace {

using building_type_registry_impl::BuildingType;
using building_type_registry_impl::SmartToolDef;
using building_type_registry_impl::SmartToolContext;
using building_type_registry_impl::SmartToolModeDefinition;
using building_type_registry_impl::SmartToolTarget;

const BuildingType *definition_for(building_type type)
{
    return building_type_registry_impl::definition_for_type(type);
}

bool is_base_drag_selection(const BuildingType &definition)
{
    return building_tool_mode_is_base_drag_selection(definition) != 0;
}

bool is_mode_switch_selection(const BuildingType &definition)
{
    return is_base_drag_selection(definition) || definition.tool().smart_tool().has_any();
}

const BuildingType *selection_owner_for_requested_type(building_type requested_type)
{
    const BuildingType *requested = definition_for(requested_type);
    if (!requested) {
        return nullptr;
    }
    if (is_mode_switch_selection(*requested)) {
        return requested;
    }

    const building_type owner_type = building_type_registry_impl::first_type_where(
        [requested](const BuildingType &candidate) {
            return is_mode_switch_selection(candidate) &&
                candidate.tool().smart_tool().mode_for_type(requested);
        });
    return definition_for(owner_type);
}

const SmartToolModeDefinition *resolved_mode(
    const BuildingType &owner,
    building_type compatibility_alias_type,
    key_modifier_type modifiers)
{
    const SmartToolDef &smart_tool = owner.tool().smart_tool();
    if (const SmartToolModeDefinition *modified = smart_tool.resolve_mode(
            (modifiers & KEY_MOD_CTRL) != 0,
            (modifiers & KEY_MOD_SHIFT) != 0)) {
        return modified;
    }

    const BuildingType *compatibility_alias = definition_for(compatibility_alias_type);
    return compatibility_alias && compatibility_alias != &owner
        ? smart_tool.mode_for_type(compatibility_alias)
        : nullptr;
}

building_type resolved_type(
    const BuildingType &owner,
    building_type compatibility_alias_type,
    key_modifier_type modifiers)
{
    const SmartToolModeDefinition *mode = resolved_mode(owner, compatibility_alias_type, modifiers);
    return mode && mode->type ? mode->type->type() : owner.type();
}

building_type resolve_dynamic_bridge_hover(
    const BuildingType &owner,
    key_modifier_type modifiers,
    int x,
    int y,
    int grid_offset,
    int construction_in_progress)
{
    // Shore probing remains procedural, but XML owns the bridge types selected
    // for this hover context. The selected road remains the session owner.
    if (construction_in_progress || !grid_offset || !owner.tool().is_road() ||
        !map_terrain_is(grid_offset, TERRAIN_WATER)) {
        return BUILDING_NONE;
    }

    int length = 0;
    int direction = 0;
    grid_slice blocking_tiles = {};
    map_bridge_calculate_length_direction(x, y, &length, &direction, &blocking_tiles);
    const SmartToolModeDefinition *mode = owner.tool().smart_tool().resolve_mode(
        (modifiers & KEY_MOD_CTRL) != 0,
        (modifiers & KEY_MOD_SHIFT) != 0,
        SmartToolContext::HoverWater);
    return mode && mode->type && mode->type->bridge().is_bridge()
        ? mode->type->type()
        : BUILDING_NONE;
}

void apply_footprint_offset(int *x, int *y, int footprint_size)
{
    if (footprint_size <= 1) {
        return;
    }

    switch (city_view_orientation()) {
        default:
            break;
        case DIR_2_RIGHT:
            *x = *x - footprint_size + 1;
            break;
        case DIR_4_BOTTOM:
            *x = *x - footprint_size + 1;
            *y = *y - footprint_size + 1;
            break;
        case DIR_6_LEFT:
            *y = *y - footprint_size + 1;
            break;
    }
}

} // namespace

int building_tool_mode_handles_requested_type(building_type requested_type)
{
    return selection_owner_for_requested_type(requested_type) != nullptr;
}

int building_tool_mode_handles_selection(building_type selection_type)
{
    const BuildingType *selection = definition_for(selection_type);
    return selection && is_mode_switch_selection(*selection);
}

building_type building_tool_mode_selection_type(building_type requested_type)
{
    const BuildingType *owner = selection_owner_for_requested_type(requested_type);
    return owner ? owner->type() : requested_type;
}

int building_tool_mode_is_drag_tool(building_type selection_type)
{
    const BuildingType *selection = definition_for(selection_type);
    return selection && is_mode_switch_selection(*selection);
}

building_type building_tool_mode_resolve(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers)
{
    const BuildingType *owner = definition_for(selection_type);
    return owner && is_mode_switch_selection(*owner)
        ? resolved_type(*owner, compatibility_alias_type, modifiers)
        : selection_type;
}

building_type building_tool_mode_resolve_context(
    building_type selection_type,
    building_type_registry_impl::SmartToolContext context,
    key_modifier_type modifiers)
{
    const BuildingType *owner = definition_for(selection_type);
    if (!owner) {
        return BUILDING_NONE;
    }
    const SmartToolModeDefinition *mode = owner->tool().smart_tool().resolve_mode(
        (modifiers & KEY_MOD_CTRL) != 0,
        (modifiers & KEY_MOD_SHIFT) != 0,
        context);
    return mode && mode->type ? mode->type->type() : BUILDING_NONE;
}

building_type building_tool_mode_resolve_for_tile(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers,
    int x,
    int y,
    int grid_offset,
    int construction_in_progress)
{
    const BuildingType *owner = definition_for(selection_type);
    if (!owner || !is_mode_switch_selection(*owner)) {
        return selection_type;
    }
    const building_type bridge = resolve_dynamic_bridge_hover(
        *owner, modifiers, x, y, grid_offset, construction_in_progress);
    return bridge != BUILDING_NONE
        ? bridge
        : resolved_type(*owner, compatibility_alias_type, modifiers);
}

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
    int *end_y)
{
    const BuildingType *owner = definition_for(selection_type);
    if (!owner || !is_mode_switch_selection(*owner)) {
        *start_x = raw_start_x;
        *start_y = raw_start_y;
        *end_x = raw_end_x;
        *end_y = raw_end_y;
        return;
    }

    const SmartToolModeDefinition *mode = resolved_mode(*owner, compatibility_alias_type, modifiers);
    if (mode && mode->target == SmartToolTarget::SingleTarget) {
        *start_x = raw_end_x;
        *start_y = raw_end_y;
        *end_x = raw_end_x;
        *end_y = raw_end_y;
        return;
    }

    *start_x = raw_start_x;
    *start_y = raw_start_y;
    *end_x = raw_end_x;
    *end_y = raw_end_y;
    const int footprint_size = mode ? mode->footprint_size : 1;
    apply_footprint_offset(start_x, start_y, footprint_size);
    apply_footprint_offset(end_x, end_y, footprint_size);
}
