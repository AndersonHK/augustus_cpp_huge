#pragma once

#include "city/warning.h"
#include "editor/tool.h"
#include "map/point.h"

#include <vector>

namespace building_type_registry_impl { class FoundationDef; }


int editor_tool_can_place_flag(tool_type type, const map_tile *tile, warning_type *warning);

int editor_tool_can_place_access_ramp(const map_tile *tile, int *orientation_index);

int editor_tool_can_place_building(
    const map_tile *tile,
    const building_type_registry_impl::FoundationDef &foundation,
    int rotation,
    std::vector<int> *blocked_tiles);

int editor_tool_can_place_custom_earthquake(const map_tile *tile);

