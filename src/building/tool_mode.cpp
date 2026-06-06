#include "map/bridge.h"
#include "tool_mode.h"

#include "building/building_record.h"
#include "building/building_type_api.h"
extern "C" {
#include "city/view.h"
#include "core/direction.h"
#include "map/terrain.h"
}

#include <array>

namespace {

enum class ToolTargetBehavior {
    DragArea,
    SingleTarget
};

struct ToolModeDefinition {
    const char *type_text_id;
    key_modifier_type modifier;
    int footprint_size;
    ToolTargetBehavior target_behavior;
};

static building_type xml_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static bool type_matches(building_type type, const char *text_id)
{
    building_type resolved = xml_type(text_id);
    return resolved != BUILDING_NONE && type == resolved;
}

class ModeSwitchTool {
public:
    ModeSwitchTool(
        const char *selection_type_text_id,
        bool is_drag_tool,
        std::array<const char *, 3> compatibility_aliases,
        ToolModeDefinition default_mode,
        std::array<ToolModeDefinition, 2> modes)
        : selection_type_text_id_(selection_type_text_id),
          is_drag_tool_(is_drag_tool),
          compatibility_aliases_(compatibility_aliases),
          default_mode_(default_mode),
          modes_(modes)
    {
    }

    bool handles_requested_type(building_type requested_type) const
    {
        if (type_matches(requested_type, selection_type_text_id_)) {
            return true;
        }
        for (const char *alias : compatibility_aliases_) {
            if (type_matches(requested_type, alias)) {
                return true;
            }
        }
        return false;
    }

    bool handles_selection(building_type selection_type) const
    {
        return type_matches(selection_type, selection_type_text_id_);
    }

    building_type selection_type() const
    {
        return xml_type(selection_type_text_id_);
    }

    bool is_drag_tool() const
    {
        return is_drag_tool_;
    }

    building_type resolve(building_type compatibility_alias_type, key_modifier_type modifiers) const
    {
        return xml_type(resolve_definition(compatibility_alias_type, modifiers).type_text_id);
    }

    building_type resolve_for_tile(
        building_type compatibility_alias_type,
        key_modifier_type modifiers,
        int x,
        int y,
        int grid_offset,
        int construction_in_progress) const
    {
        building_type bridge_type = resolve_bridge_hover_type(modifiers, x, y, grid_offset, construction_in_progress);
        if (bridge_type != BUILDING_NONE) {
            return bridge_type;
        }
        return resolve(compatibility_alias_type, modifiers);
    }

    void resolve_drag_points(
        building_type compatibility_alias_type,
        key_modifier_type modifiers,
        int raw_start_x,
        int raw_start_y,
        int raw_end_x,
        int raw_end_y,
        int *start_x,
        int *start_y,
        int *end_x,
        int *end_y) const
    {
        const ToolModeDefinition &mode = resolve_definition(compatibility_alias_type, modifiers);
        if (mode.target_behavior == ToolTargetBehavior::SingleTarget) {
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
        apply_footprint_offset(start_x, start_y, mode.footprint_size);
        apply_footprint_offset(end_x, end_y, mode.footprint_size);
    }

private:
    const ToolModeDefinition *find_definition_for_type(building_type type) const
    {
        if (type_matches(type, default_mode_.type_text_id)) {
            return &default_mode_;
        }
        for (const ToolModeDefinition &mode : modes_) {
            if (type_matches(type, mode.type_text_id)) {
                return &mode;
            }
        }
        return nullptr;
    }

    const ToolModeDefinition &resolve_definition(building_type compatibility_alias_type, key_modifier_type modifiers) const
    {
        for (const ToolModeDefinition &mode : modes_) {
            if ((modifiers & mode.modifier) == mode.modifier) {
                return mode;
            }
        }
        if (handles_requested_type(compatibility_alias_type)) {
            const ToolModeDefinition *definition = find_definition_for_type(compatibility_alias_type);
            if (definition) {
                return *definition;
            }
        }
        return default_mode_;
    }

    building_type resolve_bridge_hover_type(
        key_modifier_type modifiers,
        int x,
        int y,
        int grid_offset,
        int construction_in_progress) const
    {
        if (construction_in_progress || !grid_offset || !type_matches(selection_type(), "road")) {
            return BUILDING_NONE;
        }
        if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
            return BUILDING_NONE;
        }

        int length = 0;
        int direction = 0;
        grid_slice blocking_tiles = {};
        map_bridge_calculate_length_direction(x, y, &length, &direction, &blocking_tiles);

        return xml_type((modifiers & KEY_MOD_SHIFT) ? "ship_bridge" : "low_bridge");
    }

    static void apply_footprint_offset(int *x, int *y, int footprint_size)
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

    const char *selection_type_text_id_;
    bool is_drag_tool_;
    std::array<const char *, 3> compatibility_aliases_;
    ToolModeDefinition default_mode_;
    std::array<ToolModeDefinition, 2> modes_;
};

const ModeSwitchTool CLEAR_TOOL(
    "clear_land",
    true,
    { { "clear_land", "repair_land", "clear_trees" } },
    ToolModeDefinition{ "clear_land", KEY_MOD_NONE, 1, ToolTargetBehavior::DragArea },
    { {
        ToolModeDefinition{ "repair_land", KEY_MOD_CTRL, 1, ToolTargetBehavior::DragArea },
        ToolModeDefinition{ "clear_trees", KEY_MOD_SHIFT, 1, ToolTargetBehavior::DragArea },
    } });

const ModeSwitchTool ROAD_TOOL(
    "road",
    true,
    { { "road", "highway", "roadblock" } },
    ToolModeDefinition{ "road", KEY_MOD_NONE, 1, ToolTargetBehavior::DragArea },
    { {
        ToolModeDefinition{ "roadblock", KEY_MOD_CTRL, 1, ToolTargetBehavior::SingleTarget },
        ToolModeDefinition{ "highway", KEY_MOD_SHIFT, 2, ToolTargetBehavior::DragArea },
    } });

const std::array<const ModeSwitchTool *, 2> MODE_SWITCH_TOOLS = { { &CLEAR_TOOL, &ROAD_TOOL } };

const ModeSwitchTool *find_tool_for_requested_type(building_type requested_type)
{
    for (const ModeSwitchTool *tool : MODE_SWITCH_TOOLS) {
        if (tool->handles_requested_type(requested_type)) {
            return tool;
        }
    }
    return nullptr;
}

const ModeSwitchTool *find_tool_for_selection(building_type selection_type)
{
    for (const ModeSwitchTool *tool : MODE_SWITCH_TOOLS) {
        if (tool->handles_selection(selection_type)) {
            return tool;
        }
    }
    return nullptr;
}

} // namespace

extern "C" int building_tool_mode_handles_requested_type(building_type requested_type)
{
    return find_tool_for_requested_type(requested_type) != nullptr;
}

extern "C" int building_tool_mode_handles_selection(building_type selection_type)
{
    return find_tool_for_selection(selection_type) != nullptr;
}

extern "C" building_type building_tool_mode_selection_type(building_type requested_type)
{
    const ModeSwitchTool *tool = find_tool_for_requested_type(requested_type);
    if (!tool) {
        return requested_type;
    }
    return tool->selection_type();
}

extern "C" int building_tool_mode_is_drag_tool(building_type selection_type)
{
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    return tool && tool->is_drag_tool();
}

extern "C" building_type building_tool_mode_resolve(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers)
{
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    if (!tool) {
        return selection_type;
    }
    return tool->resolve(compatibility_alias_type, modifiers);
}

extern "C" building_type building_tool_mode_resolve_for_tile(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers,
    int x,
    int y,
    int grid_offset,
    int construction_in_progress)
{
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    if (!tool) {
        return selection_type;
    }
    return tool->resolve_for_tile(
        compatibility_alias_type,
        modifiers,
        x,
        y,
        grid_offset,
        construction_in_progress);
}

extern "C" void building_tool_mode_resolve_drag_points(
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
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    if (!tool) {
        *start_x = raw_start_x;
        *start_y = raw_start_y;
        *end_x = raw_end_x;
        *end_y = raw_end_y;
        return;
    }
    tool->resolve_drag_points(
        compatibility_alias_type,
        modifiers,
        raw_start_x,
        raw_start_y,
        raw_end_x,
        raw_end_y,
        start_x,
        start_y,
        end_x,
        end_y);
}
