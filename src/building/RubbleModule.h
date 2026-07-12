#pragma once

#include "building/RubbleDef.h"
#include "building/RubbleState.h"

class RubbleModule {
public:
    RubbleModule() = default;

    void bind(const RubbleDef *definition, RubbleState *state);
    int is_rubble() const;
    int is_burning() const;

    const RubbleDef *definition() const;
    RubbleState *state();
    const RubbleState *state() const;

    const building_type_registry_impl::BuildingType *original_type() const;

private:
    const RubbleDef *definition_ = nullptr;
    RubbleState *state_ = nullptr;
};
