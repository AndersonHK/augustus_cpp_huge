#pragma once

#include "building/building_type.h"
#include <cstdint>

struct RubbleState {
    uint16_t original_grid_offset = 0;
    uint8_t original_orientation = 0;
    const building_type_registry_impl::BuildingType *original_type = nullptr;

    bool same_origin(const RubbleState &other) const
    {
        return original_grid_offset == other.original_grid_offset &&
            original_orientation == other.original_orientation &&
            original_type == other.original_type;
    }
};

enum class RubbleRecordDisposition {
    Keep,
    NormalizeToRubble,
    Discard
};

constexpr RubbleRecordDisposition rubble_record_disposition(
    int record_state,
    bool has_rubble_definition,
    bool has_burning_definition,
    bool has_map_presence)
{
    if (record_state != BUILDING_STATE_RUBBLE && !has_rubble_definition) {
        return RubbleRecordDisposition::Keep;
    }
    if (!has_map_presence) {
        return RubbleRecordDisposition::Discard;
    }
    if (record_state == BUILDING_STATE_RUBBLE && has_burning_definition) {
        return RubbleRecordDisposition::NormalizeToRubble;
    }
    return RubbleRecordDisposition::Keep;
}
