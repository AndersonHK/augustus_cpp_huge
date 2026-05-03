#pragma once

#include "graphics/ui_primitive.h"

class UiSavedRegionPrimitive : public UiPrimitive {
public:
    UiSavedRegionPrimitive(
        UiPrimitives &primitives,
        int image_id,
        int x,
        int y,
        int width,
        int height);

    int save() const;
    void draw() const;

private:
    int image_id_;
    int x_;
    int y_;
    int width_;
    int height_;
};
