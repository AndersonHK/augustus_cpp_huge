#pragma once

#include "figure/figure.h"

namespace figuretype {

class CartPusher : public Figure {
public:
    void draw(building_info_context *c);
    int returning_empty() const;
};

} // namespace figuretype


void figure_cartpusher_action(Figure *f);

void figure_warehouseman_action(Figure *f);
