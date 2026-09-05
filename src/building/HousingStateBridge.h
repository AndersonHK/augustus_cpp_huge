#pragma once

#include "building/HousingState.h"
#include "building/LegacyBuildingSaveDto.h"

HousingState housing_state_from_legacy_save(const building_save_bridge::LegacyBuildingSaveDto &legacy);
void housing_state_to_legacy_save(
    const HousingState &state,
    building_save_bridge::LegacyBuildingSaveDto &legacy);
