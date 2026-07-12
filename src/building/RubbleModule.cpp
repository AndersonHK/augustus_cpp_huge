#include "building/RubbleModule.h"

void RubbleModule::bind(const RubbleDef *definition, RubbleState *state)
{
    definition_ = definition;
    state_ = state;
}

int RubbleModule::is_rubble() const
{
    return definition_ && definition_->is_rubble();
}

int RubbleModule::is_burning() const
{
    return definition_ && definition_->is_burning();
}

const RubbleDef *RubbleModule::definition() const
{
    return definition_;
}

RubbleState *RubbleModule::state()
{
    return state_;
}

const RubbleState *RubbleModule::state() const
{
    return state_;
}

const building_type_registry_impl::BuildingType *RubbleModule::original_type() const
{
    return state_ ? state_->original_type : nullptr;
}
