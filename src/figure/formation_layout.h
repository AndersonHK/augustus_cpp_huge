#pragma once

#include <vector>

struct FormationLayoutFootprint {
    int width;
    int height;
};

struct FormationLayoutPosition {
    int x;
    int y;
};

FormationLayoutFootprint formation_layout_legacy_footprint();

FormationLayoutPosition formation_layout_position(
    int layout,
    int index,
    int declared_capacity);

FormationLayoutPosition formation_layout_position(
    int layout,
    int index,
    int declared_capacity,
    FormationLayoutFootprint footprint);

std::vector<FormationLayoutPosition> formation_layout_positions(
    int layout,
    int live_slot_count,
    int declared_capacity);

std::vector<FormationLayoutPosition> formation_layout_positions(
    int layout,
    int live_slot_count,
    int declared_capacity,
    FormationLayoutFootprint footprint);

