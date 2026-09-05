#pragma once

#include "building/building_type.h"
#include "building/CompositionPlacementAccounting.h"

#include <vector>

struct RubbleState;

namespace building_construction {

enum class PlacementTileState {
    Allowed,
    Forbidden
};

enum class PlacementFailureReason {
    None,
    MissingDefinition,
    InvalidComposition,
    OutOfBounds,
    Figure,
    OpenWater,
    Terrain
};

struct ConstructionPlacementTile {
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    int grid_offset = 0;
    PlacementTileState state = PlacementTileState::Allowed;
    int force_cleared = 0;
    const building_type_registry_impl::FoundationCellDefinition *foundation_cell = nullptr;
    unsigned int added_terrain = 0;
    unsigned int removed_terrain = 0;
    int binds_building = 1;
    RepairRubbleOccupancy rubble = RepairRubbleOccupancy::None;
};

struct ConstructionPlacementSupersession {
    int grid_offset = 0;
    unsigned int building_id = 0;
    const building_type_registry_impl::BuildingType *existing_definition = nullptr;
    unsigned int generated_terrain = 0;
    unsigned int replacement_terrain = 0;
};

struct ConstructionPlacementPart {
    building_type type = BUILDING_NONE;
    const building_type_registry_impl::BuildingType *definition = nullptr;
    int x = 0;
    int y = 0;
    int grid_offset = 0;
    int size = 0;
    int width = 0;
    int height = 0;
    int foundation_rotation = -1;
    int building_orientation = 0;
    bool is_owner = false;
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
    // Repair plans use the destroyed building's rotation and may replace only rubble from its origin.
    ConstructionPlacementPlan(
        const building_type_registry_impl::BuildingType &definition,
        int x,
        int y,
        int exact_coordinates,
        int force_place,
        int rotation,
        const RubbleState *replaceable_rubble,
        int preferred_rotation = -1,
        bool retain_selected_rotation = false,
        bool fixed_rotation = false);

    int can_place() const;
    building_type type() const;
    const building_type_registry_impl::BuildingType &definition() const;
    int has_open_water_failure() const;
    int clear_cost() const;
    int cursor_x() const;
    int cursor_y() const;
    int origin_x() const;
    int origin_y() const;
    int placement_size() const;
    int placement_width() const;
    int placement_height() const;
    int rotation() const;
    PlacementFailureReason failure_reason() const;
    const std::vector<int> &clear_offsets() const;
    const std::vector<ConstructionPlacementSupersession> &supersessions() const;
    int replaceable_rubble_tiles() const;
    int required_rubble_tiles() const;
    int unique_occupied_tiles() const;
    int owner_charge_count() const;
    const std::vector<ConstructionPlacementPart> &parts() const;

private:
    void build();
    void reset_attempt();
    void build_rotation(int rotation);
    void add_part(
        const building_type_registry_impl::BuildingType &definition,
        int x,
        int y,
        int foundation_rotation,
        int building_orientation,
        bool is_owner);
    void validate_part(ConstructionPlacementPart &part);
    PlacementTileState validate_tile(
        ConstructionPlacementPart &part,
        ConstructionPlacementTile &tile,
        int check_figures);
    void add_force_clear_offset(int grid_offset);
    unsigned int add_supersession(
        const building_type_registry_impl::BuildingType &definition,
        int grid_offset,
        unsigned int terrain,
        unsigned int replacement_terrain);
    void finalize_cell_accounting();

    building_type type_ = BUILDING_NONE;
    const building_type_registry_impl::BuildingType *definition_ = nullptr;
    int cursor_x_ = 0;
    int cursor_y_ = 0;
    int origin_x_ = 0;
    int origin_y_ = 0;
    int requested_owner_x_ = 0;
    int requested_owner_y_ = 0;
    int placement_origin_x_ = 0;
    int placement_origin_y_ = 0;
    int exact_coordinates_ = 0;
    int force_place_ = 0;
    int rotation_ = 0;
    int preferred_rotation_ = 0;
    bool has_retained_rotation_ = false;
    bool retain_selected_rotation_ = false;
    bool fixed_rotation_ = false;
    const RubbleState *replaceable_rubble_ = nullptr;
    int placement_size_ = 0;
    int placement_width_ = 0;
    int placement_height_ = 0;
    int blocked_ = 0;
    PlacementFailureReason failure_reason_ = PlacementFailureReason::None;
    int forbidden_tiles_ = 0;
    int clear_cost_ = 0;
    std::vector<int> clear_offsets_;
    std::vector<ConstructionPlacementSupersession> supersessions_;
    int replaceable_rubble_tiles_ = 0;
    int required_rubble_tiles_ = 0;
    int unique_occupied_tiles_ = 0;
    int owner_charge_count_ = 0;
    std::vector<ConstructionPlacementPart> parts_;
};

} // namespace building_construction
