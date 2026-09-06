#include "building/construction_plan.h"
#include "building/properties.h"

#include "building/CompositionDef.h"
#include "building/PlacementRotationSelection.h"
#include "building/RubbleState.h"
#include "building/building.h"
#include "building/construction.h"
#include "building/rotation.h"
#include "building/building_type_registry_internal.h"
#include "building/water_access_runtime.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_aqueduct.h"
#include "map/terrain.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace building_construction {

using building_type_registry_impl::BuildingType;
using building_type_registry_impl::BuildingFoundation;
using building_type_registry_impl::CompositionLayoutMember;
using building_type_registry_impl::CompositionLayoutResult;
using building_type_registry_impl::FoundationCellDefinition;
using building_type_registry_impl::FoundationDef;
using building_type_registry_impl::FoundationTerrainDelta;
using building_type_registry_impl::RotatedFoundationCell;

namespace {

constexpr int FORCE_PLACE_CLEARABLE_TERRAIN = TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD;

int normalize_rotation(int rotation)
{
    return (rotation % 4 + 4) % 4;
}

int clear_land_cost()
{
    const building_type type = building_type_registry_impl::type_from_attr("clear_land");
    return type != BUILDING_NONE ? model_get_construction_cost(type) : 0;
}

int force_place_can_clear_terrain(int terrain)
{
    return terrain && !(terrain & ~FORCE_PLACE_CLEARABLE_TERRAIN);
}

int placement_part_checks_figures(const BuildingType &definition, int active_cells)
{
    const building_type_registry_impl::TileDefinition &tile = definition.tile();
    if (active_cells == 1 && tile.refresh_behavior() == building_type_registry_impl::TileRefreshBehavior::Plaza) {
        return 0;
    }
    return 1;
}

bool tile_has_bound_aqueduct_occupancy(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return false;
    }
    const Building &occupant = map_building_at(grid_offset);
    return occupant.matches("aqueduct") && map_terrain_is(grid_offset, TERRAIN_AQUEDUCT);
}

int placement_extent(const BuildingType &definition, int rotation)
{
    if (definition.has_composition()) {
        const CompositionLayoutResult layout = building_type_registry_impl::build_composition_layout(
            &definition, definition.composition(), 0, 0, rotation);
        if (layout.valid()) {
            return std::max(layout.bounds.width(), layout.bounds.height());
        }
    }
    const FoundationDef *foundation = definition.foundation_def();
    return foundation ?
        std::max(foundation->rotated_width(rotation), foundation->rotated_height(rotation)) : 0;
}

} // namespace

ConstructionPlacementPlan::ConstructionPlacementPlan(
    building_type type,
    int x,
    int y,
    int exact_coordinates,
    int force_place)
    : ConstructionPlacementPlan(
        *building_type_registry_impl::definition_for_type(type),
        x,
        y,
        exact_coordinates,
        force_place)
{
}

ConstructionPlacementPlan::ConstructionPlacementPlan(
    const BuildingType &definition,
    int x,
    int y,
    int exact_coordinates,
    int force_place)
    : ConstructionPlacementPlan(
        definition,
        x,
        y,
        exact_coordinates,
        force_place,
        [&definition]() {
            int retained_rotation = building_rotation_get_rotation();
            building_rotation_get_retained_placement_rotation(definition.type(), &retained_rotation);
            return retained_rotation;
        }(),
        nullptr,
        building_rotation_get_rotation(),
        true)
{
}

ConstructionPlacementPlan::ConstructionPlacementPlan(
    const BuildingType &definition,
    int x,
    int y,
    int exact_coordinates,
    int force_place,
    int rotation,
    const RubbleState *replaceable_rubble,
    int preferred_rotation,
    bool retain_selected_rotation,
    bool fixed_rotation)
    : type_(definition.type()),
      definition_(&definition),
      cursor_x_(x),
      cursor_y_(y),
      origin_x_(x),
      origin_y_(y),
      requested_owner_x_(x),
      requested_owner_y_(y),
      placement_origin_x_(x),
      placement_origin_y_(y),
      exact_coordinates_(exact_coordinates),
      force_place_(force_place),
      rotation_(normalize_rotation(rotation)),
      preferred_rotation_(normalize_rotation(preferred_rotation < 0 ? rotation : preferred_rotation)),
      has_retained_rotation_(preferred_rotation >= 0 && normalize_rotation(rotation) != normalize_rotation(preferred_rotation)),
      retain_selected_rotation_(retain_selected_rotation),
      fixed_rotation_(fixed_rotation),
      replaceable_rubble_(replaceable_rubble)
{
    placement_size_ = type_ == BUILDING_NONE ? 0 : placement_extent(definition, rotation_);
    if (!exact_coordinates_ && placement_size_ > 0) {
        building_construction_offset_start_from_orientation(
            &placement_origin_x_, &placement_origin_y_, placement_size_);
    }
    build();
    if (retain_selected_rotation_ && can_place()) {
        building_rotation_retain_placement_rotation(type_, rotation_);
    }
}

int ConstructionPlacementPlan::can_place() const { return !blocked_; }
building_type ConstructionPlacementPlan::type() const { return type_; }
const BuildingType &ConstructionPlacementPlan::definition() const { return *definition_; }
int ConstructionPlacementPlan::has_open_water_failure() const
{
    return failure_reason_ == PlacementFailureReason::OpenWater;
}
int ConstructionPlacementPlan::clear_cost() const { return clear_cost_; }
int ConstructionPlacementPlan::cursor_x() const { return cursor_x_; }
int ConstructionPlacementPlan::cursor_y() const { return cursor_y_; }
int ConstructionPlacementPlan::origin_x() const { return origin_x_; }
int ConstructionPlacementPlan::origin_y() const { return origin_y_; }
int ConstructionPlacementPlan::placement_size() const { return placement_size_; }
int ConstructionPlacementPlan::placement_width() const { return placement_width_; }
int ConstructionPlacementPlan::placement_height() const { return placement_height_; }
int ConstructionPlacementPlan::rotation() const { return rotation_; }
PlacementFailureReason ConstructionPlacementPlan::failure_reason() const { return failure_reason_; }

const std::vector<int> &ConstructionPlacementPlan::clear_offsets() const { return clear_offsets_; }
const std::vector<ConstructionPlacementSupersession> &ConstructionPlacementPlan::supersessions() const
{
    return supersessions_;
}
int ConstructionPlacementPlan::replaceable_rubble_tiles() const { return replaceable_rubble_tiles_; }
int ConstructionPlacementPlan::required_rubble_tiles() const { return required_rubble_tiles_; }
int ConstructionPlacementPlan::unique_occupied_tiles() const { return unique_occupied_tiles_; }
int ConstructionPlacementPlan::owner_charge_count() const { return owner_charge_count_; }
const std::vector<ConstructionPlacementPart> &ConstructionPlacementPlan::parts() const { return parts_; }

void ConstructionPlacementPlan::reset_attempt()
{
    blocked_ = 0;
    failure_reason_ = PlacementFailureReason::None;
    forbidden_tiles_ = 0;
    clear_cost_ = 0;
    clear_offsets_.clear();
    supersessions_.clear();
    replaceable_rubble_tiles_ = 0;
    required_rubble_tiles_ = 0;
    unique_occupied_tiles_ = 0;
    owner_charge_count_ = 0;
    parts_.clear();
    placement_width_ = 0;
    placement_height_ = 0;
}

void ConstructionPlacementPlan::build()
{
    if (!definition_ || type_ == BUILDING_NONE || !definition_->foundation_def() || placement_size_ <= 0) {
        blocked_ = 1;
        failure_reason_ = PlacementFailureReason::MissingDefinition;
        return;
    }

    struct FailedAttempt {
        int valid = 0;
        int rotation = 0;
        int forbidden_tiles = std::numeric_limits<int>::max();
        int failure_score = std::numeric_limits<int>::max();
        PlacementFailureReason failure_reason = PlacementFailureReason::None;
        int clear_cost = 0;
        std::vector<int> clear_offsets;
        std::vector<ConstructionPlacementSupersession> supersessions;
        int replaceable_rubble_tiles = 0;
        int required_rubble_tiles = 0;
        int unique_occupied_tiles = 0;
        int owner_charge_count = 0;
        std::vector<ConstructionPlacementPart> parts;
        int width = 0;
        int height = 0;
        int size = 0;
        int origin_x = 0;
        int origin_y = 0;
    } best_failure;

    const auto failure_score = [](PlacementFailureReason reason, int forbidden_tiles) {
        if (reason == PlacementFailureReason::MissingDefinition ||
            reason == PlacementFailureReason::InvalidComposition) {
            return 1000000 + forbidden_tiles;
        }
        return forbidden_tiles * 2 + (reason == PlacementFailureReason::OpenWater ? 0 : 1);
    };
    const int can_rotate = !fixed_rotation_ && replaceable_rubble_ == nullptr &&
        definition_->has_rotated_placement_geometry();
    const PlacementRotationCandidates candidates = placement_rotation_candidates(
        preferred_rotation_, rotation_, has_retained_rotation_, can_rotate != 0);
    for (int step = 0; step < candidates.count; ++step) {
        reset_attempt();
        const int candidate_rotation = candidates.rotations[step];
        build_rotation(candidate_rotation);
        if (!blocked_) {
            return;
        }
        const int candidate_score = failure_score(failure_reason_, forbidden_tiles_);
        if (!best_failure.valid || candidate_score < best_failure.failure_score) {
            best_failure.valid = 1;
            best_failure.rotation = candidate_rotation;
            best_failure.forbidden_tiles = forbidden_tiles_;
            best_failure.failure_score = candidate_score;
            best_failure.failure_reason = failure_reason_;
            best_failure.clear_cost = clear_cost_;
            best_failure.clear_offsets = clear_offsets_;
            best_failure.supersessions = supersessions_;
            best_failure.replaceable_rubble_tiles = replaceable_rubble_tiles_;
            best_failure.required_rubble_tiles = required_rubble_tiles_;
            best_failure.unique_occupied_tiles = unique_occupied_tiles_;
            best_failure.owner_charge_count = owner_charge_count_;
            best_failure.parts = parts_;
            best_failure.width = placement_width_;
            best_failure.height = placement_height_;
            best_failure.size = placement_size_;
            best_failure.origin_x = origin_x_;
            best_failure.origin_y = origin_y_;
        }
    }

    reset_attempt();
    blocked_ = 1;
    if (best_failure.valid) {
        rotation_ = best_failure.rotation;
        forbidden_tiles_ = best_failure.forbidden_tiles;
        failure_reason_ = best_failure.failure_reason;
        clear_cost_ = best_failure.clear_cost;
        clear_offsets_ = std::move(best_failure.clear_offsets);
        supersessions_ = std::move(best_failure.supersessions);
        replaceable_rubble_tiles_ = best_failure.replaceable_rubble_tiles;
        required_rubble_tiles_ = best_failure.required_rubble_tiles;
        unique_occupied_tiles_ = best_failure.unique_occupied_tiles;
        owner_charge_count_ = best_failure.owner_charge_count;
        parts_ = std::move(best_failure.parts);
        placement_width_ = best_failure.width;
        placement_height_ = best_failure.height;
        placement_size_ = best_failure.size;
        origin_x_ = best_failure.origin_x;
        origin_y_ = best_failure.origin_y;
    }
}

void ConstructionPlacementPlan::build_rotation(int rotation)
{
    rotation_ = normalize_rotation(rotation);
    if (definition_->has_composition()) {
        int owner_x = requested_owner_x_;
        int owner_y = requested_owner_y_;
        if (!exact_coordinates_) {
            const CompositionLayoutResult local_layout = building_type_registry_impl::build_composition_layout(
                definition_, definition_->composition(), 0, 0, rotation_);
            if (!local_layout.valid()) {
                blocked_ = 1;
                failure_reason_ = PlacementFailureReason::InvalidComposition;
                forbidden_tiles_ = 1;
                return;
            }
            owner_x = placement_origin_x_ - local_layout.bounds.min_x;
            owner_y = placement_origin_y_ - local_layout.bounds.min_y;
        }
        origin_x_ = owner_x;
        origin_y_ = owner_y;
        const CompositionLayoutResult layout = building_type_registry_impl::build_composition_layout(
            definition_, definition_->composition(), owner_x, owner_y, rotation_);
        if (!layout.valid()) {
            blocked_ = 1;
            failure_reason_ = PlacementFailureReason::InvalidComposition;
            forbidden_tiles_ = 1;
            return;
        }
        for (const CompositionLayoutMember &member : layout.members) {
            if (!member.type) {
                blocked_ = 1;
                failure_reason_ = PlacementFailureReason::MissingDefinition;
                ++forbidden_tiles_;
                continue;
            }
            add_part(
                *member.type,
                member.x,
                member.y,
                member.foundation_rotation,
                member.building_orientation,
                member.is_owner);
        }
    } else {
        origin_x_ = exact_coordinates_ ? requested_owner_x_ : placement_origin_x_;
        origin_y_ = exact_coordinates_ ? requested_owner_y_ : placement_origin_y_;
        const int foundation_rotation = definition_->foundation_def()->rotates() ? rotation_ : 0;
        add_part(*definition_, origin_x_, origin_y_, foundation_rotation, rotation_, true);
    }

    for (ConstructionPlacementPart &part : parts_) {
        validate_part(part);
    }
    if (!blocked_) {
        for (const ConstructionPlacementPart &part : parts_) {
            if (part.definition && part.definition->water_access().requires_open_water() &&
                !water_access_runtime_building_type_has_open_water_access_at(
                    part.definition, part.x, part.y, part.foundation_rotation)) {
                blocked_ = 1;
                failure_reason_ = PlacementFailureReason::OpenWater;
                ++forbidden_tiles_;
                break;
            }
        }
    }
    finalize_cell_accounting();

    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_y = std::numeric_limits<int>::min();
    for (const ConstructionPlacementPart &part : parts_) {
        for (const ConstructionPlacementTile &tile : part.tiles) {
            min_x = std::min(min_x, tile.x);
            min_y = std::min(min_y, tile.y);
            max_x = std::max(max_x, tile.x + 1);
            max_y = std::max(max_y, tile.y + 1);
        }
    }
    if (min_x != std::numeric_limits<int>::max()) {
        placement_width_ = max_x - min_x;
        placement_height_ = max_y - min_y;
        placement_size_ = std::max(placement_width_, placement_height_);
    }
}

void ConstructionPlacementPlan::add_part(
    const BuildingType &definition,
    int x,
    int y,
    int foundation_rotation,
    int building_orientation,
    bool is_owner)
{
    const FoundationDef *foundation = definition.foundation_def();
    if (!foundation) {
        blocked_ = 1;
        failure_reason_ = PlacementFailureReason::MissingDefinition;
        ++forbidden_tiles_;
        return;
    }
    ConstructionPlacementPart part;
    part.type = definition.type();
    part.definition = &definition;
    part.x = x;
    part.y = y;
    part.grid_offset = map_grid_is_inside(x, y, 1) ? map_grid_offset(x, y) : 0;
    part.foundation_rotation = normalize_rotation(foundation_rotation);
    part.building_orientation = normalize_rotation(building_orientation);
    part.is_owner = is_owner;
    part.width = foundation->rotated_width(part.foundation_rotation);
    part.height = foundation->rotated_height(part.foundation_rotation);
    part.size = std::max(part.width, part.height);
    parts_.push_back(std::move(part));
}

void ConstructionPlacementPlan::validate_part(ConstructionPlacementPart &part)
{
    const FoundationDef *foundation = part.definition ? part.definition->foundation_def() : nullptr;
    if (!foundation || part.width <= 0 || part.height <= 0 ||
        !map_grid_is_inside(part.x, part.y, 1) ||
        !map_grid_is_inside(part.x + part.width - 1, part.y + part.height - 1, 1)) {
        blocked_ = 1;
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = foundation ? PlacementFailureReason::OutOfBounds : PlacementFailureReason::MissingDefinition;
        }
        ++forbidden_tiles_;
        return;
    }

    const std::vector<RotatedFoundationCell> cells = foundation->rotated_cells(part.foundation_rotation);
    const int check_figures = placement_part_checks_figures(*part.definition, static_cast<int>(cells.size()));
    part.tiles.reserve(cells.size());
    for (const RotatedFoundationCell &rotated : cells) {
        if (!rotated.definition) {
            blocked_ = 1;
            failure_reason_ = PlacementFailureReason::MissingDefinition;
            ++forbidden_tiles_;
            continue;
        }
        ConstructionPlacementTile tile;
        tile.x = part.x + rotated.x;
        tile.y = part.y + rotated.y;
        tile.dx = rotated.x;
        tile.dy = rotated.y;
        tile.grid_offset = map_grid_offset(tile.x, tile.y);
        tile.foundation_cell = rotated.definition;
        tile.added_terrain = rotated.definition->added_terrain;
        tile.removed_terrain = rotated.definition->removed_terrain;
        tile.binds_building = rotated.definition->binds_building;
        tile.state = validate_tile(part, tile, check_figures);
        if (tile.state == PlacementTileState::Forbidden) {
            blocked_ = 1;
            ++forbidden_tiles_;
        }
        part.tiles.push_back(tile);
    }
}

PlacementTileState ConstructionPlacementPlan::validate_tile(
    ConstructionPlacementPart &part,
    ConstructionPlacementTile &tile,
    int check_figures)
{
    if ((check_figures || force_place_) && map_has_figure_at(tile.grid_offset)) {
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Figure;
        }
        return PlacementTileState::Forbidden;
    }

    const FoundationCellDefinition *cell = tile.foundation_cell;
    if (!cell) {
        failure_reason_ = PlacementFailureReason::MissingDefinition;
        return PlacementTileState::Forbidden;
    }

    const unsigned int terrain = static_cast<unsigned int>(map_terrain_get(tile.grid_offset));
    if (replaceable_rubble_ && map_building_exists_at(tile.grid_offset)) {
        const Building &occupant = map_building_at(tile.grid_offset);
        const RubbleState *occupant_state = occupant.Rubble ? occupant.Rubble->state() : nullptr;
        if (occupant_state && replaceable_rubble_->same_origin(*occupant_state)) {
            tile.rubble = RepairRubbleOccupancy::MatchingOrigin;
            if (!(cell->required_terrain & TERRAIN_WATER)) {
                return PlacementTileState::Allowed;
            }
        } else if (occupant_state) {
            tile.rubble = RepairRubbleOccupancy::ForeignOrigin;
        }
    }

    const unsigned int superseded_terrain = part.definition
        ? add_supersession(*part.definition, tile.grid_offset, terrain, cell->added_terrain)
        : 0;
    const unsigned int effective_terrain = terrain & ~superseded_terrain;
    if (cell->required_terrain &&
        (effective_terrain & cell->required_terrain) != cell->required_terrain) {
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Terrain;
        }
        return PlacementTileState::Forbidden;
    }

    const unsigned int permitted = cell->permitted_blocking_terrain;
    const unsigned int generated_transport = cell->added_terrain & (TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_AQUEDUCT);
    const bool places_road_under_aqueduct =
        (effective_terrain & TERRAIN_AQUEDUCT) && (permitted & TERRAIN_AQUEDUCT) &&
        (generated_transport & TERRAIN_ROAD);
    const bool places_highway_under_aqueduct =
        (effective_terrain & TERRAIN_AQUEDUCT) && (permitted & TERRAIN_AQUEDUCT) &&
        (generated_transport & TERRAIN_HIGHWAY);
    const bool valid_road_aqueduct_crossing =
        places_road_under_aqueduct && map_can_place_road_under_aqueduct(tile.grid_offset);
    const bool valid_highway_aqueduct_crossing =
        places_highway_under_aqueduct && map_can_place_highway_under_aqueduct(tile.grid_offset, 0);
    if (places_road_under_aqueduct && !valid_road_aqueduct_crossing) {
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Terrain;
        }
        return PlacementTileState::Forbidden;
    }
    if (places_highway_under_aqueduct && !valid_highway_aqueduct_crossing) {
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Terrain;
        }
        return PlacementTileState::Forbidden;
    }
    if ((generated_transport & TERRAIN_AQUEDUCT) && (effective_terrain & TERRAIN_ROAD) &&
        !map_can_place_aqueduct_on_road(tile.grid_offset)) {
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Terrain;
        }
        return PlacementTileState::Forbidden;
    }
    if ((generated_transport & TERRAIN_AQUEDUCT) && (effective_terrain & TERRAIN_HIGHWAY) &&
        !map_can_place_aqueduct_on_highway(tile.grid_offset, 0)) {
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Terrain;
        }
        return PlacementTileState::Forbidden;
    }

    unsigned int blocking_terrain = effective_terrain;
    if ((valid_road_aqueduct_crossing || valid_highway_aqueduct_crossing) &&
        tile_has_bound_aqueduct_occupancy(tile.grid_offset)) {
        // Aqueducts are real bound buildings, but their BUILDING bit describes
        // the same crossing occupancy as TERRAIN_AQUEDUCT. A valid transport
        // crossing may share that occupancy without granting roads permission
        // to pass through unrelated buildings.
        blocking_terrain &= ~static_cast<unsigned int>(TERRAIN_BUILDING);
    }
    const int blocked_terrain = static_cast<int>(blocking_terrain) &
        TERRAIN_NOT_CLEAR & ~static_cast<int>(permitted);
    PlacementTileState result = blocked_terrain ? PlacementTileState::Forbidden : PlacementTileState::Allowed;

    if (result == PlacementTileState::Forbidden && force_place_ && force_place_can_clear_terrain(blocked_terrain)) {
        tile.force_cleared = 1;
        add_force_clear_offset(tile.grid_offset);
        result = PlacementTileState::Allowed;
    }
    if (result == PlacementTileState::Forbidden && failure_reason_ == PlacementFailureReason::None) {
        failure_reason_ = PlacementFailureReason::Terrain;
    }
    return result;
}

unsigned int ConstructionPlacementPlan::add_supersession(
    const BuildingType &definition,
    int grid_offset,
    unsigned int terrain,
    unsigned int replacement_terrain)
{
    const Building *bound_occupant = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
    for (const BuildingType *replaceable : definition.foundation_replacement_types()) {
        const FoundationDef *foundation = replaceable ? replaceable->foundation_def() : nullptr;
        if (!foundation || foundation->cells().size() != 1) {
            continue;
        }
        const FoundationCellDefinition &cell = foundation->cells().front();
        unsigned int generated = 0;
        unsigned int building_id = 0;
        const Building *occupant = bound_occupant;
        if (!occupant) {
            occupant = BuildingFoundation::unbound_owner_at(grid_offset, replaceable);
        }
        if (occupant) {
            if (!definition.foundation_may_replace(occupant->type)) {
                continue;
            }
            building_id = occupant->id;
            const FoundationTerrainDelta *delta = occupant->Foundation
                ? occupant->Foundation->terrain_delta_at(grid_offset)
                : nullptr;
            generated = delta && delta->added_terrain
                ? delta->added_terrain
                : cell.added_terrain;
        } else {
            if (cell.binds_building || !cell.added_terrain ||
                (terrain & cell.added_terrain) != cell.added_terrain) {
                continue;
            }
            generated = cell.added_terrain;
        }
        generated &= terrain;
        if (!generated) {
            continue;
        }
        supersessions_.push_back({
            grid_offset,
            building_id,
            replaceable,
            generated,
            replacement_terrain
        });
        return generated;
    }
    return 0;
}

void ConstructionPlacementPlan::add_force_clear_offset(int grid_offset)
{
    if (std::find(clear_offsets_.begin(), clear_offsets_.end(), grid_offset) != clear_offsets_.end()) {
        return;
    }
    clear_offsets_.push_back(grid_offset);
    clear_cost_ += clear_land_cost();
}

void ConstructionPlacementPlan::finalize_cell_accounting()
{
    std::vector<PlacementAccountingCell> cells;
    int owner_part_count = 0;
    for (std::size_t part_index = 0; part_index < parts_.size(); ++part_index) {
        owner_part_count += parts_[part_index].is_owner ? 1 : 0;
        for (const ConstructionPlacementTile &tile : parts_[part_index].tiles) {
            const bool requires_rubble = tile.foundation_cell &&
                !(tile.foundation_cell->required_terrain & TERRAIN_WATER);
            cells.push_back(PlacementAccountingCell{
                tile.grid_offset,
                parts_[part_index].is_owner,
                tile.force_cleared != 0,
                requires_rubble,
                tile.rubble
            });
        }
    }

    const PlacementAccountingResult accounting = summarize_placement_cells(cells);
    unique_occupied_tiles_ = accounting.unique_cell_count;
    owner_charge_count_ = accounting.owner_charge_count;
    required_rubble_tiles_ = accounting.required_rubble_cell_count;
    replaceable_rubble_tiles_ = accounting.matching_rubble_cell_count;

    if (!accounting.publication_valid() || owner_part_count != 1) {
        blocked_ = 1;
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::InvalidComposition;
        }
        ++forbidden_tiles_;
    }
    if (replaceable_rubble_ && !accounting.repair_coverage_valid()) {
        blocked_ = 1;
        if (failure_reason_ == PlacementFailureReason::None) {
            failure_reason_ = PlacementFailureReason::Terrain;
        }
        forbidden_tiles_ += std::max(1, accounting.foreign_rubble_cell_count);
    }
}

} // namespace building_construction
