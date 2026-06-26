#pragma once

#include "building/building_type.h"

#include <vector>

namespace building_construction {

using building_type_registry_impl::FoundationCellRequirement;

enum class PlacementTileState {
    Allowed,
    Forbidden
};

struct ConstructionPlacementTile {
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    int grid_offset = 0;
    FoundationCellRequirement requirement = FoundationCellRequirement::Land;
    PlacementTileState state = PlacementTileState::Allowed;
    int force_cleared = 0;
};

struct ConstructionPlacementPart {
    building_type type = BUILDING_NONE;
    const building_type_registry_impl::BuildingType *definition = nullptr;
    int x = 0;
    int y = 0;
    int grid_offset = 0;
    int size = 0;
    int foundation_rotation = -1;
    std::vector<ConstructionPlacementTile> tiles;
};

class ConstructionPlacementPlan {
public:
    ConstructionPlacementPlan(building_type type, int x, int y, int exact_coordinates, int force_place);
    ConstructionPlacementPlan(
        const building_type_registry_impl::BuildingType &definition,
        int x,
        int y,
        int exact_coordinates,
        int force_place);

    int can_place() const;
    building_type type() const;
    const building_type_registry_impl::BuildingType &definition() const;
    int has_shoreline_failure() const;
    int has_open_water_failure() const;
    int clear_cost() const;
    int cursor_x() const;
    int cursor_y() const;
    int origin_x() const;
    int origin_y() const;
    int placement_size() const;
    int waterside_orientation_absolute() const;
    int waterside_orientation_relative() const;
    const std::vector<int> &clear_offsets() const;
    const std::vector<ConstructionPlacementPart> &parts() const;

private:
    void build();
    void add_part(building_type type, int x, int y);
    void validate_part(ConstructionPlacementPart &part);
    int select_foundation_rotation(const ConstructionPlacementPart &part);
    FoundationCellRequirement requirement_for_tile(
        const ConstructionPlacementPart &part,
        int dx,
        int dy) const;
    PlacementTileState validate_tile(
        ConstructionPlacementPart &part,
        ConstructionPlacementTile &tile,
        int check_figures);
    void add_force_clear_offset(int grid_offset);

    building_type type_ = BUILDING_NONE;
    const building_type_registry_impl::BuildingType *definition_ = nullptr;
    int cursor_x_ = 0;
    int cursor_y_ = 0;
    int origin_x_ = 0;
    int origin_y_ = 0;
    int exact_coordinates_ = 0;
    int force_place_ = 0;
    int placement_size_ = 0;
    int blocked_ = 0;
    int shoreline_failure_ = 0;
    int open_water_failure_ = 0;
    int waterside_orientation_absolute_ = -1;
    int waterside_orientation_relative_ = -1;
    int clear_cost_ = 0;
    std::vector<int> clear_offsets_;
    std::vector<ConstructionPlacementPart> parts_;
};

} // namespace building_construction
