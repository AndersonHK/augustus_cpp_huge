#pragma once

#include "building/building_fwd.h"


int building_variant_has_variants(building_type type);

int building_variant_get_number_of_variants(building_type type);


class Building;

int building_variant_get_graphics_option(const Building &building, int force_reseed, unsigned char graphics_variant);
