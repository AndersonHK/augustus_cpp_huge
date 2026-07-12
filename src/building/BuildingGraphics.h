#pragma once

#include "building/BuildingGraphicsDef.h"
#include "building/BuildingGraphicsState.h"

class Building;
class building_runtime;

class BuildingGraphics {
public:
    BuildingGraphics() = default;
    BuildingGraphics(
        Building &owner,
        const building_type_registry_impl::BuildingGraphicsDef *definition,
        BuildingGraphicsState *state);

    void bind(
        Building &owner,
        const building_type_registry_impl::BuildingGraphicsDef *definition,
        BuildingGraphicsState *state);
    void bind_owner(Building &owner);
    const building_type_registry_impl::BuildingGraphicsDef *definition() const;
    BuildingGraphicsState *state();
    const BuildingGraphicsState *state() const;
    int is_bound() const;

    unsigned char variant() const;
    int rotation() const;
    int set_variant(int variant);
    void assign_variant(int force_reseed);
    int draw_footprint(const BuildingDrawContext &ctx) const;
    int draw_top(const BuildingDrawContext &ctx) const;
    int draw_animation(const BuildingDrawContext &ctx) const;
    int draw_gatehouse_overlay(const BuildingDrawContext &ctx, int view_orientation) const;
    int mothball_status_icon_offset(int grid_offset, int icon_width, int icon_height, int *x, int *y) const;

private:
    building_runtime *runtime_instance() const;
    int state_is_runtime_state(building_runtime *runtime) const;

    Building *owner_ = nullptr;
    const building_type_registry_impl::BuildingGraphicsDef *definition_ = nullptr;
    BuildingGraphicsState *state_ = nullptr;
};
