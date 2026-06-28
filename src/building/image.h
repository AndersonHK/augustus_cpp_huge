#pragma once

#include "building/building_fwd.h"

namespace building_type_registry_impl {
class BuildingType;
}

int building_image_get_garden_gate_image(int grid_offset);

int building_image_get(const Building *building);

int building_image_get_for_type(const building_type_registry_impl::BuildingType *definition);
