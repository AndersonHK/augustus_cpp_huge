#pragma once

#include "graphics/ui_primitives.h"

class UiPrimitive {
public:
    virtual ~UiPrimitive() = default;

protected:
    explicit UiPrimitive(UiPrimitives &primitives)
        : primitives_(primitives)
    {
    }

    UiPrimitives &primitives_;
};
