#pragma once

#include <string>

enum class RubbleType {
    None,
    Rubble,
    BurningRubble
};

namespace building_type_registry_impl {
class BuildingType;
}

class RubbleDef {
public:
    RubbleType type = RubbleType::None;
    const building_type_registry_impl::BuildingType *building_type = nullptr;
    int burn_days = 0;
    std::string decays_to;
    const building_type_registry_impl::BuildingType *decay_type = nullptr;

    void set(RubbleType rubble_type, const building_type_registry_impl::BuildingType *definition);
    int has_any() const;
    int is_rubble() const;
    int is_burning() const;
};
