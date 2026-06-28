#pragma once

#include "building/RubbleDef.h"
#include "building/RubbleState.h"

class Building;

class RubbleModule {
public:
    RubbleModule() = default;
    RubbleModule(Building &owner, const RubbleDef *definition, RubbleState *state);

    void bind(Building &owner, const RubbleDef *definition, RubbleState *state);
    void clear();

    int is_bound() const;
    int is_rubble() const;
    int is_burning() const;

    const RubbleDef *definition() const;
    RubbleState *state();
    const RubbleState *state() const;

    const building_type_registry_impl::BuildingType *original_type() const;
    int has_original_data() const;

private:
    Building *owner_ = nullptr;
    const RubbleDef *definition_ = nullptr;
    RubbleState *state_ = nullptr;
};
