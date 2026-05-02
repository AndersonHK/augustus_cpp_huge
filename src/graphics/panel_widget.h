#pragma once

#include "graphics/ui_widget.h"

enum class PanelWidgetStyle {
    Outer,
    Unbordered,
    Inner
};

class PanelWidget : public UiWidget {
public:
    PanelWidget(
        UiPrimitives &primitives,
        PanelWidgetStyle style,
        int x,
        int y,
        int width_blocks,
        int height_blocks,
        color_t color);

    void draw() const;

private:
    void draw_outer() const;
    void draw_unbordered() const;
    void draw_inner() const;

    PanelWidgetStyle style_;
    int x_;
    int y_;
    int width_blocks_;
    int height_blocks_;
    color_t color_;
};
