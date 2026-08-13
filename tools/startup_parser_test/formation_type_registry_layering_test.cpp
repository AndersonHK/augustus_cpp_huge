#include "formation_type_registry_layering_test.h"

#include "figure/formation.h"
#include "figure/formation_type.h"

#include <ostream>
#include <string>

namespace {

constexpr const char *LOWER_XML =
    "<formation key=\"archers\"><grid width=\"2\" height=\"1\"/>"
    "<slot fill=\"all\" unit=\"archer\"/></formation>";
constexpr const char *UPPER_XML =
    "<formation key=\"archers\" recruit_capacity=\"2\"><grid width=\"3\" height=\"1\"/>"
    "<slot fill=\"all\" unit=\"archer\"/></formation>";
constexpr const char *DISABLED_XML =
    "<formation key=\"archers\" disabled=\"true\"></formation>";

constexpr const char *LAYOUT_TEST_POSITIONS =
    "<position x=\"0\" y=\"0\"/><position x=\"1\" y=\"0\"/>"
    "<position x=\"2\" y=\"0\"/><position x=\"3\" y=\"0\"/>"
    "<position x=\"0\" y=\"1\"/><position x=\"1\" y=\"1\"/>"
    "<position x=\"2\" y=\"1\"/><position x=\"3\" y=\"1\"/>"
    "<position x=\"0\" y=\"2\"/><position x=\"1\" y=\"2\"/>"
    "<position x=\"2\" y=\"2\"/><position x=\"3\" y=\"2\"/>"
    "<position x=\"0\" y=\"3\"/><position x=\"1\" y=\"3\"/>"
    "<position x=\"2\" y=\"3\"/><position x=\"3\" y=\"3\"/>";

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
        std::to_string(legacy_id) + "\">" + LAYOUT_TEST_POSITIONS + army + "</layout>";
}

std::string referenced_layout_xml(const char *key, int legacy_id, const char *army_from)
{
    return "<layout key=\"" + std::string(key) + "\" legacy_id=\"" +
        std::to_string(legacy_id) + "\" army_from=\"" + army_from + "\">" +
        LAYOUT_TEST_POSITIONS + "</layout>";
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

} // namespace

bool validate_formation_type_registry_layering_contract(std::ostream &errors)
{
    constexpr const char *LAYOUT_LOWER_XML =
        "<layout key=\"column\" legacy_id=\"0\">"
        "<position x=\"0\" y=\"0\"/><position x=\"1\" y=\"0\"/>"
        "<position x=\"0\" y=\"1\"/><position x=\"1\" y=\"1\"/>"
        "<position x=\"-1\" y=\"0\"/><position x=\"-1\" y=\"1\"/>"
        "<position x=\"0\" y=\"-1\"/><position x=\"1\" y=\"-1\"/>"
        "<position x=\"-1\" y=\"-1\"/><position x=\"2\" y=\"-1\"/>"
        "<position x=\"2\" y=\"0\"/><position x=\"2\" y=\"1\"/>"
        "<position x=\"0\" y=\"2\"/><position x=\"1\" y=\"2\"/>"
        "<position x=\"-1\" y=\"2\"/><position x=\"2\" y=\"2\"/>"
        "</layout>";
    constexpr const char *LAYOUT_UPPER_XML =
        "<layout key=\"column\" legacy_id=\"0\">"
        "<position x=\"8\" y=\"8\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/>"
        "</layout>";
    const formation_layout_layer_test_input layout_replacement[] = {
        {LAYOUT_LOWER_XML, 0, "Julius", "Julius/FormationLayout/column.xml"},
        {LAYOUT_UPPER_XML, 1, "Pharaoh", "Pharaoh/FormationLayout/column.xml"}
    };
    formation_layout_layer_test_result layout_result;
    if (!formation_layout_layered_definition_buffers_are_valid_for_test(
            layout_replacement, 2, "column", &layout_result) ||
        layout_result.active_count != 1 || layout_result.suppressed_count != 0 ||
        layout_result.queried_disabled || layout_result.queried_legacy_id != 0 ||
        layout_result.queried_position_count != 16 || layout_result.queried_source_layer != 1) {
        errors << "FormationLayout upper-layer replacement lost authored positions or provenance.\n";
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
        "<layout key=\"other\" legacy_id=\"0\">"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/><position x=\"0\" y=\"0\"/>"
        "<position x=\"0\" y=\"0\"/>"
        "</layout>";
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
        errors << "FormationLayout accepted an incomplete authored position list.\n";
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
        result.queried_source_layer != 1) {
        errors << "FormationType upper-layer replacement lost winner data or provenance.\n";
        return false;
    }

    const formation_type_layer_test_input inherited_only[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FormationType/archers.xml")
    };
    if (!valid(inherited_only, 1, "archers", &result) ||
        result.queried_capacity != 2 || result.queried_recruit_capacity != 2 ||
        result.queried_source_layer != 0) {
        errors << "FormationType lower definition was not inherited when the upper layer was absent.\n";
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
        "<formation key=\"archers\"><grid width=\"1\" height=\"1\"/>"
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
        "<formation key=\"archers\" recruit_capacity=\"3\"><grid width=\"2\" height=\"1\"/>"
        "<slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input oversized_recruitment[] = {
        input(OVERSIZED_RECRUITMENT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(oversized_recruitment, 1, "archers")) {
        errors << "FormationType accepted recruit_capacity larger than its authored formation.\n";
        return false;
    }

    constexpr const char *ZERO_RECRUITMENT =
        "<formation key=\"archers\" recruit_capacity=\"0\"><grid width=\"2\" height=\"1\"/>"
        "<slot fill=\"all\" unit=\"archer\"/></formation>";
    const formation_type_layer_test_input zero_recruitment[] = {
        input(ZERO_RECRUITMENT, 0, "Pharaoh", "Pharaoh/FormationType/archers.xml")
    };
    if (valid(zero_recruitment, 1, "archers")) {
        errors << "FormationType accepted a non-positive recruit_capacity.\n";
        return false;
    }

    for (int legacy_id = 0; legacy_id < formation_layout_legacy::COUNT; ++legacy_id) {
        const FormationLayoutDef *layout =
            formation_layout_registry_impl::find_layout_by_legacy_id(legacy_id);
        if (!layout || layout->legacy_id() != legacy_id || layout->authored_position_count() != 16) {
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
    const FormationLayoutPosition first_column = formation_layout_position(column_layout, 0, 16);
    const FormationLayoutPosition last_authored_column = formation_layout_position(column_layout, 15, 16);
    const FormationLayoutPosition first_extended_column =
        formation_layout_position(column_layout, 16, 64, {8, 8});
    const std::vector<FormationLayoutPosition> clamped_positions =
        formation_layout_positions(column_layout, 70, 64, {8, 8});
    if (!column_layout || first_column.x != 0 || first_column.y != 0 ||
        last_authored_column.x != 2 || last_authored_column.y != 2 ||
        first_extended_column.x != 0 || first_extended_column.y != 2 ||
        clamped_positions.size() != 64) {
        errors << "FormationLayout authored positions or footprint-derived extension changed.\n";
        return false;
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
    const FormationLayoutPosition out_of_range = column_layout->army_offset(0, 7);
    if (!column_layout->has_authored_army_offsets() ||
        column_authored.x != -3 || column_authored.y != 8 ||
        column_rotated.x != -8 || column_rotated.y != 3 ||
        !enemy_mob_layout || std::string(enemy_mob_layout->army_offsets_reference()) != "column" ||
        reused_mob.x != 8 || reused_mob.y != -3 ||
        !double_line_1_layout || !double_line_2_layout ||
        std::string(double_line_2_layout->army_offsets_reference()) != "double_line_1" ||
        reused_double_line.x != -4 || reused_double_line.y != -6 ||
        !wide_column_layout || !wide_column_layout->has_authored_army_offsets() ||
        authored_wide.x != -12 || authored_wide.y != 4 ||
        out_of_range.x != 0 || out_of_range.y != 0) {
        errors << "FormationLayout army offsets or direct army_from reuse changed.\n";
        return false;
    }
    formation layout_bridge = {};
    if (!layout_bridge.set_layout_from_legacy_id(formation_layout_legacy::TORTOISE) ||
        !layout_bridge.uses_layout("tortoise") ||
        formation_layout_to_legacy_id(layout_bridge.layout_type()) != formation_layout_legacy::TORTOISE ||
        !layout_bridge.set_layout_from_legacy_id(999) ||
        !layout_bridge.uses_layout("column")) {
        errors << "Formation runtime did not resolve legacy layout ids exactly once at the bridge.\n";
        return false;
    }

    formation unbounded_roster = {};
    unbounded_roster.add_figure_to_roster(11, 0, 0, 1);
    unbounded_roster.add_figure_to_roster(12, 0, 0, 1);
    unbounded_roster.add_figure_to_roster(13, 0, 0, 1);
    if (unbounded_roster.figure_count() != 3 || unbounded_roster.figures.size() != 3 ||
        unbounded_roster.figures[2] != 13 ||
        !unbounded_roster.has_open_slot() || unbounded_roster.overflow_count()) {
        errors << "Unbounded runtime formation roster retained an implicit legacy-size limit.\n";
        return false;
    }

    formation bounded_roster = {};
    bounded_roster.max_figures = 2;
    bounded_roster.add_figure_to_roster(21, 0, 0, 1);
    bounded_roster.add_figure_to_roster(22, 0, 0, 1);
    bounded_roster.add_figure_to_roster(23, 0, 0, 1);
    if (!bounded_roster.is_full() || bounded_roster.overflow_count() != 1 ||
        bounded_roster.figures.size() != 3 || bounded_roster.figures[2] != 23) {
        errors << "Bounded runtime formation roster lost declared overflow ownership.\n";
        return false;
    }

    return true;
}
