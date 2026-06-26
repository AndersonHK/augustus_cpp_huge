#include "building/construction_plan.h"

#include "building/construction.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/building_type_registry_internal.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_aqueduct.h"
#include "map/terrain.h"
#include "map/water.h"

#include <algorithm>

namespace building_construction {

using building_type_registry_impl::FoundationCellDefinition;
using building_type_registry_impl::FoundationCellRequirement;
using building_type_registry_impl::FoundationDefinition;

namespace {

constexpr int FORCE_PLACE_CLEARABLE_TERRAIN = TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD;

int type_size(const building_type_registry_impl::BuildingType *definition, building_type type)
{
    if (definition && definition->model().has_size()) {
        return definition->model().size();
    }
    const building_properties *props = building_properties_for_type(type);
    return props ? props->size : 1;
}

int type_placement_size(const building_type_registry_impl::BuildingType *definition, building_type type)
{
    if (!definition || !definition->has_composition()) {
        return type_size(definition, type);
    }
    const building_type_registry_impl::ComposedBuildingDefinition &composition = definition->composition();
    return std::max(composition.footprint_width(), composition.footprint_height());
}

int clear_land_cost()
{
    static int cost = -1;
    if (cost >= 0) {
        return cost;
    }
    const building_type clear_land_type = building_type_registry_impl::type_from_attr("clear_land");
    const model_building *model = clear_land_type == BUILDING_NONE ? nullptr : model_get_building(clear_land_type);
    cost = model ? model->cost : 0;
    return cost;
}

int force_place_can_clear_terrain(int terrain)
{
    return terrain && !(terrain & ~FORCE_PLACE_CLEARABLE_TERRAIN);
}

int terrain_has_blocking_bits_for_water(int terrain)
{
    return terrain & (TERRAIN_ROCK | TERRAIN_ROAD | TERRAIN_BUILDING);
}

int placement_part_checks_figures(const building_type_registry_impl::BuildingType &definition, int size)
{
    const building_type_registry_impl::TileDefinition &tile = definition.tile();
    if (size == 1 && tile.refresh_behavior() == building_type_registry_impl::TileRefreshBehavior::Plaza) {
        return 0;
    }
    return 1;
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
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int exact_coordinates,
    int force_place)
    : type_(definition.type()),
      definition_(&definition),
      cursor_x_(x),
      cursor_y_(y),
      origin_x_(x),
      origin_y_(y),
      exact_coordinates_(exact_coordinates),
      force_place_(force_place)
{
    placement_size_ = type_ == BUILDING_NONE ? 0 : type_placement_size(definition_, type_);
    if (!exact_coordinates_ && placement_size_ > 0) {
        building_construction_offset_start_from_orientation(&origin_x_, &origin_y_, placement_size_);
    }
    build();
}

int ConstructionPlacementPlan::can_place() const
{
    return !blocked_;
}

building_type ConstructionPlacementPlan::type() const
{
    return type_;
}

const building_type_registry_impl::BuildingType &ConstructionPlacementPlan::definition() const
{
    return *definition_;
}

int ConstructionPlacementPlan::has_shoreline_failure() const
{
    return shoreline_failure_;
}

int ConstructionPlacementPlan::has_open_water_failure() const
{
    return open_water_failure_;
}

int ConstructionPlacementPlan::clear_cost() const
{
    return clear_cost_;
}

int ConstructionPlacementPlan::cursor_x() const
{
    return cursor_x_;
}

int ConstructionPlacementPlan::cursor_y() const
{
    return cursor_y_;
}

int ConstructionPlacementPlan::origin_x() const
{
    return origin_x_;
}

int ConstructionPlacementPlan::origin_y() const
{
    return origin_y_;
}

int ConstructionPlacementPlan::placement_size() const
{
    return placement_size_;
}

int ConstructionPlacementPlan::waterside_orientation_absolute() const
{
    return waterside_orientation_absolute_;
}

int ConstructionPlacementPlan::waterside_orientation_relative() const
{
    return waterside_orientation_relative_;
}

const std::vector<int> &ConstructionPlacementPlan::clear_offsets() const
{
    return clear_offsets_;
}

const std::vector<ConstructionPlacementPart> &ConstructionPlacementPlan::parts() const
{
    return parts_;
}

void ConstructionPlacementPlan::build()
{
    if (!definition_ || type_ == BUILDING_NONE || placement_size_ <= 0) {
        blocked_ = 1;
        return;
    }

    if (!definition_->has_composition()) {
        add_part(type_, origin_x_, origin_y_);
    } else {
        const int rotation = building_rotation_get_rotation();
        const building_type_registry_impl::ComposedBuildingDefinition &composition = definition_->composition();
        for (const building_type_registry_impl::ComposedPartDefinition &part : composition.parts()) {
            const building_type_registry_impl::ComposedPartOffset offset = part.offset_for_rotation(rotation);
            if (part.type != BUILDING_NONE && offset.has_value) {
                add_part(part.type, origin_x_ + offset.x, origin_y_ + offset.y);
            }
        }

        const building_type_registry_impl::ComposedPartOffset main_offset =
            composition.main_offset_for_rotation(rotation);
        add_part(type_, origin_x_ + main_offset.x, origin_y_ + main_offset.y);
    }

    for (ConstructionPlacementPart &part : parts_) {
        validate_part(part);
    }
}

void ConstructionPlacementPlan::add_part(building_type type, int x, int y)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    ConstructionPlacementPart part;
    part.type = type;
    part.definition = definition;
    part.x = x;
    part.y = y;
    part.grid_offset = map_grid_offset(x, y);
    part.size = type_size(definition, type);
    parts_.push_back(part);
}

void ConstructionPlacementPlan::validate_part(ConstructionPlacementPart &part)
{
    if (!part.definition || part.size <= 0 || !map_grid_is_inside(part.x, part.y, part.size)) {
        blocked_ = 1;
        return;
    }

    part.foundation_rotation = select_foundation_rotation(part);
    if (shoreline_failure_ || open_water_failure_) {
        blocked_ = 1;
    }

    const int check_figures = placement_part_checks_figures(*part.definition, part.size);
    for (int dy = 0; dy < part.size; dy++) {
        for (int dx = 0; dx < part.size; dx++) {
            ConstructionPlacementTile tile;
            tile.x = part.x + dx;
            tile.y = part.y + dy;
            tile.dx = dx;
            tile.dy = dy;
            tile.grid_offset = map_grid_offset(tile.x, tile.y);
            tile.requirement = requirement_for_tile(part, dx, dy);
            tile.state = validate_tile(part, tile, check_figures);
            if (tile.state == PlacementTileState::Forbidden) {
                blocked_ = 1;
            }
            part.tiles.push_back(tile);
        }
    }
}

int ConstructionPlacementPlan::select_foundation_rotation(const ConstructionPlacementPart &part)
{
    if (part.definition->foundation().policy_type() != building_type_registry_impl::FoundationPolicy::Shoreline) {
        return building_rotation_get_rotation();
    }

    int absolute = -1;
    int relative = -1;
    map_water_determine_orientation(part.x, part.y, part.size, 0, &absolute, &relative, 0, nullptr);
    if (absolute < 0) {
        shoreline_failure_ = 1;
        return -1;
    }

    const waterside_tile_loop *loop = map_water_get_waterside_tile_loop(absolute, part.size);
    if (!loop || !map_water_has_water_in_front(part.x, part.y, 0, loop, nullptr)) {
        shoreline_failure_ = 1;
        return absolute;
    }

    if (part.definition->foundation().requires_open_water() &&
        !map_water_is_connected_to_open_water(part.x, part.y, part.size)) {
        open_water_failure_ = 1;
    }

    waterside_orientation_absolute_ = absolute;
    waterside_orientation_relative_ = relative;
    return absolute;
}

FoundationCellRequirement ConstructionPlacementPlan::requirement_for_tile(
    const ConstructionPlacementPart &part,
    int dx,
    int dy) const
{
    const building_type_registry_impl::FoundationDefinition &foundation = part.definition->foundation();
    for (const building_type_registry_impl::FoundationCellDefinition &cell : foundation.cells()) {
        if (cell.x == dx && cell.y == dy &&
            (cell.rotation < 0 || cell.rotation == part.foundation_rotation)) {
            return cell.requirement;
        }
    }

    return foundation.policy_requirement();
}

PlacementTileState ConstructionPlacementPlan::validate_tile(
    ConstructionPlacementPart &part,
    ConstructionPlacementTile &tile,
    int check_figures)
{
    if ((check_figures || force_place_) && map_has_figure_at(tile.grid_offset)) {
        return PlacementTileState::Forbidden;
    }

    const int terrain = map_terrain_get(tile.grid_offset);
    int blocked_terrain = 0;
    switch (tile.requirement) {
        case FoundationCellRequirement::Any:
            return PlacementTileState::Allowed;
        case FoundationCellRequirement::Water:
            if (!(terrain & TERRAIN_WATER) || terrain_has_blocking_bits_for_water(terrain)) {
                return PlacementTileState::Forbidden;
            }
            return PlacementTileState::Allowed;
        case FoundationCellRequirement::Road:
            return (terrain & TERRAIN_ROAD) && !(terrain & TERRAIN_BUILDING) ?
                PlacementTileState::Allowed :
                PlacementTileState::Forbidden;
        case FoundationCellRequirement::RoadOrLand:
            if (terrain & TERRAIN_AQUEDUCT) {
                return map_can_place_highway_under_aqueduct(tile.grid_offset, 0) ?
                    PlacementTileState::Allowed :
                    PlacementTileState::Forbidden;
            }
            blocked_terrain = terrain & TERRAIN_NOT_CLEAR & ~TERRAIN_ROAD & ~TERRAIN_HIGHWAY;
            if (!blocked_terrain) {
                return PlacementTileState::Allowed;
            }
            if (force_place_ && force_place_can_clear_terrain(blocked_terrain)) {
                tile.force_cleared = 1;
                add_force_clear_offset(tile.grid_offset);
                return PlacementTileState::Allowed;
            }
            return PlacementTileState::Forbidden;
        case FoundationCellRequirement::RoadWallOrLand:
            if (terrain & TERRAIN_WALL) {
                return PlacementTileState::Allowed;
            }
            blocked_terrain = terrain & TERRAIN_NOT_CLEAR & ~TERRAIN_ROAD & ~TERRAIN_HIGHWAY;
            if (!blocked_terrain) {
                return PlacementTileState::Allowed;
            }
            if (force_place_ && force_place_can_clear_terrain(blocked_terrain)) {
                tile.force_cleared = 1;
                add_force_clear_offset(tile.grid_offset);
                return PlacementTileState::Allowed;
            }
            return PlacementTileState::Forbidden;
        case FoundationCellRequirement::Wall:
            return (terrain & TERRAIN_WALL) ? PlacementTileState::Allowed : PlacementTileState::Forbidden;
        case FoundationCellRequirement::Aqueduct:
            return (terrain & TERRAIN_AQUEDUCT) ? PlacementTileState::Allowed : PlacementTileState::Forbidden;
        case FoundationCellRequirement::Land:
        default:
            blocked_terrain = terrain & TERRAIN_NOT_CLEAR;
            if (!blocked_terrain) {
                return PlacementTileState::Allowed;
            }
            if (force_place_ && force_place_can_clear_terrain(blocked_terrain)) {
                tile.force_cleared = 1;
                add_force_clear_offset(tile.grid_offset);
                return PlacementTileState::Allowed;
            }
            return PlacementTileState::Forbidden;
    }
}

void ConstructionPlacementPlan::add_force_clear_offset(int grid_offset)
{
    if (std::find(clear_offsets_.begin(), clear_offsets_.end(), grid_offset) != clear_offsets_.end()) {
        return;
    }
    clear_offsets_.push_back(grid_offset);
    clear_cost_ += clear_land_cost();
}

} // namespace building_construction
