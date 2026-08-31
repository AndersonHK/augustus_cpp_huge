#include "FoundationDefTest.h"

#include "building/FoundationDef.h"
#include "building/BuildingFoundation.h"
#include "building/BuildingGeometry.h"
#include "building/BuildingGeometryProjection.h"
#include "building/FoundationRegistry.h"
#include "building/building_type_registry_internal.h"
#include "building/FoundationState.h"
#include "building/FoundationStateSaveBridge.h"
#include "building/PlacementRotationSelection.h"
#include "building/RoadblockState.h"
#include "building/construction_session.h"
#include "building/tool_mode.h"
#include "game/save_version.h"
#include "map/terrain.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

bool validate_foundation_rotation_contract()
{
    using building_construction::PlacementRotationCandidates;
    using building_construction::placement_rotation_candidates;

    const PlacementRotationCandidates retained = placement_rotation_candidates(0, 2, true, true);
    const PlacementRotationCandidates preferred = placement_rotation_candidates(1, 1, false, true);
    const PlacementRotationCandidates fixed = placement_rotation_candidates(3, 1, true, false);
    if (retained.count != 4 || retained.rotations != std::array<int, 4>{ 2, 0, 1, 3 } ||
        preferred.count != 4 || preferred.rotations != std::array<int, 4>{ 1, 2, 3, 0 } ||
        fixed.count != 1 || fixed.rotations[0] != 3) {
        std::cerr << "Foundation rotation contract failed: retained/preferred candidate order changed.\n";
        return false;
    }
    for (int selected_rotation = 0; selected_rotation < 4; ++selected_rotation) {
        const PlacementRotationCandidates manual =
            placement_rotation_candidates(selected_rotation, 0, false, true);
        if (manual.count != 4 || manual.rotations[0] != selected_rotation) {
            std::cerr << "Foundation rotation contract failed: manual rotation was not authoritative.\n";
            return false;
        }
    }
    using namespace building_type_registry_impl;

    FoundationDef definition("rotation_fixture");
    definition.set_dimensions(2, 3);
    definition.set_rotates(1);
    for (int y = 0; y < definition.height(); ++y) {
        for (int x = 0; x < definition.width(); ++x) {
            FoundationCellDefinition cell;
            cell.x = x;
            cell.y = y;
            definition.add_cell(cell);
        }
    }

    if (definition.rotated_width(0) != 2 || definition.rotated_height(0) != 3 ||
        definition.rotated_width(1) != 3 || definition.rotated_height(1) != 2 ||
        definition.rotated_width(2) != 2 || definition.rotated_height(2) != 3 ||
        definition.rotated_width(3) != 3 || definition.rotated_height(3) != 2) {
        std::cerr << "Foundation rotation contract failed: rectangular dimensions are incorrect.\n";
        return false;
    }

    const FoundationDrawAnchor expected_draw_anchors[] = {
        { 0, 2, 1 }, { 0, 0, 1 }, { 1, 0, 1 }, { 1, 2, 1 }
    };
    for (int view_orientation = DIR_0_TOP; view_orientation <= DIR_6_LEFT; view_orientation += 2) {
        const FoundationDrawAnchor anchor = foundation_draw_anchor(
            definition.rotated_cells(0), definition.width(), definition.height(), view_orientation);
        const FoundationDrawAnchor &expected = expected_draw_anchors[view_orientation / 2];
        if (!anchor.valid || anchor.x != expected.x || anchor.y != expected.y) {
            std::cerr << "Foundation draw-anchor contract failed at view orientation "
                      << view_orientation << ".\n";
            return false;
        }
    }

    const RotatedFoundationCell r1_first = definition.rotated_cells(1).front();
    const RotatedFoundationCell r3_first = definition.rotated_cells(3).front();
    if (r1_first.x != 0 || r1_first.y != 1 || r3_first.x != 2 || r3_first.y != 0) {
        std::cerr << "Foundation rotation contract failed: rotation indices disagree with generic building rotation.\n";
        return false;
    }

    for (int rotation = 0; rotation < 4; ++rotation) {
        std::set<std::pair<int, int>> occupied;
        for (const RotatedFoundationCell &cell : definition.rotated_cells(rotation)) {
            if (cell.x < 0 || cell.y < 0 || cell.x >= definition.rotated_width(rotation) ||
                cell.y >= definition.rotated_height(rotation) || !occupied.emplace(cell.x, cell.y).second) {
                std::cerr << "Foundation rotation contract failed: rotated cells are out of bounds or overlap.\n";
                return false;
            }
        }
        if (occupied.size() != definition.cells().size()) {
            std::cerr << "Foundation rotation contract failed: rotation changed active cell count.\n";
            return false;
        }
    }

    const BuildingGeometry normalized_geometry = BuildingGeometry::from_world_cells({
        { 10, 10, nullptr, nullptr, FoundationPassage::None, false },
        { 11, 10, nullptr, nullptr, FoundationPassage::OwnerControlled, false },
        { 12, 10, nullptr, nullptr, FoundationPassage::None, true },
        { 12, 10, nullptr, nullptr, FoundationPassage::None, true },
        { 12, 11, nullptr, nullptr, FoundationPassage::None, true }
    });
    const std::set<std::pair<int, int>> expected_access = {
        { 11, 9 }, { 11, 11 }
    };
    const std::set<std::pair<int, int>> expected_water_candidates = {
        { 12, 9 }, { 13, 10 }, { 11, 11 }, { 13, 11 }, { 12, 12 }
    };
    const auto points_as_set = [](const std::vector<BuildingGeometryPoint> &points) {
        std::set<std::pair<int, int>> result;
        for (const BuildingGeometryPoint &point : points) {
            result.emplace(point.x, point.y);
        }
        return result;
    };
    if (!normalized_geometry.valid() || normalized_geometry.cells().size() != 4 ||
        normalized_geometry.bounds().min_x != 10 || normalized_geometry.bounds().min_y != 10 ||
        normalized_geometry.bounds().max_x != 13 || normalized_geometry.bounds().max_y != 12 ||
        !normalized_geometry.contains(10, 10) || !normalized_geometry.contains(12, 11) ||
        normalized_geometry.contains(11, 11) ||
        normalized_geometry.contains_within_range(10, 11, 0) ||
        !normalized_geometry.contains_within_range(10, 11, 1) ||
        normalized_geometry.contains_within_range(14, 10, 1) ||
        points_as_set(normalized_geometry.access_candidates()) != expected_access ||
        points_as_set(normalized_geometry.water_candidates()) != expected_water_candidates) {
        std::cerr << "BuildingGeometry contract failed: cell normalization, bounds, or exact perimeters changed.\n";
        return false;
    }

    FoundationDef planned_water("planned_water_geometry_fixture");
    planned_water.set_dimensions(2, 3);
    planned_water.set_rotates(1);
    FoundationCellDefinition planned_water_cell;
    planned_water_cell.x = 0;
    planned_water_cell.y = 0;
    planned_water_cell.required_terrain = TERRAIN_WATER;
    planned_water.add_cell(planned_water_cell);
    FoundationCellDefinition planned_land_cell;
    planned_land_cell.x = 1;
    planned_land_cell.y = 2;
    planned_water.add_cell(planned_land_cell);
    const BuildingGeometry planned_geometry =
        BuildingGeometry::from_foundation(planned_water, 20, 30, 1);
    const std::set<std::pair<int, int>> expected_planned_cells = {
        { 20, 31 }, { 22, 30 }
    };
    const std::set<std::pair<int, int>> expected_planned_water_candidates = {
        { 20, 30 }, { 19, 31 }, { 21, 31 }, { 20, 32 }
    };
    std::set<std::pair<int, int>> actual_planned_cells;
    for (const BuildingGeometryCell &cell : planned_geometry.cells()) {
        actual_planned_cells.emplace(cell.x, cell.y);
    }
    if (!planned_geometry.valid() || actual_planned_cells != expected_planned_cells ||
        planned_geometry.bounds().width() != 3 || planned_geometry.bounds().height() != 2 ||
        points_as_set(planned_geometry.water_candidates()) != expected_planned_water_candidates) {
        std::cerr << "BuildingGeometry planned-water contract failed: selected rotation or exact candidates changed.\n";
        return false;
    }
    const BuildingGeometryCell *access_interior =
        normalized_geometry.interior_cell_for_access(11, 11);
    const BuildingGeometry single_cell_geometry = BuildingGeometry::from_world_cells({
        { 4, 4, nullptr, nullptr, FoundationPassage::None, false }
    });
    const std::vector<BuildingGeometryPoint> expected_distance_one = {
        { 3, 3 }, { 4, 3 }, { 5, 3 },
        { 3, 4 },           { 5, 4 },
        { 3, 5 }, { 4, 5 }, { 5, 5 }
    };
    const std::vector<BuildingGeometryPoint> expected_access_distance_one = {
                 { 4, 3 },
        { 3, 4 },          { 5, 4 },
                 { 4, 5 }
    };
    const auto points_equal = [](const std::vector<BuildingGeometryPoint> &left,
        const std::vector<BuildingGeometryPoint> &right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (left[i].x != right[i].x || left[i].y != right[i].y) {
                return false;
            }
        }
        return true;
    };
    if (!normalized_geometry.occupies_multiple_cells() ||
        single_cell_geometry.occupies_multiple_cells() ||
        !access_interior || access_interior->x != 11 || access_interior->y != 10 ||
        normalized_geometry.interior_cell_for_access(9, 9) ||
        !points_equal(single_cell_geometry.points_at_distance(1), expected_distance_one) ||
        !points_equal(
            single_cell_geometry.access_points_at_distance(1),
            expected_access_distance_one) ||
        !single_cell_geometry.points_at_distance(0).empty()) {
        std::cerr << "BuildingGeometry contract failed: access-to-interior mapping or deterministic distance rings changed.\n";
        return false;
    }

    const BuildingGeometry two_by_two = BuildingGeometry::from_world_cells({
        { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }
    });
    const std::vector<ProjectedBuildingGeometryCell> north_projection =
        project_building_geometry(two_by_two, 0, 1, DIR_0_TOP);
    const std::vector<ProjectedBuildingGeometryCell> east_projection =
        project_building_geometry(two_by_two, 0, 0, DIR_2_RIGHT);
    const auto has_all_overlay_variants = [](const std::vector<ProjectedBuildingGeometryCell> &cells) {
        std::set<int> variants;
        for (const ProjectedBuildingGeometryCell &cell : cells) {
            variants.insert(cell.overlay_image_variant);
        }
        return cells.size() == 4 && variants == std::set<int>({ 0, 1, 2, 3 });
    };
    if (!has_all_overlay_variants(north_projection) || !has_all_overlay_variants(east_projection) ||
        north_projection.front().x != 30 || north_projection.front().y != -15 ||
        north_projection.front().overlay_image_variant != 0) {
        std::cerr << "BuildingGeometry projection contract failed: rotated overlay cells or edge variants changed.\n";
        return false;
    }

    const BuildingGeometry rectangular_union = BuildingGeometry::from_world_cells({
        { 0, 0, nullptr, nullptr, FoundationPassage::None, false },
        { 14, 0, nullptr, nullptr, FoundationPassage::None, false },
        { 0, 4, nullptr, nullptr, FoundationPassage::None, false },
        { 14, 4, nullptr, nullptr, FoundationPassage::None, false }
    });
    const BuildingGeometrySquareExtent explosion_extent = rectangular_union.centered_square_extent(5);
    if (!rectangular_union.valid() || rectangular_union.bounds().width() != 15 ||
        rectangular_union.bounds().height() != 5 || explosion_extent.x != 5 ||
        explosion_extent.y != 0 || explosion_extent.size != 5 ||
        rectangular_union.contains_within_range(7, 2, 2) ||
        !rectangular_union.contains_within_range(2, 2, 2)) {
        std::cerr << "BuildingGeometry range/effect contract failed: sparse rectangular union used scalar bounds.\n";
        return false;
    }

    const BuildingGeometry sparse_source = BuildingGeometry::from_world_cells({
        { 0, 0 }, { 10, 0 }
    });
    const BuildingGeometry sparse_gap_target = BuildingGeometry::from_world_cells({
        { 5, 0 }
    });
    if (sparse_source.distance_to(sparse_gap_target) != 5 ||
        sparse_gap_target.distance_to(sparse_source) != 5 ||
        BuildingGeometry{}.distance_to(sparse_source) != -1) {
        std::cerr << "BuildingGeometry distance contract failed: sparse gaps were treated as occupied bounds.\n";
        return false;
    }

    const FoundationDef *granary = find_foundation_definition("granary_3x3");
    if (!granary) {
        std::cerr << "Foundation road-access contract failed: granary_3x3 is missing.\n";
        return false;
    }
    const std::set<std::pair<int, int>> expected_granary_access[] = {
        { { 1, -1 }, { 3, 1 }, { 1, 3 }, { -1, 1 } },
        { { 1, -1 }, { 3, 1 }, { 1, 3 }, { -1, 1 } },
        { { 1, -1 }, { 3, 1 }, { 1, 3 }, { -1, 1 } },
        { { 1, -1 }, { 3, 1 }, { 1, 3 }, { -1, 1 } }
    };
    for (int rotation = 0; rotation < 4; ++rotation) {
        std::set<std::pair<int, int>> actual;
        for (const FoundationPerimeterCell &cell : granary->rotated_access_perimeter(rotation)) {
            actual.emplace(cell.x, cell.y);
        }
        if (actual != expected_granary_access[rotation]) {
            std::cerr << "Foundation road-access contract failed: passage perimeter changed at rotation "
                      << rotation << ".\n";
            return false;
        }
    }

    const FoundationDef *waterside = find_foundation_definition("waterside_2x2");
    if (!waterside) {
        std::cerr << "Foundation water-perimeter contract failed: waterside_2x2 is missing.\n";
        return false;
    }
    const std::set<std::pair<int, int>> expected_water_perimeter[] = {
        { { 0, -1 }, { 1, -1 }, { -1, 0 }, { 2, 0 } },
        { { 0, -1 }, { -1, 0 }, { -1, 1 }, { 0, 2 } },
        { { -1, 1 }, { 2, 1 }, { 0, 2 }, { 1, 2 } },
        { { 1, -1 }, { 2, 0 }, { 2, 1 }, { 1, 2 } }
    };
    for (int rotation = 0; rotation < 4; ++rotation) {
        std::set<std::pair<int, int>> actual;
        for (const FoundationPerimeterCell &cell : waterside->rotated_water_perimeter(rotation)) {
            actual.emplace(cell.x, cell.y);
        }
        const BuildingGeometry planned_waterside =
            BuildingGeometry::from_foundation(*waterside, 0, 0, rotation);
        if (actual != expected_water_perimeter[rotation] ||
            points_as_set(planned_waterside.water_candidates()) != expected_water_perimeter[rotation]) {
            std::cerr << "Foundation water-perimeter contract failed at rotation "
                      << rotation << ".\n";
            return false;
        }
    }

    const BuildingType *dock_type = definition_for_type(type_from_attr("dock"));
    const BuildingType *wharf_type = definition_for_type(type_from_attr("wharf"));
    const BuildingType *shipyard_type = definition_for_type(type_from_attr("shipyard"));
    if (!dock_type || !dock_type->water_access().requires_open_water() ||
        !wharf_type || wharf_type->water_access().requires_open_water() ||
        !shipyard_type || shipyard_type->water_access().requires_open_water() ||
        !building_type_water_access_open_water_attribute_is_valid_for_test("true") ||
        !building_type_water_access_open_water_attribute_is_valid_for_test("false") ||
        building_type_water_access_open_water_attribute_is_valid_for_test("sometimes")) {
        std::cerr << "WaterAccess open-water parser contract failed: dock ownership or Boolean validation changed.\n";
        return false;
    }

    FoundationDef sparse("sparse_rotation_fixture");
    sparse.set_dimensions(2, 3);
    sparse.set_rotates(1);
    FoundationCellDefinition passage;
    passage.symbol = 'P';
    passage.x = 0;
    passage.y = 0;
    passage.required_terrain = 0x10;
    passage.added_terrain = 0x20;
    passage.removed_terrain = 0x40;
    passage.binds_building = 0;
    passage.passage = FoundationPassage::OwnerControlled;
    sparse.add_cell(passage);
    FoundationCellDefinition corner;
    corner.symbol = 'C';
    corner.x = 1;
    corner.y = 2;
    sparse.add_cell(corner);
    const std::pair<int, int> expected_passage[] = {
        { 0, 0 }, { 0, 1 }, { 1, 2 }, { 2, 0 }
    };
    for (int rotation = 0; rotation < 4; ++rotation) {
        const std::vector<RotatedFoundationCell> cells = sparse.rotated_cells(rotation);
        if (cells.size() != 2 || cells[0].x != expected_passage[rotation].first ||
            cells[0].y != expected_passage[rotation].second || !cells[0].definition ||
            cells[0].definition->required_terrain != 0x10 ||
            cells[0].definition->added_terrain != 0x20 ||
            cells[0].definition->removed_terrain != 0x40 ||
            cells[0].definition->binds_building ||
            cells[0].definition->passage != FoundationPassage::OwnerControlled) {
            std::cerr << "Foundation rotation contract failed: sparse cell data did not rotate intact.\n";
            return false;
        }
    }

    const char *valid_xml =
        "<foundation type=\"fixture\" width=\"2\" height=\"1\" rotates=\"true\">"
        "<profile symbol=\"L\" requires=\"land\" adds=\"building\"/>"
        "<rows><row cells=\"LL\"/></rows></foundation>";
    const char *bad_width_xml =
        "<foundation type=\"fixture\" width=\"2\" height=\"1\" rotates=\"true\">"
        "<profile symbol=\"L\" requires=\"land\"/><rows><row cells=\"L\"/></rows></foundation>";
    const char *unknown_symbol_xml =
        "<foundation type=\"fixture\" width=\"1\" height=\"1\" rotates=\"true\">"
        "<profile symbol=\"L\" requires=\"land\"/><rows><row cells=\"X\"/></rows></foundation>";
    const char *bad_permissions_xml =
        "<foundation type=\"fixture\" width=\"1\" height=\"1\" rotates=\"true\" "
        "default_permissions=\"all\" configurable_permissions=\"none\">"
        "<profile symbol=\"L\" requires=\"land\"/><rows><row cells=\"L\"/></rows></foundation>";
    const char *bad_rotation_xml =
        "<foundation type=\"fixture\" width=\"1\" height=\"1\" rotates=\"sometimes\">"
        "<profile symbol=\"L\" requires=\"land\"/><rows><row cells=\"L\"/></rows></foundation>";
    if (!foundation_definition_buffer_is_valid_for_test(valid_xml, "fixture") ||
        foundation_definition_buffer_is_valid_for_test(bad_width_xml, "fixture") ||
        foundation_definition_buffer_is_valid_for_test(unknown_symbol_xml, "fixture") ||
        foundation_definition_buffer_is_valid_for_test(bad_permissions_xml, "fixture") ||
        foundation_definition_buffer_is_valid_for_test(bad_rotation_xml, "fixture")) {
        std::cerr << "Foundation parser contract failed: malformed fixture acceptance is incorrect.\n";
        return false;
    }

    FoundationState state;
    state.begin_publication(11, 12, 5);
    FoundationTerrainDelta delta;
    delta.cell_index = 2;
    delta.grid_offset = 1234;
    delta.added_terrain = 0x40;
    delta.removed_terrain = 0x100;
    state.record_delta(delta);
    if (!state.is_published() || state.origin_x() != 11 || state.origin_y() != 12 || state.rotation() != 1 ||
        state.terrain_deltas().size() != 1 || state.terrain_deltas().front().grid_offset != 1234 ||
        state.terrain_deltas().front().added_terrain != 0x40 ||
        state.terrain_deltas().front().removed_terrain != 0x100) {
        std::cerr << "Foundation state contract failed: exact publication deltas were not retained.\n";
        return false;
    }

    FoundationDef save_definition("save_fixture");
    save_definition.set_dimensions(2, 1);
    FoundationCellDefinition save_cell_a;
    save_cell_a.x = 0;
    save_cell_a.added_terrain = TERRAIN_BUILDING | TERRAIN_ROAD;
    save_cell_a.binds_building = 0;
    save_definition.add_cell(save_cell_a);
    FoundationCellDefinition save_cell_b;
    save_cell_b.x = 1;
    save_cell_b.added_terrain = TERRAIN_BUILDING;
    save_cell_b.removed_terrain = TERRAIN_MEADOW | TERRAIN_SHRUB;
    save_cell_b.binds_building = 0;
    save_definition.add_cell(save_cell_b);
    FoundationState save_state;
    save_state.begin_publication(20, 30, 0);
    FoundationTerrainDelta save_delta_a;
    save_delta_a.cell_index = 0;
    save_delta_a.added_terrain = TERRAIN_BUILDING;
    save_state.record_delta(save_delta_a);
    FoundationTerrainDelta save_delta_b;
    save_delta_b.cell_index = 1;
    save_delta_b.added_terrain = TERRAIN_BUILDING;
    save_delta_b.removed_terrain = TERRAIN_MEADOW;
    save_state.record_delta(save_delta_b);
    const FoundationTerrainSaveState saved =
        foundation_terrain_state_for_save(save_definition, save_state);
    std::vector<FoundationTerrainDelta> loaded_deltas;
    if (!saved.published ||
        !foundation_terrain_deltas_from_save(save_definition, saved, &loaded_deltas) ||
        loaded_deltas.size() != 2 ||
        loaded_deltas[0].added_terrain != TERRAIN_BUILDING || loaded_deltas[0].removed_terrain ||
        loaded_deltas[1].added_terrain != TERRAIN_BUILDING ||
        loaded_deltas[1].removed_terrain != TERRAIN_MEADOW ||
        loaded_deltas[0].bound_building || loaded_deltas[1].bound_building) {
        std::cerr << "Foundation save bridge contract failed: exact unbound per-cell terrain deltas changed.\n";
        return false;
    }
    FoundationTerrainSaveState unpublished;
    if (foundation_terrain_deltas_from_save(save_definition, unpublished, &loaded_deltas)) {
        std::cerr << "Foundation save bridge contract failed: unpublished state was hydrated.\n";
        return false;
    }
    FoundationTerrainSaveState invalid_saved = saved;
    invalid_saved.added[0] |= TERRAIN_AQUEDUCT;
    if (foundation_terrain_deltas_from_save(save_definition, invalid_saved, &loaded_deltas)) {
        std::cerr << "Foundation save bridge contract failed: unauthored terrain delta was accepted.\n";
        return false;
    }

    const uint32_t highway_bits[] = {
        TERRAIN_HIGHWAY_TOP_LEFT,
        TERRAIN_HIGHWAY_TOP_RIGHT,
        TERRAIN_HIGHWAY_BOTTOM_LEFT,
        TERRAIN_HIGHWAY_BOTTOM_RIGHT
    };
    std::array<uint32_t, 6> original_terrain = {
        TERRAIN_ROAD | TERRAIN_MEADOW, TERRAIN_ROAD, TERRAIN_ROAD,
        TERRAIN_ROAD, TERRAIN_ROAD | TERRAIN_MEADOW, TERRAIN_ROAD
    };
    std::array<uint32_t, 6> terrain = original_terrain;
    std::array<std::vector<FoundationTerrainDelta>, 2> batch_deltas;
    std::array<uint32_t, 6> after_first = {};
    for (int placement = 0; placement < 2; ++placement) {
        for (int cell_index = 0; cell_index < 4; ++cell_index) {
            FoundationCellDefinition cell;
            cell.added_terrain = highway_bits[cell_index];
            cell.removed_terrain = TERRAIN_ROAD;
            cell.binds_building = 0;
            const int local_x = cell_index & 1;
            const int local_y = cell_index >> 1;
            const int terrain_index = local_y * 3 + placement + local_x;
            const FoundationTerrainMutation mutation = foundation_apply_terrain_cell(
                cell, cell_index, terrain_index, terrain[terrain_index]);
            terrain[terrain_index] = mutation.terrain_after;
            batch_deltas[placement].push_back(mutation.delta);
        }
        if (placement == 0) {
            after_first = terrain;
        }
    }
    for (int placement = 1; placement >= 0; --placement) {
        for (auto it = batch_deltas[placement].rbegin(); it != batch_deltas[placement].rend(); ++it) {
            terrain[it->grid_offset] = foundation_restore_terrain_cell(terrain[it->grid_offset], *it);
        }
        const std::array<uint32_t, 6> &expected = placement == 1 ? after_first : original_terrain;
        if (terrain != expected) {
            std::cerr << "Foundation terrain transaction failed: overlapping multi-cell rollback was not exact.\n";
            return false;
        }
    }
    state.clear();
    if (state.is_published() || !state.terrain_deltas().empty()) {
        std::cerr << "Foundation state contract failed: clear retained live publication data.\n";
        return false;
    }

    RoadblockState roadblock;
    const uint16_t maintenance = 1u << 1;
    const uint16_t priest = 1u << 2;
    const uint16_t labor_seeker = 1u << 8;
    roadblock.configure("roadblock_fixture", labor_seeker, static_cast<uint16_t>(maintenance | labor_seeker));
    if (!roadblock.has_permission(8) || roadblock.has_permission(1) ||
        !roadblock.can_configure(1) || roadblock.can_configure(2) ||
        !roadblock.toggle_permission(1) || roadblock.toggle_permission(2) ||
        !roadblock.has_permission(1)) {
        std::cerr << "Roadblock state contract failed: defaults or configurable toggles are incorrect.\n";
        return false;
    }
    roadblock.hydrate(UINT16_MAX);
    if (roadblock.permissions() != (maintenance | labor_seeker) || roadblock.has_permission(2)) {
        std::cerr << "Roadblock state contract failed: hydration escaped the configurable mask.\n";
        return false;
    }
    roadblock.configure("fixed_permission_fixture", priest, maintenance);
    roadblock.accept_none();
    if (roadblock.permissions() != priest || !roadblock.has_permission(2) || roadblock.has_permission(1)) {
        std::cerr << "Roadblock state contract failed: fixed definition permissions were not preserved.\n";
        return false;
    }
    roadblock.accept_all();
    if (roadblock.permissions() != (priest | maintenance)) {
        std::cerr << "Roadblock state contract failed: accept-all ignored the definition masks.\n";
        return false;
    }
    const BuildingType *roadblock_type = definition_for_type(type_from_attr("roadblock"));
    const BuildingType *road_type = definition_for_type(type_from_attr("road"));
    const BuildingType *aqueduct_type = definition_for_type(type_from_attr("aqueduct"));
    const BuildingType *reservoir_type = definition_for_type(type_from_attr("reservoir"));
    const BuildingType *highway_type = definition_for_type(type_from_attr("highway"));
    const BuildingType *low_bridge_type = definition_for_type(type_from_attr("low_bridge"));
    if (!roadblock_type || !roadblock_type->tool().is_roadblock() ||
        !construction_session_retains_resolved_type(*roadblock_type) ||
        !road_type || construction_session_retains_resolved_type(*road_type) ||
        !low_bridge_type || !construction_session_retains_resolved_type(*low_bridge_type)) {
        std::cerr << "Construction session contract failed: path blockers or bridges are not retained.\n";
        return false;
    }
    if (!aqueduct_type || !reservoir_type ||
        !road_type->foundation_may_replace(road_type) ||
        road_type->foundation_may_replace(aqueduct_type) ||
        !aqueduct_type->foundation_may_replace(aqueduct_type) ||
        aqueduct_type->foundation_may_replace(road_type) ||
        !reservoir_type->foundation_may_replace(aqueduct_type) ||
        reservoir_type->foundation_may_replace(reservoir_type) ||
        road_type->foundation_replacement_types().size() != 1 ||
        aqueduct_type->foundation_replacement_types().size() != 1 ||
        reservoir_type->foundation_replacement_types().size() != 1) {
        std::cerr << "Foundation supersession contract failed: road_self="
            << road_type->foundation_may_replace(road_type)
            << " road_aqueduct=" << road_type->foundation_may_replace(aqueduct_type)
            << " aqueduct_self=" << (aqueduct_type ? aqueduct_type->foundation_may_replace(aqueduct_type) : -1)
            << " aqueduct_road=" << (aqueduct_type ? aqueduct_type->foundation_may_replace(road_type) : -1)
            << " reservoir_aqueduct=" << (reservoir_type ? reservoir_type->foundation_may_replace(aqueduct_type) : -1)
            << " reservoir_self=" << (reservoir_type ? reservoir_type->foundation_may_replace(reservoir_type) : -1)
            << " sizes=" << road_type->foundation_replacement_types().size() << ','
            << (aqueduct_type ? aqueduct_type->foundation_replacement_types().size() : 0) << ','
            << (reservoir_type ? reservoir_type->foundation_replacement_types().size() : 0) << ".\n";
        return false;
    }
    const BuildingType *clear_land_type = definition_for_type(type_from_attr("clear_land"));
    const std::vector<SmartToolModeDefinition> &road_modes = road_type->tool().smart_tool().modes();
    const std::vector<SmartToolModeDefinition> &clear_modes =
        clear_land_type ? clear_land_type->tool().smart_tool().modes() : road_modes;
    const std::vector<SmartToolModeDefinition> &reservoir_modes =
        reservoir_type ? reservoir_type->tool().smart_tool().modes() : road_modes;
    if (!clear_land_type || road_modes.size() != 4 || clear_modes.size() != 2 ||
        reservoir_modes.size() != 1 || !reservoir_type->matches_identity("draggable_reservoir") ||
        road_modes[0].modifier != SmartToolModifier::Control ||
        road_modes[0].context != SmartToolContext::Default ||
        road_modes[0].target != SmartToolTarget::SingleTarget ||
        !road_modes[0].type || road_modes[0].type->type() != type_from_attr("roadblock") ||
        road_modes[0].footprint_size != 1 ||
        road_modes[1].modifier != SmartToolModifier::Shift ||
        road_modes[1].context != SmartToolContext::Default ||
        road_modes[1].target != SmartToolTarget::DragArea ||
        !road_modes[1].type || road_modes[1].type->type() != type_from_attr("highway") ||
        road_modes[1].footprint_size != 2 ||
        road_modes[2].modifier != SmartToolModifier::Any ||
        road_modes[2].context != SmartToolContext::HoverWater ||
        !road_modes[2].type || road_modes[2].type != low_bridge_type ||
        road_modes[3].modifier != SmartToolModifier::Shift ||
        road_modes[3].context != SmartToolContext::HoverWater ||
        !road_modes[3].type || !road_modes[3].type->bridge().is_ship_bridge() ||
        reservoir_modes[0].modifier != SmartToolModifier::Any ||
        reservoir_modes[0].context != SmartToolContext::RoutedSegment ||
        !reservoir_modes[0].type || reservoir_modes[0].type != aqueduct_type ||
        clear_modes[0].modifier != SmartToolModifier::Control ||
        clear_modes[0].target != SmartToolTarget::DragArea ||
        !clear_modes[0].type || clear_modes[0].type->type() != type_from_attr("repair_land") ||
        clear_modes[0].footprint_size != 1 ||
        clear_modes[1].modifier != SmartToolModifier::Shift ||
        clear_modes[1].target != SmartToolTarget::DragArea ||
        !clear_modes[1].type || clear_modes[1].type->type() != type_from_attr("clear_trees") ||
        clear_modes[1].footprint_size != 1 ||
        road_type->tool().smart_tool().resolve_mode(1, 1) != &road_modes[0] ||
        road_type->tool().smart_tool().resolve_mode(0, 1) != &road_modes[1] ||
        road_type->tool().smart_tool().resolve_mode(
            1, 1, SmartToolContext::HoverWater) != &road_modes[3] ||
        road_type->tool().smart_tool().resolve_mode(
            0, 0, SmartToolContext::HoverWater) != &road_modes[2] ||
        reservoir_type->tool().smart_tool().resolve_mode(
            0, 0, SmartToolContext::RoutedSegment) != &reservoir_modes[0] ||
        road_type->tool().smart_tool().mode_for_type(roadblock_type) != &road_modes[0] ||
        road_type->tool().smart_tool().mode_for_type(low_bridge_type) != nullptr ||
        road_type->tool().smart_tool().mode_for_type(
            low_bridge_type, SmartToolContext::HoverWater) != &road_modes[2] ||
        !building_tool_mode_is_base_drag_selection(*road_type) ||
        !building_tool_mode_is_base_drag_selection(*clear_land_type) ||
        building_tool_mode_is_base_drag_selection(*roadblock_type)) {
        std::cerr << "SmartToolDef contract failed: parsed modes or resolved target types changed."
            << " road_modes=" << road_modes.size()
            << " clear_modes=" << clear_modes.size()
            << " reservoir_modes=" << reservoir_modes.size()
            << " reservoir_alias=" << (reservoir_type ? reservoir_type->matches_identity("draggable_reservoir") : 0)
            << ".\n";
        return false;
    }
    SmartToolDef fallback_modes;
    SmartToolModeDefinition any_mode;
    any_mode.modifier = SmartToolModifier::Any;
    fallback_modes.add_mode(any_mode);
    SmartToolModeDefinition control_mode;
    control_mode.modifier = SmartToolModifier::Control;
    fallback_modes.add_mode(control_mode);
    if (fallback_modes.resolve_mode(1, 0) != &fallback_modes.modes()[1] ||
        fallback_modes.resolve_mode(0, 0) != &fallback_modes.modes()[0]) {
        std::cerr << "SmartToolDef contract failed: modifier=any shadows a specific mode.\n";
        return false;
    }
    const FoundationDef *roadblock_foundation = roadblock_type ? roadblock_type->foundation_def() : nullptr;
    const FoundationDef *road_foundation = road_type ? road_type->foundation_def() : nullptr;
    const FoundationDef *highway_foundation = highway_type ? highway_type->foundation_def() : nullptr;
    const BuildingType *warehouse_type = definition_for_type(type_from_attr("warehouse"));
    const BuildingType *warehouse_space_type = definition_for_type(type_from_attr("warehouse_space"));
    const FoundationDef *warehouse_foundation = warehouse_type ? warehouse_type->foundation_def() : nullptr;
    const FoundationDef *warehouse_space_foundation =
        warehouse_space_type ? warehouse_space_type->foundation_def() : nullptr;
    if (!roadblock_foundation || roadblock_foundation->cells().size() != 1 ||
        !roadblock_foundation->cells()[0].binds_building ||
        roadblock_foundation->default_permissions() != 0 ||
        roadblock_foundation->configurable_permissions() == 0 ||
        !(roadblock_foundation->cells()[0].required_terrain & TERRAIN_ROAD) ||
        !(roadblock_foundation->cells()[0].permitted_blocking_terrain & TERRAIN_ROAD) ||
        !road_foundation || road_foundation->cells().size() != 1 ||
        road_foundation->cells()[0].binds_building ||
        !highway_foundation || highway_foundation->cells().size() != 4 ||
        std::any_of(highway_foundation->cells().begin(), highway_foundation->cells().end(),
            [](const FoundationCellDefinition &cell) {
                return cell.binds_building || !(cell.removed_terrain & TERRAIN_ROAD);
            })) {
        std::cerr << "Roadblock publication contract failed: a new bound blocker must close every configurable permission without replacing the road surface.\n";
        return false;
    }
    if (!warehouse_foundation || warehouse_foundation->cells().size() != 1 ||
        warehouse_foundation->cells()[0].passage != FoundationPassage::OwnerControlled ||
        warehouse_foundation->default_permissions() != 0 ||
        warehouse_foundation->configurable_permissions() == 0 ||
        !(warehouse_foundation->cells()[0].added_terrain & TERRAIN_ROAD) ||
        !warehouse_space_foundation || warehouse_space_foundation->cells().size() != 1 ||
        warehouse_space_foundation->cells()[0].passage != FoundationPassage::None ||
        (warehouse_space_foundation->cells()[0].added_terrain & TERRAIN_ROAD)) {
        std::cerr << "Warehouse foundation contract failed: only the freight-gated entrance may publish a road passage.\n";
        return false;
    }
    if (!smart_tool_mode_attributes_are_valid_for_test("ctrl", "roadblock", "single", 1) ||
        !smart_tool_mode_attributes_are_valid_for_test("shift", "highway", "drag", 2) ||
        !smart_tool_mode_attributes_are_valid_for_test("any", "road", "drag", 1) ||
        !smart_tool_mode_attributes_are_valid_for_test("any", "aqueduct", "drag", 1, "route") ||
        !smart_tool_mode_attributes_are_valid_for_test("shift", "ship_bridge", "drag", 1, "water") ||
        smart_tool_mode_attributes_are_valid_for_test(nullptr, "road", "drag", 1) ||
        smart_tool_mode_attributes_are_valid_for_test("alt", "road", "drag", 1) ||
        smart_tool_mode_attributes_are_valid_for_test("ctrl", "", "drag", 1) ||
        smart_tool_mode_attributes_are_valid_for_test("ctrl", "roadblock", "tile", 1) ||
        smart_tool_mode_attributes_are_valid_for_test("any", "aqueduct", "drag", 1, "shore") ||
        smart_tool_mode_attributes_are_valid_for_test("ctrl", "roadblock", "single", 2) ||
        smart_tool_mode_attributes_are_valid_for_test("ctrl", "roadblock", "drag", 0)) {
        std::cerr << "SmartToolDef parser contract failed: invalid mode metadata was accepted.\n";
        return false;
    }
    if (!bridge_definition_attribute_is_valid_for_test("low") ||
        !bridge_definition_attribute_is_valid_for_test("ship") ||
        bridge_definition_attribute_is_valid_for_test(nullptr) ||
        bridge_definition_attribute_is_valid_for_test("wall_gate")) {
        std::cerr << "Bridge parser contract failed: unsupported dynamic bridge metadata was accepted.\n";
        return false;
    }
    auto status_icon_anchor_matches = [](const BuildingType *definition, int expected_x, int expected_y) {
        int x = 0;
        int y = 0;
        return definition && definition->graphics().status_icon_anchor(&x, &y) &&
            x == expected_x && y == expected_y;
    };
    if (!status_icon_anchor_matches(definition_for_type(type_from_attr("warehouse")), 21, -60) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("granary")), 83, -120) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("fountain")), 20, -15) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("wheat_farm")), 50, -50) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("vegetable_farm")), 50, -50) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("fruit_farm")), 50, -50) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("olive_farm")), 50, -50) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("vines_farm")), 50, -50) ||
        !status_icon_anchor_matches(definition_for_type(type_from_attr("pig_farm")), 50, -50)) {
        std::cerr << "Status-icon anchor contract failed: authored native graphics offsets changed.\n";
        return false;
    }
    int unanchored_x = 0;
    int unanchored_y = 0;
    const BuildingType *doctor_type = definition_for_type(type_from_attr("doctor"));
    if (!doctor_type || doctor_type->graphics().status_icon_anchor(&unanchored_x, &unanchored_y) ||
        !graphics_status_icon_attributes_are_valid_for_test("0", "-120") ||
        !graphics_status_icon_attributes_are_valid_for_test("83", "0") ||
        !graphics_status_icon_attributes_are_valid_for_test("2147483647", "-2147483648") ||
        graphics_status_icon_attributes_are_valid_for_test(nullptr, "0") ||
        graphics_status_icon_attributes_are_valid_for_test("0", nullptr) ||
        graphics_status_icon_attributes_are_valid_for_test("", "0") ||
        graphics_status_icon_attributes_are_valid_for_test("0", "") ||
        graphics_status_icon_attributes_are_valid_for_test("1.5", "0") ||
        graphics_status_icon_attributes_are_valid_for_test("0", "north") ||
        graphics_status_icon_attributes_are_valid_for_test(" 1", "0") ||
        graphics_status_icon_attributes_are_valid_for_test("0", "1 ") ||
        graphics_status_icon_attributes_are_valid_for_test("2147483648", "0") ||
        graphics_status_icon_attributes_are_valid_for_test("0", "-2147483649")) {
        std::cerr << "Status-icon anchor parser contract failed: missing or non-integer coordinates were accepted.\n";
        return false;
    }
    if (!building_type_geometry_attributes_are_valid_for_test("land_1x1", nullptr, nullptr, nullptr) ||
        building_type_geometry_attributes_are_valid_for_test(nullptr, "land", nullptr, nullptr) ||
        building_type_geometry_attributes_are_valid_for_test("land_1x1", "land", nullptr, nullptr) ||
        building_type_geometry_attributes_are_valid_for_test("land_1x1", nullptr, "true", nullptr) ||
        building_type_geometry_attributes_are_valid_for_test("land_1x1", nullptr, nullptr, "1")) {
        std::cerr << "BuildingType geometry parser contract failed: inline foundation or model-size metadata was accepted.\n";
        return false;
    }

    const FoundationDef *dock = find_foundation_definition("dock_3x3");
    if (!dock || dock->width() != 3 || dock->height() != 3 || !dock->rotates()) {
        std::cerr << "Foundation registry contract failed: dock_3x3 is missing or non-rotatable.\n";
        return false;
    }
    const int expected_legacy_rotations[] = { 0, 3, 2, 1 };
    for (int rotation = 0; rotation < 4; ++rotation) {
        if (foundation_rotation_from_save(
                *dock,
                rotation,
                SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS,
                SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS) != expected_legacy_rotations[rotation] ||
            foundation_rotation_from_save(
                *dock,
                rotation,
                SAVE_GAME_CURRENT_VERSION,
                SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS) != rotation) {
            std::cerr << "Foundation save bridge contract failed: legacy waterside rotation "
                      << rotation << " was not converted exactly once.\n";
            return false;
        }
    }
    if (foundation_rotation_from_save(
            definition,
            1,
            SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS,
            SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS) != 1) {
        std::cerr << "Foundation save bridge contract failed: non-waterside rotation was converted.\n";
        return false;
    }
    const std::set<std::pair<int, int>> expected_water[] = {
        { { 0, 0 }, { 1, 0 }, { 2, 0 } },
        { { 0, 0 }, { 0, 1 }, { 0, 2 } },
        { { 0, 2 }, { 1, 2 }, { 2, 2 } },
        { { 2, 0 }, { 2, 1 }, { 2, 2 } }
    };
    for (int rotation = 0; rotation < 4; ++rotation) {
        int land_cells = 0;
        std::set<std::pair<int, int>> water_cells;
        for (const RotatedFoundationCell &cell : dock->rotated_cells(rotation)) {
            if (!cell.definition) {
                return false;
            }
            if (cell.definition->required_terrain & TERRAIN_WATER) {
                water_cells.emplace(cell.x, cell.y);
            } else {
                ++land_cells;
            }
        }
        if (land_cells != 6 || water_cells != expected_water[rotation]) {
            std::cerr << "Foundation registry contract failed: dock footprint changed at rotation "
                      << rotation << ".\n";
            return false;
        }
    }

    const BuildingType *farm = definition_for_type(type_from_attr("wheat_farm"));
    const BuildingType *field = definition_for_type(type_from_attr("wheat_farm_field"));
    if (!farm || !field || !farm->foundation_def() || !field->foundation_def() ||
        std::string(farm->foundation_def()->path()) != "land_2x2" ||
        std::string(field->foundation_def()->path()) != "meadow_1x1" ||
        field->foundation_def()->cells().size() != 1 ||
        field->foundation_def()->cells().front().required_terrain != TERRAIN_MEADOW) {
        std::cerr << "Foundation registry contract failed: Vespasian farm meadow ownership changed.\n";
        return false;
    }

    std::cout << "Validated FoundationDef rotations, declarative placement supersession, road permissions, dock geometry/open-water ownership, farm meadow ownership, and strict parser rejection contracts.\n";
    return true;
}
