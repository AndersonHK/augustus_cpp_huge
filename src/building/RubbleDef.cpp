#include "building/RubbleDef.h"

void RubbleDef::set(RubbleType rubble_type, const building_type_registry_impl::BuildingType *definition)
{
    type = rubble_type;
    building_type = definition;
}

int RubbleDef::has_any() const
{
    return type != RubbleType::None;
}

int RubbleDef::is_rubble() const
{
    return type != RubbleType::None;
}

int RubbleDef::is_burning() const
{
    return type == RubbleType::BurningRubble;
}
