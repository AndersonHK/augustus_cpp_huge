#pragma once

#include "building/building_type.h"

class RubbleState {
public:
    RubbleState() = default;

    unsigned short original_grid_offset = 0;
    unsigned char original_size = 0;
    unsigned char original_orientation = 0;
    const building_type_registry_impl::BuildingType *original_type = nullptr;

    int has_original_data() const;
};
