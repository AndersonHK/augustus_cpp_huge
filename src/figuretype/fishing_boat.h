#pragma once

#include "figure/figure.h"
#include "figure/figure_type_registry_internal.h"
#include "map/point.h"

class FishingBoat : public Figure {
public:
    static FishingBoat &from(Figure &figure);

    int advance(const figure_type_registry_impl::FigureTypeDefinition *definition);
    void sink();

private:
    int ensure_wharf_assignment();
    void go_to_wharf(const map_point &tile);
};
