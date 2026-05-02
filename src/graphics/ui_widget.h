#pragma once

#include "graphics/ui_primitive.h"

class UiWidget {
public:
    virtual ~UiWidget() = default;

protected:
    explicit UiWidget(UiPrimitives &primitives)
        : primitives_(primitives)
    {
    }

    UiPrimitives &primitives_;
};
