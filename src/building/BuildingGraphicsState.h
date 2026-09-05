#pragma once

#include <cstdint>

class BuildingGraphicsState {
public:
    unsigned char variant() const;
    int set_variant(int variant);
    std::uint64_t generation() const;
    void invalidate();

private:
    unsigned char variant_ = 0;
    std::uint64_t generation_ = 1;
};
