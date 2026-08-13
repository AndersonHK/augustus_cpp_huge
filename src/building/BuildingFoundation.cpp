#include "building/BuildingFoundation.h"

#include "building/building.h"
#include "city/view.h"
#include "core/direction.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/tiles.h"

#include <algorithm>
#include <unordered_map>

namespace building_type_registry_impl {

namespace {

std::unordered_map<int, std::vector<BuildingFoundation *>> g_unbound_foundations;

}

BuildingFoundation::BuildingFoundation(Building &owner, const FoundationDef &definition, FoundationState &state)
    : owner_(&owner), definition_(&definition), state_(&state)
{
    state_->roadblock().configure(
        definition.path(), definition.default_permissions(), definition.configurable_permissions());
    register_unbound_cells();
}

BuildingFoundation::~BuildingFoundation()
{
    unregister_unbound_cells();
}

Building &BuildingFoundation::owner() const { return *owner_; }
const FoundationDef &BuildingFoundation::definition() const { return *definition_; }
FoundationState &BuildingFoundation::state() const { return *state_; }
int BuildingFoundation::width(int rotation) const { return definition_->rotated_width(rotation); }
int BuildingFoundation::height(int rotation) const { return definition_->rotated_height(rotation); }
std::vector<RotatedFoundationCell> BuildingFoundation::cells(int rotation) const
{
    return definition_->rotated_cells(rotation);
}

namespace {

int definition_adds_aqueduct(const FoundationDef &definition)
{
    return std::any_of(definition.cells().begin(), definition.cells().end(),
        [](const FoundationCellDefinition &cell) {
            return (cell.added_terrain & TERRAIN_AQUEDUCT) != 0;
        });
}

void refresh_aqueduct_region(const FoundationDef &definition, int origin_x, int origin_y, int rotation)
{
    if (!definition_adds_aqueduct(definition)) {
        return;
    }
    const int x_end = origin_x + definition.rotated_width(rotation) - 1;
    const int y_end = origin_y + definition.rotated_height(rotation) - 1;
    map_tiles_update_region_aqueducts(origin_x - 3, origin_y - 3, x_end + 3, y_end + 3);
}

void refresh_removed_foundation_region(
    const FoundationDef &definition, int origin_x, int origin_y, int rotation)
{
    const int x_end = origin_x + definition.rotated_width(rotation) - 1;
    const int y_end = origin_y + definition.rotated_height(rotation) - 1;
    map_tiles_update_region_water(origin_x - 1, origin_y - 1, x_end + 1, y_end + 1);
    map_tiles_update_region_trees(origin_x - 1, origin_y - 1, x_end + 1, y_end + 1);
    map_tiles_update_region_shrub(origin_x, origin_y, x_end, y_end);
    map_tiles_update_region_empty_land(origin_x - 1, origin_y - 1, x_end + 1, y_end + 1);
    map_tiles_update_region_meadow(origin_x - 1, origin_y - 1, x_end + 1, y_end + 1);
    map_tiles_update_region_rubble(origin_x, origin_y, x_end, y_end);
    const int refresh_size = std::max(
        definition.rotated_width(rotation), definition.rotated_height(rotation)) + 2;
    map_tiles_update_area_roads(origin_x, origin_y, refresh_size);
    map_tiles_update_area_highways(origin_x, origin_y, refresh_size);
    map_tiles_update_region_aqueducts(origin_x - 3, origin_y - 3, x_end + 3, y_end + 3);
}

} // namespace

int BuildingFoundation::publish(int origin_x, int origin_y, int rotation, int image_id)
{
    const int normalized_rotation = (rotation % 4 + 4) % 4;
    if (!owner_ || !definition_ || !state_) {
        return 0;
    }
    if (state_->is_published()) {
        return state_->origin_x() == origin_x && state_->origin_y() == origin_y &&
            state_->rotation() == normalized_rotation ? refresh(image_id) : 0;
    }
    const std::vector<RotatedFoundationCell> rotated = definition_->rotated_cells(rotation);
    if (rotated.empty()) {
        return 0;
    }
    for (const RotatedFoundationCell &cell : rotated) {
        if (!cell.definition || !map_grid_is_inside(origin_x + cell.x, origin_y + cell.y, 1)) {
            return 0;
        }
        const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
        if (cell.definition->binds_building && map_building_exists_at(grid_offset) &&
            map_building_at(grid_offset).record() != owner_->record()) {
            return 0;
        }
    }

    const int width = definition_->rotated_width(rotation);
    const int height = definition_->rotated_height(rotation);
    const FoundationDrawAnchor draw_cell =
        foundation_draw_anchor(rotated, width, height, city_view_orientation());
    const int compatibility_size = std::max(width, height);
    state_->begin_publication(origin_x, origin_y, rotation);

    const std::vector<FoundationCellDefinition> &canonical_cells = definition_->cells();
    for (const RotatedFoundationCell &cell : rotated) {
        const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
        const FoundationTerrainMutation mutation = foundation_apply_terrain_cell(
            *cell.definition,
            static_cast<int>(cell.definition - canonical_cells.data()),
            grid_offset,
            static_cast<uint32_t>(map_terrain_get(grid_offset)));

        map_terrain_remove(grid_offset, static_cast<int>(cell.definition->removed_terrain));
        map_terrain_add(grid_offset, static_cast<int>(cell.definition->added_terrain));
        if (cell.definition->binds_building) {
            map_building_set(grid_offset, *owner_);
            map_property_set_legacy_multi_tile_size(grid_offset, compatibility_size);
            map_property_set_multi_tile_xy(
                grid_offset, cell.x, cell.y,
                draw_cell.valid && cell.x == draw_cell.x && cell.y == draw_cell.y);
            if (image_id >= 0) {
                map_image_set(grid_offset, image_id);
            }
        }
        map_property_clear_constructing(grid_offset);
        state_->record_delta(mutation.delta);
    }
    refresh_aqueduct_region(*definition_, origin_x, origin_y, rotation);
    register_unbound_cells();
    return 1;
}

int BuildingFoundation::refresh(int image_id)
{
    if (!owner_ || !definition_ || !state_ || !state_->is_published()) {
        return 0;
    }
    const int origin_x = state_->origin_x();
    const int origin_y = state_->origin_y();
    const int rotation = state_->rotation();
    const std::vector<RotatedFoundationCell> rotated = definition_->rotated_cells(rotation);
    if (rotated.empty()) {
        return 0;
    }
    for (const RotatedFoundationCell &cell : rotated) {
        if (!cell.definition || !map_grid_is_inside(origin_x + cell.x, origin_y + cell.y, 1)) {
            return 0;
        }
        const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
        if (cell.definition->binds_building && map_building_exists_at(grid_offset) &&
            map_building_at(grid_offset).record() != owner_->record()) {
            return 0;
        }
    }

    const int width = definition_->rotated_width(rotation);
    const int height = definition_->rotated_height(rotation);
    const FoundationDrawAnchor draw_cell =
        foundation_draw_anchor(rotated, width, height, city_view_orientation());
    const int compatibility_size = std::max(width, height);
    for (const RotatedFoundationCell &cell : rotated) {
        const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
        if (cell.definition->binds_building) {
            map_building_set(grid_offset, *owner_);
            map_property_set_legacy_multi_tile_size(grid_offset, compatibility_size);
            map_property_set_multi_tile_xy(
                grid_offset, cell.x, cell.y,
                draw_cell.valid && cell.x == draw_cell.x && cell.y == draw_cell.y);
            if (image_id >= 0) {
                map_image_set(grid_offset, image_id);
            }
        }
        map_property_clear_constructing(grid_offset);
    }
    refresh_aqueduct_region(*definition_, origin_x, origin_y, rotation);
    register_unbound_cells();
    return 1;
}

int BuildingFoundation::remove()
{
    if (!owner_ || !definition_ || !state_ || !state_->is_published()) {
        return 0;
    }
    const int origin_x = state_->origin_x();
    const int origin_y = state_->origin_y();
    const int rotation = state_->rotation();
    unregister_unbound_cells();
    const std::vector<FoundationTerrainDelta> &deltas = state_->terrain_deltas();
    const std::vector<FoundationCellDefinition> &canonical_cells = definition_->cells();
    for (auto it = deltas.rbegin(); it != deltas.rend(); ++it) {
        const FoundationTerrainDelta &delta = *it;
        if (!map_grid_is_valid_offset(delta.grid_offset)) {
            continue;
        }
        const FoundationCellDefinition *cell =
            delta.cell_index >= 0 && delta.cell_index < static_cast<int>(canonical_cells.size())
            ? &canonical_cells[delta.cell_index]
            : nullptr;
        const int authored_aqueduct = cell && (cell->added_terrain & TERRAIN_AQUEDUCT);
        if ((delta.added_terrain & TERRAIN_AQUEDUCT) || authored_aqueduct) {
            map_aqueduct_remove(delta.grid_offset);
        }
        map_terrain_remove(delta.grid_offset, static_cast<int>(delta.added_terrain) |
            (authored_aqueduct ? TERRAIN_AQUEDUCT : 0));
        map_terrain_add(delta.grid_offset, static_cast<int>(delta.removed_terrain));
        if (delta.bound_building && map_building_exists_at(delta.grid_offset) &&
            map_building_at(delta.grid_offset).record() == owner_->record()) {
            map_building_clear_at(delta.grid_offset);
            map_building_damage_clear(delta.grid_offset);
            map_sprite_clear_tile(delta.grid_offset);
        }
        if (delta.bound_building) {
            map_image_set(delta.grid_offset, 0);
            map_property_set_legacy_multi_tile_size(delta.grid_offset, 1);
            map_property_clear_multi_tile_xy(delta.grid_offset);
            map_property_mark_draw_tile(delta.grid_offset);
        }
        map_property_clear_constructing(delta.grid_offset);
    }
    if (deltas.empty()) {
        for (const RotatedFoundationCell &cell : definition_->rotated_cells(rotation)) {
            const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
            if (!cell.definition) {
                continue;
            }
            if (cell.definition->added_terrain & TERRAIN_AQUEDUCT) {
                map_aqueduct_remove(grid_offset);
            }
            map_terrain_remove(grid_offset, static_cast<int>(cell.definition->added_terrain));
            if (cell.definition->binds_building && map_building_exists_at(grid_offset) &&
                map_building_at(grid_offset).record() == owner_->record()) {
                map_building_clear_at(grid_offset);
                map_building_damage_clear(grid_offset);
                map_sprite_clear_tile(grid_offset);
                map_property_set_legacy_multi_tile_size(grid_offset, 1);
                map_property_clear_multi_tile_xy(grid_offset);
                map_property_mark_draw_tile(grid_offset);
                map_image_set(grid_offset, 0);
            }
        }
    }
    refresh_removed_foundation_region(*definition_, origin_x, origin_y, rotation);
    state_->clear();
    return 1;
}

int BuildingFoundation::rebind(int origin_x, int origin_y, int rotation)
{
    if (!owner_ || !definition_ || !state_) {
        return 0;
    }
    const int normalized_rotation = (rotation % 4 + 4) % 4;
    if (state_->is_published() &&
        (state_->origin_x() != origin_x || state_->origin_y() != origin_y ||
         state_->rotation() != normalized_rotation)) {
        return 0;
    }
    const std::vector<RotatedFoundationCell> rotated = definition_->rotated_cells(normalized_rotation);
    for (const RotatedFoundationCell &cell : rotated) {
        if (!cell.definition || !map_grid_is_inside(origin_x + cell.x, origin_y + cell.y, 1)) {
            return 0;
        }
        const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
        if (cell.definition->binds_building && map_building_exists_at(grid_offset) &&
            map_building_at(grid_offset).record() != owner_->record()) {
            return 0;
        }
    }
    if (!state_->is_published()) {
        state_->begin_publication(origin_x, origin_y, normalized_rotation);
        const std::vector<FoundationCellDefinition> &canonical = definition_->cells();
        for (const RotatedFoundationCell &cell : rotated) {
            const int grid_offset = map_grid_offset(origin_x + cell.x, origin_y + cell.y);
            FoundationTerrainDelta delta;
            delta.cell_index = static_cast<int>(cell.definition - canonical.data());
            delta.grid_offset = grid_offset;
            delta.added_terrain = static_cast<uint32_t>(map_terrain_get(grid_offset)) &
                cell.definition->added_terrain;
            delta.bound_building = cell.definition->binds_building;
            state_->record_delta(delta);
        }
    }
    for (const RotatedFoundationCell &cell : rotated) {
        if (cell.definition->binds_building) {
            map_building_set(map_grid_offset(origin_x + cell.x, origin_y + cell.y), *owner_);
        }
    }
    refresh_aqueduct_region(*definition_, origin_x, origin_y, normalized_rotation);
    register_unbound_cells();
    return 1;
}

int BuildingFoundation::draw_grid_offset(int view_orientation) const
{
    if (!definition_ || !state_ || !state_->is_published()) {
        return -1;
    }
    const int rotation = state_->rotation();
    const std::vector<RotatedFoundationCell> rotated = definition_->rotated_cells(rotation);
    const FoundationDrawAnchor draw_cell = foundation_draw_anchor(
        rotated,
        definition_->rotated_width(rotation),
        definition_->rotated_height(rotation),
        view_orientation);
    return draw_cell.valid
        ? map_grid_offset(state_->origin_x() + draw_cell.x, state_->origin_y() + draw_cell.y)
        : -1;
}

FoundationPassage BuildingFoundation::passage_at(int grid_offset) const
{
    if (!definition_ || !state_ || !state_->is_published()) {
        return FoundationPassage::None;
    }
    for (const RotatedFoundationCell &cell : definition_->rotated_cells(state_->rotation())) {
        if (cell.definition &&
            map_grid_offset(state_->origin_x() + cell.x, state_->origin_y() + cell.y) == grid_offset) {
            const int terrain = map_terrain_get(grid_offset);
            if ((cell.definition->added_terrain & TERRAIN_AQUEDUCT) &&
                (terrain & TERRAIN_AQUEDUCT) &&
                (terrain & (TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_ACCESS_RAMP))) {
                return FoundationPassage::Uncontrolled;
            }
            return cell.definition->passage;
        }
    }
    return FoundationPassage::None;
}

int BuildingFoundation::has_owner_controlled_passage() const
{
    return definition_ && definition_->has_owner_controlled_passage();
}

int BuildingFoundation::has_unrestricted_road_crossing() const
{
    if (!definition_ || !state_ || !state_->is_published()) {
        return 0;
    }
    for (const RotatedFoundationCell &cell : definition_->rotated_cells(state_->rotation())) {
        if (!cell.definition || !(cell.definition->added_terrain & TERRAIN_AQUEDUCT)) {
            continue;
        }
        const int grid_offset = map_grid_offset(
            state_->origin_x() + cell.x, state_->origin_y() + cell.y);
        const int terrain = map_terrain_get(grid_offset);
        if ((terrain & TERRAIN_AQUEDUCT) &&
            (terrain & (TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_ACCESS_RAMP))) {
            return 1;
        }
    }
    return 0;
}

int BuildingFoundation::passage_axis() const
{
    return has_owner_controlled_passage() && state_ && state_->is_published()
        ? 1 + (state_->rotation() & 1)
        : 0;
}

int BuildingFoundation::allows_passage(int grid_offset, int permission) const
{
    const FoundationPassage passage = passage_at(grid_offset);
    if (passage == FoundationPassage::Uncontrolled) {
        return 1;
    }
    if (passage != FoundationPassage::OwnerControlled) {
        return 0;
    }

    const BuildingFoundation *controller = this;
    if (owner_ && owner_->Composition && owner_->Composition->is_child()) {
        Building *composition_owner = owner_->Composition->owner();
        if (composition_owner && composition_owner->Foundation) {
            controller = composition_owner->Foundation;
        }
    }
    return controller->state_->roadblock().has_permission(permission);
}

RoadblockState &BuildingFoundation::roadblock_state() const
{
    return state_->roadblock();
}

int BuildingFoundation::contains_grid_offset(int grid_offset) const
{
    if (!definition_ || !state_ || !state_->is_published()) {
        return 0;
    }
    for (const RotatedFoundationCell &cell : definition_->rotated_cells(state_->rotation())) {
        if (cell.definition &&
            map_grid_offset(state_->origin_x() + cell.x, state_->origin_y() + cell.y) == grid_offset) {
            return 1;
        }
    }
    return 0;
}

const FoundationTerrainDelta *BuildingFoundation::terrain_delta_at(int grid_offset) const
{
    if (!state_) {
        return nullptr;
    }
    for (const FoundationTerrainDelta &delta : state_->terrain_deltas()) {
        if (delta.grid_offset == grid_offset) {
            return &delta;
        }
    }
    return nullptr;
}

Building *BuildingFoundation::unbound_owner_at(int grid_offset, const BuildingType *type)
{
    const auto found = g_unbound_foundations.find(grid_offset);
    if (found == g_unbound_foundations.end()) {
        return nullptr;
    }
    for (auto it = found->second.rbegin(); it != found->second.rend(); ++it) {
        BuildingFoundation *foundation = *it;
        if (foundation && foundation->owner_ && foundation->owner_->type == type &&
            foundation->state_ && foundation->state_->is_published() &&
            foundation->contains_grid_offset(grid_offset)) {
            return foundation->owner_;
        }
    }
    return nullptr;
}

void BuildingFoundation::detach_unbound_ownership()
{
    unregister_unbound_cells();
}

void BuildingFoundation::restore_unbound_ownership()
{
    register_unbound_cells();
}

void BuildingFoundation::register_unbound_cells()
{
    unregister_unbound_cells();
    if (!owner_ || !definition_ || !state_ || !state_->is_published()) {
        return;
    }
    for (const RotatedFoundationCell &cell : definition_->rotated_cells(state_->rotation())) {
        if (!cell.definition || cell.definition->binds_building) {
            continue;
        }
        const int grid_offset = map_grid_offset(state_->origin_x() + cell.x, state_->origin_y() + cell.y);
        std::vector<BuildingFoundation *> &owners = g_unbound_foundations[grid_offset];
        if (std::find(owners.begin(), owners.end(), this) == owners.end()) {
            owners.push_back(this);
            registered_unbound_offsets_.push_back(grid_offset);
        }
    }
}

void BuildingFoundation::unregister_unbound_cells()
{
    for (int grid_offset : registered_unbound_offsets_) {
        const auto found = g_unbound_foundations.find(grid_offset);
        if (found == g_unbound_foundations.end()) {
            continue;
        }
        std::vector<BuildingFoundation *> &owners = found->second;
        owners.erase(std::remove(owners.begin(), owners.end(), this), owners.end());
        if (owners.empty()) {
            g_unbound_foundations.erase(found);
        }
    }
    registered_unbound_offsets_.clear();
}

} // namespace building_type_registry_impl
