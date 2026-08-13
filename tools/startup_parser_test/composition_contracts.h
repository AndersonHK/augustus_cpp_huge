#pragma once

#include "building/CompositionDef.h"
#include "building/CompositionHydration.h"
#include "building/CompositionPlacementAccounting.h"
#include "building/CompositionTypeReplacement.h"
#include "building/building_type_registry_internal.h"

#include <array>
#include <ostream>
#include <string>
#include <vector>

inline building_construction::PlacementAccountingResult composition_layout_accounting(
    const building_type_registry_impl::CompositionLayoutResult &layout)
{
    std::vector<building_construction::PlacementAccountingCell> cells;
    for (std::size_t member_index = 0; member_index < layout.members.size(); ++member_index) {
        const building_type_registry_impl::CompositionLayoutMember &member = layout.members[member_index];
        const building_type_registry_impl::FoundationDef *foundation =
            member.type ? member.type->foundation_def() : nullptr;
        if (!foundation) {
            continue;
        }
        for (const building_type_registry_impl::RotatedFoundationCell &cell :
            foundation->rotated_cells(member.foundation_rotation)) {
            const int world_x = member.x + cell.x;
            const int world_y = member.y + cell.y;
            cells.push_back(building_construction::PlacementAccountingCell{
                (world_y + 256) * 1024 + world_x + 256,
                member.is_owner,
                true,
                true,
                building_construction::RepairRubbleOccupancy::MatchingOrigin
            });
        }
    }
    return building_construction::summarize_placement_cells(cells);
}

inline bool validate_composition_rotation_contracts(std::ostream &error)
{
    using building_type_registry_impl::CompositionChildDef;
    using building_type_registry_impl::CompositionOffset;

    struct OffsetContract {
        CompositionOffset canonical;
        int owner_width;
        int owner_height;
        int child_width;
        int child_height;
        std::array<CompositionOffset, 4> expected;
        const char *label;
    };

    const OffsetContract contracts[] = {
        {
            { 3, -1, true }, 3, 3, 4, 4,
            { CompositionOffset{ 3, -1, true }, CompositionOffset{ -1, -4, true },
                CompositionOffset{ -4, 0, true }, CompositionOffset{ 0, 3, true } },
            "fort mustering ground"
        },
        {
            { 5, 0, true }, 5, 5, 5, 5,
            { CompositionOffset{ 5, 0, true }, CompositionOffset{ 0, -5, true },
                CompositionOffset{ -5, 0, true }, CompositionOffset{ 0, 5, true } },
            "hippodrome middle"
        },
        {
            { 0, 2, true }, 2, 2, 1, 1,
            { CompositionOffset{ 0, 2, true }, CompositionOffset{ 2, 1, true },
                CompositionOffset{ 1, -1, true }, CompositionOffset{ -1, 0, true } },
            "farm field"
        }
    };

    for (const OffsetContract &contract : contracts) {
        for (int rotation = 0; rotation < 4; ++rotation) {
            const CompositionOffset actual = building_type_registry_impl::rotate_composition_offset(
                contract.canonical,
                rotation,
                contract.owner_width,
                contract.owner_height,
                contract.child_width,
                contract.child_height);
            const CompositionOffset &expected = contract.expected[rotation];
            if (!actual.defined || actual.x != expected.x || actual.y != expected.y) {
                error << "Composition rotation contract failed for " << contract.label
                      << " at rotation " << rotation << ".\n";
                return false;
            }
        }
    }

    CompositionChildDef warehouse_role;
    warehouse_role.set_canonical_offset(0, 1);
    warehouse_role.set_rotation_override(0, 0, 1);
    warehouse_role.set_rotation_override(1, 0, -2);
    warehouse_role.set_rotation_override(2, -2, -2);
    warehouse_role.set_rotation_override(3, -2, 0);
    if (!warehouse_role.has_complete_rotation_overrides() || warehouse_role.has_partial_rotation_overrides()) {
        error << "Composition rotation contract failed for complete warehouse overrides.\n";
        return false;
    }
    for (int rotation = 0; rotation < 4; ++rotation) {
        const CompositionOffset actual = warehouse_role.offset_for_rotation(rotation, 1, 1, 1, 1);
        const CompositionOffset expected[] = {
            { 0, 1, true }, { 0, -2, true }, { -2, -2, true }, { -2, 0, true }
        };
        if (!actual.defined || actual.x != expected[rotation].x || actual.y != expected[rotation].y) {
            error << "Composition rotation contract failed for warehouse role override "
                  << rotation << ".\n";
            return false;
        }
    }

    CompositionChildDef partial_override;
    partial_override.set_canonical_offset(1, 0);
    partial_override.set_rotation_override(0, 1, 0);
    if (!partial_override.has_partial_rotation_overrides() ||
        partial_override.offset_for_rotation(0, 1, 1, 1, 1).defined) {
        error << "Composition rotation contract failed to reject partial overrides.\n";
        return false;
    }

    using building_type_registry_impl::BuildingType;
    using building_type_registry_impl::CompositionDef;
    using building_type_registry_impl::CompositionFoundationView;
    using building_type_registry_impl::CompositionLayoutError;
    const building_type_registry_impl::CompositionFoundationResolver unit_foundation =
        [](const BuildingType &, int, CompositionFoundationView *foundation, std::string *) {
            if (!foundation) {
                return false;
            }
            foundation->width = 1;
            foundation->height = 1;
            foundation->occupied_cells = { { 0, 0 } };
            return true;
        };

    BuildingType owner(static_cast<building_type>(1), "composition_contract_owner");
    BuildingType child(static_cast<building_type>(2), "composition_contract_child");
    BuildingType nested(static_cast<building_type>(3), "composition_contract_nested");

    CompositionDef unresolved;
    unresolved.add_child("missing", "missing").set_canonical_offset(1, 0);
    if (building_type_registry_impl::build_composition_layout(
            &owner, unresolved, 0, 0, 0, unit_foundation).error !=
        CompositionLayoutError::MissingChildType) {
        error << "Composition definition contract did not reject an unresolved child.\n";
        return false;
    }

    CompositionDef self_reference;
    CompositionChildDef &self = self_reference.add_child("owner", "self");
    self.type = &owner;
    self.set_canonical_offset(1, 0);
    if (building_type_registry_impl::build_composition_layout(
            &owner, self_reference, 0, 0, 0, unit_foundation).error !=
        CompositionLayoutError::SelfReference) {
        error << "Composition definition contract did not reject self-reference.\n";
        return false;
    }

    nested.composition().add_child("owner", "cycle").set_canonical_offset(1, 0);
    CompositionDef nested_reference;
    CompositionChildDef &nested_child = nested_reference.add_child("nested", "nested");
    nested_child.type = &nested;
    nested_child.set_canonical_offset(1, 0);
    if (building_type_registry_impl::build_composition_layout(
            &owner, nested_reference, 0, 0, 0, unit_foundation).error !=
        CompositionLayoutError::NestedComposition) {
        error << "Composition definition contract did not reject nesting/cyclic ownership.\n";
        return false;
    }

    CompositionDef duplicate_roles;
    CompositionChildDef &duplicate_a = duplicate_roles.add_child("child", "duplicate");
    duplicate_a.type = &child;
    duplicate_a.set_canonical_offset(1, 0);
    CompositionChildDef &duplicate_b = duplicate_roles.add_child("child", "duplicate");
    duplicate_b.type = &child;
    duplicate_b.set_canonical_offset(2, 0);
    if (building_type_registry_impl::build_composition_layout(
            &owner, duplicate_roles, 0, 0, 0, unit_foundation).error !=
        CompositionLayoutError::DuplicateRole) {
        error << "Composition definition contract did not reject duplicate roles.\n";
        return false;
    }

    CompositionDef overlap;
    CompositionChildDef &overlapping_child = overlap.add_child("child", "overlap");
    overlapping_child.type = &child;
    overlapping_child.set_canonical_offset(0, 0);
    if (building_type_registry_impl::build_composition_layout(
            &owner, overlap, 0, 0, 0, unit_foundation).error !=
        CompositionLayoutError::Overlap) {
        error << "Composition definition contract did not reject overlapping foundations.\n";
        return false;
    }

    CompositionDef incomplete_overrides;
    CompositionChildDef &incomplete = incomplete_overrides.add_child("child", "incomplete");
    incomplete.type = &child;
    incomplete.set_canonical_offset(1, 0);
    incomplete.set_rotation_override(0, 1, 0);
    if (building_type_registry_impl::build_composition_layout(
            &owner, incomplete_overrides, 0, 0, 0, unit_foundation).error !=
        CompositionLayoutError::PartialRotationOverrides) {
        error << "Composition definition contract did not reject incomplete overrides.\n";
        return false;
    }

    using building_type_registry_impl::CompositionMembership;
    using building_type_registry_impl::CompositionTypeReplacementAction;
    using building_type_registry_impl::CompositionTypeReplacementError;
    BuildingType standalone_target(static_cast<building_type>(4), "composition_contract_target");
    BuildingType composed_target(static_cast<building_type>(5), "composition_contract_composed_target");
    composed_target.composition().add_child("child", "child").set_canonical_offset(1, 0);

    const auto direct_replacement = building_type_registry_impl::plan_composition_type_replacement(
        &child, &standalone_target, CompositionMembership::Standalone);
    if (!direct_replacement.accepted() ||
        direct_replacement.action != CompositionTypeReplacementAction::ReplaceStandalone) {
        error << "Composition replacement planner rejected standalone/dynamic-chain-compatible replacement.\n";
        return false;
    }
    const auto no_change = building_type_registry_impl::plan_composition_type_replacement(
        &child, &child, CompositionMembership::Child);
    if (!no_change.accepted() || no_change.action != CompositionTypeReplacementAction::NoChange) {
        error << "Composition replacement planner rejected a member no-op.\n";
        return false;
    }
    if (building_type_registry_impl::plan_composition_type_replacement(
            &child, nullptr, CompositionMembership::Standalone).error !=
        CompositionTypeReplacementError::MissingTarget) {
        error << "Composition replacement planner accepted a missing target definition.\n";
        return false;
    }
    if (building_type_registry_impl::plan_composition_type_replacement(
            &child, &standalone_target, CompositionMembership::Child).error !=
        CompositionTypeReplacementError::MemberReplacementRequiresOwner) {
        error << "Composition replacement planner accepted arbitrary child replacement.\n";
        return false;
    }
    if (building_type_registry_impl::plan_composition_type_replacement(
            &composed_target, &standalone_target, CompositionMembership::Owner).error !=
        CompositionTypeReplacementError::WholeCompositionReplacementUnsupported) {
        error << "Composition replacement planner accepted unplanned owner replacement.\n";
        return false;
    }
    if (building_type_registry_impl::plan_composition_type_replacement(
            &child, &composed_target, CompositionMembership::Standalone).error !=
        CompositionTypeReplacementError::CompositionCreationUnsupported) {
        error << "Composition replacement planner accepted childless composition creation.\n";
        return false;
    }
    if (building_type_registry_impl::plan_composition_type_replacement(
            &composed_target, &standalone_target, CompositionMembership::Standalone).error !=
        CompositionTypeReplacementError::InconsistentMembership) {
        error << "Composition replacement planner accepted a detached composed owner.\n";
        return false;
    }

    using building_construction::PlacementAccountingCell;
    using building_construction::RepairRubbleOccupancy;
    const auto deduplicated_cost = building_construction::summarize_placement_cells({
        PlacementAccountingCell{ 10, true, true, true, RepairRubbleOccupancy::MatchingOrigin },
        PlacementAccountingCell{ 10, false, true, true, RepairRubbleOccupancy::MatchingOrigin }
    });
    if (!deduplicated_cost.duplicate_world_cells || deduplicated_cost.unique_cell_count != 1 ||
        deduplicated_cost.unique_clear_cell_count != 1 || deduplicated_cost.owner_charge_count != 1 ||
        deduplicated_cost.publication_valid() || deduplicated_cost.total_cost(100, 3) != 103) {
        error << "Composition placement accounting multiplied a duplicate cell or owner cost.\n";
        return false;
    }
    const auto valid_publication = building_construction::summarize_placement_cells({
        PlacementAccountingCell{ 10, true, true, false, RepairRubbleOccupancy::None },
        PlacementAccountingCell{ 11, false, true, false, RepairRubbleOccupancy::None }
    });
    if (!valid_publication.publication_valid() || valid_publication.owner_charge_count != 1 ||
        valid_publication.total_cost(100, 3) != 106) {
        error << "Composition placement transaction did not charge one owner and two unique clear cells.\n";
        return false;
    }
    std::vector<PlacementAccountingCell> large_clearance_cells;
    for (int cell = 0; cell < 200; ++cell) {
        large_clearance_cells.push_back(PlacementAccountingCell{
            cell, cell == 0, true, false, RepairRubbleOccupancy::None });
    }
    const auto large_clearance =
        building_construction::summarize_placement_cells(large_clearance_cells);
    if (!large_clearance.publication_valid() || large_clearance.unique_clear_cell_count != 200 ||
        large_clearance.total_cost(100, 3) != 700) {
        error << "Composition placement transaction truncated a large unique-clearance set.\n";
        return false;
    }
    const auto ownerless_publication = building_construction::summarize_placement_cells({
        PlacementAccountingCell{ 10, false, false, false, RepairRubbleOccupancy::None }
    });
    if (ownerless_publication.publication_valid() || ownerless_publication.owner_charge_count != 0) {
        error << "Composition placement transaction accepted a layout without an owner.\n";
        return false;
    }
    const auto partial_rubble = building_construction::summarize_placement_cells({
        PlacementAccountingCell{ 10, true, false, true, RepairRubbleOccupancy::MatchingOrigin },
        PlacementAccountingCell{ 11, false, false, true, RepairRubbleOccupancy::None }
    });
    if (!partial_rubble.publication_valid() || !partial_rubble.repair_coverage_valid() ||
        partial_rubble.matching_rubble_cell_count != 1 ||
        partial_rubble.required_rubble_cell_count != 2 ||
        partial_rubble.foreign_rubble_cell_count != 0) {
        error << "Composition repair accounting rejected a clear cell after its rubble was deleted.\n";
        return false;
    }
    const auto foreign_rubble = building_construction::summarize_placement_cells({
        PlacementAccountingCell{ 10, true, false, true, RepairRubbleOccupancy::MatchingOrigin },
        PlacementAccountingCell{ 11, false, false, true, RepairRubbleOccupancy::ForeignOrigin }
    });
    if (!foreign_rubble.publication_valid() || foreign_rubble.repair_coverage_valid() ||
        foreign_rubble.matching_rubble_cell_count != 1 ||
        foreign_rubble.foreign_rubble_cell_count != 1) {
        error << "Composition repair accounting accepted foreign-origin rubble.\n";
        return false;
    }

    using building_type_registry_impl::CompositionHydrationActionKind;
    using building_type_registry_impl::CompositionHydrationError;
    using building_type_registry_impl::CompositionLayoutMember;
    using building_type_registry_impl::CompositionLayoutResult;
    using building_type_registry_impl::LoadedCompositionCandidate;

    int owner_type_token = 0;
    int child_type_token = 0;
    int unrelated_type_token = 0;
    const BuildingType *owner_type = reinterpret_cast<const BuildingType *>(&owner_type_token);
    const BuildingType *child_type = reinterpret_cast<const BuildingType *>(&child_type_token);
    const BuildingType *unrelated_type = reinterpret_cast<const BuildingType *>(&unrelated_type_token);
    CompositionLayoutResult expected_layout;
    expected_layout.members = {
        CompositionLayoutMember{ owner_type, "owner", static_cast<std::size_t>(-1), 10, 10, 0, 0, true },
        CompositionLayoutMember{ child_type, "space_1", 0, 10, 11, 0, 0, false },
        CompositionLayoutMember{ child_type, "space_2", 1, 11, 10, 0, 0, false },
        CompositionLayoutMember{ child_type, "space_3", 2, 11, 11, 0, 0, false }
    };
    const std::vector<LoadedCompositionCandidate> missing_middle_chain = {
        LoadedCompositionCandidate{ 2, 1, 4, child_type, 10, 11, true },
        LoadedCompositionCandidate{ 4, 2, 9, child_type, 11, 11, true },
        LoadedCompositionCandidate{ 9, 4, 0, unrelated_type, 30, 30, true }
    };
    const building_type_registry_impl::CompositionHydrationPlan hydration =
        building_type_registry_impl::plan_composition_hydration(
            expected_layout, 1, missing_middle_chain);
    if (!hydration.valid() || hydration.actions.size() != 3 ||
        hydration.actions[0].kind != CompositionHydrationActionKind::AdoptExisting ||
        hydration.actions[0].existing_id != 2 ||
        hydration.actions[1].kind != CompositionHydrationActionKind::CreateMissing ||
        hydration.actions[2].kind != CompositionHydrationActionKind::AdoptExisting ||
        hydration.actions[2].existing_id != 4 ||
        hydration.unrelated_tail_ids.size() != 1 || hydration.unrelated_tail_ids[0] != 9) {
        error << "Composition hydration contract shifted a repeated child role or adopted an unrelated record.\n";
        return false;
    }

    const std::vector<LoadedCompositionCandidate> same_type_unrelated_chain = {
        LoadedCompositionCandidate{ 2, 1, 9, child_type, 10, 11, true },
        LoadedCompositionCandidate{ 9, 2, 0, child_type, 30, 30, true }
    };
    const building_type_registry_impl::CompositionHydrationPlan guarded_hydration =
        building_type_registry_impl::plan_composition_hydration(
            expected_layout, 1, same_type_unrelated_chain);
    if (!guarded_hydration.valid() || guarded_hydration.actions.size() != 3 ||
        guarded_hydration.actions[0].kind != CompositionHydrationActionKind::AdoptExisting ||
        guarded_hydration.actions[0].existing_id != 2 ||
        guarded_hydration.actions[1].kind != CompositionHydrationActionKind::CreateMissing ||
        guarded_hydration.actions[2].kind != CompositionHydrationActionKind::CreateMissing ||
        guarded_hydration.unrelated_tail_ids.size() != 1 ||
        guarded_hydration.unrelated_tail_ids[0] != 9) {
        error << "Composition hydration adopted an unrelated same-type record while reconstructing children.\n";
        return false;
    }

    const std::vector<LoadedCompositionCandidate> cyclic_chain = {
        LoadedCompositionCandidate{ 2, 1, 2, child_type, 10, 11, true },
        LoadedCompositionCandidate{ 2, 2, 0, child_type, 11, 10, true }
    };
    if (building_type_registry_impl::plan_composition_hydration(
            expected_layout, 1, cyclic_chain).error != CompositionHydrationError::DuplicateOrCyclicId) {
        error << "Composition hydration contract did not reject a duplicate/cyclic saved id.\n";
        return false;
    }

    const std::vector<LoadedCompositionCandidate> owner_duplicate_chain = {
        LoadedCompositionCandidate{ 1, 1, 0, child_type, 10, 11, true }
    };
    if (building_type_registry_impl::plan_composition_hydration(
            expected_layout, 1, owner_duplicate_chain).error !=
        CompositionHydrationError::DuplicateOrCyclicId) {
        error << "Composition hydration contract did not reject owner/child id duplication.\n";
        return false;
    }

    return true;
}

inline bool validate_parsed_composition_contracts(std::ostream &error)
{
    using building_type_registry_impl::BuildingType;
    using building_type_registry_impl::CompositionChildDef;
    using building_type_registry_impl::CompositionChildOrientation;
    using building_type_registry_impl::CompositionLayoutResult;
    using building_type_registry_impl::CompositionOffset;
    using building_type_registry_impl::definition_for_type;
    using building_type_registry_impl::type_from_attr;

    const auto aggregate_required_workers = [](const BuildingType &owner) {
        int workers = owner.required_workers();
        for (const CompositionChildDef &child : owner.composition().children()) {
            workers += child.type ? child.type->required_workers() : 0;
        }
        return workers;
    };

    struct FarmContract {
        const char *owner;
        const char *field;
    };
    const FarmContract farms[] = {
        { "fruit_farm", "fruit_farm_field" },
        { "olive_farm", "olive_farm_field" },
        { "pig_farm", "pig_farm_field" },
        { "vegetable_farm", "vegetable_farm_field" },
        { "vines_farm", "vines_farm_field" },
        { "wheat_farm", "wheat_farm_field" }
    };
    const std::array<CompositionOffset, 5> farm_offsets = {
        CompositionOffset{ 0, 2, true }, CompositionOffset{ 1, 2, true },
        CompositionOffset{ 2, 2, true }, CompositionOffset{ 2, 1, true },
        CompositionOffset{ 2, 0, true }
    };

    for (const FarmContract &contract : farms) {
        const BuildingType *definition = definition_for_type(type_from_attr(contract.owner));
        if (!definition || !definition->has_composition() ||
            definition->composition().children().size() != farm_offsets.size()) {
            error << "Parsed farm composition is missing or incomplete for " << contract.owner << ".\n";
            return false;
        }
        if (aggregate_required_workers(*definition) != 10) {
            error << "Parsed farm composition labor is not the owner plus five fields for "
                  << contract.owner << ".\n";
            return false;
        }
        const std::vector<CompositionChildDef> &children = definition->composition().children();
        for (std::size_t index = 0; index < children.size(); ++index) {
            const std::string expected_role = "field_" + std::to_string(index + 1);
            const CompositionOffset canonical = children[index].canonical_offset();
            if (children[index].role != expected_role || !children[index].type ||
                !children[index].type->attr_is(contract.field) ||
                !canonical.defined || canonical.x != farm_offsets[index].x ||
                canonical.y != farm_offsets[index].y ||
                children[index].orientation != CompositionChildOrientation::Canonical) {
                error << "Parsed farm field set/order is invalid for " << contract.owner
                      << " at " << expected_role << ".\n";
                return false;
            }
        }
        for (int rotation = 0; rotation < 4; ++rotation) {
            const CompositionLayoutResult layout = building_type_registry_impl::build_composition_layout(
                definition, definition->composition(), 0, 0, rotation);
            if (!layout.valid() || layout.members.size() != farm_offsets.size() + 1 ||
                !layout.members.front().is_owner ||
                layout.bounds.width() != 3 || layout.bounds.height() != 3) {
                error << "Parsed farm layout contract failed for " << contract.owner
                      << " at rotation " << rotation << ": " << layout.detail << ".\n";
                return false;
            }
            const auto accounting = composition_layout_accounting(layout);
            if (!accounting.publication_valid() || accounting.unique_cell_count != 9 ||
                accounting.unique_clear_cell_count != 9 || accounting.owner_charge_count != 1 ||
                accounting.total_cost(100, 3) != 127) {
                error << "Farm placement accounting is not nine unique cells/one owner charge for "
                      << contract.owner << " at rotation " << rotation << ".\n";
                return false;
            }
            for (std::size_t index = 0; index < farm_offsets.size(); ++index) {
                const CompositionOffset expected = building_type_registry_impl::rotate_composition_offset(
                    farm_offsets[index], rotation, 2, 2, 1, 1);
                const building_type_registry_impl::CompositionLayoutMember &member = layout.members[index + 1];
                if (member.is_owner || member.x != expected.x || member.y != expected.y ||
                    member.foundation_rotation != rotation || member.building_orientation != 0) {
                    error << "Parsed farm rotation moved " << contract.owner << " field_"
                          << index + 1 << " incorrectly at rotation " << rotation << ".\n";
                    return false;
                }
            }
        }
    }

    const char *forts[] = {
        "fort_archers", "fort_javelin", "fort_legionaries", "fort_mounted", "fort_swords"
    };
    const CompositionOffset fort_offsets[] = {
        { 3, -1, true }, { -1, -4, true }, { -4, 0, true }, { 0, 3, true }
    };
    for (const char *fort_name : forts) {
        const BuildingType *fort = definition_for_type(type_from_attr(fort_name));
        if (!fort || !fort->has_composition() || fort->composition().children().size() != 1) {
            error << "Parsed fort composition is missing for " << fort_name << ".\n";
            return false;
        }
        if (aggregate_required_workers(*fort) != fort->required_workers()) {
            error << "Fort composition child unexpectedly carries independent labor for "
                  << fort_name << ".\n";
            return false;
        }
        const CompositionChildDef &ground = fort->composition().children().front();
        if (ground.role != "mustering_ground" || !ground.type || !ground.type->attr_is("fort_ground") ||
            ground.orientation != CompositionChildOrientation::Canonical) {
            error << "Parsed fort mustering-ground contract is invalid for " << fort_name << ".\n";
            return false;
        }
        for (int rotation = 0; rotation < 4; ++rotation) {
            const CompositionLayoutResult layout = building_type_registry_impl::build_composition_layout(
                fort, fort->composition(), 0, 0, rotation);
            const int expected_width = rotation % 2 ? 4 : 7;
            const int expected_height = rotation % 2 ? 7 : 4;
            if (!layout.valid() || layout.members.size() != 2 ||
                !layout.members.front().is_owner || layout.members[1].is_owner ||
                layout.bounds.width() != expected_width || layout.bounds.height() != expected_height ||
                layout.members[1].x != fort_offsets[rotation].x ||
                layout.members[1].y != fort_offsets[rotation].y ||
                layout.members[1].foundation_rotation != rotation ||
                layout.members[1].building_orientation != 0) {
                error << "Parsed fort layout contract failed for " << fort_name
                      << " at rotation " << rotation << ": " << layout.detail << ".\n";
                return false;
            }
            const auto accounting = composition_layout_accounting(layout);
            if (!accounting.publication_valid() || accounting.unique_cell_count != 25 ||
                accounting.unique_clear_cell_count != 25 || accounting.owner_charge_count != 1 ||
                accounting.total_cost(100, 3) != 175) {
                error << "Fort placement accounting is not 25 unique cells/one owner charge for "
                      << fort_name << " at rotation " << rotation << ".\n";
                return false;
            }
        }
    }

    const BuildingType *warehouse = definition_for_type(type_from_attr("warehouse"));
    if (!warehouse || !warehouse->has_composition() ||
        warehouse->composition().children().size() != 8) {
        error << "Parsed warehouse composition is missing or incomplete.\n";
        return false;
    }
    if (aggregate_required_workers(*warehouse) != warehouse->required_workers()) {
        error << "Warehouse spaces unexpectedly carry independent labor.\n";
        return false;
    }
    const std::vector<CompositionChildDef> &warehouse_children =
        warehouse->composition().children();
    for (std::size_t index = 0; index < warehouse_children.size(); ++index) {
        const std::string expected_role = "space_" + std::to_string(index + 1);
        if (warehouse_children[index].role != expected_role ||
            !warehouse_children[index].type ||
            !warehouse_children[index].type->attr_is("warehouse_space") ||
            !warehouse_children[index].has_complete_rotation_overrides()) {
            error << "Warehouse composition role order or rotation overrides are invalid at "
                  << expected_role << ".\n";
            return false;
        }
    }

    const CompositionOffset warehouse_offsets[4][8] = {
        { { 0, 1, true }, { 1, 0, true }, { 1, 1, true }, { 0, 2, true },
            { 2, 0, true }, { 1, 2, true }, { 2, 1, true }, { 2, 2, true } },
        { { 0, -2, true }, { 0, -1, true }, { 1, -2, true }, { 1, -1, true },
            { 2, -2, true }, { 1, 0, true }, { 2, -1, true }, { 2, 0, true } },
        { { -2, -2, true }, { -2, -1, true }, { -1, -2, true }, { -1, -1, true },
            { -2, 0, true }, { 0, -2, true }, { -1, 0, true }, { 0, -1, true } },
        { { -2, 0, true }, { -2, 1, true }, { -1, 0, true }, { -1, 1, true },
            { -2, 2, true }, { -1, 2, true }, { 0, 1, true }, { 0, 2, true } }
    };
    for (int rotation = 0; rotation < 4; ++rotation) {
        const CompositionLayoutResult layout = building_type_registry_impl::build_composition_layout(
            warehouse, warehouse->composition(), 0, 0, rotation);
        if (!layout.valid() || layout.members.size() != 9 ||
            !layout.members.front().is_owner ||
            layout.bounds.width() != 3 || layout.bounds.height() != 3) {
            error << "Parsed warehouse layout failed at rotation " << rotation
                  << ": " << layout.detail << ".\n";
            return false;
        }
        const auto accounting = composition_layout_accounting(layout);
        if (!accounting.publication_valid() || accounting.unique_cell_count != 9 ||
            accounting.unique_clear_cell_count != 9 || accounting.owner_charge_count != 1 ||
            accounting.total_cost(100, 3) != 127) {
            error << "Warehouse placement accounting is not nine unique cells/one owner charge at rotation "
                  << rotation << ".\n";
            return false;
        }
        for (std::size_t index = 0; index < 8; ++index) {
            const auto &member = layout.members[index + 1];
            if (member.is_owner || member.x != warehouse_offsets[rotation][index].x ||
                member.y != warehouse_offsets[rotation][index].y ||
                member.foundation_rotation != rotation || member.building_orientation != 0) {
                error << "Warehouse role order shifted at rotation " << rotation
                      << " for space_" << index + 1 << ".\n";
                return false;
            }
        }
    }

    const BuildingType *hippodrome = definition_for_type(type_from_attr("hippodrome"));
    if (!hippodrome || !hippodrome->has_composition() ||
        !hippodrome->has_rotated_placement_geometry() ||
        hippodrome->composition().children().size() != 2) {
        error << "Parsed hippodrome composition is missing, incomplete, or not manually rotatable.\n";
        return false;
    }
    if (aggregate_required_workers(*hippodrome) != hippodrome->required_workers()) {
        error << "Hippodrome children unexpectedly carry independent labor.\n";
        return false;
    }
    const char *hippodrome_roles[] = { "middle", "end" };
    const char *hippodrome_types[] = { "hippodrome_middle", "hippodrome_end" };
    for (std::size_t index = 0; index < 2; ++index) {
        const CompositionChildDef &child = hippodrome->composition().children()[index];
        if (child.role != hippodrome_roles[index] || !child.type ||
            !child.type->attr_is(hippodrome_types[index]) ||
            child.orientation != CompositionChildOrientation::InheritOwner) {
            error << "Hippodrome composition role/orientation is invalid at index " << index << ".\n";
            return false;
        }
    }
    const CompositionOffset hippodrome_offsets[4][2] = {
        { { 5, 0, true }, { 10, 0, true } },
        { { 0, -5, true }, { 0, -10, true } },
        { { -5, 0, true }, { -10, 0, true } },
        { { 0, 5, true }, { 0, 10, true } }
    };
    for (int rotation = 0; rotation < 4; ++rotation) {
        const CompositionLayoutResult layout = building_type_registry_impl::build_composition_layout(
            hippodrome, hippodrome->composition(), 0, 0, rotation);
        const int expected_width = rotation % 2 ? 5 : 15;
        const int expected_height = rotation % 2 ? 15 : 5;
        if (!layout.valid() || layout.members.size() != 3 ||
            !layout.members.front().is_owner ||
            layout.members.front().foundation_rotation != rotation ||
            layout.members.front().building_orientation != rotation ||
            layout.bounds.width() != expected_width || layout.bounds.height() != expected_height) {
            error << "Parsed hippodrome layout failed at rotation " << rotation
                  << ": " << layout.detail << ".\n";
            return false;
        }
        const auto accounting = composition_layout_accounting(layout);
        if (!accounting.publication_valid() || accounting.unique_cell_count != 75 ||
            accounting.unique_clear_cell_count != 75 || accounting.owner_charge_count != 1 ||
            accounting.total_cost(100, 3) != 325) {
            error << "Hippodrome placement accounting is not 75 unique cells/one owner charge at rotation "
                  << rotation << ".\n";
            return false;
        }
        for (std::size_t index = 0; index < 2; ++index) {
            const auto &member = layout.members[index + 1];
            if (member.is_owner || member.x != hippodrome_offsets[rotation][index].x ||
                member.y != hippodrome_offsets[rotation][index].y ||
                member.foundation_rotation != rotation || member.building_orientation != rotation) {
                error << "Hippodrome child rotation shifted at rotation " << rotation
                      << " for role " << hippodrome_roles[index] << ".\n";
                return false;
            }
        }
    }

    const char *dynamic_bridges[] = { "low_bridge", "ship_bridge" };
    for (const char *bridge_name : dynamic_bridges) {
        const BuildingType *bridge = definition_for_type(type_from_attr(bridge_name));
        if (!bridge || !bridge->bridge().is_bridge() || bridge->has_composition()) {
            error << "Dynamic bridge must remain outside fixed BuildingComposition: "
                  << bridge_name << ".\n";
            return false;
        }
    }
    return true;
}
