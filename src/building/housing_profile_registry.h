#pragma once

#include "building/HousingProfileDef.h"
#include "game/mod_definition_loader.h"

#include <string>
#include <vector>

namespace building_type_registry_impl {

const HousingProfileDef *find_housing_profile_definition(const char *path);
const HousingProfileDef *find_housing_profile_definition_for_compatibility_level(int level);
const HousingProfileDef *find_housing_profile_definition_for_legacy_building_path(const char *path);
int housing_profile_compatibility_level_count();
int housing_profile_compatibility_level_at(int index);
const mod_definition::DefinitionOverlayEntry *find_housing_profile_definition_overlay(const char *path);

} // namespace building_type_registry_impl

int housing_profile_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason = nullptr);
int housing_profile_registry_load();
