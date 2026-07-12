#pragma once

class BuildingGraphicsState {
public:
    unsigned char variant() const;
    int set_variant(int variant);

private:
    unsigned char variant_ = 0;
};
