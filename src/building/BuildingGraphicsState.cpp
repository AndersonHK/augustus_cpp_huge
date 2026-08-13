#include "building/BuildingGraphicsState.h"

unsigned char BuildingGraphicsState::variant() const
{
    return variant_;
}

int BuildingGraphicsState::set_variant(int variant)
{
    const unsigned char value = static_cast<unsigned char>(variant);
    if (variant_ == value) {
        return 0;
    }
    variant_ = value;
    invalidate();
    return 1;
}

std::uint64_t BuildingGraphicsState::generation() const
{
    return generation_;
}

void BuildingGraphicsState::invalidate()
{
    ++generation_;
    if (generation_ == 0) {
        generation_ = 1;
    }
}
