#pragma once

#include "figure/figure.h"

namespace figuretype {

class DepotCartPusher : public Figure {
public:
    void draw(building_info_context *c);
    int is_recalled() const;
    void recall();
};

} // namespace figuretype
