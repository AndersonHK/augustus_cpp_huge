#pragma once

#include "graphics/color.h"

struct formation;
struct FigureGraphicDrawRequest;

class FormationDestination {
public:
    explicit FormationDestination(formation *owner) : owner_(owner) {}
    FormationDestination(const FormationDestination &) = delete;
    FormationDestination &operator=(const FormationDestination &other);
    ~FormationDestination() = default;

    void place(int tile_x, int tile_y);
    void clear();
    FigureGraphicDrawRequest graphic_draw_request() const;

    static int formation_at(int grid_offset);
    static void draw_at(int grid_offset, int screen_x, int screen_y, float scale, int highlighted_formation);
    static void advance_graphics();

private:
    void publish(int grid_offset);
    void unpublish();
    void draw(int screen_x, int screen_y, float scale, color_t color_mask) const;
    void advance_animation();

    formation *owner_ = nullptr;
    FormationDestination *previous_on_tile_ = nullptr;
    FormationDestination *next_on_tile_ = nullptr;
    FormationDestination *previous_published_ = nullptr;
    FormationDestination *next_published_ = nullptr;
    int grid_offset_ = -1;
    int animation_frame_ = 0;
};
