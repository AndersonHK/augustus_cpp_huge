#pragma once

#include "graphics/ui_widget.h"

class ButtonWidget : public UiWidget {
protected:
    ButtonWidget(UiPrimitives &primitives, int x, int y)
        : UiWidget(primitives)
        , x_(x)
        , y_(y)
    {
    }

    int x_;
    int y_;
};
