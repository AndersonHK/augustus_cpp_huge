#pragma once

#include "building/BuildingGraphics.h"

class Building;

int building_runtime_graphics_selected_option(
    const Building &building,
    const building_type_registry_impl::GraphicsTarget &target,
    unsigned char graphics_variant);
