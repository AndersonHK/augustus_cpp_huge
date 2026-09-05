#include "formation_type_registry_layering_test.h"

#include "figure/formation.h"
#include "figure/formation_member_movement_plan.h"
#include "figure/formation_type.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *LOWER_XML =
    "<formation key=\"archers\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
    "<slot fill=\"all\" unit=\"archer\"/></formation>";
constexpr const char *UPPER_XML =
    "<formation key=\"archers\" recruit_capacity=\"2\"><grid width=\"3\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
    "<combat melee_attack_modifier=\"2\" melee_defense_modifier=\"1\" morale_modifier=\"5\" "
    "missile_damage_modifier=\"-1\"/>"
    "<slot fill=\"all\" unit=\"archer\"/></formation>";
constexpr const char *DISABLED_XML =
    "<formation key=\"archers\" disabled=\"true\"></formation>";

constexpr const char *LAYOUT_TEST_GEOMETRY = "<geometry shape=\"square\"/>";

std::string army_orientation_xml(const std::string &orientation, int position_count = 7)
{
    std::string xml = "<army orientation=\"" + orientation + "\">";
    for (int index = 0; index < position_count; ++index) {
        xml += "<position x=\"" + std::to_string(index) + "\" y=\"0\"/>";
    }
    return xml + "</army>";
}

std::string layout_xml(const char *key, int legacy_id, const std::string &army)
{
    return "<layout key=\"" + std::string(key) + "\" legacy_id=\"" +
        std::to_string(legacy_id) + "\">" + LAYOUT_TEST_GEOMETRY + army + "</layout>";
}

std::string referenced_layout_xml(const char *key, int legacy_id, const char *army_from)
{
    return "<layout key=\"" + std::string(key) + "\" legacy_id=\"" +
        std::to_string(legacy_id) + "\" army_from=\"" + army_from + "\">" +
        LAYOUT_TEST_GEOMETRY + "</layout>";
}

formation_type_layer_test_input input(const char *xml, int layer, const char *mod, const char *path)
{
    return {xml, layer, mod, path};
}

bool valid(
    const formation_type_layer_test_input *inputs,
    int count,
    const char *query,
    formation_type_layer_test_result *result = nullptr)
{
    return formation_type_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

bool positions_are_unique(const std::vector<FormationLayoutPosition> &positions)
{
    for (size_t first = 0; first < positions.size(); ++first) {
        for (size_t second = first + 1; second < positions.size(); ++second) {
            if (positions[first].x == positions[second].x && positions[first].y == positions[second].y) {
                return false;
            }
        }
    }
    return true;
}

bool same_destination(const FigureMovementDestination &left, const FigureMovementDestination &right)
{
    return left.x == right.x && left.y == right.y && left.plane == right.plane;
}

bool validate_formation_member_assignment_contract(std::ostream &errors)
{
    FormationLayoutDef layout("assignment_test", -1, {}, {}, "");
    std::array<RelationshipEndpoint, 102> members;
    std::vector<FigureMovementDestination> ideals;
    std::vector<FormationMemberStationRequest> ascending;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            const int slot = y * 8 + x;
            ideals.push_back({32 + x * 64, 32 + y * 64, FigureMovementPlane::Ground});
            ascending.push_back({slot, &members[static_cast<size_t>(slot)]});
        }
    }
    std::vector<FormationMemberStationRequest> descending(ascending.rbegin(), ascending.rend());
    const auto reachable = [](int, const FigureMovementDestination &candidate) {
        const int tile_x = figure_movement_cross_country_to_tile(candidate.x);
        const int tile_y = figure_movement_cross_country_to_tile(candidate.y);
        if (tile_x < 0 || tile_y < 0 || tile_x >= 4 || tile_y >= 4) return 0;
        if (tile_x == 1 && tile_y == 1) return 0;
        return 1 + tile_x + tile_y;
    };

    FormationMemberMovementPlan forward;
    FormationMemberMovementPlan reverse;
    const FormationMovementBounds four_by_four_bounds = {0, 0, 511, 511};
    if (!forward.configure(FormationMemberDestination::Standard, layout, 0, 0, 4, 4, four_by_four_bounds, ideals) ||
        !reverse.configure(FormationMemberDestination::Standard, layout, 0, 0, 4, 4, four_by_four_bounds, ideals) ||
        !forward.assign_members(ascending, reachable) || !reverse.assign_members(descending, reachable)) {
        errors << "Formation station planner rejected a complete 8x8 roster.\n";
        return false;
    }

    std::vector<FigureMovementDestination> resolved;
    for (int slot = 0; slot < 64; slot++) {
        FigureMovementDestination forward_destination = {};
        FigureMovementDestination reverse_destination = {};
        FormationStationState forward_state = FormationStationState::Unplaced;
        FormationStationState reverse_state = FormationStationState::Unplaced;
        if (!forward.resolve_member(slot, members[static_cast<size_t>(slot)], &forward_destination, &forward_state) ||
            !reverse.resolve_member(slot, members[static_cast<size_t>(slot)], &reverse_destination, &reverse_state) ||
            forward_state == FormationStationState::Unplaced || forward_state != reverse_state ||
            !same_destination(forward_destination, reverse_destination)) {
            errors << "Formation station assignment depends on roster traversal order or left a reachable member unplaced.\n";
            return false;
        }
        const int ideal_tile_x = figure_movement_cross_country_to_tile(ideals[slot].x);
        const int ideal_tile_y = figure_movement_cross_country_to_tile(ideals[slot].y);
        const bool ideal_is_blocked = ideal_tile_x == 1 && ideal_tile_y == 1;
        if ((ideal_is_blocked && forward_state != FormationStationState::Fallback) ||
            (!ideal_is_blocked && forward_state != FormationStationState::Ideal) ||
            forward_destination.x < 0 || forward_destination.x > 480 ||
            forward_destination.y < 0 || forward_destination.y > 480) {
            errors << "Formation station planner did not keep internal-obstacle fallbacks inside the formation footprint.\n";
            return false;
        }
        for (int other_slot = 0; other_slot < 64; other_slot++) {
            if (other_slot != slot && same_destination(forward_destination, ideals[other_slot])) {
                errors << "Formation fallback stole a canonical station reserved for a future or current member.\n";
                return false;
            }
        }
        for (const FigureMovementDestination &other : resolved) {
            if (same_destination(forward_destination, other)) {
                errors << "Formation station planner stacked two members on one exact endpoint.\n";
                return false;
            }
        }
        resolved.push_back(forward_destination);
    }

    FormationMemberMovementPlan partial;
    const std::vector<FormationMemberStationRequest> incumbents(ascending.begin(), ascending.begin() + 4);
    if (!partial.configure(FormationMemberDestination::Standard, layout, 0, 0, 4, 4, four_by_four_bounds, ideals) ||
        !partial.assign_members(incumbents, reachable)) {
        errors << "Formation station planner rejected a partial roster.\n";
        return false;
    }
    std::vector<FigureMovementDestination> incumbent_destinations;
    for (int slot = 0; slot < 4; slot++) {
        FigureMovementDestination destination = {};
        FormationStationState state = FormationStationState::Unplaced;
        partial.resolve_member(slot, members[static_cast<size_t>(slot)], &destination, &state);
        incumbent_destinations.push_back(destination);
    }
    if (!partial.assign_member(4, members[4], reachable)) {
        errors << "Formation station planner rejected the next free recruit slot.\n";
        return false;
    }
    for (int slot = 0; slot < 4; slot++) {
        FigureMovementDestination destination = {};
        FormationStationState state = FormationStationState::Unplaced;
        if (!partial.resolve_member(slot, members[static_cast<size_t>(slot)], &destination, &state) ||
            !same_destination(destination, incumbent_destinations[slot])) {
            errors << "Adding a recruit changed an incumbent formation assignment.\n";
            return false;
        }
    }
    partial.release_member(1, members[1]);
    if (!partial.assign_member(1, members[100], reachable)) {
        errors << "Formation station planner could not reuse a casualty's released roster slot.\n";
        return false;
    }
    for (int slot : {0, 2, 3}) {
        FigureMovementDestination destination = {};
        FormationStationState state = FormationStationState::Unplaced;
        if (!partial.resolve_member(slot, members[static_cast<size_t>(slot)], &destination, &state) ||
            !same_destination(destination, incumbent_destinations[slot])) {
            errors << "Replacing a removed member changed an unrelated incumbent assignment.\n";
            return false;
        }
    }
    if (!partial.matches(FormationMemberDestination::Standard, layout, 0, 0, 4, 4, 64)) {
        errors << "Formation station plan lost its command/layout/anchor context.\n";
        return false;
    }
    partial.invalidate();
    if (partial.matches(FormationMemberDestination::Standard, layout, 0, 0, 4, 4, 64)) {
        errors << "Formation station plan ignored explicit command invalidation.\n";
        return false;
    }

    FormationMemberMovementPlan isolated;
    const std::vector<FigureMovementDestination> isolated_ideal = {{64, 64, FigureMovementPlane::Ground}};
    if (!isolated.configure(FormationMemberDestination::Standard, layout, 0, 0, 1, 1,
            {0, 0, 127, 127}, isolated_ideal) ||
        !isolated.assign_member(0, members[99], [](int, const FigureMovementDestination &) { return 0; })) {
        errors << "Formation station planner rejected an explicitly unreachable member.\n";
        return false;
    }
    FigureMovementDestination isolated_destination = {};
    FormationStationState isolated_state = FormationStationState::Ideal;
    if (!isolated.resolve_member(0, members[99], &isolated_destination, &isolated_state) ||
        isolated_state != FormationStationState::Unplaced) {
        errors << "Formation station planner did not preserve explicit Unplaced state for an unreachable member.\n";
        return false;
    }

    // Physical capacity may be smaller than roster capacity. Excess members
    // remain explicitly unplaced; querying them again must not restart searches.
    FormationMemberMovementPlan crowded;
    const std::vector<FigureMovementDestination> crowded_ideals = {
        {0, 0, FigureMovementPlane::Ground}, {1, 0, FigureMovementPlane::Ground}, {2, 0, FigureMovementPlane::Ground}
    };
    int crowded_queries = 0;
    const auto only_one_point = [&crowded_queries](int, const FigureMovementDestination &candidate) {
        crowded_queries++;
        return candidate.x == 0 && candidate.y == 0 ? 1 : 0;
    };
    if (!crowded.configure(FormationMemberDestination::Standard, layout, 0, 0, 1, 1, {0, 0, 2, 0}, crowded_ideals) ||
        !crowded.assign_members({{0, &members[0]}, {1, &members[1]}, {2, &members[2]}}, only_one_point)) {
        errors << "Formation planner rejected a roster exceeding reachable physical capacity.\n";
        return false;
    }
    const int initial_queries = crowded_queries;
    for (int slot = 0; slot < 3; slot++) {
        FigureMovementDestination point;
        FormationStationState state;
        if (!crowded.resolve_member(slot, members[slot], &point, &state) ||
            state != (slot == 0 ? FormationStationState::Ideal : FormationStationState::Unplaced) ||
            !crowded.assign_member(slot, members[slot], only_one_point) || crowded_queries != initial_queries) {
            errors << "Excess members lost their explicit stable Unplaced assignment.\n";
            return false;
        }
    }

    FormationMemberMovementPlan thick_obstacle;
    if (!thick_obstacle.configure(FormationMemberDestination::Standard, layout, 0, 0, 1, 1,
            {0, 0, 1023, 1023}, isolated_ideal) ||
        !thick_obstacle.assign_member(0, members[101], [](int, const FigureMovementDestination &candidate) {
            return figure_movement_cross_country_to_tile(candidate.x) >= 5 ? 1 : 0;
        })) {
        errors << "Formation station planner rejected a map-bounded thick-obstacle fallback.\n";
        return false;
    }
    FigureMovementDestination thick_obstacle_destination = {};
    FormationStationState thick_obstacle_state = FormationStationState::Unplaced;
    if (!thick_obstacle.resolve_member(
            0, members[101], &thick_obstacle_destination, &thick_obstacle_state) ||
        thick_obstacle_state != FormationStationState::Fallback ||
        figure_movement_cross_country_to_tile(thick_obstacle_destination.x) != 5) {
        errors << "Formation fallback stopped at an arbitrary local envelope instead of the nearest reachable shell.\n";
        return false;
    }

    FormationMemberMovementPlan repeated_legacy_station;
    const std::vector<FigureMovementDestination> repeated_ideals = {
        {64, 64, FigureMovementPlane::Ground},
        {64, 64, FigureMovementPlane::Ground}
    };
    const std::vector<FormationMemberStationRequest> repeated_members = {
        {0, &members[0]}, {1, &members[1]}
    };
    if (!repeated_legacy_station.configure(FormationMemberDestination::Standard, layout, 0, 0, 1, 1,
            {0, 0, 255, 255}, repeated_ideals) ||
        !repeated_legacy_station.assign_members(repeated_members,
            [](int, const FigureMovementDestination &) { return 1; })) {
        errors << "Formation station planner rejected a data-authored repeated legacy tile station.\n";
        return false;
    }
    FigureMovementDestination repeated_first = {};
    FigureMovementDestination repeated_second = {};
    FormationStationState repeated_first_state = FormationStationState::Unplaced;
    FormationStationState repeated_second_state = FormationStationState::Unplaced;
    if (!repeated_legacy_station.resolve_member(0, members[0], &repeated_first, &repeated_first_state) ||
        !repeated_legacy_station.resolve_member(1, members[1], &repeated_second, &repeated_second_state) ||
        repeated_first_state != FormationStationState::Ideal ||
        repeated_second_state != FormationStationState::Fallback ||
        same_destination(repeated_first, repeated_second)) {
        errors << "Formation station planner stacked members whose legacy layout declares one tile twice.\n";
        return false;
    }
    return true;
}

bool validate_mustering_ground_fit_contract(std::ostream &errors)
{
    const FigureMovementDestination ground_destination =
        figure_movement_destination_for_tile(2, 3, FigureMovementPlane::Ground);
    const FigureMovementDestination elevated_destination =
        figure_movement_destination_for_tile(2, 3, FigureMovementPlane::Elevated);
    if (ground_destination.plane != FigureMovementPlane::Ground ||
        elevated_destination.plane != FigureMovementPlane::Elevated ||
        ground_destination.x != 256 || ground_destination.y != 384) {
        errors << "Exact movement destinations did not preserve coordinates and an explicit render plane.\n";
        return false;
    }

    if (figure_movement_cross_country_to_tile(-1) != -1 ||
        figure_movement_cross_country_to_tile(-128) != -1 ||
        figure_movement_cross_country_to_tile(-129) != -2) {
        errors << "Exact movement coordinates did not project negative offsets to their containing tile.\n";
        return false;
    }
    const FormationLayoutDef square("fit", -1, {}, {}, "");
    for (int width : {1, 3, 4, 5, 6, 8, 16}) {
        FormationType type("fit");
        type.set_grid(width, width, FormationStationAlignment::ScaledTileAnchor);
        std::vector<FigureMovementDestination> destinations;
        for (int index = 0; index < width * width; index++) {
            FormationSlotOffset offset;
            if (!type.resolve_station_offset(square, index, type.capacity(), 4, 4, &offset) ||
                offset.x < 0 || offset.x > 384 || offset.y < 0 || offset.y > 384) {
                errors << "Formation geometry escaped the live mustering ground.\n";
                return false;
            }
            for (const auto &previous : destinations) {
                if (previous.x == offset.x && previous.y == offset.y) {
                    errors << "Fixed-point conversion collapsed distinct formation slots.\n";
                    return false;
                }
            }
            destinations.push_back({offset.x, offset.y, FigureMovementPlane::Ground});
        }
        if (width > 1 && (destinations.back().x != 384 || destinations.back().y != 384)) {
            errors << "Formation pitch failed to span the mustering-ground endpoints.\n";
            return false;
        }
    }
    FormationType century("negative_offsets");
    century.set_grid(8, 8, FormationStationAlignment::ScaledTileAnchor);
    FormationLayoutDef centered("centered", -1, {FormationLayoutShape::Square, true}, {}, "");
    FormationSlotOffset offset;
    if (!century.resolve_station_offset(centered, 8, 64, 4, 4, &offset) || offset.x != -55 || offset.y != -55) {
        errors << "Centered geometry lost symmetric negative fixed-point pitch.\n";
        return false;
    }

    return true;
}

} // namespace

bool validate_formation_type_registry_layering_contract(std::ostream &errors)
{
    if (!validate_mustering_ground_fit_contract(errors)) return false;
    if (!validate_formation_member_assignment_contract(errors)) return false;

    constexpr const char *LAYOUT_LOWER_XML =
        "<layout key=\"column\" legacy_id=\"0\"><geometry shape=\"square\"/></layout>";
    constexpr const char *LAYOUT_UPPER_XML =
        "<layout key=\"column\" legacy_id=\"0\"><geometry shape=\"ranks\"/></layout>";
    const formation_layout_layer_test_input layout_replacement[] = {
        {LAYOUT_LOWER_XML, 0, "Julius", "Julius/FormationLayout/column.xml"},
        {LAYOUT_UPPER_XML, 1, "Pharaoh", "Pharaoh/FormationLayout/column.xml"}
    };
    formation_layout_layer_test_result layout_result;
    if (!formation_layout_layered_definition_buffers_are_valid_for_test(
            layout_replacement, 2, "column", &layout_result) ||
        layout_result.active_count != 1 || layout_result.suppressed_count != 0 ||
        layout_result.queried_disabled || layout_result.queried_legacy_id != 0 ||
        layout_result.queried_shape != FormationLayoutShape::Ranks || layout_result.queried_source_layer != 1) {
        errors << "FormationLayout upper-layer replacement lost geometric policy or provenance.\n";
        return false;
    }

    constexpr const char *LAYOUT_DISABLED_XML = "<layout key=\"column\" disabled=\"true\"></layout>";
    const formation_layout_layer_test_input layout_suppression[] = {
        {LAYOUT_LOWER_XML, 0, "Julius", "Julius/FormationLayout/column.xml"},
        {LAYOUT_DISABLED_XML, 1, "Pharaoh", "Pharaoh/FormationLayout/column.xml"}
    };
    if (!formation_layout_layered_definition_buffers_are_valid_for_test(
            layout_suppression, 2, "column", &layout_result) ||
        layout_result.active_count != 0 || layout_result.suppressed_count != 1 ||
        !layout_result.queried_disabled || layout_result.queried_source_layer != 1) {
        errors << "FormationLayout disabled tombstone did not suppress its inherited key.\n";
        return false;
    }

    const formation_layout_layer_test_input layout_duplicate_key[] = {
        {LAYOUT_LOWER_XML, 0, "Julius", "Julius/FormationLayout/column.xml"},
        {LAYOUT_UPPER_XML, 0, "Julius", "Julius/FormationLayout/column-copy.xml"}
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            layout_duplicate_key, 2, "column", nullptr)) {
        errors << "FormationLayout accepted a same-layer duplicate key.\n";
        return false;
    }

    constexpr const char *LAYOUT_DUPLICATE_ID_XML =
        "<layout key=\"other\" legacy_id=\"0\"><geometry shape=\"square\"/></layout>";
    const formation_layout_layer_test_input duplicate_legacy_id[] = {
        {LAYOUT_LOWER_XML, 0, "Julius", "Julius/FormationLayout/column.xml"},
        {LAYOUT_DUPLICATE_ID_XML, 0, "Julius", "Julius/FormationLayout/other.xml"}
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            duplicate_legacy_id, 2, "column", nullptr)) {
        errors << "FormationLayout accepted two active definitions with the same legacy id.\n";
        return false;
    }

    constexpr const char *LAYOUT_SHORT_XML =
        "<layout key=\"short\" legacy_id=\"1\"><position x=\"0\" y=\"0\"/></layout>";
    const formation_layout_layer_test_input short_layout[] = {
        {LAYOUT_SHORT_XML, 0, "Julius", "Julius/FormationLayout/short.xml"}
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            short_layout, 1, "short", nullptr)) {
        errors << "FormationLayout accepted removed member-position syntax.\n";
        return false;
    }
    constexpr const char *LAYOUT_INVALID_ID_XML =
        "<layout key=\"invalid\" legacy_id=\"13\"></layout>";
    const formation_layout_layer_test_input invalid_layout_id[] = {
        {LAYOUT_INVALID_ID_XML, 0, "Julius", "Julius/FormationLayout/invalid.xml"}
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            invalid_layout_id, 1, "invalid", nullptr)) {
        errors << "FormationLayout accepted a legacy id outside the save range.\n";
        return false;
    }
    constexpr const char *LAYOUT_INVALID_TOMBSTONE_XML =
        "<layout key=\"column\" disabled=\"true\"><position x=\"0\" y=\"0\"/></layout>";
    const formation_layout_layer_test_input invalid_layout_tombstone[] = {
        {LAYOUT_INVALID_TOMBSTONE_XML, 0, "Julius", "Julius/FormationLayout/column.xml"}
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            invalid_layout_tombstone, 1, "column", nullptr)) {
        errors << "FormationLayout disabled tombstone accepted authored positions.\n";
        return false;
    }

    const std::string complete_army_offsets =
        army_orientation_xml("0") + army_orientation_xml("1") +
        army_orientation_xml("2") + army_orientation_xml("3");
    const std::string army_source_xml = layout_xml("army_source", 0, complete_army_offsets);
    const std::string army_reuse_xml = referenced_layout_xml("army_reuse", 1, "army_source");
    const formation_layout_layer_test_input valid_army_reuse[] = {
        {army_source_xml.c_str(), 0, "Julius", "Julius/FormationLayout/army_source.xml"},
        {army_reuse_xml.c_str(), 0, "Julius", "Julius/FormationLayout/army_reuse.xml"},
    };
    if (!formation_layout_layered_definition_buffers_are_valid_for_test(
            valid_army_reuse, 2, "army_reuse", &layout_result)) {
        errors << "FormationLayout rejected a direct army_from reference to authored offsets.\n";
        return false;
    }

    const std::string malformed_orientation_xml =
        layout_xml("malformed_army", 0,
            army_orientation_xml("north") + army_orientation_xml("1") +
            army_orientation_xml("2") + army_orientation_xml("3"));
    const formation_layout_layer_test_input malformed_orientation[] = {
        {malformed_orientation_xml.c_str(), 0, "Julius", "Julius/FormationLayout/malformed_army.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            malformed_orientation, 1, "malformed_army", nullptr)) {
        errors << "FormationLayout accepted a non-numeric army orientation.\n";
        return false;
    }

    const std::string incomplete_orientations_xml =
        layout_xml("incomplete_army", 0,
            army_orientation_xml("0") + army_orientation_xml("1") + army_orientation_xml("2"));
    const formation_layout_layer_test_input incomplete_orientations[] = {
        {incomplete_orientations_xml.c_str(), 0, "Julius", "Julius/FormationLayout/incomplete_army.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            incomplete_orientations, 1, "incomplete_army", nullptr)) {
        errors << "FormationLayout accepted fewer than four army orientations.\n";
        return false;
    }

    const std::string incomplete_army_positions_xml =
        layout_xml("short_army", 0,
            army_orientation_xml("0", 6) + army_orientation_xml("1") +
            army_orientation_xml("2") + army_orientation_xml("3"));
    const formation_layout_layer_test_input incomplete_army_positions[] = {
        {incomplete_army_positions_xml.c_str(), 0, "Julius", "Julius/FormationLayout/short_army.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            incomplete_army_positions, 1, "short_army", nullptr)) {
        errors << "FormationLayout accepted an army orientation without seven positions.\n";
        return false;
    }

    const std::string duplicate_orientation_xml =
        layout_xml("duplicate_army", 0,
            army_orientation_xml("0") + army_orientation_xml("0") +
            army_orientation_xml("1") + army_orientation_xml("2") + army_orientation_xml("3"));
    const formation_layout_layer_test_input duplicate_orientation[] = {
        {duplicate_orientation_xml.c_str(), 0, "Julius", "Julius/FormationLayout/duplicate_army.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            duplicate_orientation, 1, "duplicate_army", nullptr)) {
        errors << "FormationLayout accepted a duplicate army orientation.\n";
        return false;
    }

    const std::string missing_reference_xml = referenced_layout_xml("missing_reference", 0, "absent");
    const formation_layout_layer_test_input missing_reference[] = {
        {missing_reference_xml.c_str(), 0, "Julius", "Julius/FormationLayout/missing_reference.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            missing_reference, 1, "missing_reference", nullptr)) {
        errors << "FormationLayout accepted an unresolved army_from reference.\n";
        return false;
    }

    const std::string self_reference_xml = referenced_layout_xml("self_reference", 0, "self_reference");
    const formation_layout_layer_test_input self_reference[] = {
        {self_reference_xml.c_str(), 0, "Julius", "Julius/FormationLayout/self_reference.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            self_reference, 1, "self_reference", nullptr)) {
        errors << "FormationLayout accepted a self-referential army_from.\n";
        return false;
    }

    const std::string indirect_middle_xml = referenced_layout_xml("army_middle", 1, "army_source");
    const std::string indirect_leaf_xml = referenced_layout_xml("army_leaf", 2, "army_middle");
    const formation_layout_layer_test_input indirect_reference[] = {
        {army_source_xml.c_str(), 0, "Julius", "Julius/FormationLayout/army_source.xml"},
        {indirect_middle_xml.c_str(), 0, "Julius", "Julius/FormationLayout/army_middle.xml"},
        {indirect_leaf_xml.c_str(), 0, "Julius", "Julius/FormationLayout/army_leaf.xml"},
    };
    if (formation_layout_layered_definition_buffers_are_valid_for_test(
            indirect_reference, 3, "army_leaf", nullptr)) {
        errors << "FormationLayout accepted an indirect army_from reference chain.\n";
        return false;
    }

    const FormationFortLinkState valid_new_link = {7, 12, 1, 1, 1, 1, 0, 0};
    const FormationFortLinkState valid_loaded_link = {7, 12, 1, 1, 1, 1, 7, 12};
    if (validate_formation_fort_link(valid_new_link) != FormationFortLinkFailure::None ||
        validate_formation_fort_link(valid_loaded_link) != FormationFortLinkFailure::None) {
        errors << "Formation-to-fort ownership validation rejected valid creation or save hydration.\n";
        return false;
    }

    FormationFortLinkState invalid_link = valid_loaded_link;
    invalid_link.has_resolved_formation_type = 0;
    if (validate_formation_fort_link(invalid_link) != FormationFortLinkFailure::MissingFormationType) {
        errors << "Formation-to-fort ownership accepted a fort without a resolved FormationType.\n";
        return false;
    }
    invalid_link = valid_loaded_link;
    invalid_link.saved_fort_formation_id = 8;
    if (validate_formation_fort_link(invalid_link) != FormationFortLinkFailure::ConflictingFormationId) {
        errors << "Formation-to-fort ownership accepted conflicting saved formation ids.\n";
        return false;
    }
    invalid_link = valid_loaded_link;
    invalid_link.saved_formation_fort_id = 13;
    if (validate_formation_fort_link(invalid_link) != FormationFortLinkFailure::ConflictingFortId) {
        errors << "Formation-to-fort ownership accepted conflicting saved fort ids.\n";
        return false;
    }
    invalid_link = valid_loaded_link;
    invalid_link.fort_is_live = 0;
    if (validate_formation_fort_link(invalid_link) != FormationFortLinkFailure::InactiveFort) {
        errors << "Formation-to-fort ownership accepted an inactive fort.\n";
        return false;
    }
    invalid_link = valid_loaded_link;
    invalid_link.formation_in_use = 0;
    if (validate_formation_fort_link(invalid_link) != FormationFortLinkFailure::InactiveFormation) {
        errors << "Formation-to-fort ownership accepted an inactive formation.\n";
        return false;
    }
    invalid_link = valid_loaded_link;
    invalid_link.formation_is_legion = 0;
    if (validate_formation_fort_link(invalid_link) != FormationFortLinkFailure::NotLegion) {
        errors << "Formation-to-fort ownership accepted a non-legion formation.\n";
        return false;
    }

    const formation_type_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FormationType/archers.xml"),
        input(UPPER_XML, 1, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    formation_type_layer_test_result result;
    if (!valid(replacement, 2, "archers", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_capacity != 3 || result.queried_recruit_capacity != 2 ||
        result.queried_melee_attack_modifier != 2 || result.queried_melee_defense_modifier != 1 ||
        result.queried_morale_modifier != 5 || result.queried_missile_damage_modifier != -1 ||
        result.queried_source_layer != 1) {
        errors << "FormationType upper-layer replacement lost winner combat data or provenance: active="
               << result.active_count << " suppressed=" << result.suppressed_count
               << " disabled=" << result.queried_disabled << " capacity=" << result.queried_capacity
               << " recruit=" << result.queried_recruit_capacity << " attack=" << result.queried_melee_attack_modifier
               << " defense=" << result.queried_melee_defense_modifier << " morale=" << result.queried_morale_modifier
               << " missile=" << result.queried_missile_damage_modifier << " source=" << result.queried_source_layer << ".\n";
        return false;
    }

    const formation_type_layer_test_input inherited_only[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FormationType/archers.xml")
    };
    if (!valid(inherited_only, 1, "archers", &result) ||
        result.queried_capacity != 2 || result.queried_recruit_capacity != 2 ||
        result.queried_melee_attack_modifier != 0 || result.queried_melee_defense_modifier != 0 ||
        result.queried_morale_modifier != 0 || result.queried_missile_damage_modifier != 0 ||
        result.queried_source_layer != 0) {
        errors << "FormationType lower definition did not preserve zero combat defaults.\n";
        return false;
    }

    const formation_type_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FormationType/archers.xml"),
        input(DISABLED_XML, 1, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (!valid(suppression, 2, "archers", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1) {
        errors << "FormationType disabled tombstone did not suppress its inherited key.\n";
        return false;
    }

    const formation_type_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FormationType/archers.xml"),
        input(UPPER_XML, 0, "Julius", "Julius/FormationType/archers-copy.xml")
    };
    if (valid(duplicate, 2, "archers")) {
        errors << "FormationType same-layer duplicate key was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_DISABLED_CONTENT =
        "<formation key=\"archers\" disabled=\"true\"><grid width=\"1\" height=\"1\"/></formation>";
    const formation_type_layer_test_input invalid_tombstone[] = {
        input(INVALID_DISABLED_CONTENT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(invalid_tombstone, 1, "archers")) {
        errors << "FormationType disabled tombstone accepted authored content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_UNIT =
        "<formation key=\"archers\"><grid width=\"1\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<slot fill=\"all\" unit=\"missing_unit\"/></formation>";
    const formation_type_layer_test_input deferred_reference[] = {
        input(INVALID_LOWER_UNIT, 0, "Julius", "Julius/FormationType/archers.xml"),
        input(UPPER_XML, 1, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (!valid(deferred_reference, 2, "archers", &result) || result.queried_capacity != 3) {
        errors << "FormationType UnitType references were resolved before final winners were collected.\n";
        return false;
    }

    const formation_type_layer_test_input unresolved_winner[] = {
        input(INVALID_LOWER_UNIT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(unresolved_winner, 1, "archers")) {
        errors << "FormationType final winner accepted an unresolved UnitType reference.\n";
        return false;
    }

    constexpr const char *OVERSIZED_RECRUITMENT =
        "<formation key=\"archers\" recruit_capacity=\"3\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input oversized_recruitment[] = {
        input(OVERSIZED_RECRUITMENT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(oversized_recruitment, 1, "archers")) {
        errors << "FormationType accepted recruit_capacity larger than its authored formation.\n";
        return false;
    }

    constexpr const char *ZERO_RECRUITMENT =
        "<formation key=\"archers\" recruit_capacity=\"0\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input zero_recruitment[] = {
        input(ZERO_RECRUITMENT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(zero_recruitment, 1, "archers")) {
        errors << "FormationType accepted a non-positive recruit_capacity.\n";
        return false;
    }

    constexpr const char *MISSING_STATION_ALIGNMENT =
        "<formation key=\"archers\"><grid width=\"2\" height=\"1\"/>"
        "<slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input missing_station_alignment[] = {
        input(MISSING_STATION_ALIGNMENT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(missing_station_alignment, 1, "archers")) {
        errors << "FormationType accepted a grid without explicit station_alignment semantics.\n";
        return false;
    }

    constexpr const char *MALFORMED_COMBAT =
        "<formation key=\"archers\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<combat melee_attack_modifier=\"1.5\"/><slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input malformed_combat[] = {
        input(MALFORMED_COMBAT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(malformed_combat, 1, "archers")) {
        errors << "FormationType accepted a non-integer combat modifier.\n";
        return false;
    }

    constexpr const char *OUT_OF_RANGE_COMBAT =
        "<formation key=\"archers\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<combat morale_modifier=\"101\"/><slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input out_of_range_combat[] = {
        input(OUT_OF_RANGE_COMBAT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(out_of_range_combat, 1, "archers")) {
        errors << "FormationType accepted an out-of-range combat modifier.\n";
        return false;
    }

    constexpr const char *EMPTY_COMBAT =
        "<formation key=\"archers\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<combat/><slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input empty_combat[] = {
        input(EMPTY_COMBAT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(empty_combat, 1, "archers")) {
        errors << "FormationType accepted an empty combat node.\n";
        return false;
    }

    constexpr const char *DUPLICATE_COMBAT =
        "<formation key=\"archers\"><grid width=\"2\" height=\"1\" station_alignment=\"scaled_tile_anchor\"/>"
        "<combat melee_attack_modifier=\"1\"/><combat morale_modifier=\"1\"/>"
        "<slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input duplicate_combat[] = {
        input(DUPLICATE_COMBAT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(duplicate_combat, 1, "archers")) {
        errors << "FormationType accepted duplicate combat nodes.\n";
        return false;
    }

    constexpr const char *HERD_SPAWN =
        "<formation key=\"wolf_herd\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"northern\" count=\"8\"/>"
        "<herd roam_distance=\"16\" roam_delay=\"6\" reproduction_delay=\"32\" aggressive=\"true\" "
        "allow_negative_desirability=\"true\" movement_sound=\"wolf_howl\"/>"
        "<herd_member rest_delay=\"400\" move_speed=\"2\" animation_frames=\"12\" alternate_rest_animation=\"false\" combat_animation=\"attack\"/>"
        "<slot fill=\"all\" unit=\"wolf\"/></formation>";
    const formation_type_layer_test_input herd_spawn[] = {
        input(HERD_SPAWN, 0, "Julius", "Julius/FormationType/wolf_herd.xml")
    };
    if (!valid(herd_spawn, 1, "wolf_herd", &result) || result.queried_capacity != 16 ||
        result.queried_spawn_role != static_cast<int>(FormationSpawnRole::Herd) ||
        result.queried_spawn_climate != 1 || result.queried_spawn_count != 8 ||
        result.queried_herd_roam_distance != 16 || result.queried_herd_roam_delay != 6 ||
        result.queried_herd_reproduction_delay != 32 || result.queried_herd_member_rest_delay != 400 ||
        result.queried_herd_member_move_speed != 2 || result.queried_herd_member_animation_frames != 12 ||
        !result.queried_herd_aggressive || !result.queried_herd_allow_negative_desirability ||
        result.queried_herd_alternate_rest_animation ||
        result.queried_herd_movement_sound != static_cast<int>(FormationHerdMovementSound::WolfHowl) ||
        result.queried_herd_combat_animation != static_cast<int>(FormationHerdCombatAnimation::Attack)) {
        errors << "FormationType rejected a fully data-defined herd spawn.\n";
        return false;
    }

    constexpr const char *SECOND_CENTRAL_HERD =
        "<formation key=\"sheep_herd\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"central\" count=\"10\"/>"
        "<herd roam_distance=\"8\" roam_delay=\"20\" reproduction_delay=\"0\" aggressive=\"false\" "
        "allow_negative_desirability=\"false\" movement_sound=\"none\"/>"
        "<herd_member rest_delay=\"400\" move_speed=\"1\" animation_frames=\"6\" alternate_rest_animation=\"true\" combat_animation=\"move\"/>"
        "<slot fill=\"all\" unit=\"sheep\"/></formation>";
    constexpr const char *FIRST_CENTRAL_HERD =
        "<formation key=\"wolf_herd\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"central\" count=\"8\"/>"
        "<herd roam_distance=\"16\" roam_delay=\"6\" reproduction_delay=\"32\" aggressive=\"true\" "
        "allow_negative_desirability=\"true\" movement_sound=\"wolf_howl\"/>"
        "<herd_member rest_delay=\"400\" move_speed=\"2\" animation_frames=\"12\" alternate_rest_animation=\"false\" combat_animation=\"attack\"/>"
        "<slot fill=\"all\" unit=\"wolf\"/></formation>";
    const formation_type_layer_test_input multiple_herds_per_climate[] = {
        input(FIRST_CENTRAL_HERD, 0, "Julius", "Julius/FormationType/wolf_herd.xml"),
        input(SECOND_CENTRAL_HERD, 0, "Julius", "Julius/FormationType/sheep_herd.xml")
    };
    if (!valid(multiple_herds_per_climate, 2, "sheep_herd", &result) ||
        result.active_count != 2 || result.queried_capacity != 16) {
        errors << "FormationType rejected multiple independently authored herds for one climate.\n";
        return false;
    }

    constexpr const char *ENEMY_SPAWN =
        "<formation key=\"enemy_formation\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"enemy\"/><slot fill=\"all\" unit=\"enemy43_spear\"/></formation>";
    const formation_type_layer_test_input enemy_spawn[] = {
        input(ENEMY_SPAWN, 0, "Julius", "Julius/FormationType/enemy_formation.xml")
    };
    if (!valid(enemy_spawn, 1, "enemy_formation", &result) || result.queried_capacity != 16) {
        errors << "FormationType rejected explicit enemy spawn behavior.\n";
        return false;
    }

    constexpr const char *DUPLICATE_HERD_ROLE =
        "<formation key=\"duplicate_herd\"><grid width=\"1\" height=\"1\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"northern\" count=\"1\"/>"
        "<herd roam_distance=\"1\" roam_delay=\"1\" reproduction_delay=\"0\" aggressive=\"false\" "
        "allow_negative_desirability=\"false\" movement_sound=\"none\"/>"
        "<herd_member rest_delay=\"1\" move_speed=\"1\" animation_frames=\"1\" alternate_rest_animation=\"false\" combat_animation=\"move\"/>"
        "<slot fill=\"all\" unit=\"enemy43_spear\"/></formation>";
    const formation_type_layer_test_input duplicate_spawn_roles[] = {
        input(ENEMY_SPAWN, 0, "Julius", "Julius/FormationType/enemy_formation.xml"),
        input(DUPLICATE_HERD_ROLE, 0, "Julius", "Julius/FormationType/duplicate_herd.xml")
    };
    if (valid(duplicate_spawn_roles, 2, "duplicate_herd")) {
        errors << "FormationType accepted one figure type in two spawn roles.\n";
        return false;
    }

    constexpr const char *INVALID_HERD_SPAWN =
        "<formation key=\"bad_herd\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"wet\" count=\"8\"/>"
        "<slot fill=\"all\" unit=\"wolf\"/></formation>";
    const formation_type_layer_test_input invalid_herd_spawn[] = {
        input(INVALID_HERD_SPAWN, 0, "Julius", "Julius/FormationType/bad_herd.xml")
    };
    if (valid(invalid_herd_spawn, 1, "bad_herd")) {
        errors << "FormationType accepted an unknown herd climate.\n";
        return false;
    }

    constexpr const char *MISSING_HERD_COUNT =
        "<formation key=\"bad_herd\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"central\"/><slot fill=\"all\" unit=\"wolf\"/></formation>";
    const formation_type_layer_test_input missing_herd_count[] = {
        input(MISSING_HERD_COUNT, 0, "Julius", "Julius/FormationType/bad_herd.xml")
    };
    if (valid(missing_herd_count, 1, "bad_herd")) {
        errors << "FormationType accepted herd behavior without an explicit positive count.\n";
        return false;
    }

    constexpr const char *OVERSIZED_HERD_COUNT =
        "<formation key=\"bad_herd\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"central\" count=\"17\"/>"
        "<herd roam_distance=\"16\" roam_delay=\"6\" reproduction_delay=\"32\" aggressive=\"true\" "
        "allow_negative_desirability=\"true\" movement_sound=\"wolf_howl\"/>"
        "<herd_member rest_delay=\"400\" move_speed=\"2\" animation_frames=\"12\" alternate_rest_animation=\"false\" combat_animation=\"attack\"/>"
        "<slot fill=\"all\" unit=\"wolf\"/></formation>";
    const formation_type_layer_test_input oversized_herd_count[] = {
        input(OVERSIZED_HERD_COUNT, 0, "Julius", "Julius/FormationType/bad_herd.xml")
    };
    if (valid(oversized_herd_count, 1, "bad_herd")) {
        errors << "FormationType accepted a herd count larger than its declared grid.\n";
        return false;
    }

    constexpr const char *INVALID_ENEMY_METADATA =
        "<formation key=\"bad_enemy\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"enemy\" count=\"8\"/><slot fill=\"all\" unit=\"enemy43_spear\"/></formation>";
    const formation_type_layer_test_input invalid_enemy_metadata[] = {
        input(INVALID_ENEMY_METADATA, 0, "Julius", "Julius/FormationType/bad_enemy.xml")
    };
    if (valid(invalid_enemy_metadata, 1, "bad_enemy")) {
        errors << "FormationType accepted herd-only metadata on enemy behavior.\n";
        return false;
    }

    constexpr const char *DUPLICATE_SPAWN =
        "<formation key=\"bad_spawn\"><grid width=\"4\" height=\"4\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"enemy\"/><spawn role=\"enemy\"/>"
        "<slot fill=\"all\" unit=\"enemy43_spear\"/></formation>";
    const formation_type_layer_test_input duplicate_spawn[] = {
        input(DUPLICATE_SPAWN, 0, "Julius", "Julius/FormationType/bad_spawn.xml")
    };
    if (valid(duplicate_spawn, 1, "bad_spawn")) {
        errors << "FormationType accepted duplicate behavior declarations.\n";
        return false;
    }

    constexpr const char *PARTIAL_SPAWN_ROSTER =
        "<formation key=\"partial_spawn\"><grid width=\"2\" height=\"2\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"herd\" climate=\"central\" count=\"1\"/>"
        "<herd roam_distance=\"16\" roam_delay=\"6\" reproduction_delay=\"32\" aggressive=\"true\" "
        "allow_negative_desirability=\"true\" movement_sound=\"wolf_howl\"/>"
        "<herd_member rest_delay=\"400\" move_speed=\"2\" animation_frames=\"12\" alternate_rest_animation=\"false\" combat_animation=\"attack\"/>"
        "<slot x=\"0\" y=\"0\" unit=\"wolf\"/></formation>";
    const formation_type_layer_test_input partial_spawn_roster[] = {
        input(PARTIAL_SPAWN_ROSTER, 0, "Julius", "Julius/FormationType/partial_spawn.xml")
    };
    if (valid(partial_spawn_roster, 1, "partial_spawn")) {
        errors << "FormationType accepted a partial direct-spawn roster.\n";
        return false;
    }

    constexpr const char *MIXED_SPAWN_ROSTER =
        "<formation key=\"mixed_spawn\"><grid width=\"2\" height=\"2\" station_alignment=\"tile_anchor\"/>"
        "<spawn role=\"enemy\"/>"
        "<slot row=\"0\" unit=\"enemy43_spear\"/>"
        "<slot row=\"1\" unit=\"enemy44_sword\"/></formation>";
    const formation_type_layer_test_input mixed_spawn_roster[] = {
        input(MIXED_SPAWN_ROSTER, 0, "Julius", "Julius/FormationType/mixed_spawn.xml")
    };
    if (valid(mixed_spawn_roster, 1, "mixed_spawn")) {
        errors << "FormationType accepted a mixed direct-spawn roster.\n";
        return false;
    }

    constexpr const char *MIXED_MANAGED_ROSTER =
        "<formation key=\"mixed_managed\"><grid width=\"2\" height=\"2\" station_alignment=\"scaled_tile_anchor\"/>"
        "<slot row=\"0\" unit=\"archer\"/>"
        "<slot row=\"1\" unit=\"legionary\"/></formation>";
    const formation_type_layer_test_input mixed_managed_roster[] = {
        input(MIXED_MANAGED_ROSTER, 0, "Vespasian", "Vespasian/FormationType/mixed_managed.xml")
    };
    if (!valid(mixed_managed_roster, 1, "mixed_managed", &result) || result.queried_capacity != 4) {
        errors << "FormationType rejected a complete mixed managed roster without direct-spawn behavior.\n";
        return false;
    }

    constexpr const char *VESPASIAN_CENTURY =
        "<formation key=\"vespasian_legionary_century\" recruit_capacity=\"64\"><grid width=\"8\" height=\"8\" station_alignment=\"scaled_tile_anchor\"/>"
        "<slot fill=\"all\" unit=\"legionary\"/></formation>";
    const formation_type_layer_test_input vespasian_century[] = {
        input(VESPASIAN_CENTURY, 0, "Vespasian", "Vespasian/FormationType/vespasian_legionary_century.xml")
    };
    if (!valid(vespasian_century, 1, "vespasian_legionary_century", &result) ||
        result.queried_capacity != 64 || result.queried_recruit_capacity != 64) {
        errors << "FormationType did not preserve an 8x8 Vespasian century and its recruit capacity.\n";
        return false;
    }

    constexpr const char *VESPASIAN_MOUNTED =
        "<formation key=\"vespasian_cavalry_turma\" recruit_capacity=\"36\"><grid width=\"6\" height=\"6\" station_alignment=\"scaled_tile_anchor\"/>"
        "<slot fill=\"all\" unit=\"mounted\"/></formation>";
    const formation_type_layer_test_input vespasian_mounted[] = {
        input(VESPASIAN_MOUNTED, 0, "Vespasian", "Vespasian/FormationType/vespasian_cavalry_turma.xml")
    };
    if (!valid(vespasian_mounted, 1, "vespasian_cavalry_turma", &result) ||
        result.queried_capacity != 36 || result.queried_recruit_capacity != 36) {
        errors << "FormationType did not preserve a 6x6 Vespasian mounted formation and its recruit capacity.\n";
        return false;
    }

    for (int legacy_id = 0; legacy_id < formation_layout_legacy::COUNT; ++legacy_id) {
        const FormationLayoutDef *layout =
            formation_layout_registry_impl::find_layout_by_legacy_id(legacy_id);
        if (!layout || layout->legacy_id() != legacy_id) {
            errors << "FormationLayout registry did not resolve a stable legacy save id.\n";
            return false;
        }
    }
    if (formation_layout_registry_impl::find_layout_by_legacy_id(-1) ||
        formation_layout_registry_impl::find_layout_by_legacy_id(formation_layout_legacy::COUNT)) {
        errors << "FormationLayout registry accepted an invalid legacy save id.\n";
        return false;
    }
    const FormationLayoutDef *column_layout = formation_layout_registry_impl::find_layout("column");
    if (!column_layout) return false;
    // Every size, including either side of the old save-prefix size, follows
    // exactly the same policy. Perfect-square prefixes must be square already.
    for (int width : {1, 3, 4, 5, 6, 8, 16}) {
        for (const auto &layout : formation_layout_registry_impl::layouts()) {
            std::vector<FormationLayoutPosition> positions(width * width);
            for (int slot = 0; slot < width * width; slot++) {
                if (!layout->try_position(slot, width * width, width, width, &positions[slot])) return false;
            }
            if (!positions_are_unique(positions)) {
                errors << "Mathematical formation geometry must provide unique slots at every capacity.\n";
                return false;
            }
        }
        const FormationLayoutDef *rest = formation_layout_registry_impl::find_layout("at_rest");
        int max_x = 0;
        int max_y = 0;
        for (int population = 1; population <= width * width; population++) {
            FormationLayoutPosition position;
            if (!rest->try_position(population - 1, width * width, width, width, &position)) return false;
            max_x = std::max(max_x, position.x);
            max_y = std::max(max_y, position.y);
            if (std::abs(max_x - max_y) > 1 || (population == 1 && (position.x || position.y))) {
                errors << "A partial square filled a row instead of growing from its anchor.\n";
                return false;
            }
        }
    }
    for (const char *key : {"double_line_1", "double_line_2", "single_line_1", "single_line_2"}) {
        const FormationLayoutDef *line = formation_layout_registry_impl::find_layout(key);
        if (!line) return false;
        const bool staggered = line->geometry().shape == FormationLayoutShape::Staggered;
        const int scale = staggered ? 4 : 2;
        for (int depth = 1; depth <= 4; depth++) {
            const int population = scale * scale * depth * depth;
            int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
            for (int slot = 0; slot < population; slot++) {
                FormationLayoutPosition position;
                if (!line->try_position(slot, 256, 16, 16, &position)) return false;
                min_x = std::min(min_x, position.x);
                max_x = std::max(max_x, position.x);
                min_y = std::min(min_y, position.y);
                max_y = std::max(max_y, position.y);
                if (slot < 64) {
                    FormationLayoutPosition smaller;
                    if (!line->try_position(slot, 64, 8, 8, &smaller) || smaller.x != position.x || smaller.y != position.y) {
                        errors << "Line geometry moved incumbents when declared capacity changed.\n";
                        return false;
                    }
                }
            }
            const int length = line->geometry().transpose ? max_y - min_y + 1 : max_x - min_x + 1;
            const int thickness = line->geometry().transpose ? max_x - min_x + 1 : max_y - min_y + 1;
            if (length != scale * scale * depth || thickness != (staggered ? 2 : 1) * depth) {
                errors << "A partially filled line lost its declared shape proportions.\n";
                return false;
            }
        }
    }
    const FormationLayoutDef *double_line_1_layout =
        formation_layout_registry_impl::find_layout("double_line_1");
    const FormationLayoutDef *double_line_2_layout =
        formation_layout_registry_impl::find_layout("double_line_2");
    const FormationLayoutDef *enemy_mob_layout =
        formation_layout_registry_impl::find_layout("enemy_mob");
    const FormationLayoutDef *wide_column_layout =
        formation_layout_registry_impl::find_layout("enemy_wide_column");
    const FormationLayoutPosition column_authored = column_layout->army_offset(0, 5);
    const FormationLayoutPosition column_rotated = column_layout->army_offset(3, 6);
    const FormationLayoutPosition reused_mob = enemy_mob_layout ? enemy_mob_layout->army_offset(1, 5) :
        FormationLayoutPosition{};
    const FormationLayoutPosition reused_double_line = double_line_2_layout ?
        double_line_2_layout->army_offset(2, 5) : FormationLayoutPosition{};
    const FormationLayoutPosition authored_wide = wide_column_layout ?
        wide_column_layout->army_offset(3, 6) : FormationLayoutPosition{};
    if (!column_layout->has_authored_army_offsets() ||
        column_authored.x != -3 || column_authored.y != 8 ||
        column_rotated.x != -8 || column_rotated.y != 3 ||
        !enemy_mob_layout || std::string(enemy_mob_layout->army_offsets_reference()) != "column" ||
        reused_mob.x != 8 || reused_mob.y != -3 ||
        !double_line_1_layout || !double_line_2_layout ||
        std::string(double_line_2_layout->army_offsets_reference()) != "double_line_1" ||
        reused_double_line.x != -4 || reused_double_line.y != -6 ||
        !wide_column_layout || !wide_column_layout->has_authored_army_offsets() ||
        authored_wide.x != -12 || authored_wide.y != 4) {
        errors << "FormationLayout army offsets or direct army_from reuse changed.\n";
        return false;
    }
    formation layout_bridge = {};
    if (!layout_bridge.set_layout_from_legacy_id(formation_layout_legacy::TORTOISE) ||
        !layout_bridge.uses_layout("tortoise") ||
        formation_layout_to_legacy_id(layout_bridge.layout_definition) != formation_layout_legacy::TORTOISE ||
        !layout_bridge.set_layout_from_legacy_id(999) ||
        !layout_bridge.uses_layout("column")) {
        errors << "Formation runtime did not resolve legacy layout ids exactly once at the bridge.\n";
        return false;
    }

    return true;
}
