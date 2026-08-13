#include "formation_layout.h"

namespace {

int positive_or(int value, int fallback)
{
    return value > 0 ? value : fallback;
}

int clamped_slot_count(int live_slot_count, int declared_capacity)
{
    if (live_slot_count <= 0) {
        return 0;
    }
    if (declared_capacity > 0 && live_slot_count > declared_capacity) {
        return declared_capacity;
    }
    return live_slot_count;
}

FormationLayoutPosition computed_position(int index, int declared_capacity, FormationLayoutFootprint footprint)
{
    const int desired_capacity = positive_or(declared_capacity, index + 1);
    const int footprint_columns = positive_or(footprint.width, 1);
    const int footprint_rows = positive_or(footprint.height, 1);
    const int footprint_capacity = footprint_columns * footprint_rows;
    int columns = footprint_columns;
    if (desired_capacity <= footprint_capacity && desired_capacity < columns) {
        columns = desired_capacity;
    }
    if (columns <= 0) {
        columns = 1;
    }
    return {index % columns, index / columns};
}

FormationLayoutPosition position_for_slot(
    const FormationLayoutDef *layout,
    int index,
    int declared_capacity,
    FormationLayoutFootprint footprint)
{
    if (layout && index >= 0 && index < layout->authored_position_count()) {
        return layout->authored_position(index);
    }
    return computed_position(index, declared_capacity, footprint);
}

} // namespace

FormationLayoutFootprint formation_layout_legacy_footprint()
{
    return {4, 4};
}

FormationLayoutPosition formation_layout_position(
    const FormationLayoutDef *layout,
    int index,
    int declared_capacity)
{
    return formation_layout_position(
        layout,
        index,
        declared_capacity,
        formation_layout_legacy_footprint());
}

FormationLayoutPosition formation_layout_position(
    const FormationLayoutDef *layout,
    int index,
    int declared_capacity,
    FormationLayoutFootprint footprint)
{
    if (index < 0) {
        return {0, 0};
    }
    return position_for_slot(layout, index, declared_capacity, footprint);
}

std::vector<FormationLayoutPosition> formation_layout_positions(
    const FormationLayoutDef *layout,
    int live_slot_count,
    int declared_capacity)
{
    return formation_layout_positions(
        layout,
        live_slot_count,
        declared_capacity,
        formation_layout_legacy_footprint());
}

std::vector<FormationLayoutPosition> formation_layout_positions(
    const FormationLayoutDef *layout,
    int live_slot_count,
    int declared_capacity,
    FormationLayoutFootprint footprint)
{
    const int slot_count = clamped_slot_count(live_slot_count, declared_capacity);
    std::vector<FormationLayoutPosition> positions;
    positions.reserve(slot_count);
    for (int index = 0; index < slot_count; index++) {
        positions.push_back(position_for_slot(layout, index, declared_capacity, footprint));
    }
    return positions;
}
