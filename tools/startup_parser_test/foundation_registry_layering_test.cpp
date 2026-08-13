#include "foundation_registry_layering_test.h"

#include "building/FoundationRegistry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_foundation_layers_" + std::to_string(suffix));
        std::error_code error;
        std::filesystem::create_directories(path_, error);
        valid_ = !error;
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path &path() const { return path_; }
    bool valid() const { return valid_; }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

bool write_file(const std::filesystem::path &path, const std::string &contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    output << contents;
    return output.good();
}

std::string foundation_xml(const char *type, int width)
{
    return
        "<foundation type=\"" + std::string(type) + "\" width=\"" +
        std::to_string(width) + "\" height=\"1\" rotates=\"true\">"
        "<profile symbol=\"B\" requires=\"land\" adds=\"building\"/>"
        "<rows><row cells=\"" + std::string(static_cast<std::size_t>(width), 'B') +
        "\"/></rows></foundation>\n";
}

mod_definition::DefinitionLayer layer(const char *name, const std::filesystem::path &path)
{
    return {name ? name : "", path.string()};
}

foundation_layer_test_input input(
    const char *xml,
    int layer_index,
    const char *mod,
    const char *definition_path,
    const char *source_path)
{
    return {xml, layer_index, mod, definition_path, source_path};
}

bool validate_buffer_stacks(std::ostream &errors)
{
    const std::string lower_xml = foundation_xml("layered", 1);
    const std::string upper_xml = foundation_xml("layered", 2);
    constexpr const char *TOMBSTONE =
        "<foundation type=\"layered\" disabled=\"true\"></foundation>";

    const foundation_layer_test_input replacement[] = {
        input(lower_xml.c_str(), 0, "Lower", "layered", "Lower/Foundations/layered.xml"),
        input(upper_xml.c_str(), 1, "Upper", "layered", "Upper/Foundations/layered.xml")
    };
    foundation_layer_test_result result;
    if (!foundation_layered_definition_buffers_are_valid_for_test(
            replacement, 2, "layered", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 ||
        result.queried_disabled || result.queried_width != 2 ||
        result.queried_source_layer != 1) {
        errors << "Foundation buffered upper-layer replacement or provenance failed.\n";
        return false;
    }

    const foundation_layer_test_input suppression[] = {
        replacement[0],
        input(TOMBSTONE, 1, "Upper", "layered", "Upper/Foundations/layered.xml")
    };
    if (!foundation_layered_definition_buffers_are_valid_for_test(
            suppression, 2, "layered", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 ||
        !result.queried_disabled || result.queried_width != 0 ||
        result.queried_source_layer != 1) {
        errors << "Foundation buffered tombstone suppression failed.\n";
        return false;
    }

    const foundation_layer_test_input duplicate[] = {
        input(lower_xml.c_str(), 0, "Lower", "layered", "Lower/Foundations/layered-a.xml"),
        input(upper_xml.c_str(), 0, "Lower", "layered", "Lower/Foundations/layered-b.xml")
    };
    if (foundation_layered_definition_buffers_are_valid_for_test(
            duplicate, 2, "layered", nullptr)) {
        errors << "Foundation same-layer duplicate identity was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_TOMBSTONE_ATTRIBUTE =
        "<foundation type=\"layered\" disabled=\"true\" width=\"1\"></foundation>";
    const foundation_layer_test_input invalid_tombstone_attribute[] = {
        input(INVALID_TOMBSTONE_ATTRIBUTE, 0, "Lower", "layered", "Lower/Foundations/layered.xml")
    };
    if (foundation_layered_definition_buffers_are_valid_for_test(
            invalid_tombstone_attribute, 1, "layered", nullptr)) {
        errors << "Foundation tombstone accepted geometry attributes.\n";
        return false;
    }

    constexpr const char *INVALID_TOMBSTONE_CONTENT =
        "<foundation type=\"layered\" disabled=\"true\"><rows/></foundation>";
    const foundation_layer_test_input invalid_tombstone_content[] = {
        input(INVALID_TOMBSTONE_CONTENT, 0, "Lower", "layered", "Lower/Foundations/layered.xml")
    };
    if (foundation_layered_definition_buffers_are_valid_for_test(
            invalid_tombstone_content, 1, "layered", nullptr)) {
        errors << "Foundation tombstone accepted definition content.\n";
        return false;
    }

    const foundation_layer_test_input mismatched_identity[] = {
        input(lower_xml.c_str(), 0, "Lower", "different_path", "Lower/Foundations/different_path.xml")
    };
    if (foundation_layered_definition_buffers_are_valid_for_test(
            mismatched_identity, 1, "layered", nullptr)) {
        errors << "Foundation type/path mismatch was accepted.\n";
        return false;
    }
    return true;
}

} // namespace

bool validate_foundation_registry_layering_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create Foundation layering fixture directory.\n";
        return false;
    }

    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path upper = temporary.path() / "Upper";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code directory_error;
    std::filesystem::create_directories(missing_upper, directory_error);
    if (directory_error ||
        !write_file(lower / "Foundations" / "layered.xml", foundation_xml("layered", 1)) ||
        !write_file(lower / "Foundations" / "removed.xml", foundation_xml("removed", 1)) ||
        !write_file(upper / "Foundations" / "layered.xml", foundation_xml("layered", 2)) ||
        !write_file(
            upper / "Foundations" / "removed.xml",
            "<foundation type=\"removed\" disabled=\"true\"></foundation>\n")) {
        errors << "Unable to write Foundation layering fixtures.\n";
        return false;
    }

    const std::vector<mod_definition::DefinitionLayer> layers = {
        layer("Lower", lower),
        layer("Upper", upper),
        layer("MissingUpper", missing_upper)
    };
    std::string failure;
    if (!foundation_registry_load_layers(layers, &failure)) {
        errors << "Foundation sparse layering failed: " << failure << '\n';
        return false;
    }

    const FoundationDef *replacement = find_foundation_definition("layered");
    const mod_definition::DefinitionOverlayEntry *replacement_overlay =
        find_foundation_definition_overlay("layered");
    const mod_definition::DefinitionOverlayEntry *removed_overlay =
        find_foundation_definition_overlay("removed");
    if (!replacement || replacement->width() != 2 || foundation_definitions().size() != 1 ||
        find_foundation_definition("removed") || !replacement_overlay ||
        replacement_overlay->disabled || replacement_overlay->source.layer_index != 1 ||
        replacement_overlay->source.mod_name != "Upper" || !removed_overlay ||
        !removed_overlay->disabled || removed_overlay->source.layer_index != 1) {
        errors << "Foundation replacement, tombstone, missing-directory, or provenance layering failed.\n";
        return false;
    }

    const std::filesystem::path invalid = temporary.path() / "Invalid";
    if (!write_file(
            invalid / "Foundations" / "layered.xml",
            foundation_xml("wrong_identity", 3))) {
        errors << "Unable to write invalid Foundation identity fixture.\n";
        return false;
    }
    failure.clear();
    if (foundation_registry_load_layers({layer("Invalid", invalid)}, &failure) ||
        failure.empty() || find_foundation_definition("layered") != replacement ||
        find_foundation_definition_overlay("layered") != replacement_overlay) {
        errors << "Foundation failed load was not diagnostic and atomic.\n";
        return false;
    }

    return validate_buffer_stacks(errors);
}
