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
