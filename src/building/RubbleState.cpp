#include "building/RubbleState.h"

int RubbleState::has_original_data() const
{
    return original_grid_offset || original_size || original_orientation || original_type;
}
