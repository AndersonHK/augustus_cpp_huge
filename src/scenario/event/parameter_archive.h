#pragma once

#include "scenario/event/parameter_data.h"
#include "building/building_type_id_bridge.h"
#include "building/god_id_bridge.h"
#include "city/constants.h"
#include "game/resource_id_bridge.h"
#include "core/Logger.h"

enum class ScenarioParameterArchive { Legacy, NativeRuntimeIds, Keyed };

// Parameter metadata is shared by XML, the editor and the binary boundary.
// Negative building selectors identify categories rather than definitions.
inline int scenario_parameter_archive_value(parameter_type type, int value, bool writing, ScenarioParameterArchive format)
{
    switch (type) {
        case PARAMETER_TYPE_RESOURCE:
        case PARAMETER_TYPE_ROUTE_RESOURCE:
            return writing ? resource_id_bridge_save_id_from_runtime(static_cast<resource_type>(value)) : resource_remap(value);
        case PARAMETER_TYPE_BUILDING:
        case PARAMETER_TYPE_BUILDING_COUNTING:
        case PARAMETER_TYPE_ALLOWED_BUILDING:
        case PARAMETER_TYPE_MODEL:
        case PARAMETER_TYPE_HOUSING_BUILDING:
        case PARAMETER_TYPE_CONSTRUCTION_BUILDING:
            if (value <= 0) return value;
            if (!writing && format == ScenarioParameterArchive::NativeRuntimeIds) {
                Logger::warning("Repairing legacy scenario building selector stored without its ledger identity", nullptr, value);
                return building_type_id_bridge_text_from_runtime(static_cast<building_type>(value)) ? value : BUILDING_NONE;
            }
            return writing ? building_type_id_bridge_save_id_from_runtime(static_cast<building_type>(value)) : building_type_id_bridge_runtime_from_save_id(static_cast<uint16_t>(value));
        case PARAMETER_TYPE_GOD:
            if (value < 0 || value == GOD_ALL || (!writing && format == ScenarioParameterArchive::NativeRuntimeIds)) return value;
            return writing ? god_id_bridge_save_id_from_runtime(value) : god_id_bridge_runtime_from_save_id(static_cast<uint16_t>(value));
        default: return value;
    }
}

inline parameter_type scenario_action_archive_parameter_type(const scenario_action_t &action, int parameter)
{
    int minimum = 0, maximum = 0;
    const auto type = scenario_events_parameter_data_get_action_parameter_type(action.type, parameter, &minimum, &maximum);
    return type == PARAMETER_TYPE_FLEXIBLE ? scenario_events_parameter_data_resolve_flexible_type(&action, parameter) : type;
}
