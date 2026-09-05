#include "water_access_type_layering_test.h"

#include "building/water_access_type.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *WELL_XML =
    "<water_access_type text_id=\"well\" number_id=\"0\"></water_access_type>";
constexpr const char *WELL_TOMBSTONE_XML =
    "<water_access_type text_id=\"well\" number_id=\"0\" disabled=\"true\"></water_access_type>";
constexpr const char *FOUNTAIN_XML =
    "<water_access_type text_id=\"fountain\" number_id=\"1\"></water_access_type>";

water_access_type_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *source,
    const char *definition_path)
{
    return {xml, layer, mod, source, definition_path};
}

bool valid(
    const water_access_type_layer_test_input *inputs,
    int count,
    const char *query,
    water_access_type_layer_test_result *result = nullptr)
{
    return water_access_type_layered_definition_buffers_are_valid_for_test(
               inputs, count, query, result) != 0;
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_water_access_type_layers_" + std::to_string(suffix));
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

bool write_file(const std::filesystem::path &path, const char *contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    output << (contents ? contents : "");
    return output.good();
}

} // namespace

bool validate_water_access_type_layering_contract(std::ostream &errors)
{
    const water_access_type_layer_test_input replacement[] = {
        input(WELL_XML, 0, "Julius", "Julius/WaterAccessType/well.xml", "well"),
        input(WELL_XML, 1, "Vespasian", "Vespasian/WaterAccessType/well.xml", "well"),
    };
    water_access_type_layer_test_result result;
    if (!valid(replacement, 2, "well", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_number_id != 0 || result.defined_mask != 1) {
        errors << "WaterAccessType upper-layer winner or provenance was not authoritative.\n";
        return false;
    }

    const water_access_type_layer_test_input suppression[] = {
        input(WELL_XML, 0, "Julius", "Julius/WaterAccessType/well.xml", "well"),
        input(
            WELL_TOMBSTONE_XML,
            1,
            "Vespasian",
            "Vespasian/WaterAccessType/well.xml",
            "well"),
    };
    if (!valid(suppression, 2, "well", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_number_id != 0 || result.defined_mask != 0) {
        errors << "WaterAccessType identity-only tombstone did not suppress its inherited definition.\n";
        return false;
    }

    const water_access_type_layer_test_input duplicate[] = {
        input(WELL_XML, 0, "Julius", "Julius/WaterAccessType/well.xml", "well"),
        input(WELL_XML, 0, "Julius", "Julius/WaterAccessType/well-copy.xml", "well"),
    };
    if (valid(duplicate, 2, "well")) {
        errors << "WaterAccessType same-layer normalized-path duplicate was accepted.\n";
        return false;
    }

    constexpr const char *MALFORMED_TOMBSTONE_XML =
        "<water_access_type text_id=\"well\" number_id=\"0\" disabled=\"true\">"
        "<definition/>"
        "</water_access_type>";
    const water_access_type_layer_test_input malformed_tombstone[] = {
        input(
            MALFORMED_TOMBSTONE_XML,
            0,
            "Julius",
            "Julius/WaterAccessType/well.xml",
            "well"),
    };
    if (valid(malformed_tombstone, 1, "well")) {
        errors << "WaterAccessType tombstone accepted non-identity definition content.\n";
        return false;
    }

    constexpr const char *MOVED_WELL_XML =
        "<water_access_type text_id=\"well\" number_id=\"2\"></water_access_type>";
    const water_access_type_layer_test_input moved_identity[] = {
        input(WELL_XML, 0, "Julius", "Julius/WaterAccessType/well.xml", "well"),
        input(MOVED_WELL_XML, 1, "Vespasian", "Vespasian/WaterAccessType/well.xml", "well"),
    };
    if (valid(moved_identity, 2, "well")) {
        errors << "WaterAccessType replacement changed its stable numeric identity.\n";
        return false;
    }

    constexpr const char *FOUNTAIN_COLLISION_XML =
        "<water_access_type text_id=\"fountain\" number_id=\"0\"></water_access_type>";
    constexpr const char *FOUNTAIN_COLLISION_TOMBSTONE_XML =
        "<water_access_type text_id=\"fountain\" number_id=\"0\" disabled=\"true\"></water_access_type>";
    const water_access_type_layer_test_input final_winner_validation[] = {
        input(WELL_XML, 0, "Julius", "Julius/WaterAccessType/well.xml", "well"),
        input(
            FOUNTAIN_COLLISION_XML,
            0,
            "Julius",
            "Julius/WaterAccessType/fountain.xml",
            "fountain"),
        input(
            FOUNTAIN_COLLISION_TOMBSTONE_XML,
            1,
            "Vespasian",
            "Vespasian/WaterAccessType/fountain.xml",
            "fountain"),
    };
    if (!valid(final_winner_validation, 3, "well", &result) ||
        result.active_count != 1 || result.suppressed_count != 1 || result.defined_mask != 1) {
        errors << "WaterAccessType numeric-slot validation ran before final winners existed.\n";
        return false;
    }

    const water_access_type_layer_test_input path_mismatch[] = {
        input(FOUNTAIN_XML, 0, "Julius", "Julius/WaterAccessType/well.xml", "well"),
    };
    if (valid(path_mismatch, 1, "well")) {
        errors << "WaterAccessType text identity was allowed to diverge from its normalized path.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create WaterAccessType missing-directory fixture.\n";
        return false;
    }
    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code error;
    std::filesystem::create_directories(missing_upper, error);
    if (error || !write_file(lower / "WaterAccessType" / "well.xml", WELL_XML)) {
        errors << "Unable to write WaterAccessType missing-directory fixture.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Lower", lower.string()},
        {"MissingUpper", missing_upper.string()},
    };
    std::string failure;
    if (!water_access_type_layered_definition_files_are_valid_for_test(
            layers, "well", &result, &failure) ||
        result.active_count != 1 || result.queried_source_layer != 0 ||
        result.queried_number_id != 0 || result.defined_mask != 1) {
        errors << "WaterAccessType missing upper directory did not preserve inheritance: " << failure << '\n';
        return false;
    }

    return true;
}
