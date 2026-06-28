#include "building/RubbleModule.h"

RubbleModule::RubbleModule(Building &owner, const RubbleDef *definition, RubbleState *state)
{
    bind(owner, definition, state);
}

void RubbleModule::bind(Building &owner, const RubbleDef *definition, RubbleState *state)
{
    owner_ = &owner;
    definition_ = definition;
    state_ = state;
}

void RubbleModule::clear()
{
    owner_ = nullptr;
    definition_ = nullptr;
    state_ = nullptr;
}

int RubbleModule::is_bound() const
{
    return owner_ && definition_ && state_;
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
    if (state_ && state_->original_type) {
        return state_->original_type;
    }
    return nullptr;
}

int RubbleModule::has_original_data() const
{
    return state_ && state_->has_original_data();
}
